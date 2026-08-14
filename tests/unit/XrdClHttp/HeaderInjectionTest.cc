#include "XrdClHttp/XrdClHttpHeaderBuilder.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

using namespace XrdClHttp;

using testing::Contains;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Pair;
using testing::SizeIs;

namespace {

// Each test states the headers it asks for the way xrdcp does for its --header
// option: a newline separated specification, handed to the builder verbatim.
class HeaderFixture : public testing::Test {
protected:
    HeaderBuilder::HeaderList headers;
};

} // namespace

/******************************************************************************/
/*                     I s F o r b i d d e n H e a d e r                      */
/******************************************************************************/

TEST(IsForbiddenHeader, ConnectionIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("connection"));
}

TEST(IsForbiddenHeader, ConnectionIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("Connection"));
}

TEST(IsForbiddenHeader, ContentLengthIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("content-length"));
}

TEST(IsForbiddenHeader, ContentLengthIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("Content-Length"));
}

TEST(IsForbiddenHeader, ExpectIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("expect"));
}

TEST(IsForbiddenHeader, ExpectIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("Expect"));
}

TEST(IsForbiddenHeader, HostIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("host"));
}

TEST(IsForbiddenHeader, HostIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("Host"));
}

TEST(IsForbiddenHeader, KeepAliveIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("keep-alive"));
}

TEST(IsForbiddenHeader, KeepAliveIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("Keep-Alive"));
}

TEST(IsForbiddenHeader, ProxyAuthenticateIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("proxy-authenticate"));
}

TEST(IsForbiddenHeader, ProxyAuthenticateIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("Proxy-Authenticate"));
}

TEST(IsForbiddenHeader, ProxyAuthorizationIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("proxy-authorization"));
}

TEST(IsForbiddenHeader, ProxyAuthorizationIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("Proxy-Authorization"));
}

TEST(IsForbiddenHeader, ProxyConnectionIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("proxy-connection"));
}

TEST(IsForbiddenHeader, ProxyConnectionIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("Proxy-Connection"));
}

TEST(IsForbiddenHeader, TeIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("te"));
}

TEST(IsForbiddenHeader, TeIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TE"));
}

TEST(IsForbiddenHeader, TrailerIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("trailer"));
}

TEST(IsForbiddenHeader, TrailerIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("Trailer"));
}

TEST(IsForbiddenHeader, TransferEncodingIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("transfer-encoding"));
}

TEST(IsForbiddenHeader, TransferEncodingIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("Transfer-Encoding"));
}

TEST(IsForbiddenHeader, UpgradeIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("upgrade"));
}

TEST(IsForbiddenHeader, UpgradeIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("Upgrade"));
}

TEST(IsForbiddenHeader, ACustomHeaderIsAllowed)
{
    EXPECT_FALSE(HeaderBuilder::IsForbiddenHeader("X-Test-Header"));
}

TEST(IsForbiddenHeader, AuthorizationIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("authorization"));
}

TEST(IsForbiddenHeader, AuthorizationIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("Authorization"));
}

// The TransferHeader prefix aims a header at the far server of a third party
// copy, and so must not let a forbidden header through.

TEST(IsForbiddenHeader, TransferHeaderAuthorizationIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("transferheaderauthorization"));
}

TEST(IsForbiddenHeader, TransferHeaderAuthorizationIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderAuthorization"));
}

TEST(IsForbiddenHeader, TransferHeaderConnectionIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderConnection"));
}

TEST(IsForbiddenHeader, TransferHeaderContentLengthIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderContent-Length"));
}

TEST(IsForbiddenHeader, TransferHeaderExpectIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderExpect"));
}

TEST(IsForbiddenHeader, TransferHeaderHostIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderHost"));
}

TEST(IsForbiddenHeader, TransferHeaderKeepAliveIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderKeep-Alive"));
}

TEST(IsForbiddenHeader, TransferHeaderProxyAuthenticateIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderProxy-Authenticate"));
}

TEST(IsForbiddenHeader, TransferHeaderProxyAuthorizationIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderProxy-Authorization"));
}

TEST(IsForbiddenHeader, TransferHeaderProxyConnectionIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderProxy-Connection"));
}

TEST(IsForbiddenHeader, TransferHeaderTeIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderTE"));
}

TEST(IsForbiddenHeader, TransferHeaderTrailerIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderTrailer"));
}

TEST(IsForbiddenHeader, TransferHeaderTransferEncodingIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderTransfer-Encoding"));
}

TEST(IsForbiddenHeader, TransferHeaderUpgradeIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderUpgrade"));
}

TEST(IsForbiddenHeader, ARepeatedTransferHeaderPrefixOnAForbiddenNameIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderTransferHeaderHost"));
}

TEST(IsForbiddenHeader, TransferHeaderOnACustomHeaderIsAllowed)
{
    EXPECT_FALSE(HeaderBuilder::IsForbiddenHeader("TransferHeaderX-Test-Header"));
}

TEST(IsForbiddenHeader, TheTransferHeaderPrefixAloneIsAllowed)
{
    EXPECT_FALSE(HeaderBuilder::IsForbiddenHeader("TransferHeader"));
}

TEST(IsForbiddenHeader, RangeIsAllowed)
{
    EXPECT_FALSE(HeaderBuilder::IsForbiddenHeader("Range"));
}

TEST(IsForbiddenHeader, WantDigestIsAllowed)
{
    EXPECT_FALSE(HeaderBuilder::IsForbiddenHeader("Want-Digest"));
}

// The match must be on the whole name; a header that merely contains a
// forbidden one is perfectly legitimate.

TEST(IsForbiddenHeader, ANameEndingInAForbiddenOneIsAllowed)
{
    EXPECT_FALSE(HeaderBuilder::IsForbiddenHeader("X-Host"));
}

TEST(IsForbiddenHeader, ANameStartingWithAForbiddenOneIsAllowed)
{
    EXPECT_FALSE(HeaderBuilder::IsForbiddenHeader("Hostname"));
}

TEST(IsForbiddenHeader, ANameExtendingAForbiddenOneIsAllowed)
{
    EXPECT_FALSE(HeaderBuilder::IsForbiddenHeader("Content-Length-Hint"));
}

TEST(IsForbiddenHeader, ANamePrefixingAForbiddenOneIsAllowed)
{
    EXPECT_FALSE(HeaderBuilder::IsForbiddenHeader("Not-Connection"));
}

TEST(IsForbiddenHeader, AnEmptyNameIsAllowed)
{
    EXPECT_FALSE(HeaderBuilder::IsForbiddenHeader(""));
}

/******************************************************************************/
/*                      B u i l d :  p a r s i n g                           */
/******************************************************************************/

TEST_F(HeaderFixture, ARequestedHeaderIsAdded)
{
    const std::string spec{"X-Test-Header: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test-Header", "value")));
}

TEST_F(HeaderFixture, TheFirstOfSeveralEntriesIsAdded)
{
    const std::string spec{"X-First: one\nX-Second: two\nX-Third: three"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, Contains(Pair("X-First", "one")));
}

TEST_F(HeaderFixture, AMiddleEntryIsAdded)
{
    const std::string spec{"X-First: one\nX-Second: two\nX-Third: three"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, Contains(Pair("X-Second", "two")));
}

TEST_F(HeaderFixture, TheLastOfSeveralEntriesIsAdded)
{
    const std::string spec{"X-First: one\nX-Second: two\nX-Third: three"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, Contains(Pair("X-Third", "three")));
}

TEST_F(HeaderFixture, TheColonNeedNotBeFollowedByASpace)
{
    const std::string spec{"X-Test:value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

TEST_F(HeaderFixture, WhitespaceBeforeTheNameIsTrimmed)
{
    const std::string spec{" \t X-Test: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

TEST_F(HeaderFixture, WhitespaceAfterTheNameIsTrimmed)
{
    const std::string spec{"X-Test \t : value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

TEST_F(HeaderFixture, WhitespaceBeforeTheValueIsTrimmed)
{
    const std::string spec{"X-Test: \t value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

TEST_F(HeaderFixture, WhitespaceAfterTheValueIsTrimmed)
{
    const std::string spec{"X-Test: value \t "};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

// Only the first colon separates, so the rest of the entry is the value.
TEST_F(HeaderFixture, AValueMayContainAColon)
{
    const std::string spec{"X-Test: a:b"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "a:b")));
}

TEST_F(HeaderFixture, AValueMayContainASpace)
{
    const std::string spec{"X-Test: a b"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "a b")));
}

// A name is an RFC 7230 token, so each of the characters below belongs in one.

TEST_F(HeaderFixture, ADigitIsAcceptedInAName)
{
    const std::string spec{"X1Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X1Y", "value")));
}

TEST_F(HeaderFixture, AnExclamationMarkIsAcceptedInAName)
{
    const std::string spec{"X!Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X!Y", "value")));
}

TEST_F(HeaderFixture, AHashIsAcceptedInAName)
{
    const std::string spec{"X#Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X#Y", "value")));
}

TEST_F(HeaderFixture, ADollarSignIsAcceptedInAName)
{
    const std::string spec{"X$Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X$Y", "value")));
}

TEST_F(HeaderFixture, APercentSignIsAcceptedInAName)
{
    const std::string spec{"X%Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X%Y", "value")));
}

TEST_F(HeaderFixture, AnAmpersandIsAcceptedInAName)
{
    const std::string spec{"X&Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X&Y", "value")));
}

TEST_F(HeaderFixture, AnApostropheIsAcceptedInAName)
{
    const std::string spec{"X'Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X'Y", "value")));
}

TEST_F(HeaderFixture, AnAsteriskIsAcceptedInAName)
{
    const std::string spec{"X*Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X*Y", "value")));
}

TEST_F(HeaderFixture, APlusSignIsAcceptedInAName)
{
    const std::string spec{"X+Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X+Y", "value")));
}

TEST_F(HeaderFixture, AHyphenIsAcceptedInAName)
{
    const std::string spec{"X-Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Y", "value")));
}

TEST_F(HeaderFixture, APeriodIsAcceptedInAName)
{
    const std::string spec{"X.Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X.Y", "value")));
}

TEST_F(HeaderFixture, ACaretIsAcceptedInAName)
{
    const std::string spec{"X^Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X^Y", "value")));
}

TEST_F(HeaderFixture, AnUnderscoreIsAcceptedInAName)
{
    const std::string spec{"X_Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X_Y", "value")));
}

TEST_F(HeaderFixture, ABackquoteIsAcceptedInAName)
{
    const std::string spec{"X`Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X`Y", "value")));
}

TEST_F(HeaderFixture, AVerticalBarIsAcceptedInAName)
{
    const std::string spec{"X|Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X|Y", "value")));
}

TEST_F(HeaderFixture, ATildeIsAcceptedInAName)
{
    const std::string spec{"X~Y: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X~Y", "value")));
}

// Blank entries are tolerated so that padding a specification is not an error.

TEST_F(HeaderFixture, ALeadingBlankEntryIsSkipped)
{
    const std::string spec{"\nX-Test: value"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

TEST_F(HeaderFixture, AnEmbeddedBlankEntryIsSkipped)
{
    const std::string spec{"X-First: one\n\nX-Second: two"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-First", "one"), Pair("X-Second", "two")));
}

TEST_F(HeaderFixture, ATrailingBlankEntryIsSkipped)
{
    const std::string spec{"X-Test: value\n"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

TEST_F(HeaderFixture, AnEmptySpecificationYieldsNoHeaders)
{
    const std::string spec{""};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, IsEmpty());
}

/******************************************************************************/
/*                    B u i l d :  r e j e c t i o n                          */
/******************************************************************************/

TEST_F(HeaderFixture, AnEntryWithoutAColonIsRejected)
{
    const std::string spec{"nocolon"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AnEntryWithoutANameIsRejected)
{
    const std::string spec{": value"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AnEntryWithoutAValueIsRejected)
{
    const std::string spec{"X-Test:"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AnEntryWithABlankValueIsRejected)
{
    const std::string spec{"X-Test:   "};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

// A name is an RFC 7230 token, so none of the characters below belongs in one.

TEST_F(HeaderFixture, ASpaceIsRejectedInAName)
{
    const std::string spec{"X Y: value"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, ATabIsRejectedInAName)
{
    const std::string spec{"X\tY: value"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AParenthesisIsRejectedInAName)
{
    const std::string spec{"X(Y: value"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AQuoteIsRejectedInAName)
{
    const std::string spec{"X\"Y: value"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

// A newline separates entries, so it cannot appear inside a value; a bare
// carriage return could otherwise forge part of the request.
TEST_F(HeaderFixture, ACarriageReturnInAValueIsRejected)
{
    const std::string spec{"X-Test: a\rEvil: b"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

// A CRLF is the wire form of the separator, so it yields two entries.

TEST_F(HeaderFixture, ACrlfEndsTheEntryBeforeIt)
{
    const std::string spec{"X-Test: a\r\nX-Other: b"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, Contains(Pair("X-Test", "a")));
}

TEST_F(HeaderFixture, ACrlfStartsTheEntryAfterIt)
{
    const std::string spec{"X-Test: a\r\nX-Other: b"};

    ASSERT_TRUE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, Contains(Pair("X-Other", "b")));
}

// Each entry a CRLF yields is validated in its own right, which is what stops a
// reserved header sneaking in behind one.
TEST_F(HeaderFixture, AForbiddenEntryAfterACrlfIsRejected)
{
    const std::string spec{"X-Test: a\r\nHost: evil"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AnAuthorizationEntryIsRejected)
{
    const std::string spec{"authorization: Bearer token"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AnAuthorizationEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Authorization: Bearer token"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, ATransferHeaderAuthorizationEntryIsRejected)
{
    const std::string spec{"transferheaderauthorization: Bearer token"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, ATransferHeaderAuthorizationEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"TransferHeaderAuthorization: Bearer token"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AConnectionEntryIsRejected)
{
    const std::string spec{"connection: close"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AConnectionEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Connection: close"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AContentLengthEntryIsRejected)
{
    const std::string spec{"content-length: 0"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AContentLengthEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Content-Length: 0"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AnExpectEntryIsRejected)
{
    const std::string spec{"expect: 100-continue"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AnExpectEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Expect: 100-continue"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AHostEntryIsRejected)
{
    const std::string spec{"host: evil.example.com"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AHostEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Host: evil.example.com"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AKeepAliveEntryIsRejected)
{
    const std::string spec{"keep-alive: timeout=5"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AKeepAliveEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Keep-Alive: timeout=5"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AProxyAuthenticateEntryIsRejected)
{
    const std::string spec{"proxy-authenticate: Basic"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AProxyAuthenticateEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Proxy-Authenticate: Basic"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AProxyAuthorizationEntryIsRejected)
{
    const std::string spec{"proxy-authorization: Basic dXNlcg=="};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AProxyAuthorizationEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Proxy-Authorization: Basic dXNlcg=="};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AProxyConnectionEntryIsRejected)
{
    const std::string spec{"proxy-connection: close"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AProxyConnectionEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Proxy-Connection: close"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, ATeEntryIsRejected)
{
    const std::string spec{"te: trailers"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, ATeEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"TE: trailers"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, ATrailerEntryIsRejected)
{
    const std::string spec{"trailer: Expires"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, ATrailerEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Trailer: Expires"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, ATransferEncodingEntryIsRejected)
{
    const std::string spec{"transfer-encoding: chunked"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, ATransferEncodingEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Transfer-Encoding: chunked"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AnUpgradeEntryIsRejected)
{
    const std::string spec{"upgrade: websocket"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderFixture, AnUpgradeEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Upgrade: websocket"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

// A forbidden entry must reject the whole request rather than silently dropping
// the offending header and sending the rest.
TEST_F(HeaderFixture, AForbiddenEntryRejectsTheWholeRequest)
{
    const std::string spec{"X-Good: value\nHost: evil.example.com\nX-Also-Good: value"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

// Rejection must not leave a half-built list behind for a caller that ignores
// the return value.
TEST_F(HeaderFixture, RejectionYieldsNoRequestedHeaders)
{
    const std::string spec{"X-Good: value\nnocolon"};

    ASSERT_FALSE(HeaderBuilder::Build(spec, headers));
    EXPECT_THAT(headers, IsEmpty());
}

/******************************************************************************/
/*                          A p p e n d M i s s i n g                         */
/******************************************************************************/

// A requested header must never displace one the request set for itself; a
// requested Range would otherwise corrupt a read.
TEST_F(HeaderFixture, ARequestHeaderIsNotDisplaced)
{
    headers.emplace_back("Range", "bytes=0-1023");

    HeaderBuilder::AppendMissing({{"Range", "bytes=999-"}}, headers);
    EXPECT_THAT(headers, Contains(Pair("Range", "bytes=0-1023")));
}

// Only one Range must be present, or curl would send both.
TEST_F(HeaderFixture, ARequestHeaderIsNotDuplicated)
{
    headers.emplace_back("Range", "bytes=0-1023");

    HeaderBuilder::AppendMissing({{"Range", "bytes=999-"}}, headers);
    EXPECT_THAT(headers, SizeIs(1));
}

// Dropping the duplicate must not cost the other entries their place.
TEST_F(HeaderFixture, ADroppedDuplicateStillLeavesTheOtherEntries)
{
    headers.emplace_back("Range", "bytes=0-1023");

    HeaderBuilder::AppendMissing({{"Range", "bytes=999-"}, {"X-Test-Header", "value"}}, headers);
    EXPECT_THAT(headers, Contains(Pair("X-Test-Header", "value")));
}

TEST_F(HeaderFixture, ADuplicateDifferingInCaseIsNotAdded)
{
    headers.emplace_back("X-Test", "original");

    HeaderBuilder::AppendMissing({{"x-TEST", "replacement"}}, headers);
    EXPECT_THAT(headers, SizeIs(1));
}

TEST_F(HeaderFixture, ADuplicateDifferingInCaseKeepsTheRequestValue)
{
    headers.emplace_back("X-Test", "original");

    HeaderBuilder::AppendMissing({{"x-TEST", "replacement"}}, headers);
    EXPECT_THAT(headers, Contains(Pair("X-Test", "original")));
}

TEST_F(HeaderFixture, AnUnrelatedNameIsAppended)
{
    headers.emplace_back("Range", "bytes=0-1023");

    HeaderBuilder::AppendMissing({{"X-Test-Header", "value"}}, headers);
    EXPECT_THAT(headers, ElementsAre(Pair("Range", "bytes=0-1023"),
                                     Pair("X-Test-Header", "value")));
}

TEST_F(HeaderFixture, NothingToAppendLeavesTheHeadersUnchanged)
{
    headers.emplace_back("Range", "bytes=0-1023");

    HeaderBuilder::AppendMissing({}, headers);
    EXPECT_THAT(headers, ElementsAre(Pair("Range", "bytes=0-1023")));
}
