//
// This C-language parser header was generated by APG Version 7.0.
// User modifications may cause unpredictable results.
//
/*  *************************************************************************************
    Copyright (c) 2021, Lowell D. Thomas
    All rights reserved.

    This file was generated by and is part of APG Version 7.0.
    APG Version 7.0 may be used under the terms of the BSD 2-Clause License.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*   *************************************************************************************/

#ifndef _IMAP_PARSER_APG_IMPL_H_
#define _IMAP_PARSER_APG_IMPL_H_

// rule ids
#define IMAP_PARSER_APG_IMPL_ADDR_ADL 9
#define IMAP_PARSER_APG_IMPL_ADDR_HOST 10
#define IMAP_PARSER_APG_IMPL_ADDR_MAILBOX 11
#define IMAP_PARSER_APG_IMPL_ADDR_NAME 12
#define IMAP_PARSER_APG_IMPL_ADDRESS 8
#define IMAP_PARSER_APG_IMPL_ALPHA 0
#define IMAP_PARSER_APG_IMPL_ANY_ASTRING_CHAR_EXCEPT_PLUS 148
#define IMAP_PARSER_APG_IMPL_ANY_TEXT_CHAR_EXCEPT_QUOTED_SPECIALS 115
#define IMAP_PARSER_APG_IMPL_ANY_TEXT_CHAR_EXCEPT_SB 128
#define IMAP_PARSER_APG_IMPL_APPEND 13
#define IMAP_PARSER_APG_IMPL_ASTRING 14
#define IMAP_PARSER_APG_IMPL_ASTRING_CHAR 15
#define IMAP_PARSER_APG_IMPL_ATOM 16
#define IMAP_PARSER_APG_IMPL_ATOM_CHAR 17
#define IMAP_PARSER_APG_IMPL_ATOM_SPECIALS 18
#define IMAP_PARSER_APG_IMPL_AUTH_TYPE 20
#define IMAP_PARSER_APG_IMPL_AUTHENTICATE 19
#define IMAP_PARSER_APG_IMPL_BASE64 21
#define IMAP_PARSER_APG_IMPL_BASE64_CHAR 22
#define IMAP_PARSER_APG_IMPL_BASE64_TERMINAL 23
#define IMAP_PARSER_APG_IMPL_BODY 24
#define IMAP_PARSER_APG_IMPL_BODY_EXT_1PART 26
#define IMAP_PARSER_APG_IMPL_BODY_EXT_MPART 27
#define IMAP_PARSER_APG_IMPL_BODY_EXTENSION 25
#define IMAP_PARSER_APG_IMPL_BODY_FIELDS 28
#define IMAP_PARSER_APG_IMPL_BODY_FLD_DESC 29
#define IMAP_PARSER_APG_IMPL_BODY_FLD_DSP 30
#define IMAP_PARSER_APG_IMPL_BODY_FLD_ENC 31
#define IMAP_PARSER_APG_IMPL_BODY_FLD_ID 32
#define IMAP_PARSER_APG_IMPL_BODY_FLD_LANG 33
#define IMAP_PARSER_APG_IMPL_BODY_FLD_LINES 35
#define IMAP_PARSER_APG_IMPL_BODY_FLD_LOC 34
#define IMAP_PARSER_APG_IMPL_BODY_FLD_MD5 36
#define IMAP_PARSER_APG_IMPL_BODY_FLD_OCTETS 37
#define IMAP_PARSER_APG_IMPL_BODY_FLD_PARAM 38
#define IMAP_PARSER_APG_IMPL_BODY_TYPE_1PART 39
#define IMAP_PARSER_APG_IMPL_BODY_TYPE_BASIC 40
#define IMAP_PARSER_APG_IMPL_BODY_TYPE_MPART 41
#define IMAP_PARSER_APG_IMPL_BODY_TYPE_MSG 42
#define IMAP_PARSER_APG_IMPL_BODY_TYPE_TEXT 43
#define IMAP_PARSER_APG_IMPL_CAPABILITY 44
#define IMAP_PARSER_APG_IMPL_CAPABILITY_DATA 45
#define IMAP_PARSER_APG_IMPL_CHAR8 46
#define IMAP_PARSER_APG_IMPL_COMMAND 47
#define IMAP_PARSER_APG_IMPL_COMMAND_ANY 48
#define IMAP_PARSER_APG_IMPL_COMMAND_AUTH 49
#define IMAP_PARSER_APG_IMPL_COMMAND_NONAUTH 50
#define IMAP_PARSER_APG_IMPL_COMMAND_SELECT 51
#define IMAP_PARSER_APG_IMPL_CONTINUE_REQ 52
#define IMAP_PARSER_APG_IMPL_COPY 53
#define IMAP_PARSER_APG_IMPL_CR 1
#define IMAP_PARSER_APG_IMPL_CREATE 54
#define IMAP_PARSER_APG_IMPL_CRLF 2
#define IMAP_PARSER_APG_IMPL_CTL 3
#define IMAP_PARSER_APG_IMPL_DATE 55
#define IMAP_PARSER_APG_IMPL_DATE_DAY 56
#define IMAP_PARSER_APG_IMPL_DATE_DAY_FIXED 57
#define IMAP_PARSER_APG_IMPL_DATE_MONTH 58
#define IMAP_PARSER_APG_IMPL_DATE_TEXT 59
#define IMAP_PARSER_APG_IMPL_DATE_TIME 61
#define IMAP_PARSER_APG_IMPL_DATE_YEAR 60
#define IMAP_PARSER_APG_IMPL_DELETE 62
#define IMAP_PARSER_APG_IMPL_DIGIT 4
#define IMAP_PARSER_APG_IMPL_DIGIT_NZ 63
#define IMAP_PARSER_APG_IMPL_DQUOTE 5
#define IMAP_PARSER_APG_IMPL_ENV_BCC 65
#define IMAP_PARSER_APG_IMPL_ENV_CC 66
#define IMAP_PARSER_APG_IMPL_ENV_DATE 67
#define IMAP_PARSER_APG_IMPL_ENV_FROM 68
#define IMAP_PARSER_APG_IMPL_ENV_IN_REPLY_TO 69
#define IMAP_PARSER_APG_IMPL_ENV_MESSAGE_ID 70
#define IMAP_PARSER_APG_IMPL_ENV_REPLY_TO 71
#define IMAP_PARSER_APG_IMPL_ENV_SENDER 72
#define IMAP_PARSER_APG_IMPL_ENV_SUBJECT 73
#define IMAP_PARSER_APG_IMPL_ENV_TO 74
#define IMAP_PARSER_APG_IMPL_ENVELOPE 64
#define IMAP_PARSER_APG_IMPL_EXAMINE 75
#define IMAP_PARSER_APG_IMPL_FETCH 76
#define IMAP_PARSER_APG_IMPL_FETCH_ATT 77
#define IMAP_PARSER_APG_IMPL_FLAG 78
#define IMAP_PARSER_APG_IMPL_FLAG_EXTENSION 79
#define IMAP_PARSER_APG_IMPL_FLAG_FETCH 80
#define IMAP_PARSER_APG_IMPL_FLAG_KEYWORD 81
#define IMAP_PARSER_APG_IMPL_FLAG_LIST 82
#define IMAP_PARSER_APG_IMPL_FLAG_PERM 83
#define IMAP_PARSER_APG_IMPL_GREETING 84
#define IMAP_PARSER_APG_IMPL_HEADER_FLD_NAME 85
#define IMAP_PARSER_APG_IMPL_HEADER_LIST 86
#define IMAP_PARSER_APG_IMPL_LF 6
#define IMAP_PARSER_APG_IMPL_LIST 87
#define IMAP_PARSER_APG_IMPL_LIST_CHAR 89
#define IMAP_PARSER_APG_IMPL_LIST_MAILBOX 88
#define IMAP_PARSER_APG_IMPL_LIST_WILDCARDS 90
#define IMAP_PARSER_APG_IMPL_LITERAL 91
#define IMAP_PARSER_APG_IMPL_LOGIN 92
#define IMAP_PARSER_APG_IMPL_LSUB 93
#define IMAP_PARSER_APG_IMPL_MAILBOX 94
#define IMAP_PARSER_APG_IMPL_MAILBOX_DATA 95
#define IMAP_PARSER_APG_IMPL_MAILBOX_LIST 96
#define IMAP_PARSER_APG_IMPL_MBX_LIST_FLAGS 97
#define IMAP_PARSER_APG_IMPL_MBX_LIST_OFLAG 98
#define IMAP_PARSER_APG_IMPL_MBX_LIST_SFLAG 99
#define IMAP_PARSER_APG_IMPL_MEDIA_BASIC 100
#define IMAP_PARSER_APG_IMPL_MEDIA_MESSAGE 101
#define IMAP_PARSER_APG_IMPL_MEDIA_SUBTYPE 102
#define IMAP_PARSER_APG_IMPL_MEDIA_TEXT 103
#define IMAP_PARSER_APG_IMPL_MESSAGE_DATA 104
#define IMAP_PARSER_APG_IMPL_MSG_ATT 105
#define IMAP_PARSER_APG_IMPL_MSG_ATT_DYNAMIC 106
#define IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC 107
#define IMAP_PARSER_APG_IMPL_NIL 108
#define IMAP_PARSER_APG_IMPL_NSTRING 109
#define IMAP_PARSER_APG_IMPL_NUMBER 110
#define IMAP_PARSER_APG_IMPL_NZ_NUMBER 111
#define IMAP_PARSER_APG_IMPL_PASSWORD 112
#define IMAP_PARSER_APG_IMPL_QUOTED 113
#define IMAP_PARSER_APG_IMPL_QUOTED_CHAR 114
#define IMAP_PARSER_APG_IMPL_QUOTED_SPECIALS 116
#define IMAP_PARSER_APG_IMPL_RENAME 117
#define IMAP_PARSER_APG_IMPL_RESP_COND_AUTH 123
#define IMAP_PARSER_APG_IMPL_RESP_COND_BYE 124
#define IMAP_PARSER_APG_IMPL_RESP_COND_STATE 125
#define IMAP_PARSER_APG_IMPL_RESP_SPECIALS 126
#define IMAP_PARSER_APG_IMPL_RESP_TEXT 127
#define IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE 129
#define IMAP_PARSER_APG_IMPL_RESPONSE 118
#define IMAP_PARSER_APG_IMPL_RESPONSE_DATA 119
#define IMAP_PARSER_APG_IMPL_RESPONSE_DONE 120
#define IMAP_PARSER_APG_IMPL_RESPONSE_FATAL 121
#define IMAP_PARSER_APG_IMPL_RESPONSE_TAGGED 122
#define IMAP_PARSER_APG_IMPL_SEARCH 130
#define IMAP_PARSER_APG_IMPL_SEARCH_KEY 131
#define IMAP_PARSER_APG_IMPL_SECTION 132
#define IMAP_PARSER_APG_IMPL_SECTION_MSGTEXT 133
#define IMAP_PARSER_APG_IMPL_SECTION_PART 134
#define IMAP_PARSER_APG_IMPL_SECTION_SPEC 135
#define IMAP_PARSER_APG_IMPL_SECTION_TEXT 136
#define IMAP_PARSER_APG_IMPL_SELECT 137
#define IMAP_PARSER_APG_IMPL_SEQ_NUMBER 138
#define IMAP_PARSER_APG_IMPL_SEQ_RANGE 139
#define IMAP_PARSER_APG_IMPL_SEQUENCE_SET 140
#define IMAP_PARSER_APG_IMPL_SP 7
#define IMAP_PARSER_APG_IMPL_STATUS 141
#define IMAP_PARSER_APG_IMPL_STATUS_ATT 142
#define IMAP_PARSER_APG_IMPL_STATUS_ATT_LIST 143
#define IMAP_PARSER_APG_IMPL_STORE 144
#define IMAP_PARSER_APG_IMPL_STORE_ATT_FLAGS 145
#define IMAP_PARSER_APG_IMPL_STRING 146
#define IMAP_PARSER_APG_IMPL_SUBSCRIBE 147
#define IMAP_PARSER_APG_IMPL_TAG 149
#define IMAP_PARSER_APG_IMPL_TEXT 150
#define IMAP_PARSER_APG_IMPL_TEXT_CHAR 151
#define IMAP_PARSER_APG_IMPL_TIME 152
#define IMAP_PARSER_APG_IMPL_UID 153
#define IMAP_PARSER_APG_IMPL_UNIQUEID 154
#define IMAP_PARSER_APG_IMPL_UNSUBSCRIBE 155
#define IMAP_PARSER_APG_IMPL_USERID 156
#define IMAP_PARSER_APG_IMPL_ZONE 157
#define RULE_COUNT_IMAP_PARSER_APG_IMPL 158

// pointer to parser initialization data
extern void* vpImapParserApgImplInit;

// Helper function(s) for setting rule/UDT name callbacks.
// Un-comment and replace named NULL with pointer to the appropriate callback function.
//  NOTE: This can easily be modified for setting AST callback functions:
//        Replace parser_callback with ast_callback and
//        vParserSetRuleCallback(vpParserCtx) with vAstSetRuleCallback(vpAstCtx) and
//        vParserSetUdtCallback(vpParserCtx) with vAstSetUdtCallback(vpAstCtx).
/****************************************************************
void vImapParserApgImplRuleCallbacks(void* vpParserCtx){
    aint ui;
    parser_callback cb[RULE_COUNT_IMAP_PARSER_APG_IMPL];
    cb[IMAP_PARSER_APG_IMPL_ADDR_ADL] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ADDR_HOST] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ADDR_MAILBOX] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ADDR_NAME] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ADDRESS] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ALPHA] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ANY_ASTRING_CHAR_EXCEPT_PLUS] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ANY_TEXT_CHAR_EXCEPT_QUOTED_SPECIALS] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ANY_TEXT_CHAR_EXCEPT_SB] = NULL;
    cb[IMAP_PARSER_APG_IMPL_APPEND] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ASTRING] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ASTRING_CHAR] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ATOM] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ATOM_CHAR] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ATOM_SPECIALS] = NULL;
    cb[IMAP_PARSER_APG_IMPL_AUTH_TYPE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_AUTHENTICATE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BASE64] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BASE64_CHAR] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BASE64_TERMINAL] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_EXT_1PART] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_EXT_MPART] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_EXTENSION] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FIELDS] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FLD_DESC] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FLD_DSP] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FLD_ENC] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FLD_ID] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FLD_LANG] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FLD_LINES] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FLD_LOC] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FLD_MD5] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FLD_OCTETS] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FLD_PARAM] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_TYPE_1PART] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_TYPE_BASIC] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_TYPE_MPART] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_TYPE_MSG] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_TYPE_TEXT] = NULL;
    cb[IMAP_PARSER_APG_IMPL_CAPABILITY] = NULL;
    cb[IMAP_PARSER_APG_IMPL_CAPABILITY_DATA] = NULL;
    cb[IMAP_PARSER_APG_IMPL_CHAR8] = NULL;
    cb[IMAP_PARSER_APG_IMPL_COMMAND] = NULL;
    cb[IMAP_PARSER_APG_IMPL_COMMAND_ANY] = NULL;
    cb[IMAP_PARSER_APG_IMPL_COMMAND_AUTH] = NULL;
    cb[IMAP_PARSER_APG_IMPL_COMMAND_NONAUTH] = NULL;
    cb[IMAP_PARSER_APG_IMPL_COMMAND_SELECT] = NULL;
    cb[IMAP_PARSER_APG_IMPL_CONTINUE_REQ] = NULL;
    cb[IMAP_PARSER_APG_IMPL_COPY] = NULL;
    cb[IMAP_PARSER_APG_IMPL_CR] = NULL;
    cb[IMAP_PARSER_APG_IMPL_CREATE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_CRLF] = NULL;
    cb[IMAP_PARSER_APG_IMPL_CTL] = NULL;
    cb[IMAP_PARSER_APG_IMPL_DATE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_DATE_DAY] = NULL;
    cb[IMAP_PARSER_APG_IMPL_DATE_DAY_FIXED] = NULL;
    cb[IMAP_PARSER_APG_IMPL_DATE_MONTH] = NULL;
    cb[IMAP_PARSER_APG_IMPL_DATE_TEXT] = NULL;
    cb[IMAP_PARSER_APG_IMPL_DATE_TIME] = NULL;
    cb[IMAP_PARSER_APG_IMPL_DATE_YEAR] = NULL;
    cb[IMAP_PARSER_APG_IMPL_DELETE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_DIGIT] = NULL;
    cb[IMAP_PARSER_APG_IMPL_DIGIT_NZ] = NULL;
    cb[IMAP_PARSER_APG_IMPL_DQUOTE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ENV_BCC] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ENV_CC] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ENV_DATE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ENV_FROM] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ENV_IN_REPLY_TO] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ENV_MESSAGE_ID] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ENV_REPLY_TO] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ENV_SENDER] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ENV_SUBJECT] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ENV_TO] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ENVELOPE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_EXAMINE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_FETCH] = NULL;
    cb[IMAP_PARSER_APG_IMPL_FETCH_ATT] = NULL;
    cb[IMAP_PARSER_APG_IMPL_FLAG] = NULL;
    cb[IMAP_PARSER_APG_IMPL_FLAG_EXTENSION] = NULL;
    cb[IMAP_PARSER_APG_IMPL_FLAG_FETCH] = NULL;
    cb[IMAP_PARSER_APG_IMPL_FLAG_KEYWORD] = NULL;
    cb[IMAP_PARSER_APG_IMPL_FLAG_LIST] = NULL;
    cb[IMAP_PARSER_APG_IMPL_FLAG_PERM] = NULL;
    cb[IMAP_PARSER_APG_IMPL_GREETING] = NULL;
    cb[IMAP_PARSER_APG_IMPL_HEADER_FLD_NAME] = NULL;
    cb[IMAP_PARSER_APG_IMPL_HEADER_LIST] = NULL;
    cb[IMAP_PARSER_APG_IMPL_LF] = NULL;
    cb[IMAP_PARSER_APG_IMPL_LIST] = NULL;
    cb[IMAP_PARSER_APG_IMPL_LIST_CHAR] = NULL;
    cb[IMAP_PARSER_APG_IMPL_LIST_MAILBOX] = NULL;
    cb[IMAP_PARSER_APG_IMPL_LIST_WILDCARDS] = NULL;
    cb[IMAP_PARSER_APG_IMPL_LITERAL] = NULL;
    cb[IMAP_PARSER_APG_IMPL_LOGIN] = NULL;
    cb[IMAP_PARSER_APG_IMPL_LSUB] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MAILBOX] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MAILBOX_DATA] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MAILBOX_LIST] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MBX_LIST_FLAGS] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MBX_LIST_OFLAG] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MBX_LIST_SFLAG] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MEDIA_BASIC] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MEDIA_MESSAGE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MEDIA_SUBTYPE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MEDIA_TEXT] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MESSAGE_DATA] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MSG_ATT] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MSG_ATT_DYNAMIC] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC] = NULL;
    cb[IMAP_PARSER_APG_IMPL_NIL] = NULL;
    cb[IMAP_PARSER_APG_IMPL_NSTRING] = NULL;
    cb[IMAP_PARSER_APG_IMPL_NUMBER] = NULL;
    cb[IMAP_PARSER_APG_IMPL_NZ_NUMBER] = NULL;
    cb[IMAP_PARSER_APG_IMPL_PASSWORD] = NULL;
    cb[IMAP_PARSER_APG_IMPL_QUOTED] = NULL;
    cb[IMAP_PARSER_APG_IMPL_QUOTED_CHAR] = NULL;
    cb[IMAP_PARSER_APG_IMPL_QUOTED_SPECIALS] = NULL;
    cb[IMAP_PARSER_APG_IMPL_RENAME] = NULL;
    cb[IMAP_PARSER_APG_IMPL_RESP_COND_AUTH] = NULL;
    cb[IMAP_PARSER_APG_IMPL_RESP_COND_BYE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_RESP_COND_STATE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_RESP_SPECIALS] = NULL;
    cb[IMAP_PARSER_APG_IMPL_RESP_TEXT] = NULL;
    cb[IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_RESPONSE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_RESPONSE_DATA] = NULL;
    cb[IMAP_PARSER_APG_IMPL_RESPONSE_DONE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_RESPONSE_FATAL] = NULL;
    cb[IMAP_PARSER_APG_IMPL_RESPONSE_TAGGED] = NULL;
    cb[IMAP_PARSER_APG_IMPL_SEARCH] = NULL;
    cb[IMAP_PARSER_APG_IMPL_SEARCH_KEY] = NULL;
    cb[IMAP_PARSER_APG_IMPL_SECTION] = NULL;
    cb[IMAP_PARSER_APG_IMPL_SECTION_MSGTEXT] = NULL;
    cb[IMAP_PARSER_APG_IMPL_SECTION_PART] = NULL;
    cb[IMAP_PARSER_APG_IMPL_SECTION_SPEC] = NULL;
    cb[IMAP_PARSER_APG_IMPL_SECTION_TEXT] = NULL;
    cb[IMAP_PARSER_APG_IMPL_SELECT] = NULL;
    cb[IMAP_PARSER_APG_IMPL_SEQ_NUMBER] = NULL;
    cb[IMAP_PARSER_APG_IMPL_SEQ_RANGE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_SEQUENCE_SET] = NULL;
    cb[IMAP_PARSER_APG_IMPL_SP] = NULL;
    cb[IMAP_PARSER_APG_IMPL_STATUS] = NULL;
    cb[IMAP_PARSER_APG_IMPL_STATUS_ATT] = NULL;
    cb[IMAP_PARSER_APG_IMPL_STATUS_ATT_LIST] = NULL;
    cb[IMAP_PARSER_APG_IMPL_STORE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_STORE_ATT_FLAGS] = NULL;
    cb[IMAP_PARSER_APG_IMPL_STRING] = NULL;
    cb[IMAP_PARSER_APG_IMPL_SUBSCRIBE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_TAG] = NULL;
    cb[IMAP_PARSER_APG_IMPL_TEXT] = NULL;
    cb[IMAP_PARSER_APG_IMPL_TEXT_CHAR] = NULL;
    cb[IMAP_PARSER_APG_IMPL_TIME] = NULL;
    cb[IMAP_PARSER_APG_IMPL_UID] = NULL;
    cb[IMAP_PARSER_APG_IMPL_UNIQUEID] = NULL;
    cb[IMAP_PARSER_APG_IMPL_UNSUBSCRIBE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_USERID] = NULL;
    cb[IMAP_PARSER_APG_IMPL_ZONE] = NULL;
    for(ui = 0; ui < (aint)RULE_COUNT_IMAP_PARSER_APG_IMPL; ui++){
        vParserSetRuleCallback(vpParserCtx, ui, cb[ui]);
    }
}
****************************************************************/

#endif /* _IMAP_PARSER_APG_IMPL_H_ */