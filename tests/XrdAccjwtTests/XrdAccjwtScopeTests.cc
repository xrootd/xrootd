#undef NDEBUG

#include "XrdAccjwt/XrdAccjwtScope.hh"

#include <gtest/gtest.h>

#include <fstream>
#include <map>
#include <string>
#include <unistd.h>
#include <vector>

using namespace XrdAccjwtScope;

namespace {

constexpr const char *kWlcgVer = "1.0";

std::vector<ScopeRule> ParseScopes(const std::string &claim,
                                   const std::string &wlcgVer = kWlcgVer)
{
    std::vector<ScopeRule> rules;
    EXPECT_TRUE(ParseScopeClaim(claim, wlcgVer, rules));
    return rules;
}

bool Authorized(const std::string &claim,
                const std::string &basePath,
                const std::string &path,
                Access_Operation oper,
                const std::string &wlcgVer = kWlcgVer)
{
    const auto rules = ParseScopes(claim, wlcgVer);
    return AuthorizeOperation(rules, basePath, path, oper);
}

} // namespace

TEST(XrdAccjwtScopeTests, DetectProfileFromWlcgVer)
{
    EXPECT_TRUE(IsWlcgProfile("1.0"));
    EXPECT_EQ(DetectProfile("1.0"), ScopeProfile::WLCG);
    EXPECT_FALSE(IsWlcgProfile(""));
    EXPECT_FALSE(IsWlcgProfile("   "));
    EXPECT_EQ(DetectProfile(""), ScopeProfile::SciTokens);
}

TEST(XrdAccjwtScopeTests, ParseMultipleWlcgStorageScopes)
{
    std::vector<ScopeRule> rules;
    ASSERT_TRUE(ParseScopeClaim("storage.read:/raw storage.modify:/users/alice", kWlcgVer, rules));
    ASSERT_EQ(rules.size(), 2u);
    EXPECT_EQ(rules[0].capability, StorageCapability::Read);
    EXPECT_EQ(rules[0].path, "/raw");
    EXPECT_EQ(rules[1].capability, StorageCapability::Modify);
    EXPECT_EQ(rules[1].path, "/users/alice");
}

TEST(XrdAccjwtScopeTests, WlcgScopesRequireWlcgVer)
{
    std::vector<ScopeRule> rules;
    EXPECT_FALSE(ParseScopeClaim("storage.read:/data", "", rules));
}

TEST(XrdAccjwtScopeTests, IgnoreWlcgComputeScopes)
{
    std::vector<ScopeRule> rules;
    EXPECT_FALSE(ParseScopeClaim("compute.read compute.create", kWlcgVer, rules));

    ASSERT_TRUE(ParseScopeClaim("storage.read:/data compute.read", kWlcgVer, rules));
    ASSERT_EQ(rules.size(), 1u);
    EXPECT_EQ(rules[0].path, "/data");
}

TEST(XrdAccjwtScopeTests, ReadScopeSubtreeAuthorization)
{
    const std::string claim = "storage.read:/datasets";

    EXPECT_TRUE(Authorized(claim, "", "/datasets/file1", AOP_Read));
    EXPECT_TRUE(Authorized(claim, "", "/datasets/subdir/file2", AOP_Readdir));
    EXPECT_TRUE(Authorized(claim, "", "/datasets", AOP_Stat));
    EXPECT_FALSE(Authorized(claim, "", "/other/file", AOP_Read));
    EXPECT_FALSE(Authorized(claim, "", "/datasets/file", AOP_Delete));
}

TEST(XrdAccjwtScopeTests, CreateScopeDoesNotAuthorizeRead)
{
    const std::string claim = "storage.create:/scratch";

    EXPECT_TRUE(Authorized(claim, "", "/scratch/newfile", AOP_Excl_Create));
    EXPECT_TRUE(Authorized(claim, "", "/scratch", AOP_Mkdir));
    EXPECT_FALSE(Authorized(claim, "", "/scratch/existing", AOP_Read));
    EXPECT_FALSE(Authorized(claim, "", "/scratch/existing", AOP_Delete));
}

TEST(XrdAccjwtScopeTests, ModifyScopeAuthorizesWriteOperations)
{
    const std::string claim = "storage.modify:/scratch";

    EXPECT_TRUE(Authorized(claim, "", "/scratch/file", AOP_Update));
    EXPECT_TRUE(Authorized(claim, "", "/scratch/file", AOP_Delete));
    EXPECT_TRUE(Authorized(claim, "", "/scratch/other", AOP_Create));
    EXPECT_FALSE(Authorized(claim, "", "/scratch/file", AOP_Read));
}

TEST(XrdAccjwtScopeTests, CombinedReadAndModifyOnSamePath)
{
    const std::string claim = "storage.read:/data storage.modify:/data";

    EXPECT_TRUE(Authorized(claim, "", "/data/file", AOP_Read));
    EXPECT_TRUE(Authorized(claim, "", "/data/file", AOP_Delete));
}

TEST(XrdAccjwtScopeTests, StageScopeAuthorizesTapeStagingOnly)
{
    const std::string claim = "storage.stage:/archive";

    EXPECT_TRUE(Authorized(claim, "", "/archive/tape/file", AOP_Stage));
    EXPECT_TRUE(Authorized(claim, "", "/archive", AOP_Stage));
    EXPECT_FALSE(Authorized(claim, "", "/archive/tape/file", AOP_Read));
    EXPECT_FALSE(Authorized(claim, "", "/archive/tape/file", AOP_Poll));
    EXPECT_FALSE(Authorized(claim, "", "/other/archive/file", AOP_Stage));
}

TEST(XrdAccjwtScopeTests, PollScopeAuthorizesStagingStatusOnly)
{
    const std::string claim = "storage.poll:/archive";

    EXPECT_TRUE(Authorized(claim, "", "/archive/tape/file", AOP_Poll));
    EXPECT_FALSE(Authorized(claim, "", "/archive/tape/file", AOP_Stage));
    EXPECT_FALSE(Authorized(claim, "", "/archive/tape/file", AOP_Read));
}

TEST(XrdAccjwtScopeTests, StageAndPollAreIndependent)
{
    const std::string stageOnly = "storage.stage:/archive";
    const std::string pollOnly = "storage.poll:/archive";

    EXPECT_TRUE(Authorized(stageOnly, "", "/archive/file", AOP_Stage));
    EXPECT_FALSE(Authorized(stageOnly, "", "/archive/file", AOP_Poll));

    EXPECT_TRUE(Authorized(pollOnly, "", "/archive/file", AOP_Poll));
    EXPECT_FALSE(Authorized(pollOnly, "", "/archive/file", AOP_Stage));
}

TEST(XrdAccjwtScopeTests, ParseSciTokensReadAndWriteScopes)
{
    std::vector<ScopeRule> rules;
    ASSERT_TRUE(ParseScopeClaim("read:/datasets write:/scratch", "", rules));
    ASSERT_EQ(rules.size(), 2u);
    EXPECT_EQ(rules[0].capability, StorageCapability::Read);
    EXPECT_EQ(rules[0].path, "/datasets");
    EXPECT_EQ(rules[1].capability, StorageCapability::Modify);
    EXPECT_EQ(rules[1].path, "/scratch");
}

TEST(XrdAccjwtScopeTests, SciTokensReadScopeAuthorization)
{
    const std::string claim = "read:/datasets";

    EXPECT_TRUE(Authorized(claim, "", "/datasets/file1", AOP_Read, ""));
    EXPECT_TRUE(Authorized(claim, "", "/datasets/subdir/file2", AOP_Readdir, ""));
    EXPECT_FALSE(Authorized(claim, "", "/datasets/file", AOP_Delete, ""));
}

TEST(XrdAccjwtScopeTests, SciTokensWriteScopeAuthorizesFullWrite)
{
    const std::string claim = "write:/scratch";

    EXPECT_TRUE(Authorized(claim, "", "/scratch/file", AOP_Update, ""));
    EXPECT_TRUE(Authorized(claim, "", "/scratch/file", AOP_Delete, ""));
    EXPECT_TRUE(Authorized(claim, "", "/scratch/other", AOP_Create, ""));
    EXPECT_FALSE(Authorized(claim, "", "/scratch/file", AOP_Read, ""));
}

TEST(XrdAccjwtScopeTests, SciTokensCombinedReadAndWriteOnSamePath)
{
    const std::string claim = "read:/users/alice write:/users/alice";

    EXPECT_TRUE(Authorized(claim, "", "/users/alice/file", AOP_Read, ""));
    EXPECT_TRUE(Authorized(claim, "", "/users/alice/file", AOP_Delete, ""));
}

TEST(XrdAccjwtScopeTests, IgnoreSciTokensComputeScopes)
{
    std::vector<ScopeRule> rules;
    EXPECT_FALSE(ParseScopeClaim("queue execute", "", rules));

    ASSERT_TRUE(ParseScopeClaim("read:/data queue execute", "", rules));
    ASSERT_EQ(rules.size(), 1u);
    EXPECT_EQ(rules[0].path, "/data");
}

TEST(XrdAccjwtScopeTests, SciTokensScopesIgnoredWhenWlcgVerPresent)
{
    std::vector<ScopeRule> rules;
    EXPECT_FALSE(ParseScopeClaim("read:/data write:/scratch", kWlcgVer, rules));
}

TEST(XrdAccjwtScopeTests, ParentDirectoryStatAllowed)
{
    const std::string claim = "storage.read:/data/subdir/file";

    EXPECT_TRUE(Authorized(claim, "", "/data", AOP_Stat));
    EXPECT_TRUE(Authorized(claim, "", "/data/subdir", AOP_Stat));
    EXPECT_FALSE(Authorized(claim, "", "/data", AOP_Read));
}

TEST(XrdAccjwtScopeTests, BasePathPrefixApplied)
{
    const std::string claim = "storage.read:/data";

    EXPECT_TRUE(Authorized(claim, "/atlas", "/atlas/data/file", AOP_Read));
    EXPECT_FALSE(Authorized(claim, "/atlas", "/data/file", AOP_Read));
}

TEST(XrdAccjwtScopeTests, ResolveBasePathAppendsEntityPathToConfig)
{
    EXPECT_EQ(ResolveBasePath("/atlas", "/eos"), "/atlas/eos");
    EXPECT_EQ(ResolveBasePath("/atlas", ""), "/atlas");
    EXPECT_EQ(ResolveBasePath("", "/eos"), "/eos");
    EXPECT_EQ(ResolveBasePath("", ""), "");
    EXPECT_EQ(ResolveBasePath("/atlas", "/eos/"), "/atlas/eos");
}

TEST(XrdAccjwtScopeTests, ResolveBasePathRejectsInvalidEntityPath)
{
    EXPECT_TRUE(ResolveBasePath("/atlas", "relative").empty());
}

TEST(XrdAccjwtScopeTests, CombinedConfigAndEntityBasePathAuthorizesUnderBoth)
{
    const auto rules = ParseScopes("storage.read:/data");
    const std::string base = ResolveBasePath("/atlas", "/eos");

    EXPECT_TRUE(AuthorizeOperation(rules, base, "/atlas/eos/data/file", AOP_Read));
    EXPECT_FALSE(AuthorizeOperation(rules, base, "/atlas/data/file", AOP_Read));
}

TEST(XrdAccjwtScopeTests, ParseRestrictedPathsJsonAcceptsArray)
{
    std::vector<std::string> paths;
    std::string emsg;
    ASSERT_TRUE(ParseRestrictedPathsJson(R"(["/public","/shared"])", paths, emsg)) << emsg;
    ASSERT_EQ(paths.size(), 2u);
    EXPECT_EQ(paths[0], "/public");
    EXPECT_EQ(paths[1], "/shared");
}

TEST(XrdAccjwtScopeTests, ParseRestrictedPathsJsonDecodesEscapesAndRejectsMalformed)
{
    std::vector<std::string> paths;
    std::string emsg;
    ASSERT_TRUE(ParseRestrictedPathsJson(R"(["\u002fpublic","/dir\\file"])", paths, emsg)) << emsg;
    ASSERT_EQ(paths.size(), 2u);
    EXPECT_EQ(paths[0], "/public");
    EXPECT_EQ(paths[1], "/dir\\file");

    EXPECT_FALSE(ParseRestrictedPathsJson(R"(["/a" "/b"])", paths, emsg));
    EXPECT_NE(emsg.find("JSON array"), std::string::npos);
    EXPECT_TRUE(paths.empty());
    EXPECT_FALSE(ParseRestrictedPathsJson(R"(["/a",,"/b"])", paths, emsg));
    EXPECT_FALSE(ParseRestrictedPathsJson(R"(["/a", 1])", paths, emsg));
    EXPECT_NE(emsg.find("must contain strings"), std::string::npos);
    EXPECT_TRUE(paths.empty());
}

TEST(XrdAccjwtScopeTests, RequestMatchesRestrictedPaths)
{
    const std::vector<std::string> restricted = {"/public", "/shared"};

    EXPECT_TRUE(RequestMatchesRestrictedPaths("/public", restricted));
    EXPECT_TRUE(RequestMatchesRestrictedPaths("/public/file", restricted));
    EXPECT_TRUE(RequestMatchesRestrictedPaths("/shared/dir/file", restricted));
    EXPECT_FALSE(RequestMatchesRestrictedPaths("/private/file", restricted));
    EXPECT_FALSE(RequestMatchesRestrictedPaths("/publicx/file", restricted));
}

TEST(XrdAccjwtScopeTests, PrivilegesForPathIncludesStageAndPoll)
{
    const auto rules = ParseScopes("storage.stage:/archive storage.poll:/archive");
    const auto privs = PrivilegesForPath(rules, "", "/archive/file");

    EXPECT_NE(privs & XrdAccPriv_Stage, 0);
    EXPECT_NE(privs & XrdAccPriv_Poll, 0);
    EXPECT_EQ(privs & XrdAccPriv_Read, 0);
}

TEST(XrdAccjwtScopeTests, MakeCanonicalCollapsesDotSegments)
{
    std::string out;
    ASSERT_TRUE(MakeCanonical("/atlas/./data/../reco", out));
    EXPECT_EQ(out, "/atlas/reco");
}

TEST(XrdAccjwtScopeTests, JsonArrayBasePathIsAlternativePrefixes)
{
    std::vector<std::string> bases;
    std::string emsg;
    ASSERT_TRUE(ResolveBasePaths("/atlas", "[\"/eos\",\"/cms\"]", bases, emsg)) << emsg;
    ASSERT_EQ(bases.size(), 2u);
    EXPECT_EQ(bases[0], "/atlas/eos");
    EXPECT_EQ(bases[1], "/atlas/cms");

    const auto rules = ParseScopes("storage.read:/data");
    EXPECT_TRUE(AuthorizeOperation(rules, bases, "/atlas/eos/data/file", AOP_Read));
    EXPECT_TRUE(AuthorizeOperation(rules, bases, "/atlas/cms/data/file", AOP_Read));
    EXPECT_FALSE(AuthorizeOperation(rules, bases, "/atlas/data/file", AOP_Read));
}

TEST(XrdAccjwtScopeTests, SingleEntityBasePathStaysOnePrefix)
{
    std::vector<std::string> bases;
    std::string emsg;
    ASSERT_TRUE(ResolveBasePaths("/atlas", "/eos", bases, emsg)) << emsg;
    ASSERT_EQ(bases.size(), 1u);
    EXPECT_EQ(bases[0], "/atlas/eos");
}

TEST(XrdAccjwtScopeTests, AuthorizationStrategy)
{
    unsigned strategy = 0;
    std::string emsg;
    ASSERT_TRUE(ParseAuthzStrategy("capability group mapping", strategy, emsg)) << emsg;
    EXPECT_EQ(strategy, AuthzStrategyDefault);
    EXPECT_TRUE(CapabilityGrants(strategy, true));
    EXPECT_FALSE(CapabilityGrants(strategy, false));

    ASSERT_TRUE(ParseAuthzStrategy("Group+Mapping", strategy, emsg)) << emsg;
    EXPECT_EQ(strategy, AuthzStrategyGroup | AuthzStrategyMapping);
    EXPECT_FALSE(CapabilityGrants(strategy, true));
    EXPECT_EQ(AuthzStrategyText(strategy), "group+mapping");

    EXPECT_FALSE(ParseAuthzStrategy("capability nope", strategy, emsg));
    EXPECT_NE(emsg.find("nope"), std::string::npos);
    EXPECT_FALSE(ParseAuthzStrategy("", strategy, emsg));
}

TEST(XrdAccjwtScopeTests, InvalidBasePathArrayIsRejected)
{
    std::vector<std::string> bases;
    std::string emsg;
    EXPECT_FALSE(ResolveBasePaths("/atlas", "[\"relative\"]", bases, emsg));
    EXPECT_FALSE(emsg.empty());
    EXPECT_FALSE(ResolveBasePaths("/atlas", "not-absolute", bases, emsg));
}

const char *kMap = R"([
  {"sub": "bbockelm", "path": "/bbockelm", "result": "bbockelm"},
  {"group": "/cms/prod", "path": "/cms", "result": "cmsprod"},
  {"group": "/cms", "result": "cmsuser"},
  {"username": "alice", "result": "alice_mapped"},
  {"sub": "skip", "result": "nope", "ignore": "1", "comment": "not used"}
])";

TEST(XrdAccjwtScopeTests, NameMapFirstRuleWins)
{
    std::vector<NameMapRule> rules;
    std::string emsg;
    ASSERT_TRUE(ParseNameMap(kMap, rules, emsg)) << emsg;
    EXPECT_EQ(rules.size(), 4u);

    const std::vector<std::string> bases{"/home"};
    EXPECT_EQ(MatchNameMap(rules, "bbockelm", "bbockelm", {},
                           "/home/bbockelm/foo", bases),
              "bbockelm");
    EXPECT_EQ(MatchNameMap(rules, "bbockelm", "bbockelm", {},
                           "/home/bbockelm2", bases),
              "");
}

TEST(XrdAccjwtScopeTests, NameMapGroupIsLimitedToBasePath)
{
    std::vector<NameMapRule> rules;
    std::string emsg;
    ASSERT_TRUE(ParseNameMap(kMap, rules, emsg)) << emsg;
    const std::vector<std::string> bases{"/eos"};
    const std::vector<std::string> groups{"/cms", "/cms/prod"};

    EXPECT_EQ(MatchNameMap(rules, "other", "other", groups, "/eos/cms/data", bases),
              "cmsprod");
    EXPECT_EQ(MatchNameMap(rules, "other", "other", groups, "/atlas/cms", bases),
              "");
    EXPECT_EQ(MatchNameMap(rules, "other", "other", {"/cms"}, "/eos/public", bases),
              "cmsuser");
}

TEST(XrdAccjwtScopeTests, NameMapMissKeepsHandshakeName)
{
    std::vector<NameMapRule> rules;
    std::string emsg;
    ASSERT_TRUE(ParseNameMap(kMap, rules, emsg)) << emsg;
    EXPECT_EQ(MatchNameMap(rules, "nobody", "nobody", {}, "/home/nobody", {"/home"}),
              "");
    EXPECT_EQ(MatchNameMap(rules, "id", "alice", {}, "/anywhere", {""}),
              "alice_mapped");
}

TEST(XrdAccjwtScopeTests, NameMapRejectsUnsafeResultAndRelativePath)
{
    std::vector<NameMapRule> rules;
    std::string emsg;
    EXPECT_FALSE(ParseNameMap(R"([{"result": "alice/prod"}])", rules, emsg));
    EXPECT_NE(emsg.find("safe account"), std::string::npos);
    EXPECT_FALSE(ParseNameMap(R"([{"path": "relative", "result": "alice"}])", rules, emsg));
    EXPECT_FALSE(ParseNameMap(R"({"result": "alice"})", rules, emsg));
    EXPECT_FALSE(ParseNameMap(R"([{"sub": "alice"}])", rules, emsg));
}

TEST(XrdAccjwtScopeTests, NameMapFileRoundTrip)
{
    std::string path = "/tmp/xrdaccjwt-namemap-XXXXXX";
    const int fd = mkstemp(path.data());
    ASSERT_GE(fd, 0);
    close(fd);
    {
        std::ofstream out(path);
        ASSERT_TRUE(out) << path;
        out << R"([{"group": "/ops", "path": "/ops", "result": "ops"}])";
    }
    std::vector<NameMapRule> rules;
    std::string emsg;
    ASSERT_TRUE(LoadNameMapFile(path, rules, emsg)) << emsg;
    ASSERT_EQ(rules.size(), 1u);
    EXPECT_EQ(rules[0].result, "ops");
    EXPECT_EQ(MatchNameMap(rules, "sub", "user", {"/ops"}, "/data/ops/a", {"/data"}),
              "ops");
    EXPECT_FALSE(LoadNameMapFile("/tmp/xrdaccjwt-namemap-missing.json", rules, emsg));
    unlink(path.c_str());
}

TEST(XrdAccjwtScopeTests, OpenAuthzUsesTheRequestTokenIdentity)
{
    EXPECT_EQ(OpenAuthzToken(nullptr), nullptr);
    EXPECT_EQ(OpenAuthzToken(""), nullptr);
    EXPECT_STREQ(OpenAuthzToken("eyJhbGciOiJub25lIn0.e30.sig"), "eyJhbGciOiJub25lIn0.e30.sig");

    const std::map<std::string, std::string> token = {
        {"token.subject", "token-sub"},
        {"request.name", "token-user"},
        {"jwt.grps", "/cms /ops"},
    };
    const MapIdentity fromToken = IdentityForMap(&token, "handshake", "/login", "handshake-sub");
    EXPECT_EQ(fromToken.sub, "token-sub");
    EXPECT_EQ(fromToken.username, "token-user");
    ASSERT_EQ(fromToken.groups.size(), 2u);
    EXPECT_EQ(fromToken.groups[0], "/cms");
    EXPECT_EQ(fromToken.groups[1], "/ops");

    const MapIdentity fromLogin = IdentityForMap(nullptr, "handshake", "/login", "handshake-sub");
    EXPECT_EQ(fromLogin.sub, "handshake-sub");
    EXPECT_EQ(fromLogin.username, "handshake");
    ASSERT_EQ(fromLogin.groups.size(), 1u);
    EXPECT_EQ(fromLogin.groups[0], "/login");
}

TEST(XrdAccjwtScopeTests, EosJwtTypeSkipsEvaluation)
{
    const char *eos = "eyJhbGciOiJFUzI1NiIsInR5cCI6ImVvcytqd3QifQ.e30.sig";
    EXPECT_EQ(TokenHeaderType(eos), "eos+jwt");
    EXPECT_TRUE(IsEosJwtType(TokenHeaderType(eos)));
    EXPECT_TRUE(IsEosJwtType("application/eos+jwt"));
    EXPECT_TRUE(IsEosJwtType("EOS+JWT"));

    const char *bearer = "Bearer eyJhbGciOiJFUzI1NiIsInR5cCI6ImVvcytqd3QifQ.e30.sig";
    EXPECT_TRUE(IsEosJwtType(TokenHeaderType(bearer)));

    EXPECT_EQ(TokenHeaderType("eyJ0eXAiOiJKV1QifQ.e30.sig"), "JWT");
    EXPECT_FALSE(IsEosJwtType(TokenHeaderType("eyJ0eXAiOiJKV1QifQ.e30.sig")));
    EXPECT_FALSE(IsEosJwtType("at+jwt"));
    EXPECT_TRUE(TokenHeaderType("not-a-jwt").empty());
    EXPECT_TRUE(TokenHeaderType("eyJhbGciOiJub25lIn0.e30.sig").empty());
}
