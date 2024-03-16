#include "user_tree.hpp"

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <fstream>

namespace mailer::user_tree {

expected<string> load_file(std::filesystem::path p) {
    std::ifstream f(p.string(), std::ios_base::in);
    if (!f.good()) {
        log_error("failed opening user tree file: '{}", p.string());
        return unexpected(std::make_error_code(static_cast<std::errc>(errno)));
    }

    string data{std::istreambuf_iterator<char>{f}, std::istreambuf_iterator<char>{}};
    if (f.good()) {
        return std::move(data);
    } else {
        log_error("fialed reading user tree data from file");
        return unexpected(std::make_error_code(static_cast<std::errc>(errno)));
    }

    return data;
}

expected<Node> parse_node(const rapidjson::Value& v) {
    Node new_node;

    if (!v.IsObject()) {
        log_error("failed parsing ndoe: node not an object");
        return unexpected(make_error_code(std::errc::io_error));
    }

    if (!v.HasMember("label") || !v["label"].IsString()) {
        log_error("failed parsing node: no label or wrong type");
        return unexpected(make_error_code(std::errc::io_error));
    }
    new_node.label = v["label"].GetString();

    if (!v.HasMember("flags") || !v["flags"].IsInt()) {
        log_error("failed parsing node: no flags or wrong type");
        return unexpected(make_error_code(std::errc::io_error));
    }
    new_node.flags = v["flags"].GetInt();

    if (!v.HasMember("children") || !v["children"].IsArray()) {
        log_error("failed parsing node: no children or wrong type");
        return unexpected(make_error_code(std::errc::io_error));
    }

    const rapidjson::Value& children_node = v["children"];
    for (auto it = children_node.Begin(); it != children_node.End(); ++it) {
        auto node_or_err = parse_node(*it);
        if (!node_or_err) {
            log_error("failed parsing one of the children");
            return unexpected(make_error_code(std::errc::io_error));
        }
        new_node.children.emplace_back(std::make_unique<Node>(std::move(*node_or_err)));
    }

    return new_node;
}

expected<Node> load_tree_from_file(std::filesystem::path p) {
    auto data_or_err = load_file(p);
    if (!data_or_err) {
        log_error("failed loading user tree data");
        return unexpected(data_or_err.error());
    }

    rapidjson::Document d;
    d.Parse(data_or_err->c_str());
    if (d.HasParseError()) {
        log_error("failed parsing user tree data: {}", GetParseError_En(d.GetParseError()));
        return unexpected(make_error_code(std::errc::io_error));
    }

    return parse_node(d);
}

namespace {
void print_tree_it(const Node& n, size_t level) {
    string indent(level, ' ');
    std::cout << indent;
    std::cout << fmt::format("Node(label: {}, flags: {})\n", n.label, n.flags);
    for (auto& c : n.children) {
        print_tree_it(*c, level + 4);
    }
}
}  // namespace

void print_tree(const Node& n) {
    print_tree_it(n, 0);
}

}  // namespace mailer::user_tree
