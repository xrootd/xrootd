
#include "XrdSciTokens/XrdSciTokensAccess.hh"

#include <gtest/gtest.h>

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