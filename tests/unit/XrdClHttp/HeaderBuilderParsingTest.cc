#include "HeaderBuilderFixture.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

using namespace XrdClHttp;

using testing::ElementsAre;
using testing::IsEmpty;
using testing::Pair;

namespace {

class HeaderParsing : public HeaderBuilderFixture {};

} // namespace

/******************************************************************************/
/*                      B u i l d :  p a r s i n g                            */
/******************************************************************************/

TEST_F(HeaderParsing, ARequestedHeaderIsAdded)
{
    const std::string spec{"X-Test-Header: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test-Header", "value")));
}

// A user who holds a token has to be able to present it, so an Authorization
// entry is built like any other, for the near server and for the far one.

TEST_F(HeaderParsing, AnAuthorizationEntryIsAdded)
{
    const std::string spec{"Authorization: Bearer token"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("Authorization", "Bearer token")));
}

TEST_F(HeaderParsing, ATransferHeaderAuthorizationEntryIsAdded)
{
    const std::string spec{"TransferHeaderAuthorization: Bearer token"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers,
        ElementsAre(Pair("TransferHeaderAuthorization", "Bearer token")));
}

TEST_F(HeaderParsing, TheColonNeedNotBeFollowedByASpace)
{
    const std::string spec{"X-Test:value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

TEST_F(HeaderParsing, WhitespaceBeforeTheNameIsTrimmed)
{
    const std::string spec{" \t X-Test: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

TEST_F(HeaderParsing, WhitespaceAfterTheNameIsTrimmed)
{
    const std::string spec{"X-Test \t : value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

TEST_F(HeaderParsing, WhitespaceBeforeTheValueIsTrimmed)
{
    const std::string spec{"X-Test: \t value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

TEST_F(HeaderParsing, WhitespaceAfterTheValueIsTrimmed)
{
    const std::string spec{"X-Test: value \t "};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

// Only the first colon separates, so the rest of the entry is the value.
TEST_F(HeaderParsing, AValueMayContainAColon)
{
    const std::string spec{"X-Test: a:b"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "a:b")));
}

TEST_F(HeaderParsing, AValueMayContainASpace)
{
    const std::string spec{"X-Test: a b"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "a b")));
}

// A name is an RFC 7230 token, so each of the characters below belongs in one.

TEST_F(HeaderParsing, ADigitIsAcceptedInAName)
{
    const std::string spec{"X1Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X1Y", "value")));
}

TEST_F(HeaderParsing, AnExclamationMarkIsAcceptedInAName)
{
    const std::string spec{"X!Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X!Y", "value")));
}

TEST_F(HeaderParsing, AHashIsAcceptedInAName)
{
    const std::string spec{"X#Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X#Y", "value")));
}

TEST_F(HeaderParsing, ADollarSignIsAcceptedInAName)
{
    const std::string spec{"X$Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X$Y", "value")));
}

TEST_F(HeaderParsing, APercentSignIsAcceptedInAName)
{
    const std::string spec{"X%Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X%Y", "value")));
}

TEST_F(HeaderParsing, AnAmpersandIsAcceptedInAName)
{
    const std::string spec{"X&Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X&Y", "value")));
}

TEST_F(HeaderParsing, AnApostropheIsAcceptedInAName)
{
    const std::string spec{"X'Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X'Y", "value")));
}

TEST_F(HeaderParsing, AnAsteriskIsAcceptedInAName)
{
    const std::string spec{"X*Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X*Y", "value")));
}

TEST_F(HeaderParsing, APlusSignIsAcceptedInAName)
{
    const std::string spec{"X+Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X+Y", "value")));
}

TEST_F(HeaderParsing, AHyphenIsAcceptedInAName)
{
    const std::string spec{"X-Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Y", "value")));
}

TEST_F(HeaderParsing, APeriodIsAcceptedInAName)
{
    const std::string spec{"X.Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X.Y", "value")));
}

TEST_F(HeaderParsing, ACaretIsAcceptedInAName)
{
    const std::string spec{"X^Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X^Y", "value")));
}

TEST_F(HeaderParsing, AnUnderscoreIsAcceptedInAName)
{
    const std::string spec{"X_Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X_Y", "value")));
}

TEST_F(HeaderParsing, ABackquoteIsAcceptedInAName)
{
    const std::string spec{"X`Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X`Y", "value")));
}

TEST_F(HeaderParsing, AVerticalBarIsAcceptedInAName)
{
    const std::string spec{"X|Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X|Y", "value")));
}

TEST_F(HeaderParsing, ATildeIsAcceptedInAName)
{
    const std::string spec{"X~Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X~Y", "value")));
}

TEST_F(HeaderParsing, AnEmptySpecificationYieldsNoHeaders)
{
    const std::string spec{""};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, IsEmpty());
}

/******************************************************************************/
/*                B u i l d :  m a l f o r m e d  e n t r y                   */
/******************************************************************************/

TEST_F(HeaderParsing, AnEntryWithoutAColonIsRejected)
{
    const std::string spec{"nocolon"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderParsing, AnEntryWithoutANameIsRejected)
{
    const std::string spec{": value"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderParsing, AnEntryWithoutAValueIsRejected)
{
    const std::string spec{"X-Test:"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderParsing, AnEntryWithABlankValueIsRejected)
{
    const std::string spec{"X-Test:   "};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

// A name is an RFC 7230 token, so none of the characters below belongs in one.

TEST_F(HeaderParsing, ASpaceIsRejectedInAName)
{
    const std::string spec{"X Y: value"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderParsing, ATabIsRejectedInAName)
{
    const std::string spec{"X\tY: value"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderParsing, AParenthesisIsRejectedInAName)
{
    const std::string spec{"X(Y: value"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderParsing, AQuoteIsRejectedInAName)
{
    const std::string spec{"X\"Y: value"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

// Rejection must not leave a half-built list behind for a caller that ignores
// the return value.
TEST_F(HeaderParsing, RejectionYieldsNoRequestedHeaders)
{
    const std::string spec{"X-Good: value\nnocolon"};

    ASSERT_FALSE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, IsEmpty());
}
