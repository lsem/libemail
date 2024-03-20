#include "user_tree.hpp"

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <fstream>

namespace mailer::user_tree {

namespace {

expected<void> save_to_file(const std::string& s, std::filesystem::path p) {
    std::ofstream f(p.string(), std::ios_base::out);
    if (!f.good()) {
        log_error("failed opening user tree file for writing: '{}", p.string());
        return unexpected(std::make_error_code(static_cast<std::errc>(errno)));
    }

    f << s;

    if (!f.good()) {
        log_error("failed to write user tree file: {}", strerror(errno));
        return unexpected(std::make_error_code(static_cast<std::errc>(errno)));
    }

    return {};
}

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

expected<vector<set<string>>> parse_contact_groups(const rapidjson::Value& v) {
    if (!v.IsArray()) {
        log_error("failed parsing recipients sets: node not an array");
        return unexpected(make_error_code(std::errc::io_error));
    }

    vector<set<string>> result;

    for (auto it = v.Begin(); it != v.End(); ++it) {
        set<string> this_group;
        const rapidjson::Value& v = *it;
        if (!v.IsArray()) {
            log_error("failed parsing one of the contact groups: node an array in array");
            return unexpected(make_error_code(std::errc::io_error));
        }

        for (auto contact_it = it->Begin(); contact_it != it->End(); ++contact_it) {
            const rapidjson::Value& u = *contact_it;
            if (!u.IsString()) {
                log_error(
                    "failed parsing one of the contact groups: contact expected to be string");
                return unexpected(make_error_code(std::errc::io_error));
            }
            this_group.insert(u.GetString());
        }
        result.emplace_back(std::move(this_group));
    }

    return std::move(result);
}

expected<Node> parse_node(const rapidjson::Value& v) {
    Node new_node;

    if (!v.IsObject()) {
        log_error("failed parsing node: node not an object");
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

    if (!v.HasMember("contact_groups") || !v["contact_groups"].IsArray()) {
        log_error("failed parsing node: no contact_groups node or wrong type");
        return unexpected(make_error_code(std::errc::io_error));
    }

    const rapidjson::Value& contact_groups_node = v["contact_groups"];
    auto contact_groups_or_err = parse_contact_groups(contact_groups_node);
    if (!contact_groups_or_err) {
        log_error("failed parsing contact groups");
        return unexpected(make_error_code(std::errc::io_error));
    }

    new_node.contact_groups = std::move(*contact_groups_or_err);

    return new_node;
}
}  // namespace

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

template <class Writer>
void serialize_node_to_json(const Node& node, Writer& writer) {
    writer.StartObject();
    writer.Key("label");
    writer.String(node.label.c_str());
    writer.Key("flags");
    writer.Int(node.flags);
    writer.Key("children");
    writer.StartArray();
    for (auto& c_ptr : node.children) {
        serialize_node_to_json(*c_ptr, writer);
    }
    writer.EndArray();
    writer.Key("contact_groups");
    writer.StartArray();
    for (const auto& group : node.contact_groups) {
        writer.StartArray();
        for (const auto& contact : group) {
            writer.String(contact.c_str());
        }
        writer.EndArray();
    }
    writer.EndArray();
    writer.EndObject();
}

expected<void> save_tree_to_file(const Node& root, std::filesystem::path p) {
    rapidjson::StringBuffer s;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(s);

    serialize_node_to_json(root, writer);
    if (auto result = save_to_file(s.GetString(), p); !result) {
        log_error("failed writing user settings to file");
        return unexpected(result.error());
    }

    return {};
}

namespace {
void print_tree_it(const Node& n, size_t level) {
    string indent(level, ' ');
    std::cout << indent;
    std::cout << fmt::format("Node(label: {}, flags: {}: contact groups: {})\n", n.label, n.flags,
                             n.contact_groups);
    for (auto& c : n.children) {
        print_tree_it(*c, level + 4);
    }
}
}  // namespace

void print_tree(const Node& n) {
    print_tree_it(n, 0);
}

}  // namespace mailer::user_tree
