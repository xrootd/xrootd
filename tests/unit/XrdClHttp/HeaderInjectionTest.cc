#include "XrdClHttp/XrdClHttpOps.hh"

#include <XrdCl/XrdClDefaultEnv.hh>
#include <XrdCl/XrdClEnv.hh>
#include <XrdCl/XrdClLog.hh>

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

// A minimal concrete operation; BuildRequestHeaders only needs the URL, the
// logger and the operation's own header list.
class TestOp final : public CurlOperation {
public:
    TestOp(const std::string &url)
        : CurlOperation(nullptr, url, {10, 0}, XrdCl::DefaultEnv::GetLog(), nullptr, nullptr)
    {}

    void Success() override {}
    HttpVerb GetVerb() const override {return HttpVerb::GET;}

    // Add a header the operation itself requires, as CurlReadOp does for Range.
    void AddOpHeader(const std::string &name, const std::string &value) {
        m_headers_list.emplace_back(name, value);
    }
};

// Each test asks for its headers through the HttpHeaders setting, the way xrdcp
// does for its --header option, and builds them with `op`, whose URL is the
// endpoint used wherever the test does not care which one it is.
class HeaderFixture : public testing::Test {
protected:
    // The endpoint binding lives for the process, so each test has to start from
    // an unbound state.  Clearing the request and building once releases it.
    void SetUp() override {
        XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "");
        op.BuildRequestHeaders(headers);
    }

    TestOp                    op{"https://example.com/path"};
    CurlOperation::HeaderList headers;
};

} // namespace

/******************************************************************************/
/*                     I s F o r b i d d e n H e a d e r                      */
/******************************************************************************/

TEST(IsForbiddenHeader, ConnectionIsForbidden)
{
    EXPECT_TRUE(CurlOperation::IsForbiddenHeader("connection"));
}

TEST(IsForbiddenHeader, ConnectionIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(CurlOperation::IsForbiddenHeader("Connection"));
}

TEST(IsForbiddenHeader, ContentLengthIsForbidden)
{
    EXPECT_TRUE(CurlOperation::IsForbiddenHeader("content-length"));
}

TEST(IsForbiddenHeader, ContentLengthIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(CurlOperation::IsForbiddenHeader("Content-Length"));
}

TEST(IsForbiddenHeader, ExpectIsForbidden)
{
    EXPECT_TRUE(CurlOperation::IsForbiddenHeader("expect"));
}

TEST(IsForbiddenHeader, ExpectIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(CurlOperation::IsForbiddenHeader("Expect"));
}

TEST(IsForbiddenHeader, HostIsForbidden)
{
    EXPECT_TRUE(CurlOperation::IsForbiddenHeader("host"));
}

TEST(IsForbiddenHeader, HostIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(CurlOperation::IsForbiddenHeader("Host"));
}

TEST(IsForbiddenHeader, KeepAliveIsForbidden)
{
    EXPECT_TRUE(CurlOperation::IsForbiddenHeader("keep-alive"));
}

TEST(IsForbiddenHeader, KeepAliveIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(CurlOperation::IsForbiddenHeader("Keep-Alive"));
}

TEST(IsForbiddenHeader, ProxyAuthenticateIsForbidden)
{
    EXPECT_TRUE(CurlOperation::IsForbiddenHeader("proxy-authenticate"));
}

TEST(IsForbiddenHeader, ProxyAuthenticateIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(CurlOperation::IsForbiddenHeader("Proxy-Authenticate"));
}

TEST(IsForbiddenHeader, ProxyAuthorizationIsForbidden)
{
    EXPECT_TRUE(CurlOperation::IsForbiddenHeader("proxy-authorization"));
}

TEST(IsForbiddenHeader, ProxyAuthorizationIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(CurlOperation::IsForbiddenHeader("Proxy-Authorization"));
}

TEST(IsForbiddenHeader, ProxyConnectionIsForbidden)
{
    EXPECT_TRUE(CurlOperation::IsForbiddenHeader("proxy-connection"));
}

TEST(IsForbiddenHeader, ProxyConnectionIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(CurlOperation::IsForbiddenHeader("Proxy-Connection"));
}

TEST(IsForbiddenHeader, TeIsForbidden)
{
    EXPECT_TRUE(CurlOperation::IsForbiddenHeader("te"));
}

TEST(IsForbiddenHeader, TeIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(CurlOperation::IsForbiddenHeader("TE"));
}

TEST(IsForbiddenHeader, TrailerIsForbidden)
{
    EXPECT_TRUE(CurlOperation::IsForbiddenHeader("trailer"));
}

TEST(IsForbiddenHeader, TrailerIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(CurlOperation::IsForbiddenHeader("Trailer"));
}

TEST(IsForbiddenHeader, TransferEncodingIsForbidden)
{
    EXPECT_TRUE(CurlOperation::IsForbiddenHeader("transfer-encoding"));
}

TEST(IsForbiddenHeader, TransferEncodingIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(CurlOperation::IsForbiddenHeader("Transfer-Encoding"));
}

TEST(IsForbiddenHeader, UpgradeIsForbidden)
{
    EXPECT_TRUE(CurlOperation::IsForbiddenHeader("upgrade"));
}

TEST(IsForbiddenHeader, UpgradeIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(CurlOperation::IsForbiddenHeader("Upgrade"));
}

TEST(IsForbiddenHeader, ACustomHeaderIsAllowed)
{
    EXPECT_FALSE(CurlOperation::IsForbiddenHeader("X-Test-Header"));
}

TEST(IsForbiddenHeader, AuthorizationIsAllowed)
{
    EXPECT_FALSE(CurlOperation::IsForbiddenHeader("Authorization"));
}

TEST(IsForbiddenHeader, RangeIsAllowed)
{
    EXPECT_FALSE(CurlOperation::IsForbiddenHeader("Range"));
}

TEST(IsForbiddenHeader, WantDigestIsAllowed)
{
    EXPECT_FALSE(CurlOperation::IsForbiddenHeader("Want-Digest"));
}

// The match must be on the whole name; a header that merely contains a
// forbidden one is perfectly legitimate.

TEST(IsForbiddenHeader, ANameEndingInAForbiddenOneIsAllowed)
{
    EXPECT_FALSE(CurlOperation::IsForbiddenHeader("X-Host"));
}

TEST(IsForbiddenHeader, ANameStartingWithAForbiddenOneIsAllowed)
{
    EXPECT_FALSE(CurlOperation::IsForbiddenHeader("Hostname"));
}

TEST(IsForbiddenHeader, ANameExtendingAForbiddenOneIsAllowed)
{
    EXPECT_FALSE(CurlOperation::IsForbiddenHeader("Content-Length-Hint"));
}

TEST(IsForbiddenHeader, ANamePrefixingAForbiddenOneIsAllowed)
{
    EXPECT_FALSE(CurlOperation::IsForbiddenHeader("Not-Connection"));
}

TEST(IsForbiddenHeader, AnEmptyNameIsAllowed)
{
    EXPECT_FALSE(CurlOperation::IsForbiddenHeader(""));
}

/******************************************************************************/
/*             B u i l d R e q u e s t H e a d e r s :  p a r s i n g         */
/******************************************************************************/

TEST_F(HeaderFixture, ARequestedHeaderIsAdded)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Test-Header: value");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test-Header", "value")));
}

TEST_F(HeaderFixture, TheFirstOfSeveralEntriesIsAdded)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders",
                                           "X-First: one\nX-Second: two\nX-Third: three");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, Contains(Pair("X-First", "one")));
}

TEST_F(HeaderFixture, AMiddleEntryIsAdded)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders",
                                           "X-First: one\nX-Second: two\nX-Third: three");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, Contains(Pair("X-Second", "two")));
}

TEST_F(HeaderFixture, TheLastOfSeveralEntriesIsAdded)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders",
                                           "X-First: one\nX-Second: two\nX-Third: three");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, Contains(Pair("X-Third", "three")));
}

TEST_F(HeaderFixture, TheColonNeedNotBeFollowedByASpace)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Test:value");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

TEST_F(HeaderFixture, WhitespaceBeforeTheNameIsTrimmed)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", " \t X-Test: value");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

TEST_F(HeaderFixture, WhitespaceAfterTheNameIsTrimmed)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Test \t : value");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

TEST_F(HeaderFixture, WhitespaceBeforeTheValueIsTrimmed)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Test: \t value");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

TEST_F(HeaderFixture, WhitespaceAfterTheValueIsTrimmed)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Test: value \t ");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

// Only the first colon separates, so the rest of the entry is the value.
TEST_F(HeaderFixture, AValueMayContainAColon)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Test: a:b");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "a:b")));
}

TEST_F(HeaderFixture, AValueMayContainASpace)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Test: a b");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "a b")));
}

// A name is an RFC 7230 token, so each of the characters below belongs in one.

TEST_F(HeaderFixture, ADigitIsAcceptedInAName)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X1Y: value");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X1Y", "value")));
}

TEST_F(HeaderFixture, AnExclamationMarkIsAcceptedInAName)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X!Y: value");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X!Y", "value")));
}

TEST_F(HeaderFixture, AHashIsAcceptedInAName)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X#Y: value");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X#Y", "value")));
}

TEST_F(HeaderFixture, ADollarSignIsAcceptedInAName)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X$Y: value");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X$Y", "value")));
}

TEST_F(HeaderFixture, APercentSignIsAcceptedInAName)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X%Y: value");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X%Y", "value")));
}

TEST_F(HeaderFixture, AnAmpersandIsAcceptedInAName)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X&Y: value");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X&Y", "value")));
}

TEST_F(HeaderFixture, AnApostropheIsAcceptedInAName)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X'Y: value");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X'Y", "value")));
}

TEST_F(HeaderFixture, AnAsteriskIsAcceptedInAName)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X*Y: value");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X*Y", "value")));
}

TEST_F(HeaderFixture, APlusSignIsAcceptedInAName)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X+Y: value");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X+Y", "value")));
}

TEST_F(HeaderFixture, AHyphenIsAcceptedInAName)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Y: value");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Y", "value")));
}

TEST_F(HeaderFixture, APeriodIsAcceptedInAName)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X.Y: value");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X.Y", "value")));
}

TEST_F(HeaderFixture, ACaretIsAcceptedInAName)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X^Y: value");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X^Y", "value")));
}

TEST_F(HeaderFixture, AnUnderscoreIsAcceptedInAName)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X_Y: value");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X_Y", "value")));
}

TEST_F(HeaderFixture, ABackquoteIsAcceptedInAName)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X`Y: value");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X`Y", "value")));
}

TEST_F(HeaderFixture, AVerticalBarIsAcceptedInAName)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X|Y: value");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X|Y", "value")));
}

TEST_F(HeaderFixture, ATildeIsAcceptedInAName)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X~Y: value");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X~Y", "value")));
}

// Blank entries are tolerated so that padding a specification is not an error.

TEST_F(HeaderFixture, ALeadingBlankEntryIsSkipped)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "\nX-Test: value");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

TEST_F(HeaderFixture, AnEmbeddedBlankEntryIsSkipped)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-First: one\n\nX-Second: two");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-First", "one"), Pair("X-Second", "two")));
}

TEST_F(HeaderFixture, ATrailingBlankEntryIsSkipped)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Test: value\n");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test", "value")));
}

// Nothing requested must leave the operation's own headers exactly as they were.
TEST_F(HeaderFixture, NothingRequestedLeavesTheOperationUnchanged)
{
    op.AddOpHeader("Range", "bytes=0-1023");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("Range", "bytes=0-1023")));
}

/******************************************************************************/
/*           B u i l d R e q u e s t H e a d e r s :  r e j e c t i o n       */
/******************************************************************************/

TEST_F(HeaderFixture, AnEntryWithoutAColonIsRejected)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "nocolon");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, AnEntryWithoutANameIsRejected)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", ": value");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, AnEntryWithoutAValueIsRejected)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Test:");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, AnEntryWithABlankValueIsRejected)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Test:   ");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

// A name is an RFC 7230 token, so none of the characters below belongs in one.

TEST_F(HeaderFixture, ASpaceIsRejectedInAName)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X Y: value");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, ATabIsRejectedInAName)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X\tY: value");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, AParenthesisIsRejectedInAName)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X(Y: value");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, AQuoteIsRejectedInAName)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X\"Y: value");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

// A newline separates entries, so it cannot appear inside a value; a bare
// carriage return could otherwise forge part of the request.
TEST_F(HeaderFixture, ACarriageReturnInAValueIsRejected)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Test: a\rEvil: b");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

// A CRLF is the wire form of the separator, so it yields two entries.

TEST_F(HeaderFixture, ACrlfEndsTheEntryBeforeIt)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Test: a\r\nX-Other: b");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, Contains(Pair("X-Test", "a")));
}

TEST_F(HeaderFixture, ACrlfStartsTheEntryAfterIt)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Test: a\r\nX-Other: b");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, Contains(Pair("X-Other", "b")));
}

// Each entry a CRLF yields is validated in its own right, which is what stops a
// reserved header sneaking in behind one.
TEST_F(HeaderFixture, AForbiddenEntryAfterACrlfIsRejected)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Test: a\r\nHost: evil");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, AConnectionEntryIsRejected)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "connection: close");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, AConnectionEntryIsRejectedWhenCapitalized)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "Connection: close");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, AContentLengthEntryIsRejected)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "content-length: 0");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, AContentLengthEntryIsRejectedWhenCapitalized)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "Content-Length: 0");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, AnExpectEntryIsRejected)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "expect: 100-continue");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, AnExpectEntryIsRejectedWhenCapitalized)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "Expect: 100-continue");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, AHostEntryIsRejected)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "host: evil.example.com");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, AHostEntryIsRejectedWhenCapitalized)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "Host: evil.example.com");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, AKeepAliveEntryIsRejected)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "keep-alive: timeout=5");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, AKeepAliveEntryIsRejectedWhenCapitalized)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "Keep-Alive: timeout=5");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, AProxyAuthenticateEntryIsRejected)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "proxy-authenticate: Basic");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, AProxyAuthenticateEntryIsRejectedWhenCapitalized)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "Proxy-Authenticate: Basic");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, AProxyAuthorizationEntryIsRejected)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "proxy-authorization: Basic dXNlcg==");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, AProxyAuthorizationEntryIsRejectedWhenCapitalized)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "Proxy-Authorization: Basic dXNlcg==");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, AProxyConnectionEntryIsRejected)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "proxy-connection: close");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, AProxyConnectionEntryIsRejectedWhenCapitalized)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "Proxy-Connection: close");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, ATeEntryIsRejected)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "te: trailers");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, ATeEntryIsRejectedWhenCapitalized)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "TE: trailers");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, ATrailerEntryIsRejected)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "trailer: Expires");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, ATrailerEntryIsRejectedWhenCapitalized)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "Trailer: Expires");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, ATransferEncodingEntryIsRejected)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "transfer-encoding: chunked");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, ATransferEncodingEntryIsRejectedWhenCapitalized)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "Transfer-Encoding: chunked");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, AnUpgradeEntryIsRejected)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "upgrade: websocket");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

TEST_F(HeaderFixture, AnUpgradeEntryIsRejectedWhenCapitalized)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "Upgrade: websocket");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

// A forbidden entry must reject the whole request rather than silently dropping
// the offending header and sending the rest.
TEST_F(HeaderFixture, AForbiddenEntryRejectsTheWholeRequest)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders",
                                           "X-Good: value\nHost: evil.example.com\nX-Also-Good: value");

    EXPECT_FALSE(op.BuildRequestHeaders(headers));
}

// Rejection must not leave a half-built list behind for a caller that ignores
// the return value.
TEST_F(HeaderFixture, RejectionYieldsNoRequestedHeaders)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Good: value\nnocolon");

    ASSERT_FALSE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, IsEmpty());
}

/******************************************************************************/
/*           B u i l d R e q u e s t H e a d e r s :  a p p e n d i n g       */
/******************************************************************************/

// A requested header must never displace one the operation set for itself; a
// requested Range would otherwise corrupt a read.
TEST_F(HeaderFixture, AnOperationHeaderIsNotDisplaced)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "Range: bytes=999-");
    op.AddOpHeader("Range", "bytes=0-1023");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, Contains(Pair("Range", "bytes=0-1023")));
}

// Only one Range must be present, or curl would send both.
TEST_F(HeaderFixture, AnOperationHeaderIsNotDuplicated)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "Range: bytes=999-");
    op.AddOpHeader("Range", "bytes=0-1023");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, SizeIs(1));
}

// Dropping the duplicate must not cost the other entries their place.
TEST_F(HeaderFixture, ADroppedDuplicateStillLeavesTheOtherEntries)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders",
                                           "Range: bytes=999-\nX-Test-Header: value");
    op.AddOpHeader("Range", "bytes=0-1023");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, Contains(Pair("X-Test-Header", "value")));
}

TEST_F(HeaderFixture, ADuplicateDifferingInCaseIsNotAdded)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "x-TEST: replacement");
    op.AddOpHeader("X-Test", "original");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, SizeIs(1));
}

TEST_F(HeaderFixture, ADuplicateDifferingInCaseKeepsTheOperationValue)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "x-TEST: replacement");
    op.AddOpHeader("X-Test", "original");

    ASSERT_TRUE(op.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, Contains(Pair("X-Test", "original")));
}

/******************************************************************************/
/*            B u i l d R e q u e s t H e a d e r s :  b i n d i n g          */
/******************************************************************************/

// The headers bind to the first endpoint contacted, `op`'s, so that a credential
// meant for one host is not handed to another.
TEST_F(HeaderFixture, AnotherEndpointDoesNotGetTheHeaders)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Test-Header: value");
    ASSERT_TRUE(op.BuildRequestHeaders(headers));

    TestOp other("https://other.example.com/path");
    ASSERT_TRUE(other.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, IsEmpty());
}

TEST_F(HeaderFixture, TheBoundEndpointKeepsGettingTheHeaders)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Test-Header: value");
    ASSERT_TRUE(op.BuildRequestHeaders(headers));

    TestOp again("https://example.com/another");
    ASSERT_TRUE(again.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test-Header", "value")));
}

// The binding is per host and port, so the same host on another port is a
// different endpoint.
TEST_F(HeaderFixture, AnotherPortDoesNotGetTheHeaders)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Test-Header: value");
    ASSERT_TRUE(op.BuildRequestHeaders(headers));

    TestOp other("https://example.com:9443/path");
    ASSERT_TRUE(other.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, IsEmpty());
}

// The default port is implied, so these two URLs are the same endpoint.
TEST_F(HeaderFixture, TheDefaultPortIsImplied)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Test-Header: value");
    ASSERT_TRUE(op.BuildRequestHeaders(headers));

    TestOp explicitPort("https://example.com:443/path");
    ASSERT_TRUE(explicitPort.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test-Header", "value")));
}

// Asking for different headers starts a new transfer, so the binding is released.
TEST_F(HeaderFixture, AChangeOfRequestReleasesTheBinding)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Test-Header: value");
    ASSERT_TRUE(op.BuildRequestHeaders(headers));

    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Test-Header: other");
    TestOp other("https://other.example.com/path");
    ASSERT_TRUE(other.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test-Header", "other")));
}

// Clearing the request likewise releases it; the fixture relies on this to keep
// the tests independent of one another.
TEST_F(HeaderFixture, ClearingTheRequestReleasesTheBinding)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Test-Header: value");
    ASSERT_TRUE(op.BuildRequestHeaders(headers));

    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "");
    ASSERT_TRUE(op.BuildRequestHeaders(headers));

    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Test-Header: value");
    TestOp other("https://other.example.com/path");
    ASSERT_TRUE(other.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("X-Test-Header", "value")));
}

// An endpoint the headers are not bound to still gets the operation's own headers.
TEST_F(HeaderFixture, AnUnboundEndpointKeepsItsOwnHeaders)
{
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpHeaders", "X-Test-Header: value");
    ASSERT_TRUE(op.BuildRequestHeaders(headers));

    TestOp other("https://other.example.com/path");
    other.AddOpHeader("Range", "bytes=0-1023");
    ASSERT_TRUE(other.BuildRequestHeaders(headers));
    EXPECT_THAT(headers, ElementsAre(Pair("Range", "bytes=0-1023")));
}
