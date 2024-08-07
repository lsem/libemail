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
#define IMAP_PARSER_APG_IMPL_ADDR_ADL 12
#define IMAP_PARSER_APG_IMPL_ADDR_HOST 13
#define IMAP_PARSER_APG_IMPL_ADDR_MAILBOX 14
#define IMAP_PARSER_APG_IMPL_ADDR_NAME 15
#define IMAP_PARSER_APG_IMPL_ADDRESS 11
#define IMAP_PARSER_APG_IMPL_ALPHA 0
#define IMAP_PARSER_APG_IMPL_ANY_ASTRING_CHAR_EXCEPT_PLUS 179
#define IMAP_PARSER_APG_IMPL_ANY_TEXT_CHAR_EXCEPT_QUOTED_SPECIALS 135
#define IMAP_PARSER_APG_IMPL_ANY_TEXT_CHAR_EXCEPT_SB 151
#define IMAP_PARSER_APG_IMPL_APPEND 16
#define IMAP_PARSER_APG_IMPL_ASTRING 17
#define IMAP_PARSER_APG_IMPL_ASTRING_CHAR 18
#define IMAP_PARSER_APG_IMPL_ATOM 19
#define IMAP_PARSER_APG_IMPL_ATOM_CHAR 20
#define IMAP_PARSER_APG_IMPL_ATOM_SPECIALS 21
#define IMAP_PARSER_APG_IMPL_AUTH_TYPE 23
#define IMAP_PARSER_APG_IMPL_AUTHENTICATE 22
#define IMAP_PARSER_APG_IMPL_BAD 147
#define IMAP_PARSER_APG_IMPL_BASE64 24
#define IMAP_PARSER_APG_IMPL_BASE64_CHAR 25
#define IMAP_PARSER_APG_IMPL_BASE64_TERMINAL 26
#define IMAP_PARSER_APG_IMPL_BIT 1
#define IMAP_PARSER_APG_IMPL_BODY 27
#define IMAP_PARSER_APG_IMPL_BODY_EXT_1PART 29
#define IMAP_PARSER_APG_IMPL_BODY_EXT_MPART 30
#define IMAP_PARSER_APG_IMPL_BODY_EXTENSION 28
#define IMAP_PARSER_APG_IMPL_BODY_FIELDS 31
#define IMAP_PARSER_APG_IMPL_BODY_FLD_DESC 32
#define IMAP_PARSER_APG_IMPL_BODY_FLD_DSP 34
#define IMAP_PARSER_APG_IMPL_BODY_FLD_DSP_STRING 33
#define IMAP_PARSER_APG_IMPL_BODY_FLD_ENC 35
#define IMAP_PARSER_APG_IMPL_BODY_FLD_ID 36
#define IMAP_PARSER_APG_IMPL_BODY_FLD_LANG 37
#define IMAP_PARSER_APG_IMPL_BODY_FLD_LINES 39
#define IMAP_PARSER_APG_IMPL_BODY_FLD_LOC 38
#define IMAP_PARSER_APG_IMPL_BODY_FLD_MD5 40
#define IMAP_PARSER_APG_IMPL_BODY_FLD_OCTETS 41
#define IMAP_PARSER_APG_IMPL_BODY_FLD_PARAM 44
#define IMAP_PARSER_APG_IMPL_BODY_FLD_PARAM_NAME 42
#define IMAP_PARSER_APG_IMPL_BODY_FLD_PARAM_VALUE 43
#define IMAP_PARSER_APG_IMPL_BODY_TYPE_1PART 45
#define IMAP_PARSER_APG_IMPL_BODY_TYPE_BASIC 46
#define IMAP_PARSER_APG_IMPL_BODY_TYPE_MPART 47
#define IMAP_PARSER_APG_IMPL_BODY_TYPE_MSG 48
#define IMAP_PARSER_APG_IMPL_BODY_TYPE_TEXT 49
#define IMAP_PARSER_APG_IMPL_CAPABILITY 50
#define IMAP_PARSER_APG_IMPL_CAPABILITY_DATA 51
#define IMAP_PARSER_APG_IMPL_CHAR 2
#define IMAP_PARSER_APG_IMPL_CHAR8 52
#define IMAP_PARSER_APG_IMPL_CLOSE_BRACE 105
#define IMAP_PARSER_APG_IMPL_COMMAND 53
#define IMAP_PARSER_APG_IMPL_COMMAND_ANY 54
#define IMAP_PARSER_APG_IMPL_COMMAND_AUTH 55
#define IMAP_PARSER_APG_IMPL_COMMAND_NONAUTH 56
#define IMAP_PARSER_APG_IMPL_COMMAND_SELECT 57
#define IMAP_PARSER_APG_IMPL_CONTINUE_REQ 58
#define IMAP_PARSER_APG_IMPL_COPY 59
#define IMAP_PARSER_APG_IMPL_CR 3
#define IMAP_PARSER_APG_IMPL_CREATE 60
#define IMAP_PARSER_APG_IMPL_CRLF 4
#define IMAP_PARSER_APG_IMPL_CTL 5
#define IMAP_PARSER_APG_IMPL_DATE 61
#define IMAP_PARSER_APG_IMPL_DATE_DAY 62
#define IMAP_PARSER_APG_IMPL_DATE_DAY_FIXED 63
#define IMAP_PARSER_APG_IMPL_DATE_MONTH 64
#define IMAP_PARSER_APG_IMPL_DATE_TEXT 65
#define IMAP_PARSER_APG_IMPL_DATE_TIME 67
#define IMAP_PARSER_APG_IMPL_DATE_YEAR 66
#define IMAP_PARSER_APG_IMPL_DELETE 68
#define IMAP_PARSER_APG_IMPL_DIGIT 6
#define IMAP_PARSER_APG_IMPL_DIGIT_NZ 69
#define IMAP_PARSER_APG_IMPL_DQUOTE 7
#define IMAP_PARSER_APG_IMPL_ENV_BCC 71
#define IMAP_PARSER_APG_IMPL_ENV_CC 72
#define IMAP_PARSER_APG_IMPL_ENV_DATE 73
#define IMAP_PARSER_APG_IMPL_ENV_FROM 74
#define IMAP_PARSER_APG_IMPL_ENV_IN_REPLY_TO 75
#define IMAP_PARSER_APG_IMPL_ENV_MESSAGE_ID 76
#define IMAP_PARSER_APG_IMPL_ENV_REPLY_TO 77
#define IMAP_PARSER_APG_IMPL_ENV_SENDER 78
#define IMAP_PARSER_APG_IMPL_ENV_SUBJECT 79
#define IMAP_PARSER_APG_IMPL_ENV_TO 80
#define IMAP_PARSER_APG_IMPL_ENVELOPE 70
#define IMAP_PARSER_APG_IMPL_EXAMINE 81
#define IMAP_PARSER_APG_IMPL_EXPUNGE_MESSAGE_DATA 115
#define IMAP_PARSER_APG_IMPL_FETCH 82
#define IMAP_PARSER_APG_IMPL_FETCH_ATT 83
#define IMAP_PARSER_APG_IMPL_FETCH_MESSAGE_DATA 116
#define IMAP_PARSER_APG_IMPL_FLAG 84
#define IMAP_PARSER_APG_IMPL_FLAG_EXTENSION 85
#define IMAP_PARSER_APG_IMPL_FLAG_FETCH 86
#define IMAP_PARSER_APG_IMPL_FLAG_KEYWORD 87
#define IMAP_PARSER_APG_IMPL_FLAG_LIST 88
#define IMAP_PARSER_APG_IMPL_FLAG_PERM 89
#define IMAP_PARSER_APG_IMPL_GREETING 90
#define IMAP_PARSER_APG_IMPL_HEADER_FLD_NAME 91
#define IMAP_PARSER_APG_IMPL_HEADER_LIST 92
#define IMAP_PARSER_APG_IMPL_LF 8
#define IMAP_PARSER_APG_IMPL_LIST 93
#define IMAP_PARSER_APG_IMPL_LIST_CHAR 95
#define IMAP_PARSER_APG_IMPL_LIST_MAILBOX 94
#define IMAP_PARSER_APG_IMPL_LIST_WILDCARDS 96
#define IMAP_PARSER_APG_IMPL_LITERAL 97
#define IMAP_PARSER_APG_IMPL_LOGIN 98
#define IMAP_PARSER_APG_IMPL_LSUB 99
#define IMAP_PARSER_APG_IMPL_MAILBOX 100
#define IMAP_PARSER_APG_IMPL_MAILBOX_DATA 103
#define IMAP_PARSER_APG_IMPL_MAILBOX_DATA_EXISTS 101
#define IMAP_PARSER_APG_IMPL_MAILBOX_DATA_RECENT 102
#define IMAP_PARSER_APG_IMPL_MAILBOX_LIST 106
#define IMAP_PARSER_APG_IMPL_MBX_LIST_FLAGS 107
#define IMAP_PARSER_APG_IMPL_MBX_LIST_OFLAG 108
#define IMAP_PARSER_APG_IMPL_MBX_LIST_SFLAG 109
#define IMAP_PARSER_APG_IMPL_MEDIA_BASIC 111
#define IMAP_PARSER_APG_IMPL_MEDIA_BASIC_TYPE_TAG 110
#define IMAP_PARSER_APG_IMPL_MEDIA_MESSAGE 112
#define IMAP_PARSER_APG_IMPL_MEDIA_SUBTYPE 113
#define IMAP_PARSER_APG_IMPL_MEDIA_TEXT 114
#define IMAP_PARSER_APG_IMPL_MESSAGE_DATA 117
#define IMAP_PARSER_APG_IMPL_MSG_ATT 118
#define IMAP_PARSER_APG_IMPL_MSG_ATT_DYNAMIC 119
#define IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC 127
#define IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_BODY_SECTION 124
#define IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_BODY_STRUCTURE 123
#define IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_ENVELOPE 120
#define IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_INTERNALDATE 122
#define IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_RFC822 125
#define IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_RFC822_SIZE 126
#define IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_UID 121
#define IMAP_PARSER_APG_IMPL_NIL 128
#define IMAP_PARSER_APG_IMPL_NO 146
#define IMAP_PARSER_APG_IMPL_NSTRING 129
#define IMAP_PARSER_APG_IMPL_NUMBER 130
#define IMAP_PARSER_APG_IMPL_NZ_NUMBER 131
#define IMAP_PARSER_APG_IMPL_OCTET 9
#define IMAP_PARSER_APG_IMPL_OK 145
#define IMAP_PARSER_APG_IMPL_OPEN_BRACE 104
#define IMAP_PARSER_APG_IMPL_PASSWORD 132
#define IMAP_PARSER_APG_IMPL_QUOTED 133
#define IMAP_PARSER_APG_IMPL_QUOTED_CHAR 134
#define IMAP_PARSER_APG_IMPL_QUOTED_SPECIALS 136
#define IMAP_PARSER_APG_IMPL_RENAME 137
#define IMAP_PARSER_APG_IMPL_RESP_COND_AUTH 143
#define IMAP_PARSER_APG_IMPL_RESP_COND_BYE 144
#define IMAP_PARSER_APG_IMPL_RESP_COND_STATE 148
#define IMAP_PARSER_APG_IMPL_RESP_SPECIALS 149
#define IMAP_PARSER_APG_IMPL_RESP_TEXT 150
#define IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE 160
#define IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_ALERT 152
#define IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_PERMANENT_FLAGS 153
#define IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_READ_ONLY 158
#define IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_READ_WRITE 157
#define IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_TRY_CREATE 159
#define IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_UID_NEXT 156
#define IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_UIDVALIDITY 154
#define IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_UNSEEN 155
#define IMAP_PARSER_APG_IMPL_RESPONSE 138
#define IMAP_PARSER_APG_IMPL_RESPONSE_DATA 139
#define IMAP_PARSER_APG_IMPL_RESPONSE_DONE 140
#define IMAP_PARSER_APG_IMPL_RESPONSE_FATAL 141
#define IMAP_PARSER_APG_IMPL_RESPONSE_TAGGED 142
#define IMAP_PARSER_APG_IMPL_SEARCH 161
#define IMAP_PARSER_APG_IMPL_SEARCH_KEY 162
#define IMAP_PARSER_APG_IMPL_SECTION 163
#define IMAP_PARSER_APG_IMPL_SECTION_MSGTEXT 164
#define IMAP_PARSER_APG_IMPL_SECTION_PART 165
#define IMAP_PARSER_APG_IMPL_SECTION_SPEC 166
#define IMAP_PARSER_APG_IMPL_SECTION_TEXT 167
#define IMAP_PARSER_APG_IMPL_SELECT 168
#define IMAP_PARSER_APG_IMPL_SEQ_NUMBER 169
#define IMAP_PARSER_APG_IMPL_SEQ_RANGE 170
#define IMAP_PARSER_APG_IMPL_SEQUENCE_SET 171
#define IMAP_PARSER_APG_IMPL_SP 10
#define IMAP_PARSER_APG_IMPL_STATUS 172
#define IMAP_PARSER_APG_IMPL_STATUS_ATT 173
#define IMAP_PARSER_APG_IMPL_STATUS_ATT_LIST 174
#define IMAP_PARSER_APG_IMPL_STORE 175
#define IMAP_PARSER_APG_IMPL_STORE_ATT_FLAGS 176
#define IMAP_PARSER_APG_IMPL_STRING 177
#define IMAP_PARSER_APG_IMPL_SUBSCRIBE 178
#define IMAP_PARSER_APG_IMPL_TAG 180
#define IMAP_PARSER_APG_IMPL_TEXT 181
#define IMAP_PARSER_APG_IMPL_TEXT_CHAR 182
#define IMAP_PARSER_APG_IMPL_TIME 183
#define IMAP_PARSER_APG_IMPL_UID 184
#define IMAP_PARSER_APG_IMPL_UNIQUEID 185
#define IMAP_PARSER_APG_IMPL_UNSUBSCRIBE 186
#define IMAP_PARSER_APG_IMPL_USERID 187
#define IMAP_PARSER_APG_IMPL_ZONE 188
#define RULE_COUNT_IMAP_PARSER_APG_IMPL 189

// UDT ids
#define IMAP_PARSER_APG_IMPL_U_LITERAL_DATA 1
#define IMAP_PARSER_APG_IMPL_U_LITERAL_SIZE 0
#define UDT_COUNT_IMAP_PARSER_APG_IMPL 2

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
    cb[IMAP_PARSER_APG_IMPL_BAD] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BASE64] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BASE64_CHAR] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BASE64_TERMINAL] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BIT] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_EXT_1PART] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_EXT_MPART] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_EXTENSION] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FIELDS] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FLD_DESC] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FLD_DSP] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FLD_DSP_STRING] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FLD_ENC] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FLD_ID] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FLD_LANG] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FLD_LINES] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FLD_LOC] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FLD_MD5] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FLD_OCTETS] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FLD_PARAM] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FLD_PARAM_NAME] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_FLD_PARAM_VALUE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_TYPE_1PART] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_TYPE_BASIC] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_TYPE_MPART] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_TYPE_MSG] = NULL;
    cb[IMAP_PARSER_APG_IMPL_BODY_TYPE_TEXT] = NULL;
    cb[IMAP_PARSER_APG_IMPL_CAPABILITY] = NULL;
    cb[IMAP_PARSER_APG_IMPL_CAPABILITY_DATA] = NULL;
    cb[IMAP_PARSER_APG_IMPL_CHAR] = NULL;
    cb[IMAP_PARSER_APG_IMPL_CHAR8] = NULL;
    cb[IMAP_PARSER_APG_IMPL_CLOSE_BRACE] = NULL;
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
    cb[IMAP_PARSER_APG_IMPL_EXPUNGE_MESSAGE_DATA] = NULL;
    cb[IMAP_PARSER_APG_IMPL_FETCH] = NULL;
    cb[IMAP_PARSER_APG_IMPL_FETCH_ATT] = NULL;
    cb[IMAP_PARSER_APG_IMPL_FETCH_MESSAGE_DATA] = NULL;
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
    cb[IMAP_PARSER_APG_IMPL_MAILBOX_DATA_EXISTS] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MAILBOX_DATA_RECENT] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MAILBOX_LIST] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MBX_LIST_FLAGS] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MBX_LIST_OFLAG] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MBX_LIST_SFLAG] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MEDIA_BASIC] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MEDIA_BASIC_TYPE_TAG] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MEDIA_MESSAGE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MEDIA_SUBTYPE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MEDIA_TEXT] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MESSAGE_DATA] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MSG_ATT] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MSG_ATT_DYNAMIC] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_BODY_SECTION] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_BODY_STRUCTURE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_ENVELOPE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_INTERNALDATE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_RFC822] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_RFC822_SIZE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_MSG_ATT_STATIC_UID] = NULL;
    cb[IMAP_PARSER_APG_IMPL_NIL] = NULL;
    cb[IMAP_PARSER_APG_IMPL_NO] = NULL;
    cb[IMAP_PARSER_APG_IMPL_NSTRING] = NULL;
    cb[IMAP_PARSER_APG_IMPL_NUMBER] = NULL;
    cb[IMAP_PARSER_APG_IMPL_NZ_NUMBER] = NULL;
    cb[IMAP_PARSER_APG_IMPL_OCTET] = NULL;
    cb[IMAP_PARSER_APG_IMPL_OK] = NULL;
    cb[IMAP_PARSER_APG_IMPL_OPEN_BRACE] = NULL;
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
    cb[IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_ALERT] = NULL;
    cb[IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_PERMANENT_FLAGS] = NULL;
    cb[IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_READ_ONLY] = NULL;
    cb[IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_READ_WRITE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_TRY_CREATE] = NULL;
    cb[IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_UID_NEXT] = NULL;
    cb[IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_UIDVALIDITY] = NULL;
    cb[IMAP_PARSER_APG_IMPL_RESP_TEXT_CODE_UNSEEN] = NULL;
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
void vImapParserApgImplUdtCallbacks(void* vpParserCtx){
    aint ui;
    parser_callback cb[UDT_COUNT_IMAP_PARSER_APG_IMPL];
    cb[IMAP_PARSER_APG_IMPL_U_LITERAL_DATA] = NULL;
    cb[IMAP_PARSER_APG_IMPL_U_LITERAL_SIZE] = NULL;
    for(ui = 0; ui < (aint)UDT_COUNT_IMAP_PARSER_APG_IMPL; ui++){
        vParserSetUdtCallback(vpParserCtx, ui, cb[ui]);
    }
}
****************************************************************/

#endif /* _IMAP_PARSER_APG_IMPL_H_ */
