#pragma once

#include <emailkit/global.hpp>

#include <filesystem>

namespace mailer::user_tree {

struct Node {
    string label;
    uint32_t flags;
    vector<std::unique_ptr<Node>> children;
};

expected<Node> load_tree_from_file(std::filesystem::path p);
void print_tree(const Node&);

}  // namespace mailer::user_tree
