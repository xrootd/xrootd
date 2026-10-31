#include "HeaderBuilderFixture.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

using namespace XrdClHttp;

using testing::Contains;
using testing::ElementsAre;
using testing::Pair;

namespace {

class EntrySeparation : public HeaderBuilderFixture {};

} // namespace

/******************************************************************************/
/*                B u i l d :  e n t r y  s e p a r a t i o n                 */
/******************************************************************************/

TEST_F(EntrySeparation, TheFirstOfSeveralEntriesIsAdded)
{
    const std::string spec{"X-First: one\nX-Second: two\nX-Third: three"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, Contains(Pair("X-First", "one")));
}

TEST_F(EntrySeparation, AMiddleEntryIsAdded)
{
    const std::string spec{"X-First: one\nX-Second: two\nX-Third: three"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, Contains(Pair("X-Second", "two")));
}

TEST_F(EntrySeparation, TheLastOfSeveralEntriesIsAdded)
{
    const std::string spec{"X-First: one\nX-Second: two\nX-Third: three"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, Contains(Pair("X-Third", "three")));
}

// Blank entries are tolerated so that padding a specification is not an error.

TEST_F(EntrySeparation, ALeadingBlankEntryIsSkipped)
{
    const std::string spec{"\nX-Test: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

TEST_F(EntrySeparation, AnEmbeddedBlankEntryIsSkipped)
{
    const std::string spec{"X-First: one\n\nX-Second: two"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-First", "one"), Pair("X-Second", "two")));
}

TEST_F(EntrySeparation, ATrailingBlankEntryIsSkipped)
{
    const std::string spec{"X-Test: value\n"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

// A newline separates entries, so it cannot appear inside a value; a bare
// carriage return could otherwise forge part of the request.
TEST_F(EntrySeparation, ACarriageReturnInAValueIsRejected)
{
    const std::string spec{"X-Test: a\rEvil: b"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

// A CRLF is the wire form of the separator, so it yields two entries.

TEST_F(EntrySeparation, ACrlfEndsTheEntryBeforeIt)
{
    const std::string spec{"X-Test: a\r\nX-Other: b"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, Contains(Pair("X-Test", "a")));
}

TEST_F(EntrySeparation, ACrlfStartsTheEntryAfterIt)
{
    const std::string spec{"X-Test: a\r\nX-Other: b"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, Contains(Pair("X-Other", "b")));
}

// Each entry a CRLF yields is validated in its own right, which is what stops a
// reserved header sneaking in behind one.
TEST_F(EntrySeparation, AForbiddenEntryAfterACrlfIsRejected)
{
    const std::string spec{"X-Test: a\r\nHost: evil"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}
