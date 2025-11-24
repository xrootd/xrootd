
#include "XrdOuc/XrdOucEnv.hh"

#include <gtest/gtest.h>

static const std::pair<std::string, std::string> env_tests[] = {
    {"foo=bar", "&foo=bar"},
    {"authz=bar", "&"},
    {"authz=bar&foo=1", "&foo=1"},
    {"&authz=bar&authz=1", "&"},
    {"authz=bar&authz=1", "&"},
    {"&authz=bar", "&"},
    {"&access_token=bar", "&"},
    {"foo=1&authz=bar", "&foo=1"},
    {"foo=1&authz=foo&access_token=bar", "&foo=1"},
    {"authz=foo&access_token=bar", "&"},
    {"authz=foo&foo=bar", "&foo=bar"},
    {"authz=foo&foo=bar&access_token=3", "&foo=bar"},
    {"authz=1&access_token=2&authz=3&access_token=4", "&"},
};

TEST(XrdOucEnv, EnvTidy) {
    for (const auto &env_str : env_tests) {
        int envlen;
        XrdOucEnv env(env_str.first.c_str(), env_str.first.size());
        ASSERT_STREQ(env_str.second.c_str(), env.EnvTidy(envlen)) << "Testing tidy of " << env_str.first;
    }
}
