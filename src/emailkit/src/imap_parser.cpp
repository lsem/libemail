#include <emailkit/global.hpp>

#include "imap_parser.hpp"

#include <emailkit/log.hpp>

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

void parse_flags_list(std::string_view input) {
    apg_invoke_parser(IMAP_PARSER_APG_IMPL_MBX_LIST_FLAGS, input,
                      {
                          {IMAP_PARSER_APG_IMPL_MBX_LIST_FLAGS,
                           [](std::string_view tok) {
                               log_debug("IMAP_PARSER_APG_IMPL_MBX_LIST_FLAGS handler: '{}'", tok);
                           }},
                          {IMAP_PARSER_APG_IMPL_MBX_LIST_OFLAG,
                           [](std::string_view tok) {
                               log_debug("IMAP_PARSER_APG_IMPL_MBX_LIST_OFLAG handler: '{}'", tok);
                           }},
                          {IMAP_PARSER_APG_IMPL_MBX_LIST_SFLAG,
                           [](std::string_view tok) {
                               log_debug("IMAP_PARSER_APG_IMPL_MBX_LIST_SFLAG handler: '{}'", tok);
                           }},
                          {IMAP_PARSER_APG_IMPL_FLAG_EXTENSION,
                           [](std::string_view tok) {
                               log_debug("IMAP_PARSER_APG_IMPL_FLAG_EXTENSION handler: '{}'", tok);
                           }},
                          {IMAP_PARSER_APG_IMPL_QUOTED_CHAR,
                           [](std::string_view tok) {
                               log_debug("IMAP_PARSER_APG_IMPL_QUOTED_CHAR handler: '{}'", tok);
                           }},
                          {IMAP_PARSER_APG_IMPL_ATOM_CHAR,
                           [](std::string_view tok) {
                               log_debug("IMAP_PARSER_APG_IMPL_ATOM_CHAR handler: '{}'", tok);
                           }},

                      });
}

static aint ast_mailbox_data_cb(ast_data* spData) {
    if (spData->uiState == ID_AST_POST) {
        // increment the A node count
        // my_data* spMyData = (my_data*)spData->vpUserData;
        // spMyData->uiCountA++;
    }
    return ID_AST_OK;
}

static aint ast_flag_list_cb(ast_data* spData) {
    if (spData->uiState == ID_AST_POST) {
        // increment the A node count
        // my_data* spMyData = (my_data*)spData->vpUserData;
        // spMyData->uiCountA++;
    }
    return ID_AST_OK;
}

static aint ast_flag_cb(ast_data* spData) {
    if (spData->uiState == ID_AST_POST) {
        // increment the A node count
        // my_data* spMyData = (my_data*)spData->vpUserData;
        // spMyData->uiCountA++;
    }
    return ID_AST_OK;
}

// static aint ast_oflag_cb(ast_data* spData) {
//     if (spData->uiState == ID_AST_POST) {
//         // increment the A node count
//         // my_data* spMyData = (my_data*)spData->vpUserData;
//         // spMyData->uiCountA++;
//     }
//     return ID_AST_OK;
// }

namespace {

struct rule_and_callback__ast {
    rule_and_callback__ast(aint rule, fu2::function_view<void(std::string_view)> cb_ref)
        : rule(rule), cb_ref(cb_ref) {}

    aint rule;
    fu2::function_view<void(std::string_view)> cb_ref;  // non-owning callback
};

void apg_invoke_parser__ast(uint32_t starting_rule,
                            std::string_view input_text,
                            std::initializer_list<rule_and_callback__ast> cbs,
                            fu2::function_view<void(const ast_record*, const ast_record*)> ast_cb) {
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
    void* ast = NULL;

    XCTOR(apg_exception);
    if (apg_exception.try_) {
        log_debug("APG TRY section begin");

        log_debug("constructing APG parser object");
        parser = ::vpParserCtor(&apg_exception, vpImapParserApgImplInit);
        log_debug("constructing APG parser object -- done");

        log_debug("constructing APG AST object");
        ast = ::vpAstCtor(parser);
        log_debug("constructing APG AST object -- done");

        for (auto& [rule, callback_ref] : cbs) {
            ctx.callbacks_map[rule] = callback_ref;
            ::vAstSetRuleCallback(
                ast, rule, +[](ast_data* ast_data_ptr) -> aint {
                    auto* invoke_ctx = static_cast<apg_invoke_context*>(ast_data_ptr->vpUserData);
                    if (ast_data_ptr->uiState == ID_AST_POST) {
                        auto& callbacks_map = invoke_ctx->callbacks_map;

                        if (ast_data_ptr->uiIndex >= callbacks_map.size()) {
                            log_error(
                                "FATAL: something went wrong. uiRuleIndex: {}, callback map size: "
                                "{}",
                                ast_data_ptr->uiIndex, callbacks_map.size());
                            return ID_AST_SKIP;
                        }

                        if (callbacks_map[ast_data_ptr->uiIndex]) {
                            const char* match_begin =
                                reinterpret_cast<const char*>(ast_data_ptr->acpString) +
                                ast_data_ptr->uiPhraseOffset;

                            std::string_view match_sv{match_begin, ast_data_ptr->uiPhraseLength};
                            callbacks_map[ast_data_ptr->uiIndex](match_sv);
                        }
                    }

                    return ID_AST_OK;
                });
        }

        apg_parser_config.acpInput = input_text_data.data();
        apg_parser_config.uiInputLength = input_text_data.size();
        apg_parser_config.uiStartRule = starting_rule;
        apg_parser_config.bParseSubString = APG_FALSE;
        apg_parser_config.uiLookBehindLength = 0;
        apg_parser_config.vpUserData =
            NULL;  // not used for AST, instead passed to translate function

        log_debug("invoking APG parser");
        ::vParserParse(parser, &apg_parser_config, &apg_parser_state);
        if (!apg_parser_state.uiSuccess) {
            log_error("invoking APG parser -- error; parser state: {}",
                      format_apg_parser_state(apg_parser_state));
        } else {
            log_debug("invoking APG parser -- done");

            // translate the AST
            log_debug("translating AST");
            ::vAstTranslate(ast, &ctx);
            log_debug("translating AST -- done");

            // log_debug("dumping the AST to XML");
            // ::bUtilAstToXml(ast, "u", NULL);
            // log_debug("dumping the AST to XML -- done");

            ast_info info;
            ::vAstInfo(ast, &info);
            size_t indent = 0;
            constexpr size_t INDENT_WIDTH = 4;

            ast_cb(&(info.spRecords[0]), &(info.spRecords[info.uiRecordCount]));
            // for (auto r = info.spRecords; r != info.spRecords + info.uiRecordCount; ++r) {
            //     if (r->uiState == ID_AST_PRE) {
            //         indent += INDENT_WIDTH;

            //         std::string_view match_text{input_text.data() + r->uiPhraseOffset,
            //                                     r->uiPhraseLength};
            //         log_debug("{}PRE: {} ('{}') (offset: {})", std::string(indent, ' '),
            //         r->cpName,
            //                   match_text, r->uiPhraseOffset);
            //     } else {
            //         assert(r->uiState == ID_AST_POST);
            //         log_debug("{}POST: {}", std::string(indent, ' '), r->cpName);
            //         indent -= INDENT_WIDTH;
            //     }
            // }
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
}

}  // namespace

expected<mailbox_data_t> parse_mailbox_data(std::string_view input_text) {
    apg_invoke_parser__ast(
        IMAP_PARSER_APG_IMPL_RESPONSE, input_text,
        {
            {IMAP_PARSER_APG_IMPL_MAILBOX_DATA,
             [&](std::string_view tok) {
                 log_debug("IMAP_PARSER_APG_IMPL_MAILBOX_DATA: '{}'", tok);
             }},
            {IMAP_PARSER_APG_IMPL_FLAG_LIST,
             [&](std::string_view tok) { log_debug("IMAP_PARSER_APG_IMPL_FLAG_LIST: '{}'", tok); }},

            {IMAP_PARSER_APG_IMPL_FLAG,
             [&](std::string_view tok) { log_debug("IMAP_PARSER_APG_IMPL_FLAG: '{}'", tok); }},

            {IMAP_PARSER_APG_IMPL_RESP_COND_STATE,
             [&](std::string_view tok) {
                 log_debug("IMAP_PARSER_APG_IMPL_RESP_COND_STATE: '{}'", tok);
             }},

            {IMAP_PARSER_APG_IMPL_RESP_TEXT,
             [&](std::string_view tok) { log_debug("IMAP_PARSER_APG_IMPL_RESP_TEXT: '{}'", tok); }},
            {IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE,
             [&](std::string_view tok) {
                 log_debug("IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE: '{}'", tok);
             }},
            {IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_PERMANENT_FLAGS,
             [&](std::string_view tok) {
                 log_debug("IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_PERMANENT_FLAGS: '{}'", tok);
             }},

            {IMAP_PARSER_APG_IMPL_MAILBOX_DATA_EXISTS,
             [&](std::string_view tok) {
                 log_debug("IMAP_PARSER_APG_IMPL_MAILBOX_DATA_EXISTS: '{}'", tok);
             }},
            {IMAP_PARSER_APG_IMPL_MAILBOX_DATA_RECENT,
             [&](std::string_view tok) {
                 log_debug("IMAP_PARSER_APG_IMPL_MAILBOX_DATA_RECENT: '{}'", tok);
             }},

            {IMAP_PARSER_APG_IMPL_NUMBER,
             [&](std::string_view tok) { log_debug("IMAP_PARSER_APG_IMPL_NUMBER: '{}'", tok); }},

            {IMAP_PARSER_APG_IMPL_OK,
             [&](std::string_view tok) { log_debug("IMAP_PARSER_APG_IMPL_OK: '{}'", tok); }},
            {IMAP_PARSER_APG_IMPL_NO,
             [&](std::string_view tok) { log_debug("IMAP_PARSER_APG_IMPL_OK: '{}'", tok); }},
            {IMAP_PARSER_APG_IMPL_BAD,
             [&](std::string_view tok) { log_debug("IMAP_PARSER_APG_IMPL_OK: '{}'", tok); }},

        },
        [&](const ast_record* begin, const ast_record* end) {
            // So, response for SELECT command is combination of weird records. There could be
            // multiple lines of mailbox-data and multiple lines of resp-cond-state.
            // mailbox-data itself is variant of different stuff which however cannot be returned
            // from LIST command.
            // SELECT_RESPONSE = VECTOR<VARIANT< RESP_COND_STATE, MAILBOX_DATA >>

            constexpr size_t INDENT_WIDTH = 4;
            size_t indent = 0;
            for (auto it = begin; it != end; ++it) {
                if (it->uiState == ID_AST_PRE) {
                    indent += INDENT_WIDTH;

                    std::string_view match_text{input_text.data() + it->uiPhraseOffset,
                                                it->uiPhraseLength};
                    log_debug("{}PRE: {} ('{}') (offset: {})", std::string(indent, ' '), it->cpName,
                              match_text, it->uiPhraseOffset);
                } else {
                    assert(it->uiState == ID_AST_POST);
                    log_debug("{}POST: {}", std::string(indent, ' '), it->cpName);
                    indent -= INDENT_WIDTH;
                }
            }
        });

    return recent_mailbox_data_t{};
}

}  // namespace emailkit::imap_parser
