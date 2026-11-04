#include "HeaderBuilderFixture.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

using namespace XrdClHttp;

using testing::ElementsAre;
using testing::IsEmpty;
using testing::Pair;

namespace {

class HeaderBuilderParsingTest : public HeaderBuilderFixture {};

} // namespace

/******************************************************************************/
/*                      B u i l d :  p a r s i n g                            */
/******************************************************************************/

TEST_F(HeaderBuilderParsingTest, ARequestedHeaderIsAdded)
{
    const std::string spec{"X-Test-Header: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test-Header", "value")));
}

// A user who holds a token has to be able to present it, so an Authorization
// entry is built like any other, for the near server and for the far one.

TEST_F(HeaderBuilderParsingTest, AnAuthorizationEntryIsAdded)
{
    const std::string spec{"Authorization: Bearer token"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("Authorization", "Bearer token")));
}

TEST_F(HeaderBuilderParsingTest, ATransferHeaderAuthorizationEntryIsAdded)
{
    const std::string spec{"TransferHeaderAuthorization: Bearer token"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers,
        ElementsAre(Pair("TransferHeaderAuthorization", "Bearer token")));
}

TEST_F(HeaderBuilderParsingTest, TheColonNeedNotBeFollowedByASpace)
{
    const std::string spec{"X-Test:value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

TEST_F(HeaderBuilderParsingTest, WhitespaceBeforeTheNameIsTrimmed)
{
    const std::string spec{" \t X-Test: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

TEST_F(HeaderBuilderParsingTest, WhitespaceAfterTheNameIsTrimmed)
{
    const std::string spec{"X-Test \t : value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

TEST_F(HeaderBuilderParsingTest, WhitespaceBeforeTheValueIsTrimmed)
{
    const std::string spec{"X-Test: \t value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

TEST_F(HeaderBuilderParsingTest, WhitespaceAfterTheValueIsTrimmed)
{
    const std::string spec{"X-Test: value \t "};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

// Only the first colon separates, so the rest of the entry is the value.
TEST_F(HeaderBuilderParsingTest, AValueMayContainAColon)
{
    const std::string spec{"X-Test: a:b"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "a:b")));
}

TEST_F(HeaderBuilderParsingTest, AValueMayContainASpace)
{
    const std::string spec{"X-Test: a b"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "a b")));
}

// A name is an RFC 7230 token, so each of the characters below belongs in one.

TEST_F(HeaderBuilderParsingTest, ADigitIsAcceptedInAName)
{
    const std::string spec{"X1Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X1Y", "value")));
}

TEST_F(HeaderBuilderParsingTest, AnExclamationMarkIsAcceptedInAName)
{
    const std::string spec{"X!Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X!Y", "value")));
}

TEST_F(HeaderBuilderParsingTest, AHashIsAcceptedInAName)
{
    const std::string spec{"X#Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X#Y", "value")));
}

TEST_F(HeaderBuilderParsingTest, ADollarSignIsAcceptedInAName)
{
    const std::string spec{"X$Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X$Y", "value")));
}

TEST_F(HeaderBuilderParsingTest, APercentSignIsAcceptedInAName)
{
    const std::string spec{"X%Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X%Y", "value")));
}

TEST_F(HeaderBuilderParsingTest, AnAmpersandIsAcceptedInAName)
{
    const std::string spec{"X&Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X&Y", "value")));
}

TEST_F(HeaderBuilderParsingTest, AnApostropheIsAcceptedInAName)
{
    const std::string spec{"X'Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X'Y", "value")));
}

TEST_F(HeaderBuilderParsingTest, AnAsteriskIsAcceptedInAName)
{
    const std::string spec{"X*Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X*Y", "value")));
}

TEST_F(HeaderBuilderParsingTest, APlusSignIsAcceptedInAName)
{
    const std::string spec{"X+Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X+Y", "value")));
}

TEST_F(HeaderBuilderParsingTest, AHyphenIsAcceptedInAName)
{
    const std::string spec{"X-Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Y", "value")));
}

TEST_F(HeaderBuilderParsingTest, APeriodIsAcceptedInAName)
{
    const std::string spec{"X.Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X.Y", "value")));
}

TEST_F(HeaderBuilderParsingTest, ACaretIsAcceptedInAName)
{
    const std::string spec{"X^Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X^Y", "value")));
}

TEST_F(HeaderBuilderParsingTest, AnUnderscoreIsAcceptedInAName)
{
    const std::string spec{"X_Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X_Y", "value")));
}

TEST_F(HeaderBuilderParsingTest, ABackquoteIsAcceptedInAName)
{
    const std::string spec{"X`Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X`Y", "value")));
}

TEST_F(HeaderBuilderParsingTest, AVerticalBarIsAcceptedInAName)
{
    const std::string spec{"X|Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X|Y", "value")));
}

TEST_F(HeaderBuilderParsingTest, ATildeIsAcceptedInAName)
{
    const std::string spec{"X~Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X~Y", "value")));
}

TEST_F(HeaderBuilderParsingTest, AnEmptySpecificationYieldsNoHeaders)
{
    const std::string spec{""};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, IsEmpty());
}

/******************************************************************************/
/*                B u i l d :  m a l f o r m e d  e n t r y                   */
/******************************************************************************/

TEST_F(HeaderBuilderParsingTest, AnEntryWithoutAColonIsRejected)
{
    const std::string spec{"nocolon"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderParsingTest, AnEntryWithoutANameIsRejected)
{
    const std::string spec{": value"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderParsingTest, AnEntryWithoutAValueIsRejected)
{
    const std::string spec{"X-Test:"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderParsingTest, AnEntryWithABlankValueIsRejected)
{
    const std::string spec{"X-Test:   "};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

// A name is an RFC 7230 token, so none of the characters below belongs in one.

TEST_F(HeaderBuilderParsingTest, ASpaceIsRejectedInAName)
{
    const std::string spec{"X Y: value"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderParsingTest, ATabIsRejectedInAName)
{
    const std::string spec{"X\tY: value"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderParsingTest, AParenthesisIsRejectedInAName)
{
    const std::string spec{"X(Y: value"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderParsingTest, AQuoteIsRejectedInAName)
{
    const std::string spec{"X\"Y: value"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

// Rejection must not leave a half-built list behind for a caller that ignores
// the return value.
TEST_F(HeaderBuilderParsingTest, RejectionYieldsNoRequestedHeaders)
{
    const std::string spec{"X-Good: value\nnocolon"};

    ASSERT_FALSE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, IsEmpty());
}
