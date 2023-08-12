#include <cctype>
#include <emailkit/log.hpp>
#include <iostream>
#include <sstream>
#include <vector>

bool is_CHAR(int x);
bool is_atom_specials(int x);
bool is_CTL(int x);
bool is_list_wildcards(int x);
bool is_quoted_specials(int x);
bool is_resp_specials(int x);
bool is_TEXT_CHAR(int x);
bool is_ASTRING_CHAR(int x);

// ATOM-CHAR       = <any CHAR except atom-specials>
bool is_atom_char(int x) {
    return is_CHAR(x) && !is_atom_specials(x);
}

//  CHAR           =  %x01-7F
bool is_CHAR(int x) {
    return x >= 0x01 && x <= 0x7F;
}

//  atom-specials  = "(" / ")" / "{" / SP / CTL / list-wildcards / quoted-specials / resp-specials
bool is_atom_specials(int x) {
    return x == '(' || x == ')' || x == '{' || x == ' ' || is_CTL(x) || is_list_wildcards(x) ||
           is_quoted_specials(x) || is_resp_specials(x);
}

// list-wildcards  = "%" / "*"
bool is_list_wildcards(int x) {
    return x == '%' || x == '*';
}

// quoted-specials = DQUOTE / "\"
bool is_quoted_specials(int x) {
    return x == 0x22 || x == '\\';
}

bool is_resp_specials(int x) {
    return x == ']';
}
// CTL            =  %x00-1F / %x7F
bool is_CTL(int x) {
    return (x >= 0x00 && x <= 0x1F) || x == 0x7F;
}

bool is_TEXT_CHAR(int x) {
    return is_CHAR(x) && x != '\r' && x != '\n';
}

// ASTRING-CHAR   = ATOM-CHAR / resp-specials
bool is_ASTRING_CHAR(int x) {
    return is_atom_char(x) || is_resp_specials(x);
}

// QUOTED-CHAR: <any TEXTCHAR except quoted-specials>
bool any_TEXT_CHAR_except_quoted_specials(int x) {
    return is_TEXT_CHAR(x) && !is_quoted_specials(x);
}

// <any TEXT-CHAR except "]">
bool any_TEXT_CHAR_except_closing_square_bracket(int x) {
    return is_TEXT_CHAR(x) && x != ']';
}

// <any ASTRING-CHAR except "+">
bool any_ASTRING_CHAR_except_plus(int x) {
    return is_ASTRING_CHAR(x) && x != '+';
}

//                    quoted-specials / resp-specials
// SP             =  %x20
// CR             =  %x0D
// LF             =  %x0A
// CRLF           =  CR LF
// DQUOTE         =  %x22
// DIGIT          =  %x30-39

// resp-specials   = "]"

template <class Cb>
std::string print_sequence(Cb cb) {
    std::stringstream comment_ss;
    for (int i = 0; i < 128; ++i) {
        if (cb(i)) {
            if (isgraph(i)) {
                comment_ss << (char)i;
            } else {
                comment_ss << "\\x" << i;
            }
        }
    }

    return comment_ss.str();
}

template <class Cb>
std::string generate_sequence(Cb cb, bool print_seq_in_command = true) {
    std::string sep = "";
    std::stringstream ss;
    int seq_start = 0;
    bool in_sequence = false;
    for (int i = 0; i < 128; ++i) {
        if (cb(i)) {
            if (!in_sequence) {
                in_sequence = true;
                seq_start = i;
            }
        } else {
            if (in_sequence) {
                in_sequence = false;
                if (i - seq_start == 1) {
                    ss << sep << std::hex << "%x" << seq_start;
                    sep = " / ";
                } else if ((i - seq_start) > 1) {
                    ss << sep << std::hex << "%x" << seq_start << "-" << i - 1;
                    sep = " / ";
                }
            }
        }
    }

    if (in_sequence) {
        if (128 - seq_start == 1) {
            ss << sep << std::hex << "%x" << seq_start;
        } else if ((128 - seq_start) > 1) {
            ss << sep << std::hex << "%x" << seq_start << "-" << 128 - 1;
        }
    }

    return ss.str();
}

template <class Cb>
void generate_rule(std::string name, Cb cb, bool generate_commant = true) {
    if (generate_commant)
        std::cout << "; " << print_sequence(cb) << "\n";
    std::cout << name << "    = " << generate_sequence(cb) << "\n";
}

// IMAP official grammar has some rules defines on high level like "all CHARs except someting"
// which is not directly expressible in ABNF and so we generate this ranged by manually encoding
// these rules.
void dump_missing_imap_abnf_rules() {}

int main() {
    generate_rule("ATOM-CHAR", is_atom_char);
    generate_rule("TEXT-CHAR", is_TEXT_CHAR);
    generate_rule("ANY-TEXT-CHAR-EXCEPT-QUOTED-SPECIALS", any_TEXT_CHAR_except_quoted_specials);
    generate_rule("ANY-TEXT-CHAR-EXCEPT-SB", any_TEXT_CHAR_except_closing_square_bracket);
    generate_rule("ANY-ASTRING-CHAR-EXCEPT-PLUS", any_ASTRING_CHAR_except_plus);
}
