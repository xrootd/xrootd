
#include "XrdSciTokens/XrdSciTokensAccess.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"

#include <gtest/gtest.h>

#include <algorithm>

TEST(XrdSciTokens, appendWLCGAudiences) {
    XrdSysLogger log;
    XrdSysError eDest(&log, "XrdSciTokens");
    std::vector<std::string> audiences;
    std::vector<std::string> hostnames = {"host1", "host2"};
    XrdOucEnv env;

    // Mock the environment variables
    char xrdHost[] = "XRDHOST=localhost";
    putenv(xrdHost);
    char xrdPort[] = "XRDPORT=1094";
    putenv(xrdPort);
    env.PutPtr("xrdEnv*", &env);
    env.PutPtr("XrdTlsContext*", &env);

    ASSERT_TRUE(appendWLCGAudiences(hostnames, &env, eDest, audiences));

    EXPECT_EQ(audiences.size(), 13);
    ASSERT_TRUE(std::find(audiences.begin(), audiences.end(), "root://localhost") != audiences.end());
    ASSERT_TRUE(std::find(audiences.begin(), audiences.end(), "roots://localhost") != audiences.end());
    ASSERT_TRUE(std::find(audiences.begin(), audiences.end(), "https://localhost:1094") != audiences.end());
    ASSERT_TRUE(std::find(audiences.begin(), audiences.end(), "https://host1:1094") != audiences.end());
    ASSERT_TRUE(std::find(audiences.begin(), audiences.end(), "root://host1") != audiences.end());
    ASSERT_TRUE(std::find(audiences.begin(), audiences.end(), "roots://host1") != audiences.end());
    ASSERT_TRUE(std::find(audiences.begin(), audiences.end(), "https://host2:1094") != audiences.end());
    ASSERT_TRUE(std::find(audiences.begin(), audiences.end(), "root://host2") != audiences.end());
    ASSERT_TRUE(std::find(audiences.begin(), audiences.end(), "roots://host2") != audiences.end());
    ASSERT_TRUE(std::find(audiences.begin(), audiences.end(), "localhost") != audiences.end());
    ASSERT_TRUE(std::find(audiences.begin(), audiences.end(), "host1") != audiences.end());
    ASSERT_TRUE(std::find(audiences.begin(), audiences.end(), "host2") != audiences.end());
    ASSERT_TRUE(std::find(audiences.begin(), audiences.end(), "https://wlcg.cern.ch/jwt/v1/any") != audiences.end());
}

TEST(XrdSciTokens, appendWLCGAudiences_noHostnames) {
    XrdSysLogger log;
    XrdSysError eDest(&log, "XrdSciTokens");
    std::vector<std::string> audiences = {"https://wlcg.cern.ch/jwt/v1/any"};
    std::vector<std::string> hostnames;
    XrdOucEnv env;

    // Mock the environment variables
    char xrdHost[] = "XRDHOST=localhost";
    putenv(xrdHost);
    char xrdPort[] = "XRDPORT=443";
    putenv(xrdPort);
    env.PutPtr("xrdEnv*", &env);
    env.PutPtr("XrdTlsContext*", &env);

    ASSERT_TRUE(appendWLCGAudiences(hostnames, &env, eDest, audiences));

    EXPECT_EQ(audiences.size(), 5);
    ASSERT_TRUE(std::find(audiences.begin(), audiences.end(), "root://localhost:443") != audiences.end());
    ASSERT_TRUE(std::find(audiences.begin(), audiences.end(), "roots://localhost:443") != audiences.end());
    ASSERT_TRUE(std::find(audiences.begin(), audiences.end(), "https://localhost") != audiences.end());
    ASSERT_TRUE(std::find(audiences.begin(), audiences.end(), "localhost") != audiences.end());
    ASSERT_TRUE(std::find(audiences.begin(), audiences.end(), "https://wlcg.cern.ch/jwt/v1/any") != audiences.end());
}

TEST(XrdSciTokens, appendWLCGAudiences_noTLS) {
    XrdSysLogger log;
    XrdSysError eDest(&log, "XrdSciTokens");
    std::vector<std::string> audiences = {"https://wlcg.cern.ch/jwt/v1/any"};
    std::vector<std::string> hostnames;
    XrdOucEnv env;

    // Mock the environment variables
    char xrdHost[] = "XRDHOST=example.com";
    putenv(xrdHost);
    char xrdPort[] = "XRDPORT=8443";
    putenv(xrdPort);

    ASSERT_TRUE(appendWLCGAudiences(hostnames, &env, eDest, audiences));

    EXPECT_EQ(audiences.size(), 3);
    ASSERT_TRUE(std::find(audiences.begin(), audiences.end(), "root://example.com:8443") != audiences.end());
    ASSERT_TRUE(std::find(audiences.begin(), audiences.end(), "example.com") != audiences.end());
    ASSERT_TRUE(std::find(audiences.begin(), audiences.end(), "https://wlcg.cern.ch/jwt/v1/any") != audiences.end());
}

TEST(XrdSciTokens, splitEntries) {
    std::string_view entry_string = "entry1,entry2,entry3";
    std::vector<std::string> entries;

    splitEntries(entry_string, entries);

    ASSERT_EQ(entries.size(), 3);
    ASSERT_EQ(entries[0], "entry1");
    ASSERT_EQ(entries[1], "entry2");
    ASSERT_EQ(entries[2], "entry3");
}

TEST(XrdSciTokens, splitEntries_EmptyString) {
    std::string_view entry_string = "";
    std::vector<std::string> entries;

    splitEntries(entry_string, entries);

    ASSERT_EQ(entries.size(), 0);
}

TEST(XrdSciTokens, splitEntries_SingleEntry) {
    std::string_view entry_string = "single_entry";
    std::vector<std::string> entries;

    splitEntries(entry_string, entries);

    ASSERT_EQ(entries.size(), 1);
    ASSERT_EQ(entries[0], "single_entry");
}

TEST(XrdSciTokens, splitEntries_MultipleCommas) {
    std::string_view entry_string = "entry1,,entry2,entry3,,";
    std::vector<std::string> entries;

    splitEntries(entry_string, entries);

    ASSERT_EQ(entries.size(), 3);
    ASSERT_EQ(entries[0], "entry1");
    ASSERT_EQ(entries[1], "entry2");
    ASSERT_EQ(entries[2], "entry3");
}

TEST(XrdSciTokens, splitEntries_CommasSpaces) {
    std::string_view entry_string = " entry1, entry2 entry3 ,";
    std::vector<std::string> entries;

    splitEntries(entry_string, entries);

    ASSERT_EQ(entries.size(), 3);
    ASSERT_EQ(entries[0], "entry1");
    ASSERT_EQ(entries[1], "entry2");
    ASSERT_EQ(entries[2], "entry3");
}

TEST(XrdSciTokens, splitEntries_DuplicateEntries) {
    std::string_view entry_string = " entry1\tentry1 entry2";
    std::vector<std::string> entries;

    splitEntries(entry_string, entries);

    ASSERT_EQ(entries.size(), 2);
    ASSERT_EQ(entries[0], "entry1");
    ASSERT_EQ(entries[1], "entry2");
}

TEST(XrdSciTokens, MapRule) {
  MapRule rule("subject", "user", "/prefix", "group", "result");
  ASSERT_EQ("", rule.match("not subject", "not user", "/foo", {"not group"}));
  ASSERT_EQ("", rule.match("subject", "not user", "/foo", {"not group"}));
  ASSERT_EQ("", rule.match("subject", "not user", "/foo", {"not group"}));
  ASSERT_EQ("", rule.match("not subject", "user", "/foo", {"not group"}));
  ASSERT_EQ("", rule.match("not subject", "not user", "/prefix/baz", {"not group"}));
  ASSERT_EQ("", rule.match("not subject", "not user", "/foo", {"group"}));
  ASSERT_EQ("result", rule.match("subject", "user", "/prefix/foo", {"group"}));
  ASSERT_EQ("result", rule.match("subject", "user", "/prefix/foo", {"not group", "group"}));
}

TEST(XrdSciTokens, SubpathMatch) {
  SubpathMatch matcher({{AOP_Read, "/prefix"}});
  ASSERT_EQ(false, matcher.apply(AOP_Read, "/prefix1"));
  ASSERT_EQ(false, matcher.apply(AOP_Create, "/prefix"));
  ASSERT_EQ(true, matcher.apply(AOP_Read, "/prefix"));
  ASSERT_EQ(true, matcher.apply(AOP_Read, "/prefix/foo"));

  // Test handling of root paths
  matcher = SubpathMatch({{AOP_Create, "/"}});
  ASSERT_EQ(true, matcher.apply(AOP_Create, "/bar/baz"));
  ASSERT_EQ(false, matcher.apply(AOP_Stat, "/bar/baz"));

  // Test special handling of parent prefixes
  matcher = SubpathMatch({{AOP_Stat, "/foo/bar"}});
  ASSERT_EQ(true, matcher.apply(AOP_Stat, "/foo"));
  ASSERT_EQ(false, matcher.apply(AOP_Read, "/foo"));
  matcher = SubpathMatch({{AOP_Mkdir, "/foo/bar"}});
  ASSERT_EQ(true, matcher.apply(AOP_Mkdir, "/foo"));
  ASSERT_EQ(1, matcher.size());
  ASSERT_FALSE(matcher.empty());
  ASSERT_EQ("/foo/bar:mkdir", matcher.str());

  matcher = SubpathMatch();
  ASSERT_EQ(0, matcher.size());
  ASSERT_TRUE(matcher.empty());
}


TEST(XrdSciTokens, AuthorizesRequiredIssuers) {
  AccessRulesRaw rules{{AOP_Read, "/prefix"}, {AOP_Mkdir, "/prefix2/nested"}};
  auto matcher = std::make_unique<SubpathMatch>(rules);
  std::vector<std::pair<std::unique_ptr<SubpathMatch>, std::string>> required_issuers;
  required_issuers.emplace_back(std::move(matcher), "https://example.com");

  auto access_rule_entry_ptr = new XrdAccRules(0, "username", "token_sub", "https://example.com", {}, {}, IssuerAuthz::Capability, AuthzSetting::None);
  std::shared_ptr<XrdAccRules> access_rule_entry(access_rule_entry_ptr);
  access_rule_entry->parse({{AOP_Read, "/prefix"}});

  access_rule_entry_ptr = new XrdAccRules(0, "username", "token_sub", "https://example-other.com", {}, {}, IssuerAuthz::Capability, AuthzSetting::None);
  std::shared_ptr<XrdAccRules> access_rule_entry_other(access_rule_entry_ptr);
  access_rule_entry_other->parse({{AOP_Read, "/prefix"}});

  ASSERT_EQ(true, AuthorizesRequiredIssuers(AOP_Read, "/prefix/foo", required_issuers, {access_rule_entry}));
  ASSERT_EQ(true, AuthorizesRequiredIssuers(AOP_Read, "/foo", required_issuers, {access_rule_entry}));
  ASSERT_EQ(false, AuthorizesRequiredIssuers(AOP_Read, "/prefix/foo", required_issuers, {access_rule_entry_other}));
  ASSERT_EQ(false, AuthorizesRequiredIssuers(AOP_Read, "/prefix", required_issuers, {access_rule_entry_other}));
  ASSERT_EQ(true, AuthorizesRequiredIssuers(AOP_Read, "/prefix2", required_issuers, {access_rule_entry_other}));
  ASSERT_EQ(true, AuthorizesRequiredIssuers(AOP_Create, "/prefix/foo", required_issuers, {access_rule_entry_other}));
  ASSERT_EQ(true, AuthorizesRequiredIssuers(AOP_Mkdir, "/prefix", required_issuers, {access_rule_entry_other}));
  ASSERT_EQ(true, AuthorizesRequiredIssuers(AOP_Mkdir, "/prefix", required_issuers, {access_rule_entry}));
  ASSERT_EQ(false, AuthorizesRequiredIssuers(AOP_Mkdir, "/prefix2", required_issuers, {access_rule_entry}));

  required_issuers.emplace_back(std::make_unique<SubpathMatch>(rules), "https://example-other.com");
  ASSERT_EQ(false, AuthorizesRequiredIssuers(AOP_Read, "/prefix/foo", required_issuers, {access_rule_entry}));
  ASSERT_EQ(true, AuthorizesRequiredIssuers(AOP_Read, "/prefix/foo", required_issuers, {access_rule_entry, access_rule_entry_other}));
}
