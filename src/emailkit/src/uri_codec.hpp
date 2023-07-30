#include <string>

namespace emailkit {

std::string decode_uri_component(std::string encoded);
std::string encode_uri_component(std::string decoded);

}  // namespace emailkit