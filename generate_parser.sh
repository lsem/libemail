#!/usr/bin/env bash

cd "$(dirname "$0")"
./bld/vendor/apg-7.0/apg/apg70 -i src/emailkit/src/grammars/abnf_grammar.abnf -o  src/emailkit/src/imap_parser_apg_impl
