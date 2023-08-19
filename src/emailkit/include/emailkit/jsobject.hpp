#pragma once

#include <map>
#include <string>
#include <variant>
#include <vector>

#include <emailkit/log.hpp>

namespace emailkit {

class jsobject {
   public:
    using object_type = std::map<std::string, jsobject>;
    using array_type = std::vector<jsobject>;
    using number_type = double;

    // explicit jsobject(inner_type_tag)

    // how we are supposed to store objects? std::map, std hash, std::vector + own index?
    bool is_bool() const { return std::holds_alternative<bool>(m_data); }
    bool is_object() const { return std::holds_alternative<std::unique_ptr<object_type>>(m_data); }
    bool is_array() const { return std::holds_alternative<std::unique_ptr<array_type>>(m_data); }
    bool is_string() const { return std::holds_alternative<std::string>(m_data); }
    bool is_number() const { return std::holds_alternative<double>(m_data); }

    static jsobject from_json(const std::string& json);
    std::string to_json(const std::string& json);

    // how we are supposed to keep our variants?
    // one way is to have all representations, another is using std::variant.
    static jsobject make_obj() { return jsobject{std::make_unique<object_type>()}; }
    static jsobject make_string() { return jsobject{std::string{}}; }
    static jsobject make_array() { return jsobject{std::make_unique<array_type>()}; }
    static jsobject make_bool() { return jsobject{bool{}}; }
    static jsobject make_number() { return jsobject{number_type{}}; }
    static jsobject make_number(number_type x) { return jsobject{x}; }

    object_type& as_object() { return *std::get<std::unique_ptr<object_type>>(m_data); }
    const object_type& as_object() const { return *std::get<std::unique_ptr<object_type>>(m_data); }
    array_type& as_array() { return *std::get<std::unique_ptr<array_type>>(m_data); }
    const array_type& as_array() const { return *std::get<std::unique_ptr<array_type>>(m_data); }
    std::string& as_string() { return std::get<std::string>(m_data); }
    const std::string& as_string() const { return std::get<std::string>(m_data); }
    double& as_number() { return std::get<double>(m_data); }
    double as_number() const { return std::get<double>(m_data); }
    bool& as_bool() { return std::get<bool>(m_data); }
    bool as_bool() const { return std::get<bool>(m_data); }

    jsobject() = default;
    ~jsobject() = default;

    // TODO: can we look up map by string view.
    jsobject& operator[](std::string key) {
        if (!this->is_object()) {
            log_warning("creating subobject {}", key);
            // turn itself into object.
            *this = make_obj();
            auto& this_as_obj = this->as_object();
            this_as_obj[key] = jsobject{};
            return this_as_obj[key];
        }
        auto& o = this->as_object();
        if (auto it = o.find(key); it != o.end()) {
            return it->second;
        } else {
            o[key] = jsobject{};
            return o[key];
        }
    }

    const jsobject& operator[](std::string key) const {
        if (!this->is_object()) {
            log_warning("(2)returning empty object address, it is: {}",
                        (void*)(g_empty_object.get()));
            return *g_empty_object;
        }
        auto& o = this->as_object();
        // TODO: why cannot lookup by string_view.
        if (auto it = o.find(key); it != o.end()) {
            return it->second;
        } else {
            log_warning("returning empty object address, it is: {}", (void*)(g_empty_object.get()));
            return *g_empty_object;
        }
    }

    jsobject(const jsobject& rhs) {
        if (rhs.is_object()) {
            m_data = std::make_unique<object_type>(rhs.as_object());
        } else if (rhs.is_array()) {
            m_data = std::make_unique<array_type>(rhs.as_array());
        } else if (rhs.is_string()) {
            m_data = rhs.as_string();
        } else if (rhs.is_number()) {
            m_data = rhs.as_number();
        } else if (rhs.is_bool()) {
            m_data = rhs.as_bool();
        }
    }
    jsobject(jsobject&& rhs) { m_data = std::move(rhs.m_data); }

    jsobject& operator=(const jsobject& r) {
        if (this == g_empty_object.get()) {
            assert(false && "attempt to modify shared empty object");
            std::abort();
        }
        auto copy = r;
        m_data = std::move(copy.m_data);
        return *this;
    }

    jsobject& operator=(jsobject&& r) {
        if (this == g_empty_object.get()) {
            assert(false && "attempt to modify shared empty object");
            std::abort();
        }
        m_data = std::move(r.m_data);
        return *this;
    }

   private:
    // TODO: how we can get rid of this?
    static inline std::unique_ptr<jsobject> g_empty_object = std::make_unique<jsobject>();

   private:
    using variant_t = std::variant<std::monostate,
                                   bool,
                                   double,
                                   std::string,
                                   std::unique_ptr<array_type>,
                                   std::unique_ptr<object_type>>;
    jsobject(variant_t d) : m_data(std::move(d)) {}

   private:
    variant_t m_data;
};  // namespace emailkit

}  // namespace emailkit