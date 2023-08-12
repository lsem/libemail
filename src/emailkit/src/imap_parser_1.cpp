#include "imap_parser_1.hpp"
#include <emailkit/log.hpp>

#include <parser.h>       // apg70
#include "imap_parser.h"  // grammar generated for apg70

#include <vector>

namespace emailkit {

std::string dump_state(const parser_state& state) {
    return fmt::format(
        "parser_state(uiState: {}, phrase_len: {}, string_len: {}, max_tree_depth: {}, hit_count: "
        "{})",
        state.uiState, state.uiPhraseLength, state.uiStringLength, state.uiMaxTreeDepth,
        state.uiHitCount);
}

void do_parse(std::string text) {
    log_debug("started parsing: '{}'", text);

    parser_state sState;
    parser_config sInput;

    exception spException;
    XCTOR(spException);
    if (spException.try_) {
        log_debug("constructing");

        void* parser = vpParserCtor(&spException, vpImapParserInit);

        log_debug("constructed");

        vImapParserRuleCallbacks(parser);

        std::vector<uint8_t> data(text.size());
        for (size_t i = 0; i < text.size(); ++i) {
            data[i] = text[i];
        }

        sInput.acpInput = data.data();
        sInput.uiInputLength = data.size();
        sInput.uiStartRule = IMAP_PARSER_MAILBOX_LIST;
        sInput.vpUserData = nullptr;
        sInput.bParseSubString = APG_FALSE;

        log_debug("parsing!");

        vParserParse(parser, &sInput, &sState);
        if (!sState.uiSuccess) {
            log_error("vParserParse failed: {}", dump_state(sState));
        } else {
            log_debug("parsed!");
        }
    } else {
        log_error("caught weird exception");
    }
}

}  // namespace emailkit
