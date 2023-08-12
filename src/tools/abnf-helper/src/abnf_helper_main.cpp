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

//                    quoted-specials / resp-specials
// SP             =  %x20
// CR             =  %x0D
// LF             =  %x0A
// CRLF           =  CR LF
// DQUOTE         =  %x22
// DIGIT          =  %x30-39

// resp-specials   = "]"

template <class Cb>
std::string generate_sequence(Cb cb) {
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

    return ss.str();
}

// IMAP official grammar has some rules defines on high level like "all CHARs except someting"
// which is not directly expressible in ABNF and so we generate this ranged by manually encoding
// these rules.
void dump_missing_imap_abnf_rules() {}

int main() {
    std::cout << "ATOM-CHAR    = " << generate_sequence(is_atom_char) << "\n";
}
