#include <emailkit/global.hpp>

#include "imap_parser.hpp"

#include <emailkit/log.hpp>
#include <emailkit/utils.hpp>

#include "utils.hpp"

#include <ast.h>     // apg70
#include <parser.h>  // apg70
#include <utilities.h>
#include "imap_parser_apg_impl.h"  // grammar generated for apg70

#include <function2/function2.hpp>
#include <vector>

#include <cstdlib>

namespace emailkit::imap_parser {

namespace {
std::string format_apg_parser_state(const parser_state& state) {
    return fmt::format(
        "uiState: {}, phrase_len: {}, string_len: {}, max_tree_depth: {}, hit_count: {}",
        state.uiState, state.uiPhraseLength, state.uiStringLength, state.uiMaxTreeDepth,
        state.uiHitCount);
}

std::string format_apg_exception(const exception& e) {
    return fmt::format("{}:{}: {}: {}", e.caFile, e.uiLine, e.caFunc, e.caMsg);
}

struct rule_and_callback {
    rule_and_callback(aint rule, fu2::function_view<void(std::string_view)> cb_ref)
        : rule(rule), cb_ref(cb_ref) {}

    aint rule;
    fu2::function_view<void(std::string_view)> cb_ref;  // non-owning callback
};

struct ast_parse_invoke_user_data_t {
    std::optional<uint32_t> literal_size_opt;
};

auto udt_literal_size_callback(callback_data* spData) -> void {
    const achar* begin = &spData->acpString[spData->uiParserOffset];
    const achar* end = spData->acpString + spData->uiStringLength;

    auto* user_data_ptr = reinterpret_cast<ast_parse_invoke_user_data_t*>(spData->vpUserData);
    if (!user_data_ptr) {
        log_error("logic error, no user data");
        return;
    }

    if (user_data_ptr->literal_size_opt.has_value()) {
        log_warning("overwriting previous literal size with new one");
    }

    spData->uiCallbackState = ID_NOMATCH;
    spData->uiCallbackPhraseLength = 0;

    const achar* curr = begin;
    uint32_t value = 0;
    while (curr != end && ::isdigit(*curr)) {
        value = value * 10 + (*curr - '0');
        ++curr;
    }

    if (curr != begin) {
        // have a match
        spData->uiCallbackState = ID_MATCH;
        // of length
        spData->uiCallbackPhraseLength = curr - begin;
        log_debug("matched '{}', value is: {}", std::string{begin, curr}, value);
        user_data_ptr->literal_size_opt = value;
    }
}

auto udt_literal_data_callback(callback_data* spData) -> void {
    const achar* begin = &spData->acpString[spData->uiParserOffset];
    const achar* end = spData->acpString + spData->uiStringLength;

    spData->uiCallbackState = ID_NOMATCH;
    spData->uiCallbackPhraseLength = 0;

    auto* user_data_ptr = reinterpret_cast<ast_parse_invoke_user_data_t*>(spData->vpUserData);
    if (!user_data_ptr) {
        log_error("logic error, no user data");
        return;
    }
    if (!user_data_ptr->literal_size_opt.has_value()) {
        log_warning("attmept to match data without data size hit");
        return;
    }

    log_debug("have pending data size: {}", user_data_ptr->literal_size_opt.value_or(0));

    // log_debug("matching '{}' against u_literal-data UDT", std::string{begin, end});
    const size_t literal_size_bytes = user_data_ptr->literal_size_opt.value_or(0);
    if ((end - begin) < literal_size_bytes) {
        log_error("not enough data bytes, literal size is {} while available only {} bytes",
                  literal_size_bytes, end - begin);
        return;
    }
    // TODO: we still need to validate that data is OK and has no zero but it can
    // possibly be relaxed to speed up things if needed.
    if (!std::all_of(begin, begin + literal_size_bytes, [](unsigned char c) {
            return c >= 0x01 && c <= 0xff;  // see CHAR8 rule.
        })) {
        log_error("not all bytes are correct");
        return;
    }

    // note, since we matched CHAR8 we most probably consumed everything until the end
    // of input allready. This is exactly the original problem with greedy matching and
    // greedy rule.

    log_debug("have match of size {}: '{}'", literal_size_bytes,
              std::string{begin, begin + literal_size_bytes});

    spData->uiCallbackState = ID_MATCH;
    spData->uiCallbackPhraseLength = literal_size_bytes;
}

void apg_invoke_parser(uint32_t starting_rule,
                       std::string_view input_text,
                       std::initializer_list<rule_and_callback> cbs) {
    struct apg_invoke_context {
        // registered callbacks for which we set parsers C-callbacks.
        std::vector<fu2::function_view<void(std::string_view)>> callbacks_map{
            RULE_COUNT_IMAP_PARSER_APG_IMPL};
    };

    apg_invoke_context ctx;
    log_debug("started parsing: '{}'", input_text);

    //////////////////////////////////////////////////////////////////////////////////////////////////
    // WARNING: since apg uses longjmp we should take care of c++ destructors and make sure that our
    // objects lifetimes reside after try/catch of apg.
    // Block for C++ resources that need to have destructors.
    //////////////////////////////////////////////////////////////////////////////////////////////////
    std::vector<uint8_t> input_text_data(input_text.size());
    for (size_t i = 0; i < input_text.size(); ++i) {
        input_text_data[i] = input_text[i];
    }
    //////////////////////////////////////////////////////////////////

    parser_state apg_parser_state;
    parser_config apg_parser_config;
    exception apg_exception;
    void* parser = nullptr;
    void* vpTrace = nullptr;

    // XCTOR macros sets kind of label (setjmp) that can be jumped to. So in case exception occurs
    // in APG it will jump back (longjmp) to this label but this time apg_exception.try_ will be set
    // to FALSE.
    XCTOR(apg_exception);
    if (apg_exception.try_) {
        log_debug("constructing APG parser object");
        parser = ::vpParserCtor(&apg_exception, vpImapParserApgImplInit);
        log_debug("constructing APG parser object -- done");

        if (std::getenv("MMAP_TRACE")) {
            vpTrace = vpTraceCtor(parser);
            vTraceConfigGen(vpTrace, NULL);
        }

        ::vParserSetUdtCallback(parser, IMAP_PARSER_APG_IMPL_U_LITERAL_SIZE,
                                &udt_literal_size_callback);
        ::vParserSetUdtCallback(parser, IMAP_PARSER_APG_IMPL_U_LITERAL_DATA,
                                &udt_literal_data_callback);

        // Set single callback for each rule and use context for routing to user-defined callbacks.
        for (auto& [rule, callback_ref] : cbs) {
            ctx.callbacks_map[rule] = callback_ref;
            ::vParserSetRuleCallback(
                parser, rule, +[](callback_data* cb_data) {
                    auto* invoke_ctx = static_cast<apg_invoke_context*>(cb_data->vpUserData);
                    if (cb_data->uiParserState == ID_MATCH) {
                        // log_debug(
                        //     "uiParserState: {}, uiCallbackState: {}: uiCallbackPhraseLength: {}",
                        //     cb_data->uiParserState, cb_data->uiCallbackState,
                        //     cb_data->uiCallbackPhraseLength);
                        auto& callbacks_map = invoke_ctx->callbacks_map;

                        if (cb_data->uiRuleIndex >= callbacks_map.size()) {
                            log_error(
                                "FATAL: something went wrong. uiRuleIndex: {}, callback map size: "
                                "{}",
                                cb_data->uiRuleIndex, callbacks_map.size());
                            return;
                        }

                        if (callbacks_map[cb_data->uiRuleIndex]) {
                            const char* match_begin =
                                reinterpret_cast<const char*>(cb_data->acpString) +
                                cb_data->uiParserOffset;

                            std::string_view match_sv{match_begin, cb_data->uiParserPhraseLength};
                            callbacks_map[cb_data->uiRuleIndex](match_sv);
                        }
                    }
                });
        }

        apg_parser_config.acpInput = input_text_data.data();
        apg_parser_config.uiInputLength = input_text_data.size();
        apg_parser_config.uiStartRule = starting_rule;
        apg_parser_config.bParseSubString = APG_FALSE;
        apg_parser_config.uiLookBehindLength = 0;
        apg_parser_config.vpUserData = &ctx;

        log_debug("invoking APG parser");
        ::vParserParse(parser, &apg_parser_config, &apg_parser_state);
        if (!apg_parser_state.uiSuccess) {
            log_error("invoking APG parser -- error; parser state: {}",
                      format_apg_parser_state(apg_parser_state));
        } else {
            log_debug("invoking APG parser -- done");
        }
    } else {
        log_error("APG EXCEPTION: {}", format_apg_exception(apg_exception));
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    // WARNING: scope guarding technique is specifically not used to emphasize that we want to have
    // our parser return at the end of the function.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    if (parser) {
        ::vParserDtor(parser);
    }
}

// FIXME: because of some bug in grammar/parser flags rule is hit twice so we work around this.
void emplace_unique(std::vector<std::string>& v, std::string_view tok) {
    if (std::find_if(v.begin(), v.end(), [&tok](auto& x) { return x == tok; }) == v.end())
        v.emplace_back(tok);
}

}  // namespace

expected<list_response_t> parse_list_response_line(std::string_view input) {
    list_response_t parsed_line;

    bool list_matched =
        false;  // flag tells that among all mailbox alternatives exactly LIST matched

    bool flags_list_done = false;

    apg_invoke_parser(IMAP_PARSER_APG_IMPL_MAILBOX_DATA, input,
                      {
                          {IMAP_PARSER_APG_IMPL_MAILBOX_LIST,
                           [&](std::string_view tok) {
                               log_debug("IMAP_PARSER_APG_IMPL_MAILBOX_LIST: '{}'", tok);
                               list_matched = true;
                           }},

                          {IMAP_PARSER_APG_IMPL_OPEN_BRACE,
                           [&](std::string_view tok) {
                               log_debug("IMAP_PARSER_APG_IMPL_OPEN_BRACE: '{}'", tok);
                           }},
                          {IMAP_PARSER_APG_IMPL_CLOSE_BRACE,
                           [&](std::string_view tok) {
                               log_debug("IMAP_PARSER_APG_IMPL_CLOSE_BRACE: '{}'", tok);
                               flags_list_done = true;
                           }},

                          {IMAP_PARSER_APG_IMPL_DQUOTE,
                           [&](std::string_view tok) {
                               //    if (flags_list_done) {
                               //        log_debug("DQUOTE: '{}'", tok);
                               //        dquote_count++;
                               //    }
                           }},

                          {IMAP_PARSER_APG_IMPL_MBX_LIST_FLAGS,
                           [&](std::string_view tok) {
                               log_debug("IMAP_PARSER_APG_IMPL_MBX_LIST_FLAGS: '{}'", tok);
                               flags_list_done = true;
                           }},

                          {IMAP_PARSER_APG_IMPL_QUOTED_CHAR,
                           [&](std::string_view tok) {
                               // log_debug("IMAP_PARSER_APG_IMPL_QUOTED_CHAR: '{}'", tok);
                               // if (flags_list_done && dquote_count == 1) {
                               if (flags_list_done && parsed_line.hierarchy_delimiter.empty()) {
                                   parsed_line.hierarchy_delimiter = tok;
                                   // log_warning("this is our delimiter");
                               }
                           }},
                          {IMAP_PARSER_APG_IMPL_NIL,
                           [&](std::string_view tok) {
                               log_debug("IMAP_PARSER_APG_IMPL_NIL: '{}'", tok);
                               if (flags_list_done) {
                                   log_warning("our delimited is NIL");
                               }
                           }},

                          {IMAP_PARSER_APG_IMPL_MBX_LIST_OFLAG,
                           [&](std::string_view tok) {
                               log_debug("IMAP_PARSER_APG_IMPL_MBX_LIST_OFLAG: '{}'", tok);
                               emplace_unique(parsed_line.mailbox_list_flags, tok);
                           }},
                          {IMAP_PARSER_APG_IMPL_MBX_LIST_SFLAG,
                           [&](std::string_view tok) {
                               log_debug("IMAP_PARSER_APG_IMPL_MBX_LIST_SFLAG: '{}'", tok);
                               emplace_unique(parsed_line.mailbox_list_flags, tok);
                           }},
                          {IMAP_PARSER_APG_IMPL_MAILBOX,
                           [&](std::string_view tok) {
                               // TODO: it might be possible to negotiate encoding to utf8 before so
                               // this should be passed as flag here if we want to avoid dealing
                               // with utf7.
                               log_debug("IMAP_PARSER_APG_IMPL_MAILBOX: '{}'", tok);

                               auto stripped_tok = emailkit::utils::strip_double_quotes(tok);
                               parsed_line.mailbox = stripped_tok;
                           }},
                      });

    if (!list_matched) {
        // return llvm::createStringError("list not matched");
        return unexpected(make_error_code(std::errc::io_error));
    }

    return parsed_line;
}

namespace {

struct rule_and_callback__ast {
    rule_and_callback__ast(aint rule, fu2::function_view<void(std::string_view)> cb_ref)
        : rule(rule), cb_ref(cb_ref) {}

    aint rule;
    fu2::function_view<void(std::string_view)> cb_ref;  // non-owning callback
};

std::error_code apg_invoke_parser__ast(
    uint32_t starting_rule,
    std::string_view input_text,
    std::initializer_list<aint> rules,
    fu2::function_view<void(const ast_record*, const ast_record*)> ast_cb) {
    struct apg_invoke_context {
        // registered callbacks for which we set parsers C-callbacks.
        // std::vector<fu2::function_view<void(std::string_view)>> callbacks_map{
        //     RULE_COUNT_IMAP_PARSER_APG_IMPL};
    };

    apg_invoke_context ctx;
    log_debug("started parsing: '{}'", input_text);

    //////////////////////////////////////////////////////////////////////////////////////////////////
    // WARNING: since apg uses longjmp we should take care of c++ destructors and make sure that our
    // objects lifetimes reside after try/catch of apg.
    // Block for C++ resources that need to have destructors.
    //////////////////////////////////////////////////////////////////////////////////////////////////
    std::vector<uint8_t> input_text_data(input_text.size());
    for (size_t i = 0; i < input_text.size(); ++i) {
        input_text_data[i] = input_text[i];
    }
    //////////////////////////////////////////////////////////////////

    parser_state apg_parser_state;
    parser_config apg_parser_config;
    exception apg_exception;
    void* parser = nullptr;
    void* vpTrace = nullptr;
    void* ast = NULL;

    std::error_code result{};

    XCTOR(apg_exception);
    if (apg_exception.try_) {
        log_debug("APG TRY section begin");

        log_debug("constructing APG parser object");
        parser = ::vpParserCtor(&apg_exception, vpImapParserApgImplInit);
        log_debug("constructing APG parser object -- done");

        if (std::getenv("MMAP_TRACE")) {
            vpTrace = vpTraceCtor(parser);
            vTraceConfigGen(vpTrace, NULL);
        }

        log_debug("constructing APG AST object");
        ast = ::vpAstCtor(parser);
        log_debug("constructing APG AST object -- done");

        ::vParserSetUdtCallback(parser, IMAP_PARSER_APG_IMPL_U_LITERAL_SIZE,
                                &udt_literal_size_callback);
        ::vParserSetUdtCallback(parser, IMAP_PARSER_APG_IMPL_U_LITERAL_DATA,
                                &udt_literal_data_callback);

        for (auto rule : rules) {
            ::vAstSetRuleCallback(
                ast, rule, +[](ast_data* ast_data_ptr) -> aint { return ID_AST_OK; });
        }

        ast_parse_invoke_user_data_t parsing_user_data;

        apg_parser_config.acpInput = input_text_data.data();
        apg_parser_config.uiInputLength = input_text_data.size();
        apg_parser_config.uiStartRule = starting_rule;
        apg_parser_config.bParseSubString = APG_FALSE;
        apg_parser_config.uiLookBehindLength = 0;
        apg_parser_config.vpUserData = &parsing_user_data;
        //            NULL;  // not used for AST, instead passed to translate function

        log_debug("invoking APG parser");
        ::vParserParse(parser, &apg_parser_config, &apg_parser_state);
        if (!apg_parser_state.uiSuccess) {
            log_error("invoking APG parser -- error; parser state: {}",
                      format_apg_parser_state(apg_parser_state));
            result = make_error_code(std::errc::protocol_error);
        } else {
            log_debug("invoking APG parser -- done");

            // translate the AST
            log_debug("translating AST");
            ::vAstTranslate(ast, &ctx);
            log_debug("translating AST -- done");

            ast_info info;
            ::vAstInfo(ast, &info);
            size_t indent = 0;
            constexpr size_t INDENT_WIDTH = 4;

            ast_cb(&(info.spRecords[0]), &(info.spRecords[info.uiRecordCount]));
        }

        log_debug("APG TRY section end");
    } else {
        log_error("APG EXCEPTION: {}", format_apg_exception(apg_exception));
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    // WARNING: scope guarding technique is specifically not used to emphasize that we want to have
    // our parser return at the end of the function.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    if (ast) {
        ::vAstDtor(ast);
    }

    if (parser) {
        ::vParserDtor(parser);
    }

    return result;
}

}  // namespace

expected<std::vector<mailbox_data_t>> parse_mailbox_data_records(std::string_view input_text) {
    std::vector<mailbox_data_t> parsed_records;

    std::vector<int> current_path;

    auto current_path_is = [&current_path](std::initializer_list<int> l) {
        return std::equal(current_path.begin(), current_path.end(), l.begin(), l.end());
    };

    flags_mailbox_data_t current_flags;
    permanent_flags_mailbox_data_t current_permanent_flags;

    auto ec = apg_invoke_parser__ast(
        IMAP_PARSER_APG_IMPL_RESPONSE, input_text,
        {
            // clang-format off
            IMAP_PARSER_APG_IMPL_MAILBOX_DATA,
            IMAP_PARSER_APG_IMPL_FLAG_LIST,
            IMAP_PARSER_APG_IMPL_FLAG,
            IMAP_PARSER_APG_IMPL_RESP_COND_STATE,
            IMAP_PARSER_APG_IMPL_RESP_TEXT,
            IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE,
            IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_PERMANENT_FLAGS,
            IMAP_PARSER_APG_IMPL_FLAG_PERM,
            IMAP_PARSER_APG_IMPL_MAILBOX_DATA_EXISTS,
            IMAP_PARSER_APG_IMPL_MAILBOX_DATA_RECENT,
            IMAP_PARSER_APG_IMPL_NUMBER,
            IMAP_PARSER_APG_IMPL_OK,
            IMAP_PARSER_APG_IMPL_NO,
            IMAP_PARSER_APG_IMPL_BAD,
            IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_UIDVALIDITY,
            IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_UNSEEN,
            IMAP_PARSER_APG_IMPL_NZ_NUMBER,
            IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_UID_NEXT,
            IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_READ_WRITE,
            IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_READ_ONLY,
            IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_TRY_CREATE,
        },
        // clang-format on
        [&](const ast_record* begin, const ast_record* end) {
            constexpr size_t INDENT_WIDTH = 4;
            size_t indent = 0;
            for (auto it = begin; it != end; ++it) {
                if (it->uiState == ID_AST_PRE) {
                    current_path.emplace_back(it->uiIndex);

                    std::string_view match_text{input_text.data() + it->uiPhraseOffset,
                                                it->uiPhraseLength};

                    if (current_path_is(
                            {IMAP_PARSER_APG_IMPL_MAILBOX_DATA, IMAP_PARSER_APG_IMPL_FLAG_LIST})) {
                        current_flags = {};
                    } else if (current_path_is(
                                   {IMAP_PARSER_APG_IMPL_RESP_COND_STATE,
                                    IMAP_PARSER_APG_IMPL_RESP_TEXT,
                                    IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE,
                                    IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_PERMANENT_FLAGS})) {
                        current_permanent_flags = {};
                    } else if (current_path_is({IMAP_PARSER_APG_IMPL_MAILBOX_DATA,
                                                IMAP_PARSER_APG_IMPL_FLAG_LIST,
                                                IMAP_PARSER_APG_IMPL_FLAG})) {
                        current_flags.flags_vec.emplace_back(std::string{match_text});
                    } else if (current_path_is({IMAP_PARSER_APG_IMPL_RESP_COND_STATE,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_PERMANENT_FLAGS,
                                                IMAP_PARSER_APG_IMPL_FLAG}) ||
                               current_path_is({IMAP_PARSER_APG_IMPL_RESP_COND_STATE,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_PERMANENT_FLAGS,
                                                IMAP_PARSER_APG_IMPL_FLAG_PERM})) {
                        current_permanent_flags.flags_vec.emplace_back(std::string{match_text});
                    } else if (current_path_is({IMAP_PARSER_APG_IMPL_RESP_COND_STATE,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_UIDVALIDITY,
                                                IMAP_PARSER_APG_IMPL_NZ_NUMBER})) {
                        // TODO: fix narrowing.
                        parsed_records.emplace_back(
                            uidvalidity_data_t{.value = std::stoul(std::string{match_text})});

                    } else if (current_path_is({IMAP_PARSER_APG_IMPL_RESP_COND_STATE,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_UNSEEN,
                                                IMAP_PARSER_APG_IMPL_NZ_NUMBER})) {
                        parsed_records.emplace_back(
                            unseen_resp_text_code_t{.value = std::stoul(std::string{match_text})});

                    } else if (current_path_is({IMAP_PARSER_APG_IMPL_RESP_COND_STATE,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_UID_NEXT,
                                                IMAP_PARSER_APG_IMPL_NZ_NUMBER})) {
                        parsed_records.emplace_back(
                            uidnext_resp_text_code_t{.value = std::stoul(std::string{match_text})});

                    } else if (current_path_is({IMAP_PARSER_APG_IMPL_RESP_COND_STATE,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_READ_ONLY})) {
                        parsed_records.emplace_back(read_only_resp_text_code_t{});

                    } else if (current_path_is({IMAP_PARSER_APG_IMPL_RESP_COND_STATE,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_READ_WRITE})) {
                        parsed_records.emplace_back(read_write_resp_text_code_t{});

                    } else if (current_path_is({IMAP_PARSER_APG_IMPL_RESP_COND_STATE,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_TRY_CREATE})) {
                        parsed_records.emplace_back(try_create_resp_text_code_t{});

                    } else if (current_path_is({IMAP_PARSER_APG_IMPL_MAILBOX_DATA,
                                                IMAP_PARSER_APG_IMPL_MAILBOX_DATA_RECENT,
                                                IMAP_PARSER_APG_IMPL_NUMBER})) {
                        parsed_records.emplace_back(
                            recent_mailbox_data_t{.value = std::stoul(std::string{match_text})});

                    } else if (current_path_is({IMAP_PARSER_APG_IMPL_MAILBOX_DATA,
                                                IMAP_PARSER_APG_IMPL_MAILBOX_DATA_EXISTS,
                                                IMAP_PARSER_APG_IMPL_NUMBER})) {
                        parsed_records.emplace_back(
                            exists_mailbox_data_t{.value = std::stoul(std::string{match_text})});
                    }

                    indent += INDENT_WIDTH;

                } else {
                    assert(it->uiState == ID_AST_POST);
                    indent -= INDENT_WIDTH;

                    if (current_path_is(
                            {IMAP_PARSER_APG_IMPL_MAILBOX_DATA, IMAP_PARSER_APG_IMPL_FLAG_LIST})) {
                        parsed_records.emplace_back(mailbox_data_t{std::move(current_flags)});
                    } else if (current_path_is(
                                   {IMAP_PARSER_APG_IMPL_RESP_COND_STATE,
                                    IMAP_PARSER_APG_IMPL_RESP_TEXT,
                                    IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE,
                                    IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_PERMANENT_FLAGS})) {
                        parsed_records.emplace_back(
                            mailbox_data_t{std::move(current_permanent_flags)});
                    }

                    current_path.pop_back();
                }
            }
        });

    if (ec) {
        return unexpected(ec);
    }

    return parsed_records;
}

expected<std::vector<message_data_t>> parse_message_data_records(std::string_view input_text) {
    std::vector<message_data_t> result;

    std::vector<int> current_path;
    current_path.reserve(1024);

    auto current_path_is = [&current_path](auto... l) {
        const auto il = std::initializer_list<int>{l...};
        return std::equal(current_path.begin(), current_path.end(), il.begin(), il.end());
    };
    auto current_path_is_superset_of = [&current_path](auto... l) {
        return emailkit::utils::subset_match(std::initializer_list<int>{l...}, current_path);
    };

    std::optional<imap_parser::msg_attr_envelope_t> curr_envelope_opt{};

    std::vector<msg_static_attr_t> pased_static_attrs;

    bool got_message_data = false;

    auto ec = apg_invoke_parser__ast(
        IMAP_PARSER_APG_IMPL_RESPONSE, input_text,
        {
            // clang-format off
            IMAP_PARSER_APG_IMPL_MESSAGE_DATA,
            IMAP_PARSER_APG_IMPL_NZ_NUMBER,
            IMAP_PARSER_APG_IMPL_FETCH_MESSAGE_DATA,
            IMAP_PARSER_APG_IMPL_MSG_ATT,
            IMAP_PARSER_APG_IMPL_MSG_ATT_DYNAMIC,
            IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC,
            IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_ENVELOPE,
            IMAP_PARSER_APG_IMPL_ENVELOPE,
            IMAP_PARSER_APG_IMPL_ENV_DATE,
            IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_INTERNALDATE,
            IMAP_PARSER_APG_IMPL_DATE_TIME,
            IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_RFC822,
            IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_RFC822_SIZE,
            IMAP_PARSER_APG_IMPL_NUMBER,
            IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_BODY_STRUCTURE,
            IMAP_PARSER_APG_IMPL_BODY,
            IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_UID,
            IMAP_PARSER_APG_IMPL_UNIQUEID,
            // clang-format on
        },

        [&](const ast_record* begin, const ast_record* end) {
            constexpr size_t INDENT_WIDTH = 4;
            size_t indent = 0;
            for (auto it = begin; it != end; ++it) {
                if (it->uiState == ID_AST_PRE) {
                    std::string_view match_text{input_text.data() + it->uiPhraseOffset,
                                                it->uiPhraseLength};

                    current_path.emplace_back(it->uiIndex);

                    if (current_path_is(IMAP_PARSER_APG_IMPL_MESSAGE_DATA)) {
                        result.emplace_back(message_data_t{});
                    } else if (current_path_is(IMAP_PARSER_APG_IMPL_MESSAGE_DATA,
                                               IMAP_PARSER_APG_IMPL_NZ_NUMBER)) {
                        got_message_data = true;
                        log_debug("message-data, number: {}", match_text);
                        result.back().message_number = std::stoul(std::string{match_text});
                    } else if (current_path_is(IMAP_PARSER_APG_IMPL_MESSAGE_DATA,
                                               IMAP_PARSER_APG_IMPL_FETCH_MESSAGE_DATA,
                                               IMAP_PARSER_APG_IMPL_MSG_ATT)) {
                        log_debug("message-data, fetch-message-data");
                    } else if (current_path_is(IMAP_PARSER_APG_IMPL_MESSAGE_DATA,
                                               IMAP_PARSER_APG_IMPL_FETCH_MESSAGE_DATA,
                                               IMAP_PARSER_APG_IMPL_MSG_ATT,
                                               IMAP_PARSER_APG_IMPL_MSG_ATT_DYNAMIC)) {
                        log_debug("message-data, att-dynamic: '{}'", match_text);
                    } else if (current_path_is(IMAP_PARSER_APG_IMPL_MESSAGE_DATA,
                                               IMAP_PARSER_APG_IMPL_FETCH_MESSAGE_DATA,
                                               IMAP_PARSER_APG_IMPL_MSG_ATT,
                                               IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC)) {
                        log_debug("message-data, att-static: '{}'", match_text);
                    } else if (current_path_is(IMAP_PARSER_APG_IMPL_MESSAGE_DATA,
                                               IMAP_PARSER_APG_IMPL_FETCH_MESSAGE_DATA,
                                               IMAP_PARSER_APG_IMPL_MSG_ATT,
                                               IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC,
                                               IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_ENVELOPE)) {
                        log_debug("static attribute, envelope: '{}'", match_text);
                        curr_envelope_opt = imap_parser::msg_attr_envelope_t{};
                    } else if (current_path_is_superset_of(IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC,
                                                           IMAP_PARSER_APG_IMPL_ENVELOPE,
                                                           IMAP_PARSER_APG_IMPL_ENV_DATE)) {
                        log_debug("static attribute, envelope, date: '{}'", match_text);
                        if (!curr_envelope_opt.has_value()) {
                            log_error("logic error: no envelop static attr");
                            continue;
                        }
                        // TODO: handle a case when server returned NUL
                        curr_envelope_opt->date_opt = match_text;
                    } else if (current_path_is(IMAP_PARSER_APG_IMPL_MESSAGE_DATA,
                                               IMAP_PARSER_APG_IMPL_FETCH_MESSAGE_DATA,
                                               IMAP_PARSER_APG_IMPL_MSG_ATT,
                                               IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC,
                                               IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_UID)) {
                        log_debug("static attribute, uid: '{}'", match_text);
                    } else if (current_path_is(IMAP_PARSER_APG_IMPL_MESSAGE_DATA,
                                               IMAP_PARSER_APG_IMPL_FETCH_MESSAGE_DATA,
                                               IMAP_PARSER_APG_IMPL_MSG_ATT,
                                               IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC,
                                               IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_INTERNALDATE)) {
                        log_debug("static attribute, internal-date: '{}'", match_text);
                    } else if (current_path_is(
                                   IMAP_PARSER_APG_IMPL_MESSAGE_DATA,
                                   IMAP_PARSER_APG_IMPL_FETCH_MESSAGE_DATA,
                                   IMAP_PARSER_APG_IMPL_MSG_ATT,
                                   IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC,
                                   IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_BODY_STRUCTURE)) {
                        log_debug("static attribute, body-structure: '{}'", match_text);
                    } else if (current_path_is(IMAP_PARSER_APG_IMPL_MESSAGE_DATA,
                                               IMAP_PARSER_APG_IMPL_FETCH_MESSAGE_DATA,
                                               IMAP_PARSER_APG_IMPL_MSG_ATT,
                                               IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC,
                                               IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_RFC822)) {
                        log_debug("static attribute, rfc822: '{}'", match_text);
                    } else if (current_path_is(IMAP_PARSER_APG_IMPL_MESSAGE_DATA,
                                               IMAP_PARSER_APG_IMPL_FETCH_MESSAGE_DATA,
                                               IMAP_PARSER_APG_IMPL_MSG_ATT,
                                               IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC,
                                               IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_RFC822_SIZE)) {
                        log_debug("static attribute, rfc822-size: '{}'", match_text);
                    }

                } else {
                    if (current_path_is(IMAP_PARSER_APG_IMPL_MESSAGE_DATA,
                                        IMAP_PARSER_APG_IMPL_FETCH_MESSAGE_DATA,
                                        IMAP_PARSER_APG_IMPL_MSG_ATT,
                                        IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC,
                                        IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_ENVELOPE)) {
                        pased_static_attrs.emplace_back(*curr_envelope_opt);
                    } else if (current_path_is(IMAP_PARSER_APG_IMPL_MESSAGE_DATA,
                                               IMAP_PARSER_APG_IMPL_FETCH_MESSAGE_DATA,
                                               IMAP_PARSER_APG_IMPL_MSG_ATT,
                                               IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC)) {
                        result.back().static_attrs = std::move(pased_static_attrs);
                    }

                    current_path.pop_back();
                }
            }
        });

    if (ec) {
        return unexpected(ec);
    }

    if (!got_message_data) {
        // what if there are no messages?
        log_error("no message data in response");
        return unexpected(make_error_code(std::errc::io_error));  // TODO: dedicated error code
    }

    return result;
}

}  // namespace emailkit::imap_parser
