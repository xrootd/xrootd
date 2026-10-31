#include "HeaderBuilderFixture.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace XrdClHttp;

using testing::Contains;
using testing::ElementsAre;
using testing::Pair;
using testing::SizeIs;

namespace {

class AppendMissing : public HeaderBuilderFixture {};

} // namespace

/******************************************************************************/
/*                          A p p e n d M i s s i n g                         */
/******************************************************************************/

// A requested header must never displace one the request set for itself; a
// requested Range would otherwise corrupt a read.
TEST_F(AppendMissing, ARequestHeaderIsNotDisplaced)
{
    headers.emplace_back("Range", "bytes=0-1023");

    HeaderBuilder::AppendMissing({{"Range", "bytes=999-"}}, headers);
    EXPECT_THAT(headers, Contains(Pair("Range", "bytes=0-1023")));
}

// Only one Range must be present, or curl would send both.
TEST_F(AppendMissing, ARequestHeaderIsNotDuplicated)
{
    headers.emplace_back("Range", "bytes=0-1023");

    HeaderBuilder::AppendMissing({{"Range", "bytes=999-"}}, headers);
    EXPECT_THAT(headers, SizeIs(1));
}

// Dropping the duplicate must not cost the other entries their place.
TEST_F(AppendMissing, ADroppedDuplicateStillLeavesTheOtherEntries)
{
    headers.emplace_back("Range", "bytes=0-1023");

    HeaderBuilder::AppendMissing({{"Range", "bytes=999-"}, {"X-Test-Header", "value"}}, headers);
    EXPECT_THAT(headers, Contains(Pair("X-Test-Header", "value")));
}

TEST_F(AppendMissing, ADuplicateDifferingInCaseIsNotAdded)
{
    headers.emplace_back("X-Test", "original");

    HeaderBuilder::AppendMissing({{"x-TEST", "replacement"}}, headers);
    EXPECT_THAT(headers, SizeIs(1));
}

TEST_F(AppendMissing, ADuplicateDifferingInCaseKeepsTheRequestValue)
{
    headers.emplace_back("X-Test", "original");

    HeaderBuilder::AppendMissing({{"x-TEST", "replacement"}}, headers);
    EXPECT_THAT(headers, Contains(Pair("X-Test", "original")));
}

TEST_F(AppendMissing, AnUnrelatedNameIsAppended)
{
    headers.emplace_back("Range", "bytes=0-1023");

    HeaderBuilder::AppendMissing({{"X-Test-Header", "value"}}, headers);
    EXPECT_THAT(headers, ElementsAre(Pair("Range", "bytes=0-1023"),
                                     Pair("X-Test-Header", "value")));
}

TEST_F(AppendMissing, NothingToAppendLeavesTheHeadersUnchanged)
{
    headers.emplace_back("Range", "bytes=0-1023");

    HeaderBuilder::AppendMissing({}, headers);
    EXPECT_THAT(headers, ElementsAre(Pair("Range", "bytes=0-1023")));
}
