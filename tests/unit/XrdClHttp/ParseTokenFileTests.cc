#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "XrdCl/XrdClDefaultEnv.hh"

#include "XrdClHttp/XrdClHttpOps.hh"

#include <filesystem>
#include <fstream>
#include <string>

using namespace std::string_literals;

using namespace testing;

#include "XrdClHttp/XrdClHttpFilesystem.cc"

class ParseTokenFileFixture : public testing::Test
{
protected:
    const std::string token_file_path{(std::filesystem::temp_directory_path() / "xrdclhttp-tests-parse-token-file").string()};

    XrdClHttp::CurlCopyOp::Headers src_hdrs;
    XrdClHttp::CurlCopyOp::Headers dst_hdrs;

    void write_token_file(const std::string &content)
    {
        std::ofstream file(token_file_path);
        file << content;
    }

    void SetUp() override
    {
        // A previous run can leave the file behind. Start from a clean state.
        std::filesystem::remove(token_file_path);
    }

    void TearDown() override
    {
        std::filesystem::remove(token_file_path);
    }
};

TEST_F(ParseTokenFileFixture, FailsWhenTheFileDoesNotExist)
{
    ASSERT_FALSE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs, XrdCl::DefaultEnv::GetLog()));
    ASSERT_THAT(src_hdrs, IsEmpty());
    ASSERT_THAT(dst_hdrs, IsEmpty());
}

TEST_F(ParseTokenFileFixture, SetsBothAuthorizationHeadersWhenTheFileHasTwoLines)
{
    write_token_file("TokenA\nTokenB\n");

    ASSERT_TRUE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs, XrdCl::DefaultEnv::GetLog()));
    ASSERT_THAT(src_hdrs, ElementsAre(Pair("Authorization"s, "Bearer TokenA"s)));
    ASSERT_THAT(dst_hdrs, ElementsAre(Pair("Authorization"s, "Bearer TokenB"s)));
}

TEST_F(ParseTokenFileFixture, SetsOnlyTheSourceAuthorizationHeaderWhenTheFileHasOneLine)
{
    write_token_file("TokenA\n");

    ASSERT_TRUE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs, XrdCl::DefaultEnv::GetLog()));
    ASSERT_THAT(src_hdrs, ElementsAre(Pair("Authorization"s, "Bearer TokenA"s)));
    ASSERT_THAT(dst_hdrs, IsEmpty());
}

TEST_F(ParseTokenFileFixture, SetsOnlyTheSourceAuthorizationHeaderWhenTheSecondLineIsEmpty)
{
    write_token_file("TokenA\n\n");

    ASSERT_TRUE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs, XrdCl::DefaultEnv::GetLog()));
    ASSERT_THAT(src_hdrs, ElementsAre(Pair("Authorization"s, "Bearer TokenA"s)));
    ASSERT_THAT(dst_hdrs, IsEmpty());
}

TEST_F(ParseTokenFileFixture, SetsOnlyTheDestinationAuthorizationHeaderWhenTheFirstLineIsEmpty)
{
    write_token_file("\nTokenB\n");

    ASSERT_TRUE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs, XrdCl::DefaultEnv::GetLog()));
    ASSERT_THAT(src_hdrs, IsEmpty());
    ASSERT_THAT(dst_hdrs, ElementsAre(Pair("Authorization"s, "Bearer TokenB"s)));
}

TEST_F(ParseTokenFileFixture, FailsWhenTheFileHasOnlyEmptyLines)
{
    write_token_file("\n\n");

    ASSERT_FALSE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs, XrdCl::DefaultEnv::GetLog()));
    ASSERT_THAT(src_hdrs, IsEmpty());
    ASSERT_THAT(dst_hdrs, IsEmpty());
}

TEST_F(ParseTokenFileFixture, FailsWhenTheFileIsEmpty)
{
    write_token_file("");

    ASSERT_FALSE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs, XrdCl::DefaultEnv::GetLog()));
    ASSERT_THAT(src_hdrs, IsEmpty());
    ASSERT_THAT(dst_hdrs, IsEmpty());
}

TEST_F(ParseTokenFileFixture, SetsBothAuthorizationHeadersWhenTheJsonHasSrcAndDst)
{
    write_token_file(R"({ "src": "TokenA", "dst": "TokenB" })");

    ASSERT_TRUE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs, XrdCl::DefaultEnv::GetLog()));
    ASSERT_THAT(src_hdrs, ElementsAre(Pair("Authorization"s, "Bearer TokenA"s)));
    ASSERT_THAT(dst_hdrs, ElementsAre(Pair("Authorization"s, "Bearer TokenB"s)));
}

TEST_F(ParseTokenFileFixture, SetsOnlyTheSourceAuthorizationHeaderWhenTheJsonHasSrcOnly)
{
    write_token_file(R"({ "src": "TokenA" })");

    ASSERT_TRUE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs, XrdCl::DefaultEnv::GetLog()));
    ASSERT_THAT(src_hdrs, ElementsAre(Pair("Authorization"s, "Bearer TokenA"s)));
    ASSERT_THAT(dst_hdrs, IsEmpty());
}

TEST_F(ParseTokenFileFixture, SetsOnlyTheDestinationAuthorizationHeaderWhenTheJsonHasDstOnly)
{
    write_token_file(R"({ "dst": "TokenB" })");

    ASSERT_TRUE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs, XrdCl::DefaultEnv::GetLog()));
    ASSERT_THAT(src_hdrs, IsEmpty());
    ASSERT_THAT(dst_hdrs, ElementsAre(Pair("Authorization"s, "Bearer TokenB"s)));
}

TEST_F(ParseTokenFileFixture, SetsOnlyTheSourceAuthorizationHeaderWhenTheJsonDstIsNotAString)
{
    write_token_file(R"({ "src": "TokenA", "dst": 1234 })");

    ASSERT_TRUE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs, XrdCl::DefaultEnv::GetLog()));
    ASSERT_THAT(src_hdrs, ElementsAre(Pair("Authorization"s, "Bearer TokenA"s)));
    ASSERT_THAT(dst_hdrs, IsEmpty());
}

TEST_F(ParseTokenFileFixture, SetsOnlyTheDestinationAuthorizationHeaderWhenTheJsonSrcIsNotAString)
{
    write_token_file(R"({ "src": 1234, "dst": "TokenB" })");

    ASSERT_TRUE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs, XrdCl::DefaultEnv::GetLog()));
    ASSERT_THAT(src_hdrs, IsEmpty());
    ASSERT_THAT(dst_hdrs, ElementsAre(Pair("Authorization"s, "Bearer TokenB"s)));
}

TEST_F(ParseTokenFileFixture, FailsWhenTheJsonHasNeitherSrcNorDst)
{
    write_token_file(R"({ "token": "TokenA" })");

    ASSERT_FALSE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs, XrdCl::DefaultEnv::GetLog()));
    ASSERT_THAT(src_hdrs, IsEmpty());
    ASSERT_THAT(dst_hdrs, IsEmpty());
}

TEST_F(ParseTokenFileFixture, FailsWhenTheJsonSrcIsNotAString)
{
    write_token_file(R"({ "src": 1234 })");

    ASSERT_FALSE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs, XrdCl::DefaultEnv::GetLog()));
    ASSERT_THAT(src_hdrs, IsEmpty());
    ASSERT_THAT(dst_hdrs, IsEmpty());
}

TEST_F(ParseTokenFileFixture, FailsWhenTheJsonSrcIsEmpty)
{
    write_token_file(R"({ "src": "" })");

    ASSERT_FALSE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs, XrdCl::DefaultEnv::GetLog()));
    ASSERT_THAT(src_hdrs, IsEmpty());
    ASSERT_THAT(dst_hdrs, IsEmpty());
}

TEST_F(ParseTokenFileFixture, FailsWhenTheJsonDstIsNotAString)
{
    write_token_file(R"({ "dst": 1234 })");

    ASSERT_FALSE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs, XrdCl::DefaultEnv::GetLog()));
    ASSERT_THAT(src_hdrs, IsEmpty());
    ASSERT_THAT(dst_hdrs, IsEmpty());
}

TEST_F(ParseTokenFileFixture, FailsWhenTheJsonDstIsEmpty)
{
    write_token_file(R"({ "dst": "" })");

    ASSERT_FALSE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs, XrdCl::DefaultEnv::GetLog()));
    ASSERT_THAT(src_hdrs, IsEmpty());
    ASSERT_THAT(dst_hdrs, IsEmpty());
}
