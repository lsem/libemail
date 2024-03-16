#pragma once

#include <emailkit/global.hpp>

#include <filesystem>

namespace mailer::user_tree {

struct Node {
    string label;
    uint32_t flags;
    vector<unique_ptr<Node>> children;
    vector<set<string>> contact_groups;
};

expected<Node> load_tree_from_file(std::filesystem::path p);
expected<void> save_to_file(const Node& root, std::filesystem::path p);
void print_tree(const Node&);

}  // namespace mailer::user_tree
