#include "imap_parser.hpp"
#include <emailkit/log.hpp>

#include <parser.h>                // apg70
#include "imap_parser_apg_impl.h"  // grammar generated for apg70

#include <function2/function2.hpp>
#include <vector>

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

    // XCTOR macros sets kind of label (setjmp) that can be jumped to. So in case exception occurs
    // in APG it will jump back (longjmp) to this label but this time apg_exception.try_ will be set
    // to FALSE.
    XCTOR(apg_exception);
    if (apg_exception.try_) {
        log_debug("constructing APG parser object");
        parser = ::vpParserCtor(&apg_exception, vpImapParserApgImplInit);
        log_debug("constructing APG parser object -- done");

        // Set single callback for each rule and use context for routing to user-defined callbacks.
        for (auto& [rule, callback_ref] : cbs) {
            ctx.callbacks_map[rule] = callback_ref;
            ::vParserSetRuleCallback(
                parser, rule, +[](callback_data* cb_data) {
                    auto* invoke_ctx = static_cast<apg_invoke_context*>(cb_data->vpUserData);
                    if (cb_data->uiParserState == ID_MATCH) {
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
        apg_parser_config.vpUserData = &ctx;
        apg_parser_config.bParseSubString = APG_FALSE;

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

}  // namespace

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

void parse_mailbox_data(std::string_view input) {
    // 6.3.8 LIST Command https://datatracker.ietf.org/doc/html/rfc3501#section-6.3.8
    // 7.2.2 LIST Response (https://datatracker.ietf.org/doc/html/rfc3501#section-7.2.2)
    // Related grammar rules: mailbox-data, mailbox-list, mailbox-data, mbx-list-flags,
    // mbx-list-sflag

    struct local_state {
        // std::vector<std::string>
    };

    apg_invoke_parser(IMAP_PARSER_APG_IMPL_MAILBOX_DATA, input,
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
                          {IMAP_PARSER_APG_IMPL_MAILBOX,
                           [](std::string_view tok) {
                               log_debug("IMAP_PARSER_APG_IMPL_MAILBOX handler: '{}'", tok);
                           }},
                          {IMAP_PARSER_APG_IMPL_QUOTED_CHAR,
                           [](std::string_view tok) {
                               log_debug("IMAP_PARSER_APG_IMPL_QUOTED_CHAR handler: '{}'", tok);
                           }},
                          {IMAP_PARSER_APG_IMPL_NIL,
                           [](std::string_view tok) {
                               log_debug("IMAP_PARSER_APG_IMPL_NIL handler: '{}'", tok);
                           }},
                          {IMAP_PARSER_APG_IMPL_ATOM_CHAR,
                           [](std::string_view tok) {
                               log_debug("IMAP_PARSER_APG_IMPL_ATOM_CHAR handler: '{}'", tok);
                           }},

                      });
}  // namespace emailkit

}  // namespace emailkit::imap_parser
