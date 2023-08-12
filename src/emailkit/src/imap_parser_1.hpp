#pragma once
#include <string_view>

namespace emailkit {

void parse_mailbox_list(std::string_view input);
void parse_mailbox_data(std::string_view input);

}  // namespace emailkit