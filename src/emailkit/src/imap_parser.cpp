#include <emailkit/global.hpp>

#include "imap_parser.hpp"

#include <ast.h>     // apg70
#include <parser.h>  // apg70
#include <utilities.h>
#include "imap_parser_apg_impl.h"  // grammar generated for apg70

#include <emailkit/log.hpp>
#include <emailkit/utils.hpp>

#include "imap_parser__rfc822.hpp"
#include "utils.hpp"

#include <function2/function2.hpp>

#include <any>
#include <cstdlib>
#include <set>
#include <vector>

#include <gmime/gmime.h>

#include <scope_guard/scope_guard.hpp>

#include <fcntl.h>
#include <fstream>

namespace emailkit::imap_parser {

namespace {
class parser_err_category_t : public std::error_category {
   public:
    const char* name() const noexcept override { return "imap_parser_err"; }
    std::string message(int ev) const override {
        switch (static_cast<parser_errc>(ev)) {
            case parser_errc::parser_fail_l0:
                return "parser fail at syntax level (l0)";
            case parser_errc::parser_fail_l1:
                return "parser fail at semantics level (l1)";
            case parser_errc::parser_fail_l2:
                return "parser fail at derived parsing level (l2)";
            default:
                return "unknown error";
        }
    }
};
const parser_err_category_t the_parser_err_cat{};
}  // namespace

expected<void> initialize() {
    return rfc822::initialize();
}

expected<void> finalize() {
    return rfc822::finalize();
}

std::error_code make_error_code(parser_errc ec) {
    return std::error_code(static_cast<int>(ec), the_parser_err_cat);
}

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
        // log_warning("overwriting previous literal size with new one");
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
        // log_debug("available text is: '{}'", std::string{begin, end});
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

    // log_debug("have match of size {}: '{}'", literal_size_bytes,
    //           std::string{begin, begin + literal_size_bytes});

    log_debug("returning ID_MATCH");
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
    // void* vpTrace = nullptr;

    // XCTOR macros sets kind of label (setjmp) that can be jumped to. So in case exception occurs
    // in APG it will jump back (longjmp) to this label but this time apg_exception.try_ will be set
    // to FALSE.
    XCTOR(apg_exception);
    if (apg_exception.try_) {
        log_debug("constructing APG parser object");
        parser = ::vpParserCtor(&apg_exception, vpImapParserApgImplInit);
        log_debug("constructing APG parser object -- done");

        // if (std::getenv("MMAP_TRACE")) {
        //     vpTrace = vpTraceCtor(parser);
        //     vTraceConfigGen(vpTrace, NULL);
        // }

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
    //   std::map<std::string, std::any>& out_udt_values,
    fu2::function_view<void(const ast_record*, const ast_record*)> ast_cb) {
    struct apg_invoke_context {
        // registered callbacks for which we set parsers C-callbacks.
        // std::vector<fu2::function_view<void(std::string_view)>> callbacks_map{
        //     RULE_COUNT_IMAP_PARSER_APG_IMPL};
    };

    apg_invoke_context ctx;
    // log_debug("started parsing: '{}'", input_text);

    //////////////////////////////////////////////////////////////////////////////////////////////////
    // WARNING: since apg uses longjmp we should take care of c++ destructors and make sure that our
    // objects lifetimes reside after try/catch of apg.
    // Block for C++ resources that need to have destructors.
    //////////////////////////////////////////////////////////////////////////////////////////////////
    // std::vector<uint8_t> input_text_data(input_text.size());
    // for (size_t i = 0; i < input_text.size(); ++i) {
    //     input_text_data[i] = input_text[i];
    // }
    //////////////////////////////////////////////////////////////////

    parser_state apg_parser_state;
    parser_config apg_parser_config;
    exception apg_exception;
    void* parser = nullptr;
    // void* vpTrace = nullptr;
    void* ast = NULL;

    std::error_code result{};

    XCTOR(apg_exception);
    if (apg_exception.try_) {
        log_debug("APG TRY section begin");

        log_debug("constructing APG parser object");
        parser = ::vpParserCtor(&apg_exception, vpImapParserApgImplInit);
        log_debug("constructing APG parser object -- done");

        // if (std::getenv("MMAP_TRACE")) {
        //     vpTrace = vpTraceCtor(parser);
        //     vTraceConfigGen(vpTrace, NULL);
        // }

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

        ::vAstSetUdtCallback(
            ast, IMAP_PARSER_APG_IMPL_U_LITERAL_SIZE, +[](ast_data*) -> aint { return ID_AST_OK; });
        ::vAstSetUdtCallback(
            ast, IMAP_PARSER_APG_IMPL_U_LITERAL_DATA, +[](ast_data*) -> aint { return ID_AST_OK; });

        ast_parse_invoke_user_data_t parsing_user_data;

        apg_parser_config.acpInput = reinterpret_cast<const unsigned char*>(input_text.data());
        apg_parser_config.uiInputLength = input_text.size();
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

            //	    out_udt_values[""]

            ast_info info;
            ::vAstInfo(ast, &info);
            size_t indent = 0;
            constexpr size_t INDENT_WIDTH = 4;

            ast_cb(&(info.spRecords[0]), &(info.spRecords[info.uiRecordCount]));
        }

        log_debug("APG TRY section end");
    } else {
        log_error("APG EXCEPTION: {}", format_apg_exception(apg_exception));
        result = make_error_code(parser_errc::parser_fail_l0);
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
                        parsed_records.emplace_back(uidvalidity_data_t{
                            .value = static_cast<uint32_t>(std::stoul(std::string{match_text}))});

                    } else if (current_path_is({IMAP_PARSER_APG_IMPL_RESP_COND_STATE,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_UNSEEN,
                                                IMAP_PARSER_APG_IMPL_NZ_NUMBER})) {
                        parsed_records.emplace_back(unseen_resp_text_code_t{
                            .value = static_cast<uint32_t>(std::stoul(std::string{match_text}))});

                    } else if (current_path_is({IMAP_PARSER_APG_IMPL_RESP_COND_STATE,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE,
                                                IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_UID_NEXT,
                                                IMAP_PARSER_APG_IMPL_NZ_NUMBER})) {
                        parsed_records.emplace_back(uidnext_resp_text_code_t{
                            .value = static_cast<uint32_t>(std::stoul(std::string{match_text}))});

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
                        parsed_records.emplace_back(recent_mailbox_data_t{
                            .value = static_cast<uint32_t>(std::stoul(std::string{match_text}))});

                    } else if (current_path_is({IMAP_PARSER_APG_IMPL_MAILBOX_DATA,
                                                IMAP_PARSER_APG_IMPL_MAILBOX_DATA_EXISTS,
                                                IMAP_PARSER_APG_IMPL_NUMBER})) {
                        parsed_records.emplace_back(exists_mailbox_data_t{
                            .value = static_cast<uint32_t>(std::stoul(std::string{match_text}))});
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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Utilities
const ast_record* skip_until(const ast_record* begin,
                             const ast_record* end,
                             unsigned until_index,
                             unsigned state) {
    while (begin != end && !(begin->uiIndex == until_index && begin->uiState == state)) {
        begin++;
    }
    return begin;
}

const ast_record* skip_until(const ast_record* begin,
                             const ast_record* end,
                             std::initializer_list<unsigned> until_indices,
                             unsigned state) {
    while (begin != end && !(std::find(until_indices.begin(), until_indices.end(),
                                       begin->uiIndex) != until_indices.end() &&
                             begin->uiState == state)) {
        begin++;
    }
    return begin;
}

#define RETURN_IF_END(It) \
    do {                  \
        auto it__ = (It); \
        if (it__ == end)  \
            return it__;  \
    } while (false)

const ast_record* parse_fld_dsp(std::string_view input,
                                const ast_record* begin,
                                const ast_record* end,
                                std::string& out_dsp_type,
                                std::vector<param_value_t>& out_dsp_params) {
    std::string parsed_dsp_type;
    std::vector<param_value_t> parsed_dsp_params;

    assert(begin->uiIndex == IMAP_PARSER_APG_IMPL_BODY_FLD_DSP);

    const ast_record* it = begin + 1;
    for (; it != end && it->uiIndex != IMAP_PARSER_APG_IMPL_BODY_FLD_DSP; ++it) {
        const std::string_view match_text{input.data() + it->uiPhraseOffset, it->uiPhraseLength};
        if (it->uiState == ID_AST_PRE) {
            switch (it->uiIndex) {
                case IMAP_PARSER_APG_IMPL_BODY_FLD_DSP_STRING: {
                    parsed_dsp_type = match_text;
                    break;
                }
                case IMAP_PARSER_APG_IMPL_BODY_FLD_PARAM_NAME: {
                    parsed_dsp_params.emplace_back(match_text, "");
                    break;
                }
                case IMAP_PARSER_APG_IMPL_BODY_FLD_PARAM_VALUE: {
                    parsed_dsp_params.back().second = match_text;
                    break;
                }
            }
        }
    }

    out_dsp_type = std::move(parsed_dsp_type);
    out_dsp_params = std::move(parsed_dsp_params);

    return it;
}

const ast_record* parse_envelope(std::string_view input,
                                 const ast_record* begin,
                                 const ast_record* end,
                                 msg_attr_envelope_t& out_envelope) {
    // envelope        = "(" env-date SP env-subject SP env-from SP
    //                   env-sender SP env-reply-to SP env-to SP env-cc SP
    //                   env-bcc SP env-in-reply-to SP env-message-id ")"
    msg_attr_envelope_t parsed_envelope;

    assert(begin->uiIndex == IMAP_PARSER_APG_IMPL_ENVELOPE);

    const ast_record* it = begin + 1;
    for (; it != end && it->uiIndex != IMAP_PARSER_APG_IMPL_ENVELOPE; ++it) {
        const std::string_view match_text{input.data() + it->uiPhraseOffset, it->uiPhraseLength};

        if (it->uiState == ID_AST_PRE) {
            switch (it->uiIndex) {
                case IMAP_PARSER_APG_IMPL_ENV_DATE: {
                    parsed_envelope.date_opt = match_text;

                    // TODO: the data here is in the same format as SMTP I guess:
                    // https://datatracker.ietf.org/doc/html/rfc2822#section-3.3
                    break;
                }
                case IMAP_PARSER_APG_IMPL_ENV_SUBJECT: {
                    // TODO: subject is MIME encoded here. We shold either find gmime function for
                    // decoding or do it on our own.

                    parsed_envelope.subject = match_text;
                    break;
                }
                case IMAP_PARSER_APG_IMPL_ENV_FROM: {
                    parsed_envelope.from = match_text;
                    break;
                }
                case IMAP_PARSER_APG_IMPL_ENV_SENDER: {
                    parsed_envelope.sender = match_text;
                    break;
                }
                case IMAP_PARSER_APG_IMPL_ENV_REPLY_TO: {
                    parsed_envelope.reply_to = match_text;
                    break;
                }
                case IMAP_PARSER_APG_IMPL_ENV_TO: {
                    parsed_envelope.to = match_text;
                    break;
                }
                case IMAP_PARSER_APG_IMPL_ENV_CC: {
                    parsed_envelope.cc = match_text;
                    break;
                }
                case IMAP_PARSER_APG_IMPL_ENV_BCC: {
                    parsed_envelope.bcc = match_text;
                    break;
                }
                case IMAP_PARSER_APG_IMPL_ENV_IN_REPLY_TO: {
                    parsed_envelope.in_reply_to = match_text;
                    break;
                }
                case IMAP_PARSER_APG_IMPL_ENV_MESSAGE_ID: {
                    parsed_envelope.message_id = match_text;
                    break;
                }
            }
        }
    }

    out_envelope = std::move(parsed_envelope);

    return it;
}

const ast_record* parse_number(std::string_view input,
                               const ast_record* begin,
                               const ast_record* end,
                               uint32_t& out_result) {
    assert(begin->uiIndex == IMAP_PARSER_APG_IMPL_NUMBER ||
           begin->uiIndex == IMAP_PARSER_APG_IMPL_NZ_NUMBER);

    const unsigned parsed_index = begin->uiIndex;

    const std::string_view match_text{input.data() + begin->uiPhraseOffset, begin->uiPhraseLength};
    out_result = static_cast<uint32_t>(std::stoul(std::string{match_text}));

    auto it = begin + 1;

    // Skip all tokens until post/number node is reached just in case there are tokens for
    // individual digits.
    it = skip_until(it, end, parsed_index, ID_AST_POST);
    RETURN_IF_END(it);
    it++;

    assert((it - 1)->uiIndex == parsed_index && (it - 1)->uiState == ID_AST_POST);

    return it;
}

const ast_record* parse_string(std::string_view input,
                               const ast_record* begin,
                               const ast_record* end,
                               std::string& out_result) {
    // string          = quoted / literal
    // quoted          = DQUOTE *QUOTED-CHAR DQUOTE
    // literal         = "{" u_literal-size "}" CRLF u_literal-data

    assert(begin->uiIndex == IMAP_PARSER_APG_IMPL_STRING);

    const ast_record* it = begin + 1;

    for (; it != end && it->uiIndex != IMAP_PARSER_APG_IMPL_STRING; ++it) {
        const std::string_view match_text{input.data() + it->uiPhraseOffset, it->uiPhraseLength};

        if (it->uiState == ID_AST_PRE) {
            switch (it->uiIndex) {
                case IMAP_PARSER_APG_IMPL_QUOTED: {
                    out_result = emailkit::utils::strip_double_quotes(match_text);
                    break;
                }
                case IMAP_PARSER_APG_IMPL_LITERAL: {
                    // TODO:
                    break;
                }
                case IMAP_PARSER_APG_IMPL_U_LITERAL_SIZE: {
                    // TODO:
                    break;
                }
                case IMAP_PARSER_APG_IMPL_U_LITERAL_DATA: {
                    // TODO:
                    break;
                }
            }
        }
    }

    it++;
    assert((it - 1)->uiIndex == IMAP_PARSER_APG_IMPL_STRING && (it - 1)->uiState == ID_AST_POST);

    return it;
}

const ast_record* parse_nstring(std::string_view input,
                                const ast_record* begin,
                                const ast_record* end,
                                std::string& out_result) {
    // nstring         = string / nil
    // string          = quoted / literal
    // quoted          = DQUOTE *QUOTED-CHAR DQUOTE
    // literal         = "{" u_literal-size "}" CRLF u_literal-data

    assert(begin->uiIndex == IMAP_PARSER_APG_IMPL_NSTRING);

    const ast_record* it = begin + 1;

    for (; it != end && it->uiIndex != IMAP_PARSER_APG_IMPL_NSTRING; ++it) {
        const std::string_view match_text{input.data() + it->uiPhraseOffset, it->uiPhraseLength};

        if (it->uiState == ID_AST_PRE) {
            switch (it->uiIndex) {
                case IMAP_PARSER_APG_IMPL_NIL: {
                    // Just skip a nil and have empty string.
                    break;
                }
                case IMAP_PARSER_APG_IMPL_QUOTED: {
                    out_result = emailkit::utils::strip_double_quotes(match_text);
                    break;
                }
                case IMAP_PARSER_APG_IMPL_LITERAL: {
                    log_warning("literal is ignored: {}", match_text);
                    break;
                }
                case IMAP_PARSER_APG_IMPL_U_LITERAL_SIZE: {
                    log_debug("literal size is ignored: {}", match_text);
                    break;
                }
                case IMAP_PARSER_APG_IMPL_U_LITERAL_DATA: {
                    out_result = match_text;
                    break;
                }
            }
        }
    }

    RETURN_IF_END(it);
    it++;

    assert((it - 1)->uiIndex == IMAP_PARSER_APG_IMPL_NSTRING && (it - 1)->uiState == ID_AST_POST);

    return it;
}

const ast_record* parse_fld_param(std::string_view input,
                                  const ast_record* begin,
                                  const ast_record* end,
                                  std::vector<param_value_t>& out_fld_param) {
    assert(begin->uiIndex == IMAP_PARSER_APG_IMPL_BODY_FLD_PARAM);

    const ast_record* it = begin + 1;
    for (; it != end && it->uiIndex != IMAP_PARSER_APG_IMPL_BODY_FLD_PARAM; ++it) {
        const std::string_view match_text{input.data() + it->uiPhraseOffset, it->uiPhraseLength};

        if (it->uiState == ID_AST_PRE) {
            switch (it->uiIndex) {
                case IMAP_PARSER_APG_IMPL_BODY_FLD_PARAM_NAME: {
                    out_fld_param.emplace_back(match_text, "");
                    break;
                }
                case IMAP_PARSER_APG_IMPL_BODY_FLD_PARAM_VALUE: {
                    out_fld_param.back().second = match_text;
                    break;
                }
            }
        }
    }

    return it;
}

const ast_record* parse_ext_part_impl(
    std::string_view input,
    const ast_record* begin,
    const ast_record* end,
    std::variant<wip::BodyExt1Part, wip::BodyExtMPart>& out_result) {
    assert(begin->uiIndex == IMAP_PARSER_APG_IMPL_BODY_EXT_1PART ||
           begin->uiIndex == IMAP_PARSER_APG_IMPL_BODY_EXT_MPART);

    const std::string_view match_text{input.data() + begin->uiPhraseOffset, begin->uiPhraseLength};

    std::optional<std::string> maybe_parsed_md5;
    std::optional<std::vector<param_value_t>> maybe_parsed_body_fld_params;
    wip::BodyFieldDSP parsed_body_field_dsp;

    // this parser works with two very similar rules which has first element different.
    const auto part_rule_index = begin->uiIndex;

    const ast_record* it = begin + 1;

    for (; it != end && it->uiIndex != part_rule_index; ++it) {
        const std::string_view match_text{input.data() + it->uiPhraseOffset, it->uiPhraseLength};

        if (it->uiState == ID_AST_PRE) {
            switch (it->uiIndex) {
                case IMAP_PARSER_APG_IMPL_BODY_FLD_MD5: {
                    // This is 1part
                    maybe_parsed_md5 = std::string{};
                    it = parse_nstring(input, it + 1, end, *maybe_parsed_md5);
                    break;
                }
                case IMAP_PARSER_APG_IMPL_BODY_FLD_PARAM: {
                    // This is mpart (body-fld-parm may appear down in body-fld-dsp but since we are
                    // parsing in recursive descent manner it should be unique indication of mpart)
                    // msg_attr_body_structure_t::body_fld_param fld_param;
                    maybe_parsed_body_fld_params = std::vector<param_value_t>{};
                    it = parse_fld_param(input, it, end, *maybe_parsed_body_fld_params);
                    break;
                }
                case IMAP_PARSER_APG_IMPL_BODY_FLD_DSP: {
                    it = parse_fld_dsp(input, it, end, parsed_body_field_dsp.field_dsp_string,
                                       parsed_body_field_dsp.field_params);
                    break;
                }
                    // TODO: body-fld-lang
                    // TODO: body-fld-loc
                    // TODO: *(SP body-extension)
            }
        }
    }

    if (maybe_parsed_md5) {
        // 1part
        if (*maybe_parsed_md5 == "NIL") {
            maybe_parsed_md5->clear();
        }

        out_result = wip::BodyExt1Part{.md5 = std::move(*maybe_parsed_md5),
                                       .body_field_dsp = std::move(parsed_body_field_dsp)};
    } else {
        // mpart
        // TODO: check!
        assert(maybe_parsed_body_fld_params);
        out_result = wip::BodyExtMPart{.body_fld_params = std::move(*maybe_parsed_body_fld_params),
                                       .body_field_dsp = std::move(parsed_body_field_dsp)};
    }

    if (it != end) {
        ++it;
    }

    assert((it - 1)->uiIndex == IMAP_PARSER_APG_IMPL_BODY_EXT_1PART ||
           (it - 1)->uiIndex == IMAP_PARSER_APG_IMPL_BODY_EXT_MPART);

    return it;
}

const ast_record* parse_ext_1part(std::string_view input,
                                  const ast_record* begin,
                                  const ast_record* end,
                                  wip::BodyExt1Part& out_ext_part) {
    std::variant<wip::BodyExt1Part, wip::BodyExtMPart> ext_part;
    auto it = parse_ext_part_impl(input, begin, end, ext_part);
    assert(std::holds_alternative<wip::BodyExt1Part>(ext_part));
    out_ext_part = std::get<wip::BodyExt1Part>(ext_part);
    return it;
}

const ast_record* parse_ext_mpart(std::string_view input,
                                  const ast_record* begin,
                                  const ast_record* end,
                                  wip::BodyExtMPart& out_ext_part) {
    std::variant<wip::BodyExt1Part, wip::BodyExtMPart> ext_part;
    auto it = parse_ext_part_impl(input, begin, end, ext_part);
    assert(std::holds_alternative<wip::BodyExtMPart>(ext_part));
    out_ext_part = std::get<wip::BodyExtMPart>(ext_part);
    return it;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Declarations
const ast_record* parse_body(std::string_view input,
                             const ast_record* begin,
                             const ast_record* end,
                             wip::Body& out_result);
const ast_record* parse_nstring(std::string_view input,
                                const ast_record* begin,
                                const ast_record* end,
                                std::string& out_result);
const ast_record* parse_string(std::string_view input,
                               const ast_record* begin,
                               const ast_record* end,
                               std::string& out_result);
const ast_record* parse_ext_part(std::string_view input,
                                 const ast_record* begin,
                                 const ast_record* end,
                                 std::variant<wip::BodyExt1Part, wip::BodyExtMPart>& out_result);

const ast_record* parse_media_subtype(std::string_view input,
                                      const ast_record* begin,
                                      const ast_record* end,
                                      std::string& out_result) {
    log_debug("parsing media subtype");
    assert(begin->uiIndex == IMAP_PARSER_APG_IMPL_MEDIA_SUBTYPE);

    auto it = begin + 1;

    assert(it->uiIndex == IMAP_PARSER_APG_IMPL_STRING);

    const std::string_view match_text{input.data() + it->uiPhraseOffset, it->uiPhraseLength};
    out_result = emailkit::utils::strip_double_quotes(match_text);

    it = skip_until(it, end, IMAP_PARSER_APG_IMPL_MEDIA_SUBTYPE, ID_AST_POST);
    RETURN_IF_END(it);

    assert(it->uiIndex == IMAP_PARSER_APG_IMPL_MEDIA_SUBTYPE);

    return it + 1;
}

// const ast_record* parse_body_ext_mpart(std::string_view input,
//                                        const ast_record* begin,
//                                        const ast_record* end,
//                                        std::string& out_result) {
//     assert(begin != end && begin->uiIndex == IMAP_PARSER_APG_IMPL_BODY_EXT_MPART);
//     return begin;
// }

const ast_record* parse_body_fields(std::string_view input,
                                    const ast_record* begin,
                                    const ast_record* end,
                                    wip::BodyFields& out_result) {
    // body-fields     = body-fld-param SP body-fld-id SP body-fld-desc SP body-fld-enc SP
    //                   body-fld-octets

    assert(begin != end && begin->uiIndex == IMAP_PARSER_APG_IMPL_BODY_FIELDS);

    auto it = begin + 1;

    for (; it != end && it->uiIndex != IMAP_PARSER_APG_IMPL_BODY_FIELDS; ++it) {
        if (it->uiState == ID_AST_PRE) {
            const std::string_view match_text{input.data() + it->uiPhraseOffset,
                                              it->uiPhraseLength};
            switch (it->uiIndex) {
                case IMAP_PARSER_APG_IMPL_BODY_FLD_PARAM: {
                    // body-fld-param  = "(" body-fld-param-name SP body-fld-param-value *(SP
                    // body-fld-param-name SP body-fld-param-value) ")" / nil
                    break;
                }
                case IMAP_PARSER_APG_IMPL_BODY_FLD_ID: {
                    // body-fld-id     = nstring

                    // TODO: change semantics to the following: in the remaining stream parse
                    // closest NSTRING and return either end or token after nstring.
                    it = parse_nstring(input, it + 1, end, out_result.field_id);
                    break;
                }
                case IMAP_PARSER_APG_IMPL_BODY_FLD_DESC: {
                    // body-fld-desc     = nstring
                    it = parse_nstring(input, it + 1, end, out_result.field_desc);
                    break;
                }
                case IMAP_PARSER_APG_IMPL_BODY_FLD_ENC: {
                    // Original was:
                    // body-fld-enc    = (DQUOTE ("7BIT" / "8BIT" / "BINARY" / "BASE64"/
                    //                  "QUOTED-PRINTABLE") DQUOTE) / string
                    // But I changed it to:
                    // body-fld-enc = string
                    it = parse_string(input, it + 1, end, out_result.encoding);
                    break;
                }
                case IMAP_PARSER_APG_IMPL_BODY_FLD_OCTETS: {
                    it = parse_number(input, it + 1, end, out_result.octets);
                    break;
                }
                case IMAP_PARSER_APG_IMPL_BODY_FLD_PARAM_NAME: {
                    out_result.params.emplace_back(match_text, "");
                    break;
                }
                case IMAP_PARSER_APG_IMPL_BODY_FLD_PARAM_VALUE: {
                    out_result.params.back().second = match_text;
                    break;
                }
            }
        }
    }
    if (it != end) {
        it++;
    }

    assert((it - 1)->uiIndex == IMAP_PARSER_APG_IMPL_BODY_FIELDS &&
           (it - 1)->uiState == ID_AST_POST);

    return it;
}

const ast_record* parse_body_type_text(std::string_view input,
                                       const ast_record* begin,
                                       const ast_record* end,
                                       wip::BodyTypeText& out_result) {
    // body-type-text  = media-text SP body-fields SP body-fld-lines
    // media-text      = DQUOTE "TEXT" DQUOTE SP media-subtype

    log_debug("parsing body-type-text");

    assert(begin != end && begin->uiIndex == IMAP_PARSER_APG_IMPL_BODY_TYPE_TEXT);
    auto it = begin + 1;

    while (it != end && it->uiIndex != IMAP_PARSER_APG_IMPL_MEDIA_SUBTYPE) {
        ++it;
    }
    if (it == end) {
        log_error("reached end unexpectedly");
        return it;
    }

    std::string media_subtype;
    it = parse_media_subtype(input, it, end, media_subtype);
    if (it == end) {
        log_error("reached end unexpectedly");
        return it;
    }
    out_result.media_subtype = media_subtype;

    // media-subtype must be the last token of media-text
    assert(it->uiIndex == IMAP_PARSER_APG_IMPL_MEDIA_TEXT && it->uiState == ID_AST_POST);

    it = skip_until(it, end, IMAP_PARSER_APG_IMPL_BODY_FIELDS, ID_AST_PRE);
    RETURN_IF_END(it);

    parse_body_fields(input, it, end, out_result.body_fields);

    // skip body-fld-lines
    it = skip_until(it, end, IMAP_PARSER_APG_IMPL_BODY_TYPE_TEXT, ID_AST_POST);
    RETURN_IF_END(it);
    ++it;

    assert((it - 1)->uiIndex == IMAP_PARSER_APG_IMPL_BODY_TYPE_TEXT &&
           (it - 1)->uiState == ID_AST_POST);

    // log_info("parsed body type text. media_subtype: {}, body_fields: {}", media_subtype,
    //          body_fields);
    return it;
}

const ast_record* parse_body_type_basic(std::string_view input,
                                        const ast_record* begin,
                                        const ast_record* end,
                                        wip::BodyTypeBasic& out_result) {
    // body-type-basic = media-basic SP body-fields ; MESSAGE subtype MUST NOT be "RFC822"
    // media-basic-type-tag = string
    // media-basic     = media-basic-type-tag SP media-subtype
    // media-subtype   = string ; Defined in [MIME-IMT]

    log_debug("parsing body-type-basic");

    assert(begin != end && begin->uiIndex == IMAP_PARSER_APG_IMPL_BODY_TYPE_BASIC);
    auto it = begin + 1;

    // media type
    it = skip_until(it, end, IMAP_PARSER_APG_IMPL_MEDIA_BASIC_TYPE_TAG, ID_AST_PRE);
    RETURN_IF_END(it);

    it++;  // new macros ENSURE_NEXT_OR_RETURN
    assert(it != end && it->uiIndex == IMAP_PARSER_APG_IMPL_STRING);

    it = parse_string(input, it, end, out_result.media_type);

    // media subtype
    it = skip_until(it, end, IMAP_PARSER_APG_IMPL_MEDIA_SUBTYPE, ID_AST_PRE);
    RETURN_IF_END(it);
    it++;  // new macros ENSURE_NEXT_OR_RETURN
    assert(it != end && it->uiIndex == IMAP_PARSER_APG_IMPL_STRING);

    it = parse_string(input, it, end, out_result.media_subtype);

    // body_field
    it = skip_until(it, end, IMAP_PARSER_APG_IMPL_BODY_FIELDS, ID_AST_PRE);
    RETURN_IF_END(it);

    it = parse_body_fields(input, it, end, out_result.body_fields);

    if (it != end) {
        ++it;
    }

    // log_info("parsed body type basic. media_type: {}, media_subtype: {}, body_fields: {}",
    //          media_type, media_subtype, body_fields);

    assert((it - 1)->uiIndex == IMAP_PARSER_APG_IMPL_BODY_TYPE_BASIC &&
           (it - 1)->uiState == ID_AST_POST);

    return it;
}

const ast_record* parse_body_type_msg(std::string_view input,
                                      const ast_record* begin,
                                      const ast_record* end,
                                      wip::BodyTypeMsg& out_result) {
    log_debug("parsing body-type-msg");

    assert(begin != end && begin->uiIndex == IMAP_PARSER_APG_IMPL_BODY_TYPE_MSG);
    auto it = begin + 1;

    it = skip_until(it, end, IMAP_PARSER_APG_IMPL_BODY_TYPE_MSG, ID_AST_POST);
    RETURN_IF_END(it);
    it++;

    log_error("parsed for body-type-msg not implemented, skipping {} tokens", it - begin);

    assert((it - 1)->uiIndex == IMAP_PARSER_APG_IMPL_BODY_TYPE_MSG &&
           (it - 1)->uiState == ID_AST_POST);

    return it;
}

const ast_record* parse_body_1part(std::string_view input,
                                   const ast_record* begin,
                                   const ast_record* end,
                                   wip::BodyType1Part& out_result) {
    // body-type-1part = (body-type-text / body-type-basic / body-type-msg) [SP body-ext-1part]

    log_debug("parsing body-type-1part");

    assert(begin != end && begin->uiIndex == IMAP_PARSER_APG_IMPL_BODY_TYPE_1PART);
    auto it = begin + 1;

    switch (it->uiIndex) {
        case IMAP_PARSER_APG_IMPL_BODY_TYPE_TEXT: {
            log_debug("IMAP_PARSER_APG_IMPL_BODY_TYPE_TEXT");
            wip::BodyTypeText text_body;
            it = parse_body_type_text(input, it, end, text_body);

            out_result.part_body = std::move(text_body);
            break;
        }
        case IMAP_PARSER_APG_IMPL_BODY_TYPE_BASIC: {
            log_debug("IMAP_PARSER_APG_IMPL_BODY_TYPE_BASIC");
            wip::BodyTypeBasic basic_body;
            it = parse_body_type_basic(input, it, end, basic_body);

            out_result.part_body = std::move(basic_body);
            break;
        }
        case IMAP_PARSER_APG_IMPL_BODY_TYPE_MSG: {
            log_debug("IMAP_PARSER_APG_IMPL_BODY_TYPE_MSG");
            wip::BodyTypeMsg message_body;
            it = parse_body_type_msg(input, it, end, message_body);

            out_result.part_body = std::move(message_body);
            break;
        }
        default: {
            assert(false);
            // TODO: how to report error? (idea: return end and get all upstream parsers propagate
            // if end; we can have also context which is referenced by all parsers).
        }
    }

    if (it->uiIndex == IMAP_PARSER_APG_IMPL_BODY_EXT_1PART) {
        out_result.part_body_ext = wip::BodyExt1Part{};
        it = parse_ext_1part(input, it, end, *(out_result.part_body_ext));
        if (it != end) {
            ++it;
        }
    } else {
        ++it;
    }

    assert((it - 1)->uiIndex == IMAP_PARSER_APG_IMPL_BODY_TYPE_1PART &&
           (it - 1)->uiState == ID_AST_POST);

    return it;
}  // namespace emailkit::imap_parser

const ast_record* parse_body_mpart(std::string_view input,
                                   const ast_record* begin,
                                   const ast_record* end,
                                   wip::BodyTypeMPart& out_result) {
    // body-type-mpart = 1*body SP media-subtype [SP body-ext-mpart]

    assert(begin != end && begin->uiIndex == IMAP_PARSER_APG_IMPL_BODY_TYPE_MPART);

    auto it = begin + 1;

    // next we expect one of more body elements.
    do {
        wip::Body next_body;
        it = parse_body(input, it, end, next_body);

        out_result.body_ptrs.emplace_back(std::move(next_body));

        // by convention, parsed should stop at the node after the last one, so for body it should
        // be the one next after POST/body.
        assert((it - 1)->uiIndex == IMAP_PARSER_APG_IMPL_BODY &&
               (it - 1)->uiState == ID_AST_POST);  // TODO: consider removing.

    } while (it != end && it->uiIndex == IMAP_PARSER_APG_IMPL_BODY);

    if (it != end && it->uiIndex == IMAP_PARSER_APG_IMPL_MEDIA_SUBTYPE) {
        it = parse_media_subtype(input, it, end, out_result.media_subtype);
    }

    if (it->uiIndex == IMAP_PARSER_APG_IMPL_BODY_EXT_MPART) {
        out_result.multipart_body_ext = wip::BodyExtMPart{};
        it = parse_ext_mpart(input, it, end, *(out_result.multipart_body_ext));
        if (it != end) {
            it++;
        }
    } else {
        it++;
    }

    // TODO: this should rather be: it == end || (it-1)-> ...
    assert((it - 1)->uiIndex == IMAP_PARSER_APG_IMPL_BODY_TYPE_MPART &&
           (it - 1)->uiState == ID_AST_POST);

    return it;
}

const ast_record* parse_body(std::string_view input,
                             const ast_record* begin,
                             const ast_record* end,
                             wip::Body& out_result) {
    log_debug("parsing body at token {} from end", end - begin);
    // body            = "(" (body-type-1part / body-type-mpart) ")"
    assert(begin->uiIndex == IMAP_PARSER_APG_IMPL_BODY);

    const ast_record* it = begin + 1;

    for (; it != end && it->uiIndex != IMAP_PARSER_APG_IMPL_BODY;) {
        const std::string_view match_text{input.data() + it->uiPhraseOffset, it->uiPhraseLength};

        if (it->uiState == ID_AST_PRE) {
            switch (it->uiIndex) {
                case IMAP_PARSER_APG_IMPL_BODY_TYPE_1PART: {
                    auto part_ptr = std::make_unique<wip::BodyType1Part>();
                    it = parse_body_1part(input, it, end, *part_ptr);
                    out_result = std::move(part_ptr);
                    break;
                }
                case IMAP_PARSER_APG_IMPL_BODY_TYPE_MPART: {
                    // TODO: check that there is at least one token.
                    auto part_ptr = std::make_unique<wip::BodyTypeMPart>();
                    it = parse_body_mpart(input, it, end, *part_ptr);

                    out_result = std::move(part_ptr);
                    break;
                }
                default: {
                    assert(false);
                }
            }
        }
    }

    if (it != end) {
        ++it;
    }

    assert((it - 1)->uiIndex == IMAP_PARSER_APG_IMPL_BODY && (it - 1)->uiState == ID_AST_POST);

    return it;
}

const ast_record* parse_bodystructure(std::string_view input,
                                      const ast_record* begin,
                                      const ast_record* end,
                                      wip::Body& out_result) {
    // msg-att-static-body-structure = "BODY" ["STRUCTURE"] SP body

    assert(begin->uiIndex == IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_BODY_STRUCTURE);
    assert(end - begin > 2);

    auto it = parse_body(input, begin + 1, end, out_result);

    if (it != end) {
        ++it;
    }

    assert((it - 1)->uiIndex == IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_BODY_STRUCTURE &&
           (it - 1)->uiState == ID_AST_POST);

    return it;
}

const ast_record* parse_as_env_address_list(std::string_view input,
                                            const ast_record* begin,
                                            const ast_record* end,
                                            std::vector<Address>& out_result) {
    // "(" 1*address ")" / nil
    // address     = "(" addr-name SP addr-adl SP addr-mailbox SP addr-host ")"

    auto it = begin + 1;
    while (it != end && !(it->uiIndex == begin->uiIndex && it->uiState == ID_AST_POST)) {
        if (it->uiState == ID_AST_PRE) {
            switch (it->uiIndex) {
                case IMAP_PARSER_APG_IMPL_ADDRESS: {
                    out_result.push_back(Address{});
                    break;
                }
                case IMAP_PARSER_APG_IMPL_ADDR_NAME: {
                    it = parse_nstring(input, it, end, out_result.back().addr_name);
                    break;
                }
                case IMAP_PARSER_APG_IMPL_ADDR_ADL: {
                    it = parse_nstring(input, it, end, out_result.back().addr_adl);
                    break;
                }
                case IMAP_PARSER_APG_IMPL_ADDR_MAILBOX: {
                    it = parse_nstring(input, it, end, out_result.back().addr_mailbox);
                    break;
                }
                case IMAP_PARSER_APG_IMPL_ADDR_HOST: {
                    it = parse_nstring(input, it, end, out_result.back().addr_host);
                    break;
                }
                default: {
                }
            }
        }
        ++it;
    }

    RETURN_IF_END(it);
    it++;

    assert((it - 1)->uiIndex == begin->uiIndex && (it - 1)->uiState == ID_AST_POST);

    return it;
}

const ast_record* parse_msg_att_static_envelope(std::string_view input,
                                                const ast_record* begin,
                                                const ast_record* end,
                                                Envelope& out_result) {
    // envelope        = "(" env-date SP env-subject SP env-from SP
    //                   env-sender SP env-reply-to SP env-to SP env-cc SP
    //                   env-bcc SP env-in-reply-to SP env-message-id ")"
    //
    // env-bcc         = "(" 1*address ")" / nil
    // env-cc          = "(" 1*address ")" / nil
    // env-date        = nstring
    // env-from        = "(" 1*address ")" / nil
    // env-in-reply-to = nstring
    // env-message-id  = nstring
    // env-reply-to    = "(" 1*address ")" / nil
    // env-sender      = "(" 1*address ")" / nil
    // env-subject     = nstring
    // env-to          = "(" 1*address ")" / nil

    assert(begin->uiIndex == IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_ENVELOPE);

    auto it = begin;

    it = skip_until(begin, end, IMAP_PARSER_APG_IMPL_ENV_DATE, ID_AST_PRE);
    RETURN_IF_END(it);

    it = parse_nstring(input, it + 1, end, out_result.date);
    RETURN_IF_END(it);

    it = skip_until(begin, end, IMAP_PARSER_APG_IMPL_ENV_SUBJECT, ID_AST_PRE);
    RETURN_IF_END(it);

    it = parse_nstring(input, it + 1, end, out_result.subject);
    RETURN_IF_END(it);

    it = skip_until(begin, end, IMAP_PARSER_APG_IMPL_ENV_FROM, ID_AST_PRE);
    RETURN_IF_END(it);

    it = parse_as_env_address_list(input, it, end, out_result.from);
    RETURN_IF_END(it);

    it = skip_until(begin, end, IMAP_PARSER_APG_IMPL_ENV_SENDER, ID_AST_PRE);
    RETURN_IF_END(it);

    it = parse_as_env_address_list(input, it, end, out_result.sender);
    RETURN_IF_END(it);

    it = skip_until(begin, end, IMAP_PARSER_APG_IMPL_ENV_REPLY_TO, ID_AST_PRE);
    RETURN_IF_END(it);

    it = parse_as_env_address_list(input, it, end, out_result.reply_to);
    RETURN_IF_END(it);

    it = skip_until(begin, end, IMAP_PARSER_APG_IMPL_ENV_TO, ID_AST_PRE);
    RETURN_IF_END(it);

    it = parse_as_env_address_list(input, it, end, out_result.to);
    RETURN_IF_END(it);

    it = skip_until(begin, end, IMAP_PARSER_APG_IMPL_ENV_CC, ID_AST_PRE);
    RETURN_IF_END(it);

    it = parse_as_env_address_list(input, it, end, out_result.cc);
    RETURN_IF_END(it);

    it = skip_until(begin, end, IMAP_PARSER_APG_IMPL_ENV_BCC, ID_AST_PRE);
    RETURN_IF_END(it);

    it = parse_as_env_address_list(input, it, end, out_result.bcc);
    RETURN_IF_END(it);

    it = skip_until(begin, end, IMAP_PARSER_APG_IMPL_ENV_IN_REPLY_TO, ID_AST_PRE);
    RETURN_IF_END(it);

    it = parse_nstring(input, it + 1, end, out_result.in_reply_to);
    RETURN_IF_END(it);

    it = skip_until(begin, end, IMAP_PARSER_APG_IMPL_ENV_MESSAGE_ID, ID_AST_PRE);
    RETURN_IF_END(it);

    it = parse_nstring(input, it + 1, end, out_result.message_id);
    RETURN_IF_END(it);

    it = skip_until(begin, end, IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_ENVELOPE, ID_AST_POST);
    RETURN_IF_END(it);

    it++;

    assert((it - 1)->uiIndex == IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_ENVELOPE &&
           (it - 1)->uiState == ID_AST_POST);
    return it;
}

const ast_record* parse_msg_att_static_uid(std::string_view input,
                                           const ast_record* begin,
                                           const ast_record* end,
                                           unsigned& out_uid) {
    // msg-att-static-uid = "UID" SP uniqueid
    // uniqueid        = nz-number ; Strictly ascending
    assert(begin->uiIndex == IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_UID);

    auto it = begin + 1;

    it = skip_until(it, end, IMAP_PARSER_APG_IMPL_NZ_NUMBER, ID_AST_PRE);
    RETURN_IF_END(it);

    it = parse_number(input, it, end, out_uid);
    RETURN_IF_END(it);

    it = skip_until(it, end, IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_UID, ID_AST_POST);
    RETURN_IF_END(it);
    ++it;

    assert((it - 1)->uiIndex == IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_UID &&
           (it - 1)->uiState == ID_AST_POST);
    return it;
}

const ast_record* parse_msg_att_static_internaldate(std::string_view input,
                                                    const ast_record* begin,
                                                    const ast_record* end) {
    assert(begin->uiIndex == IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_INTERNALDATE);

    auto it = begin + 1;

    // TODO: implement

    it = skip_until(begin, end, IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_INTERNALDATE, ID_AST_POST);
    RETURN_IF_END(it);
    ++it;

    assert((it - 1)->uiIndex == IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_INTERNALDATE &&
           (it - 1)->uiState == ID_AST_POST);
    return it;
}

const ast_record* parse_msg_att_static_body_structure(std::string_view input,
                                                      const ast_record* begin,
                                                      const ast_record* end,
                                                      wip::Body& out_result) {
    // msg-att-static-body-structure = "BODY" ["STRUCTURE"] SP body

    assert(begin->uiIndex == IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_BODY_STRUCTURE);
    assert(end - begin > 2);

    auto it = parse_body(input, begin + 1, end, out_result);

    if (it != end) {
        ++it;
    }

    assert((it - 1)->uiIndex == IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_BODY_STRUCTURE &&
           (it - 1)->uiState == ID_AST_POST);

    return it;
}

const ast_record* parse_msg_att_static_body_section(std::string_view input,
                                                    const ast_record* begin,
                                                    const ast_record* end) {
    assert(begin->uiIndex == IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_BODY_SECTION);

    auto it = begin + 1;

    assert((it - 1)->uiIndex == IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_BODY_SECTION &&
           (it - 1)->uiState == ID_AST_POST);
    return it;
}

const ast_record* parse_msg_att_static_rfc822(std::string_view input,
                                              const ast_record* begin,
                                              const ast_record* end,
                                              MsgAttrRFC822& out_rfc822) {
    assert(begin->uiIndex == IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_RFC822);

    auto it = begin + 1;

    // TODO: we need to check if it is HEADER or complete message by adding HEADER as separate token
    // into ABNF. Having this makes possible to have proper expectation and parse only headers when
    // we got only headers.

    it = skip_until(it, end, IMAP_PARSER_APG_IMPL_NSTRING, ID_AST_PRE);
    RETURN_IF_END(it);

    std::string parsed_rfc822_message;
    it = parse_nstring(input, it, end, out_rfc822.msg_data);

    it = skip_until(it, end, IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_RFC822, ID_AST_POST);
    it++;

    assert((it - 1)->uiIndex == IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_RFC822 &&
           (it - 1)->uiState == ID_AST_POST);
    return it;
}

const ast_record* parse_msg_att_static_rfc822_size(std::string_view input,
                                                   const ast_record* begin,
                                                   const ast_record* end,
                                                   uint32_t& out_result) {
    // msg-att-static-rfc822-size = "RFC822.SIZE" SP number
    assert(begin->uiIndex == IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_RFC822_SIZE);

    auto it = begin + 1;

    it = parse_number(input, it, end, out_result);
    RETURN_IF_END(it);

    it++;

    assert((it - 1)->uiIndex == IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_RFC822_SIZE &&
           (it - 1)->uiState == ID_AST_POST);
    return it;
}

const ast_record* parse_msg_att_static(std::string_view input,
                                       const ast_record* begin,
                                       const ast_record* end,
                                       MsgAttrStatic& out_result) {
    // msg-att-static  = msg-att-static-envelope /
    //                   msg-att-static-uid /
    //                   msg-att-static-internaldate /
    //                   msg-att-static-body-structure /
    //                   msg-att-static-body-section /
    //                   msg-att-static-rfc822 /
    //                   msg-att-static-rfc822-size

    assert(begin->uiIndex == IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC);

    auto it = begin + 1;

    switch (it->uiIndex) {
        case IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_ENVELOPE: {
            Envelope parsed_envelope;
            it = parse_msg_att_static_envelope(input, it, end, parsed_envelope);
            out_result = std::move(parsed_envelope);
            break;
        }
        case IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_UID: {
            msg_attr_uid_t parsed_uid;
            it = parse_msg_att_static_uid(input, it, end, parsed_uid.value);

            out_result = std::move(parsed_uid);
            break;
        }
        case IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_INTERNALDATE: {
            it = parse_msg_att_static_internaldate(input, it, end);
            break;
        }
        case IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_BODY_STRUCTURE: {
            wip::Body parsed_body_structure;
            it = parse_msg_att_static_body_structure(input, it, end, parsed_body_structure);
            out_result = std::move(parsed_body_structure);
            break;
        }
        case IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_BODY_SECTION: {
            it = parse_msg_att_static_body_section(input, it, end);
            break;
        }
        case IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_RFC822: {
            MsgAttrRFC822 parsed_rfc822;
            it = parse_msg_att_static_rfc822(input, it, end, parsed_rfc822);
            out_result = std::move(parsed_rfc822);
            break;
        }
        case IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_RFC822_SIZE: {
            uint32_t parsed_size = 0;
            it = parse_msg_att_static_rfc822_size(input, it, end, parsed_size);
            out_result = MsgAttrRFC822Size{.value = parsed_size};
            break;
        }
        default: {
            log_warning("unexpected attribute returned:\r\n{}", input);
        }
    }

    RETURN_IF_END(it);
    it++;

    assert((it - 1)->uiIndex == IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC &&
           (it - 1)->uiState == ID_AST_POST);

    return it;
}

const ast_record* parse_fetch_message_data(std::string_view input,
                                           const ast_record* begin,
                                           const ast_record* end,
                                           MessageData& out_result) {
    // fetch-message-data = "FETCH" SP msg-att
    // msg-att         = "(" (msg-att-dynamic / msg-att-static) *(SP (msg-att-dynamic
    //                     / msg-att-static)) ")"
    // msg-att-dynamic = "FLAGS" SP "(" [flag-fetch *(SP flag-fetch)] ")"

    assert(begin->uiIndex == IMAP_PARSER_APG_IMPL_FETCH_MESSAGE_DATA);

    auto it = begin;

    it = skip_until(it, end, IMAP_PARSER_APG_IMPL_MSG_ATT, ID_AST_PRE);

    while (it != end &&
           !(it->uiIndex == IMAP_PARSER_APG_IMPL_MSG_ATT && it->uiState == ID_AST_POST)) {
        if (it->uiIndex == IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC && it->uiState == ID_AST_PRE) {
            MsgAttrStatic parsed_static_att;
            it = parse_msg_att_static(input, it, end, parsed_static_att);
            RETURN_IF_END(it);
            out_result.static_attributes.emplace_back(std::move(parsed_static_att));
        } else if (it->uiIndex == IMAP_PARSER_APG_IMPL_MSG_ATT_DYNAMIC &&
                   it->uiState == ID_AST_PRE) {
            // TODO:
            log_error("not implemented");
            it++;
        } else {
            it++;
        }
    }

    assert(it->uiIndex == IMAP_PARSER_APG_IMPL_MSG_ATT && it->uiState == ID_AST_POST);
    it++;
    assert(it->uiIndex == IMAP_PARSER_APG_IMPL_FETCH_MESSAGE_DATA && it->uiState == ID_AST_POST);
    it++;

    assert((it - 1)->uiIndex == IMAP_PARSER_APG_IMPL_FETCH_MESSAGE_DATA &&
           (it - 1)->uiState == ID_AST_POST);
    return it;
}

const ast_record* parse_message_data(std::string_view input,
                                     const ast_record* begin,
                                     const ast_record* end,
                                     MessageData& out_result) {
    // message-data    = nz-number SP (expunge-message-data / fetch-message-data)
    assert(begin->uiIndex == IMAP_PARSER_APG_IMPL_MESSAGE_DATA);

    auto it = begin + 1;

    it = skip_until(it, end, IMAP_PARSER_APG_IMPL_NZ_NUMBER, ID_AST_PRE);
    RETURN_IF_END(it);

    it = parse_number(input, it, end, out_result.message_number);
    RETURN_IF_END(it);

    it = skip_until(
        it, end,
        {IMAP_PARSER_APG_IMPL_FETCH_MESSAGE_DATA, IMAP_PARSER_APG_IMPL_EXPUNGE_MESSAGE_DATA},
        ID_AST_PRE);
    RETURN_IF_END(it);

    switch (it->uiIndex) {
        case IMAP_PARSER_APG_IMPL_FETCH_MESSAGE_DATA: {
            it = parse_fetch_message_data(input, it, end, out_result);
            break;
        }
        case IMAP_PARSER_APG_IMPL_EXPUNGE_MESSAGE_DATA: {
            // TODO:
            log_warning("GOT IMAP_PARSER_APG_IMPL_EXPUNGE_MESSAGE_DATA");
            it++;
            break;
        }
        default: {
            // TODO:
            log_error("unexpected grammar element: {}", it->uiIndex);
            assert(false);
        }
    }

    it++;

    assert((it - 1)->uiIndex == IMAP_PARSER_APG_IMPL_MESSAGE_DATA &&
           (it - 1)->uiState == ID_AST_POST);

    return it;
}

expected<std::vector<MessageData>> parse_message_data_records(std::string_view input_text) {
    std::vector<MessageData> result;

    auto parsing_start_time = std::chrono::steady_clock::now();

    // NOTE, we parse as IMAP response. Because of this, if pass non message-data response the
    // parsing will be fine and we don't report this as error.

    auto ec = apg_invoke_parser__ast(
        IMAP_PARSER_APG_IMPL_RESPONSE, input_text,
        {
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
            IMAP_PARSER_APG_IMPL_NSTRING,
            IMAP_PARSER_APG_IMPL_STRING,
            IMAP_PARSER_APG_IMPL_QUOTED,
            IMAP_PARSER_APG_IMPL_U_LITERAL_DATA,
            IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_BODY_STRUCTURE,
            IMAP_PARSER_APG_IMPL_BODY,
            IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_UID,
            IMAP_PARSER_APG_IMPL_UNIQUEID,
            IMAP_PARSER_APG_IMPL_BODY_TYPE_1PART,
            IMAP_PARSER_APG_IMPL_BODY_TYPE_TEXT,
            IMAP_PARSER_APG_IMPL_BODY_TYPE_BASIC,
            IMAP_PARSER_APG_IMPL_BODY_TYPE_MSG,
            IMAP_PARSER_APG_IMPL_BODY_EXT_1PART,
            IMAP_PARSER_APG_IMPL_BODY_TYPE_MPART,
            IMAP_PARSER_APG_IMPL_MEDIA_TEXT,
            IMAP_PARSER_APG_IMPL_MEDIA_BASIC,
            IMAP_PARSER_APG_IMPL_MEDIA_MESSAGE,
            IMAP_PARSER_APG_IMPL_MEDIA_SUBTYPE,
            IMAP_PARSER_APG_IMPL_MEDIA_BASIC_TYPE_TAG,
            IMAP_PARSER_APG_IMPL_BODY_FIELDS,
            IMAP_PARSER_APG_IMPL_BODY_FLD_PARAM,
            IMAP_PARSER_APG_IMPL_BODY_FLD_ID,
            IMAP_PARSER_APG_IMPL_BODY_FLD_DESC,
            IMAP_PARSER_APG_IMPL_BODY_FLD_ENC,
            IMAP_PARSER_APG_IMPL_BODY_FLD_OCTETS,
            IMAP_PARSER_APG_IMPL_BODY_FLD_PARAM_NAME,
            IMAP_PARSER_APG_IMPL_BODY_FLD_PARAM_VALUE,
            IMAP_PARSER_APG_IMPL_BODY_FLD_DSP,
            IMAP_PARSER_APG_IMPL_BODY_FLD_DSP_STRING,
            IMAP_PARSER_APG_IMPL_BODY_FLD_MD5,
            IMAP_PARSER_APG_IMPL_BODY_EXT_MPART,
            IMAP_PARSER_APG_IMPL_ENV_DATE,
            IMAP_PARSER_APG_IMPL_ENV_SUBJECT,
            IMAP_PARSER_APG_IMPL_ENV_FROM,
            IMAP_PARSER_APG_IMPL_ENV_SENDER,
            IMAP_PARSER_APG_IMPL_ENV_REPLY_TO,
            IMAP_PARSER_APG_IMPL_ENV_TO,
            IMAP_PARSER_APG_IMPL_ENV_CC,
            IMAP_PARSER_APG_IMPL_ENV_BCC,
            IMAP_PARSER_APG_IMPL_ENV_IN_REPLY_TO,
            IMAP_PARSER_APG_IMPL_ENV_MESSAGE_ID,
        },

        [&](const ast_record* begin, const ast_record* end) {
            log_info("l2 parsing almost done , now process AST, time taken so far: {}ms",
                     (std::chrono::steady_clock::now() - parsing_start_time) / 1ms);

            auto it = begin;

            while (it != end) {
                if (it->uiIndex == IMAP_PARSER_APG_IMPL_MESSAGE_DATA && it->uiState == ID_AST_PRE) {
                    MessageData parsed_message_data;
                    it = parse_message_data(input_text, it, end, parsed_message_data);
                    result.emplace_back(std::move(parsed_message_data));
                } else {
                    it++;
                }
            }
        });

    if (ec) {
        return unexpected(ec);
    }

    return result;
}

static void write_message_to_screen(GMimeMessage* message) {
    GMimeStream* stream;

    /* create a new stream for writing to stdout */
    stream = g_mime_stream_pipe_new(STDOUT_FILENO);
    g_mime_stream_pipe_set_owner((GMimeStreamPipe*)stream, FALSE);

    /* write the message to the stream */
    g_mime_object_write_to_stream((GMimeObject*)message, NULL, stream);

    /* flush the stream (kinda like fflush() in libc's stdio) */
    g_mime_stream_flush(stream);

    /* free the output stream */
    g_object_unref(stream);
}

void visit_gmime_message(GMimeMessage* message) {
    g_mime_message_foreach(
        message,
        [](GMimeObject* parent, GMimeObject* part, gpointer user_data) {
            if (GMIME_IS_MESSAGE_PART(part)) {
                log_warning("PART");

                auto* part_message = g_mime_message_part_get_message((GMimeMessagePart*)part);
                if (part_message) {
                    log_warning("parsed message part, descending dwon");
                    visit_gmime_message(part_message);
                } else {
                    log_error("not message part");
                }
            } else if (GMIME_IS_MESSAGE_PARTIAL(part)) {
                /* message/partial */

                /* this is an incomplete message part, probably a
                   large message that the sender has broken into
                   smaller parts and is sending us bit by bit. we
                   could save some info about it so that we could
                   piece this back together again once we get all the
                   parts? */

            } else if (GMIME_IS_MULTIPART(part)) {
                auto multipart = (GMimeMultipart*)part;
                /* multipart/mixed, multipart/alternative,
                 * multipart/related, multipart/signed,
                 * multipart/encrypted, etc... */

                /* we'll get to finding out if this is a
                 * signed/encrypted multipart later... */

                // GMimeMultipart* multipart = (GMimeMultipart*)part;
                // GMimeObject* subpart;

                int n = g_mime_multipart_get_count(multipart);
                log_info("MULTIPART ({} parts)", n);
                for (int i = 0; i < n; i++) {
                    auto subpart = g_mime_multipart_get_part(multipart, i);
                    // visit_gmime_message(subpart);
                    // write_part_bodystructure(subpart, fp);
                }

            } else if (GMIME_IS_PART(part)) {
                /* a normal leaf part, could be text/plain or
                 * image/jpeg etc */
                log_warning("REGULAR PART");

                GMimeContentDisposition* disposition;
                GMimeParamList* params;

                disposition = g_mime_object_get_content_disposition(part);
                if (disposition) {
                    switch (g_mime_part_get_content_encoding((GMimePart*)part)) {
                        case GMIME_CONTENT_ENCODING_7BIT:
                            log_debug("encoding: 7bit");
                            break;
                        case GMIME_CONTENT_ENCODING_8BIT:
                            log_debug("encoding: 8bit");
                            break;
                        case GMIME_CONTENT_ENCODING_BINARY:
                            log_debug("encoding: binary");
                            break;
                        case GMIME_CONTENT_ENCODING_BASE64:
                            log_debug("encoding: base64");
                            break;
                        case GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE:
                            log_debug("encoding: quoted primtable");
                            break;
                        case GMIME_CONTENT_ENCODING_UUENCODE:
                            log_debug("encoding: xuencode");
                            break;
                        default:
                            log_debug("encoding: nil");
                    }
                } else {
                    log_info("no content disposition");
                }
                // if (disposition) {
                // fprintf (fp, "\"%s\" ", g_mime_content_disposition_get_disposition
                // (disposition));
                // 	params = g_mime_content_disposition_get_parameters (disposition);
                // 	if ((n = g_mime_param_list_length (params)) > 0) {
                // 		fputc ('(', fp);
                // 		for (i = 0; i < n; i++) {
                // 			if (i > 0)
                // 				fputc (' ', fp);

                // 			param = g_mime_param_list_get_parameter_at (params, i);
                // 			fprintf (fp, "\"%s\" \"%s\"", g_mime_param_get_name (param),
                // 				 g_mime_param_get_value (param));
                // 		}
                // 		fputs (") ", fp);
                // 	} else {
                // 		fputs ("NIL ", fp);
                // 	}
                // } else {
                // 	fputs ("NIL NIL ", fp);
                // }
                // switch (g_mime_part_get_content_encoding ((GMimePart *) part)) {
                // case GMIME_CONTENT_ENCODING_7BIT:
                // 	fputs ("\"7bit\"", fp);
                // 	break;
                // case GMIME_CONTENT_ENCODING_8BIT:
                // 	fputs ("\"8bit\"", fp);
                // 	break;
                // case GMIME_CONTENT_ENCODING_BINARY:
                // 	fputs ("\"binary\"", fp);
                // 	break;
                // case GMIME_CONTENT_ENCODING_BASE64:
                // 	fputs ("\"base64\"", fp);
                // 	break;
                // case GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE:
                // 	fputs ("\"quoted-printable\"", fp);
                // 	break;
                // case GMIME_CONTENT_ENCODING_UUENCODE:
                // 	fputs ("\"x-uuencode\"", fp);
                // 	break;
                // default:
                // 	fputs ("NIL", fp);
                // }
            }
        },
        nullptr);
}

// functions for working with parts
// Part known to have:
//  1) content disposition
//  2) content type
//  3) encoding (base64, binary, quoted-printable, etc..)
//  4) charset (e.g. utf8, can it have something else?)
enum class content_disposition_t {};

// Returns content type and media subtype.
struct content_type_t {
    std::string type;
    std::string media_subtype;
    std::vector<std::pair<std::string, std::string>> params;
};

// TODO: use expected.
expected<content_type_t> get_part_content_type(GMimeObject* part) {
    content_type_t result;

    GMimeContentType* content_type = g_mime_object_get_content_type(part);
    if (!content_type) {
        log_error("no content type in part");
        return unexpected(make_error_code(parser_errc::parser_fail_l2));
    }

    const char* type_str = g_mime_content_type_get_media_type(content_type);
    if (type_str) {
        result.type = type_str;
    }

    const char* media_subtype_str = g_mime_content_type_get_media_subtype(content_type);
    if (media_subtype_str) {
        result.media_subtype = media_subtype_str;
    }

    /* Content-Type params */
    GMimeParamList* params = g_mime_content_type_get_parameters(content_type);
    if (params) {
        int params_count = g_mime_param_list_length(params);
        if (params_count > 0) {
            for (int i = 0; i < params_count; ++i) {
                auto param = g_mime_param_list_get_parameter_at(params, i);
                auto name = g_mime_param_get_name(param);
                auto value = g_mime_param_get_value(param);
                if (name && value) {
                    result.params.emplace_back(name, value);
                }
            }
        }
    }

    return result;
}

expected<rfc822_headers_t> decode_headers_from_part(GMimeObject* part) {
    rfc822_headers_t rfc822_headers;

    GMimeHeaderList* header_list = g_mime_object_get_header_list(part);
    if (!header_list) {
        log_error("no headers in part");
        return unexpected(make_error_code(parser_errc::parser_fail_l2));
    }

    const int headers_num = g_mime_header_list_get_count(header_list);
    for (int i = 0; i < headers_num; ++i) {
        auto header = g_mime_header_list_get_header_at(header_list, i);
        if (!header) {
            log_warning("no header at {}", i);
            continue;
        }
        auto header_name = g_mime_header_get_name(header);
        if (!header_name) {
            log_warning("header at {} does not have a name", i);
            continue;
        }

        auto header_value = g_mime_header_get_value(header);
        if (!header_value) {
            log_warning("header at {} does not have a value", i);
            continue;
        }

        rfc822_headers.emplace_back(header_name, header_value);
    }

    return std::move(rfc822_headers);
}

// Assuming \part is leaf part with content type text/html, returns HTML in utf8 charset.
expected<std::string> decode_html_content_from_part(GMimeObject* part) {
    GMimeContentEncoding encoding = g_mime_part_get_content_encoding((GMimePart*)part);
    if (encoding != GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE &&
        encoding != GMIME_CONTENT_ENCODING_DEFAULT && encoding != GMIME_CONTENT_ENCODING_BASE64) {
        log_error("dont know how to decode html from {}", static_cast<int>(encoding));
        return unexpected(make_error_code(parser_errc::parser_fail_l2));
    }

    // TODO: support more: GMIME_CONTENT_ENCODING_7BIT, GMIME_CONTENT_ENCODING_8BIT,
    // GMIME_CONTENT_ENCODING_BASE64, GMIME_CONTENT_ENCODING_UUENCODE, GMIME_CONTENT_ENCODING_BASE64

    GMimeDataWrapper* content = g_mime_part_get_content((GMimePart*)part);
    if (!content) {
        log_error("no content in part");
        return unexpected(make_error_code(parser_errc::parser_fail_l2));
    }

    GMimeStream* content_stream = g_mime_data_wrapper_get_stream(content);
    if (!content_stream) {
        log_error("failed getting stream for content");
        return unexpected(make_error_code(parser_errc::parser_fail_l2));
    }

    // For the sake of simplicity data is read out before decoding. It should be just fine for html
    // text.
    // TODO: we need to put some limit here.
    ssize_t bytes_read = 0;
    std::string all_content;
    std::string buffer(4096, 0);
    while ((bytes_read = g_mime_stream_read(content_stream, buffer.data(), buffer.size())) > 0) {
        all_content.append(buffer.data(), bytes_read);
    }

    if (encoding == GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE) {
        GMimeEncoding decoder;
        g_mime_encoding_init_decode(&decoder, encoding);

        size_t out_bytes_needed = g_mime_encoding_outlen(&decoder, all_content.size());
        log_debug("bytes needed for decoding utf8 html from qp: {}", out_bytes_needed);
        std::string decoded_content(out_bytes_needed, '\0');
        size_t n = g_mime_encoding_step(&decoder, all_content.data(), all_content.size(),
                                        decoded_content.data());
        decoded_content.resize(n);
        return std::move(decoded_content);
    } else if (encoding == GMIME_CONTENT_ENCODING_DEFAULT) {
        // TODO: check, but it seems like this means no encoding at all, just use as is (there is
        // aslo param 'charset' which needs to be taken into account.)
        return std::move(all_content);
    } else if (encoding == GMIME_CONTENT_ENCODING_BASE64) {
        GMimeEncoding decoder;
        g_mime_encoding_init_decode(&decoder, encoding);

        size_t out_bytes_needed = g_mime_encoding_outlen(&decoder, all_content.size());

        log_info("decoding html from base64, expected output: {}", out_bytes_needed);

        // TODO: some max size from parser settings?
        std::string decoded_content(out_bytes_needed, '\0');

        size_t n = g_mime_encoding_step(&decoder, all_content.data(), all_content.size(),
                                        decoded_content.data());
        decoded_content.resize(n);
        return decoded_content;
    } else {
        // TODO: support more. Qeuestion: how to know whether it is UTF8 or not?
        log_error("unsupported encoding format");
        return unexpected(make_error_code(parser_errc::parser_fail_l2));
    }
}

struct image_data_t {
    std::vector<char> data;
};
expected<image_data_t> decode_image_content_from_part(GMimeObject* part) {
    GMimeContentEncoding encoding = g_mime_part_get_content_encoding((GMimePart*)part);

    if (encoding != GMIME_CONTENT_ENCODING_BASE64) {
        log_error("unsupported encoding image format: {}", static_cast<int>(encoding));
        return unexpected(make_error_code(parser_errc::parser_fail_l2));
    }

    GMimeDataWrapper* content = g_mime_part_get_content((GMimePart*)part);
    if (!content) {
        log_error("no content in image part");
        return unexpected(make_error_code(parser_errc::parser_fail_l2));
    }

    GMimeStream* content_stream = g_mime_data_wrapper_get_stream(content);
    if (!content_stream) {
        log_error("failed getting stream for image content");
        return unexpected(make_error_code(parser_errc::parser_fail_l2));
    }

    if (encoding == GMIME_CONTENT_ENCODING_BASE64) {
        ssize_t bytes_read = 0;
        std::string all_content;
        std::string buffer(4096, 0);
        while ((bytes_read = g_mime_stream_read(content_stream, buffer.data(), buffer.size())) >
               0) {
            all_content.append(buffer.data(), bytes_read);
        }

        GMimeEncoding decoder;
        g_mime_encoding_init_decode(&decoder, encoding);

        size_t out_bytes_needed = g_mime_encoding_outlen(&decoder, all_content.size());

        log_info("decoding image from base64, expected output: {}", out_bytes_needed);

        // TODO: some max size from parser settings?
        std::vector<char> decoded_content(out_bytes_needed, 0);

        size_t n = g_mime_encoding_step(&decoder, all_content.data(), all_content.size(),
                                        decoded_content.data());
        decoded_content.resize(n);

        return image_data_t{.data = std::move(decoded_content)};

    } else {
        // TODO: support more. Qeuestion: how to know whether it is UTF8 or not?
        log_error("unsupported encoding format");
        return unexpected(make_error_code(parser_errc::parser_fail_l2));
    }
}

static void process_part(GMimeObject* parent, GMimeObject* part, void* user_data) {
    static int counter = 0;
    ++counter;

    /* find out what class 'part' is... */
    if (GMIME_IS_MESSAGE_PART(part)) {
        /* message/rfc822 or message/news */
        GMimeMessage* message;

        log_debug("message/rfc822 or message/news");

        /* g_mime_message_foreach() won't descend into
           child message parts, so if we want to count any
           subparts of this child message, we'll have to call
           g_mime_message_foreach() again here. */

        message = g_mime_message_part_get_message((GMimeMessagePart*)part);
        g_mime_message_foreach(message, process_part, user_data);
    } else if (GMIME_IS_MESSAGE_PARTIAL(part)) {
        /* message/partial */

        log_debug("message/partial");

        /* this is an incomplete message part, probably a
           large message that the sender has broken into
           smaller parts and is sending us bit by bit. we
           could save some info about it so that we could
           piece this back together again once we get all the
           parts? */
    } else if (GMIME_IS_MULTIPART(part)) {
        /* multipart/mixed, multipart/alternative,
         * multipart/related, multipart/signed,
         * multipart/encrypted, etc... */

        log_debug("multipart/mixed");

        /* we'll get to finding out if this is a
         * signed/encrypted multipart later... */
    } else if (GMIME_IS_PART(part)) {
        /* a normal leaf part, could be text/plain or
         * image/jpeg etc */
        log_empty_line();
        log_info("processing part ..");

        GMimeHeaderList* header_list = g_mime_object_get_header_list(part);
        if (header_list) {
            auto rfc822_headers_or_err = decode_headers_from_part(part);
            if (rfc822_headers_or_err) {
                const rfc822_headers_t& rfc822_headers = *rfc822_headers_or_err;
                for (auto& [k, v] : rfc822_headers) {
                    log_info("'{}': '{}'", k, v);
                }

            } else {
                log_error("no headers in rfc822 message: {}", rfc822_headers_or_err.error());
            }

            // static const std::set<std::string> envelope_headers = {
            //     envelope_fields::date,      envelope_fields::subject, envelope_fields::subject,
            //     envelope_fields::from,      envelope_fields::sender,  envelope_fields::reply_to,
            //     envelope_fields::to,        envelope_fields::cc,      envelope_fields::bcc,
            //     envelope_fields::message_id};
        }

        auto content_type_or_err = get_part_content_type(part);
        if (!content_type_or_err) {
            log_error("failed parsing content type: {}", content_type_or_err.error());
            return;
        }
        const auto& content_type = *content_type_or_err;

        if (content_type.type == "text" && content_type.media_subtype == "html") {
            log_info("found text/html, here are params:");
            for (auto& [p, v] : content_type.params) {
                log_info("  {}: {}", p, v);
            }

            auto text_html_or_err = decode_html_content_from_part(part);
            if (!text_html_or_err) {
                log_error("failed decoding html from part: {}", text_html_or_err.error());
                return;
            }
            const auto& text_html = *text_html_or_err;

            std::string file_name = fmt::format("part_content_html_{}.html", counter);
            std::ofstream file_stream(file_name, std::ios_base::out);
            if (file_stream) {
                file_stream << text_html;

                if (file_stream.good()) {
                    log_info("decoded html for part, stored in file: {}", file_name);
                } else {
                    log_error("failed writing file for part.size was: {}", text_html.size());
                }
            }

        } else if (content_type.type == "text" && content_type.media_subtype == "plain") {
            // TODO:
            log_info("skipping text/plain");
            return;
        } else if (content_type.type == "image") {
            auto image_data_or_err = decode_image_content_from_part(part);
            if (!image_data_or_err) {
                log_error("failed decoding image data for part: {}", image_data_or_err.error());
                return;
            }
            auto& image_data = *image_data_or_err;

            std::string file_name = fmt::format("part_content_html_{}.png", counter);
            std::ofstream file_stream(file_name, std::ios_base::out | std::ios_base::binary);
            if (file_stream) {
                file_stream.write(image_data.data.data(), image_data.data.size());

                if (file_stream.good()) {
                    log_info("decoded png part, stored in file: {}", file_name);
                } else {
                    log_error("failed writing file png data for part.size was: {}",
                              image_data.data.size());
                }
            }

        } else {
            log_info("skipping unknown yet part: {}/{}", content_type.type,
                     content_type.media_subtype);
        }

    } else {
        g_assert_not_reached();
    }
}

expected<void> decode_mesage_parts(GMimeMessage* message) {
    // TODO: why message but not part?
    g_mime_message_foreach(message, &process_part, nullptr);
    return {};
}

expected<void> parse_rfc822_message(std::string_view input_text) {
    GMimeStream* stream = g_mime_stream_mem_new_with_buffer(input_text.data(), input_text.size());
    if (!stream) {
        log_error("failed creating stream");
        return unexpected(make_error_code(parser_errc::parser_fail_l2));
    }
    GMimeParser* parser = g_mime_parser_new_with_stream(stream);
    if (!parser) {
        log_error("failed creating parser from stream");
        return unexpected(make_error_code(parser_errc::parser_fail_l2));
    }
    log_debug("parser created");

    GMimeMessage* message = g_mime_parser_construct_message(parser, nullptr);
    if (!message) {
        log_error("failed constructing message from parser");
        return unexpected(make_error_code(parser_errc::parser_fail_l2));
    }

    log_debug("message parsed, message id: {}", message->message_id);

    // now we are going to get the following:
    //  1. header
    //  2. bodystructure
    //  3. parts

    auto rfc822_headers_or_err = decode_headers_from_part((GMimeObject*)message);
    if (!rfc822_headers_or_err) {
        log_error("no headers in rfc822 message: {}", rfc822_headers_or_err.error());
        return unexpected(rfc822_headers_or_err.error());
    }
    const rfc822_headers_t& rfc822_headers = *rfc822_headers_or_err;

    static const std::set<std::string> envelope_headers = {
        envelope_fields::date,      envelope_fields::subject, envelope_fields::subject,
        envelope_fields::from,      envelope_fields::sender,  envelope_fields::reply_to,
        envelope_fields::to,        envelope_fields::cc,      envelope_fields::bcc,
        envelope_fields::message_id};
    for (auto& [k, v] : rfc822_headers) {
        if (envelope_headers.count(k) > 0) {
            log_info("'{}': '{}'", k, v);
        }
    }

    auto nothing_or_err = decode_mesage_parts(message);
    if (!nothing_or_err) {
        log_error("failed decoding message parts: {}", nothing_or_err.error());
        return unexpected(nothing_or_err.error());
    }

    return {};
}

}  // namespace emailkit::imap_parser
