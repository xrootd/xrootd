#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "XrdClHttp/XrdClHttpOps.hh"
#include "XrdClHttp/XrdClHttpTokenFileParser.hh"

#include <filesystem>
#include <fstream>
#include <string>

#include <unistd.h>

using namespace std::string_literals;

using namespace testing;
using namespace XrdClHttp;

class ParseTokenFileTest : public testing::Test
{
protected:
    // Suffixed with the pid because ctest may run the tests of this suite in
    // parallel, each in its own process, and they share the temp directory.
    const std::string token_file_path{(std::filesystem::temp_directory_path() /
        ("xrdclhttp-tests-parse-token-file-" + std::to_string(getpid()))).string()};

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

TEST_F(ParseTokenFileTest, FailsWhenTheFileDoesNotExist)
{
    ASSERT_FALSE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs));
    ASSERT_THAT(src_hdrs, IsEmpty());
    ASSERT_THAT(dst_hdrs, IsEmpty());
}

TEST_F(ParseTokenFileTest, SetsBothAuthorizationHeadersWhenTheFileHasTwoLines)
{
    write_token_file("TokenA\nTokenB\n");

    ASSERT_TRUE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs));
    ASSERT_THAT(src_hdrs, ElementsAre(Pair("Authorization"s, "Bearer TokenA"s)));
    ASSERT_THAT(dst_hdrs, ElementsAre(Pair("Authorization"s, "Bearer TokenB"s)));
}

TEST_F(ParseTokenFileTest, SetsOnlyTheSourceAuthorizationHeaderWhenTheFileHasOneLine)
{
    write_token_file("TokenA\n");

    ASSERT_TRUE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs));
    ASSERT_THAT(src_hdrs, ElementsAre(Pair("Authorization"s, "Bearer TokenA"s)));
    ASSERT_THAT(dst_hdrs, IsEmpty());
}

TEST_F(ParseTokenFileTest, SetsOnlyTheSourceAuthorizationHeaderWhenTheSecondLineIsEmpty)
{
    write_token_file("TokenA\n\n");

    ASSERT_TRUE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs));
    ASSERT_THAT(src_hdrs, ElementsAre(Pair("Authorization"s, "Bearer TokenA"s)));
    ASSERT_THAT(dst_hdrs, IsEmpty());
}

TEST_F(ParseTokenFileTest, SetsOnlyTheDestinationAuthorizationHeaderWhenTheFirstLineIsEmpty)
{
    write_token_file("\nTokenB\n");

    ASSERT_TRUE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs));
    ASSERT_THAT(src_hdrs, IsEmpty());
    ASSERT_THAT(dst_hdrs, ElementsAre(Pair("Authorization"s, "Bearer TokenB"s)));
}

TEST_F(ParseTokenFileTest, FailsWhenTheFileHasOnlyEmptyLines)
{
    write_token_file("\n\n");

    ASSERT_FALSE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs));
    ASSERT_THAT(src_hdrs, IsEmpty());
    ASSERT_THAT(dst_hdrs, IsEmpty());
}

TEST_F(ParseTokenFileTest, FailsWhenTheFileIsEmpty)
{
    write_token_file("");

    ASSERT_FALSE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs));
    ASSERT_THAT(src_hdrs, IsEmpty());
    ASSERT_THAT(dst_hdrs, IsEmpty());
}

TEST_F(ParseTokenFileTest, SetsBothAuthorizationHeadersWhenTheJsonHasSrcAndDst)
{
    write_token_file(R"({ "src": "TokenA", "dst": "TokenB" })");

    ASSERT_TRUE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs));
    ASSERT_THAT(src_hdrs, ElementsAre(Pair("Authorization"s, "Bearer TokenA"s)));
    ASSERT_THAT(dst_hdrs, ElementsAre(Pair("Authorization"s, "Bearer TokenB"s)));
}

TEST_F(ParseTokenFileTest, SetsOnlyTheSourceAuthorizationHeaderWhenTheJsonHasSrcOnly)
{
    write_token_file(R"({ "src": "TokenA" })");

    ASSERT_TRUE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs));
    ASSERT_THAT(src_hdrs, ElementsAre(Pair("Authorization"s, "Bearer TokenA"s)));
    ASSERT_THAT(dst_hdrs, IsEmpty());
}

TEST_F(ParseTokenFileTest, SetsOnlyTheDestinationAuthorizationHeaderWhenTheJsonHasDstOnly)
{
    write_token_file(R"({ "dst": "TokenB" })");

    ASSERT_TRUE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs));
    ASSERT_THAT(src_hdrs, IsEmpty());
    ASSERT_THAT(dst_hdrs, ElementsAre(Pair("Authorization"s, "Bearer TokenB"s)));
}

TEST_F(ParseTokenFileTest, SetsOnlyTheSourceAuthorizationHeaderWhenTheJsonDstIsNotAString)
{
    write_token_file(R"({ "src": "TokenA", "dst": 1234 })");

    ASSERT_TRUE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs));
    ASSERT_THAT(src_hdrs, ElementsAre(Pair("Authorization"s, "Bearer TokenA"s)));
    ASSERT_THAT(dst_hdrs, IsEmpty());
}

TEST_F(ParseTokenFileTest, SetsOnlyTheDestinationAuthorizationHeaderWhenTheJsonSrcIsNotAString)
{
    write_token_file(R"({ "src": 1234, "dst": "TokenB" })");

    ASSERT_TRUE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs));
    ASSERT_THAT(src_hdrs, IsEmpty());
    ASSERT_THAT(dst_hdrs, ElementsAre(Pair("Authorization"s, "Bearer TokenB"s)));
}

TEST_F(ParseTokenFileTest, FailsWhenTheJsonHasNeitherSrcNorDst)
{
    write_token_file(R"({ "token": "TokenA" })");

    ASSERT_FALSE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs));
    ASSERT_THAT(src_hdrs, IsEmpty());
    ASSERT_THAT(dst_hdrs, IsEmpty());
}

TEST_F(ParseTokenFileTest, FailsWhenTheJsonSrcIsNotAString)
{
    write_token_file(R"({ "src": 1234 })");

    ASSERT_FALSE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs));
    ASSERT_THAT(src_hdrs, IsEmpty());
    ASSERT_THAT(dst_hdrs, IsEmpty());
}

TEST_F(ParseTokenFileTest, FailsWhenTheJsonSrcIsEmpty)
{
    write_token_file(R"({ "src": "" })");

    ASSERT_FALSE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs));
    ASSERT_THAT(src_hdrs, IsEmpty());
    ASSERT_THAT(dst_hdrs, IsEmpty());
}

TEST_F(ParseTokenFileTest, FailsWhenTheJsonDstIsNotAString)
{
    write_token_file(R"({ "dst": 1234 })");

    ASSERT_FALSE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs));
    ASSERT_THAT(src_hdrs, IsEmpty());
    ASSERT_THAT(dst_hdrs, IsEmpty());
}

TEST_F(ParseTokenFileTest, FailsWhenTheJsonDstIsEmpty)
{
    write_token_file(R"({ "dst": "" })");

    ASSERT_FALSE(ParseTokenFile(token_file_path, src_hdrs, dst_hdrs));
    ASSERT_THAT(src_hdrs, IsEmpty());
    ASSERT_THAT(dst_hdrs, IsEmpty());
}
