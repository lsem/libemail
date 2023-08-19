#include <gtest/gtest.h>
#include <emailkit/jsobject.hpp>

using emailkit::jsobject;

TEST(jsobject_test, basic_test) {
    {
        jsobject o;

        ASSERT_FALSE(o.is_string());
        ASSERT_FALSE(o.is_number());
        ASSERT_FALSE(o.is_bool());
        ASSERT_FALSE(o.is_array());
        ASSERT_FALSE(o.is_object());
    }
    {
        jsobject o = jsobject::make_obj();

        ASSERT_FALSE(o.is_string());
        ASSERT_FALSE(o.is_number());
        ASSERT_FALSE(o.is_bool());
        ASSERT_FALSE(o.is_array());
        ASSERT_TRUE(o.is_object());

        std::map<std::string, jsobject>& obj = o.as_object();
        const std::map<std::string, jsobject>& const_obj = o.as_object();

        ASSERT_EQ(obj.size(), 0);
        ASSERT_EQ(const_obj.size(), 0);
    }
    {
        jsobject o = jsobject::make_array();

        ASSERT_FALSE(o.is_string());
        ASSERT_FALSE(o.is_number());
        ASSERT_FALSE(o.is_bool());
        ASSERT_TRUE(o.is_array());
        ASSERT_FALSE(o.is_object());

        std::vector<jsobject>& arr = o.as_array();
        const std::vector<jsobject>& const_arr = o.as_array();
        EXPECT_EQ(const_arr.size(), 0);
    }
    {
        jsobject o = jsobject::make_string();

        ASSERT_TRUE(o.is_string());
        ASSERT_FALSE(o.is_number());
        ASSERT_FALSE(o.is_bool());
        ASSERT_FALSE(o.is_array());
        ASSERT_FALSE(o.is_object());

        std::string& s = o.as_string();
        const std::string& const_s = o.as_string();
        EXPECT_EQ(s, "");
        EXPECT_EQ(const_s, "");
    }
    {
        jsobject o = jsobject::make_number();

        ASSERT_FALSE(o.is_string());
        ASSERT_TRUE(o.is_number());
        ASSERT_FALSE(o.is_bool());
        ASSERT_FALSE(o.is_array());
        ASSERT_FALSE(o.is_object());
    }
}

TEST(jsobject_test, access_by_key_test) {
    jsobject x;
    jsobject& x_ref = x;
    const jsobject& const_x_ref = x;
    x_ref["a"]["b"]["c"] = jsobject::make_string();
    ASSERT_TRUE(const_x_ref["a"]["b"]["c"].is_string());
    ASSERT_TRUE(!const_x_ref["x"]["y"]["z"].is_string());
}
