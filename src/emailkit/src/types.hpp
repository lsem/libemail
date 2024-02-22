#pragma once

#include <vector>

namespace emailkit::types {

// Represents email address like user@example.com. This is native type for our library and apps.
using EmailAddress = std::string;

using EmailAddressVec = std::vector<EmailAddress>;

using MessageID = std::string;

struct EmailDate {};

}  // namespace emailkit::types
