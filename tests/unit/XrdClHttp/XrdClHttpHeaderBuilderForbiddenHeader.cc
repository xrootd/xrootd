#include "XrdClHttp/XrdClHttpHeaderBuilder.hh"

#include <gtest/gtest.h>

using namespace XrdClHttp;

/******************************************************************************/
/*                     I s F o r b i d d e n H e a d e r                      */
/******************************************************************************/

TEST(HeaderBuilderForbiddenHeaderTest, ConnectionIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("connection"));
}

TEST(HeaderBuilderForbiddenHeaderTest, ConnectionIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("Connection"));
}

TEST(HeaderBuilderForbiddenHeaderTest, ContentLengthIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("content-length"));
}

TEST(HeaderBuilderForbiddenHeaderTest, ContentLengthIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("Content-Length"));
}

TEST(HeaderBuilderForbiddenHeaderTest, ExpectIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("expect"));
}

TEST(HeaderBuilderForbiddenHeaderTest, ExpectIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("Expect"));
}

TEST(HeaderBuilderForbiddenHeaderTest, HostIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("host"));
}

TEST(HeaderBuilderForbiddenHeaderTest, HostIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("Host"));
}

TEST(HeaderBuilderForbiddenHeaderTest, KeepAliveIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("keep-alive"));
}

TEST(HeaderBuilderForbiddenHeaderTest, KeepAliveIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("Keep-Alive"));
}

TEST(HeaderBuilderForbiddenHeaderTest, ProxyAuthenticateIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("proxy-authenticate"));
}

TEST(HeaderBuilderForbiddenHeaderTest, ProxyAuthenticateIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("Proxy-Authenticate"));
}

TEST(HeaderBuilderForbiddenHeaderTest, ProxyAuthorizationIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("proxy-authorization"));
}

TEST(HeaderBuilderForbiddenHeaderTest, ProxyAuthorizationIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("Proxy-Authorization"));
}

TEST(HeaderBuilderForbiddenHeaderTest, ProxyConnectionIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("proxy-connection"));
}

TEST(HeaderBuilderForbiddenHeaderTest, ProxyConnectionIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("Proxy-Connection"));
}

TEST(HeaderBuilderForbiddenHeaderTest, TeIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("te"));
}

TEST(HeaderBuilderForbiddenHeaderTest, TeIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TE"));
}

TEST(HeaderBuilderForbiddenHeaderTest, TrailerIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("trailer"));
}

TEST(HeaderBuilderForbiddenHeaderTest, TrailerIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("Trailer"));
}

TEST(HeaderBuilderForbiddenHeaderTest, TransferEncodingIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("transfer-encoding"));
}

TEST(HeaderBuilderForbiddenHeaderTest, TransferEncodingIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("Transfer-Encoding"));
}

TEST(HeaderBuilderForbiddenHeaderTest, UpgradeIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("upgrade"));
}

TEST(HeaderBuilderForbiddenHeaderTest, UpgradeIsForbiddenWhenCapitalized)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("Upgrade"));
}

TEST(HeaderBuilderForbiddenHeaderTest, ACustomHeaderIsAllowed)
{
    EXPECT_FALSE(HeaderBuilder::IsForbiddenHeader("X-Test-Header"));
}

// A user who holds a token has to be able to present it, so Authorization is
// the caller's to set, on the near server and on the far one alike.

TEST(HeaderBuilderForbiddenHeaderTest, AuthorizationIsAllowed)
{
    EXPECT_FALSE(HeaderBuilder::IsForbiddenHeader("authorization"));
}

TEST(HeaderBuilderForbiddenHeaderTest, AuthorizationIsAllowedWhenCapitalized)
{
    EXPECT_FALSE(HeaderBuilder::IsForbiddenHeader("Authorization"));
}

TEST(HeaderBuilderForbiddenHeaderTest, TransferHeaderAuthorizationIsAllowed)
{
    EXPECT_FALSE(HeaderBuilder::IsForbiddenHeader("transferheaderauthorization"));
}

TEST(HeaderBuilderForbiddenHeaderTest, TransferHeaderAuthorizationIsAllowedWhenCapitalized)
{
    EXPECT_FALSE(HeaderBuilder::IsForbiddenHeader("TransferHeaderAuthorization"));
}

// The TransferHeader prefix aims a header at the far server of a third party
// copy, and so must not let a forbidden header through.

TEST(HeaderBuilderForbiddenHeaderTest, TransferHeaderConnectionIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderConnection"));
}

TEST(HeaderBuilderForbiddenHeaderTest, TransferHeaderContentLengthIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderContent-Length"));
}

TEST(HeaderBuilderForbiddenHeaderTest, TransferHeaderExpectIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderExpect"));
}

TEST(HeaderBuilderForbiddenHeaderTest, TransferHeaderHostIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderHost"));
}

TEST(HeaderBuilderForbiddenHeaderTest, TransferHeaderKeepAliveIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderKeep-Alive"));
}

TEST(HeaderBuilderForbiddenHeaderTest, TransferHeaderProxyAuthenticateIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderProxy-Authenticate"));
}

TEST(HeaderBuilderForbiddenHeaderTest, TransferHeaderProxyAuthorizationIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderProxy-Authorization"));
}

TEST(HeaderBuilderForbiddenHeaderTest, TransferHeaderProxyConnectionIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderProxy-Connection"));
}

TEST(HeaderBuilderForbiddenHeaderTest, TransferHeaderTeIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderTE"));
}

TEST(HeaderBuilderForbiddenHeaderTest, TransferHeaderTrailerIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderTrailer"));
}

TEST(HeaderBuilderForbiddenHeaderTest, TransferHeaderTransferEncodingIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderTransfer-Encoding"));
}

TEST(HeaderBuilderForbiddenHeaderTest, TransferHeaderUpgradeIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderUpgrade"));
}

TEST(HeaderBuilderForbiddenHeaderTest, ARepeatedTransferHeaderPrefixOnAForbiddenNameIsForbidden)
{
    EXPECT_TRUE(HeaderBuilder::IsForbiddenHeader("TransferHeaderTransferHeaderHost"));
}

TEST(HeaderBuilderForbiddenHeaderTest, TransferHeaderOnACustomHeaderIsAllowed)
{
    EXPECT_FALSE(HeaderBuilder::IsForbiddenHeader("TransferHeaderX-Test-Header"));
}

TEST(HeaderBuilderForbiddenHeaderTest, TheTransferHeaderPrefixAloneIsAllowed)
{
    EXPECT_FALSE(HeaderBuilder::IsForbiddenHeader("TransferHeader"));
}

TEST(HeaderBuilderForbiddenHeaderTest, RangeIsAllowed)
{
    EXPECT_FALSE(HeaderBuilder::IsForbiddenHeader("Range"));
}

TEST(HeaderBuilderForbiddenHeaderTest, WantDigestIsAllowed)
{
    EXPECT_FALSE(HeaderBuilder::IsForbiddenHeader("Want-Digest"));
}

// The match must be on the whole name; a header that merely contains a
// forbidden one is perfectly legitimate.

TEST(HeaderBuilderForbiddenHeaderTest, ANameEndingInAForbiddenOneIsAllowed)
{
    EXPECT_FALSE(HeaderBuilder::IsForbiddenHeader("X-Host"));
}

TEST(HeaderBuilderForbiddenHeaderTest, ANameStartingWithAForbiddenOneIsAllowed)
{
    EXPECT_FALSE(HeaderBuilder::IsForbiddenHeader("Hostname"));
}

TEST(HeaderBuilderForbiddenHeaderTest, ANameExtendingAForbiddenOneIsAllowed)
{
    EXPECT_FALSE(HeaderBuilder::IsForbiddenHeader("Content-Length-Hint"));
}

TEST(HeaderBuilderForbiddenHeaderTest, ANamePrefixingAForbiddenOneIsAllowed)
{
    EXPECT_FALSE(HeaderBuilder::IsForbiddenHeader("Not-Connection"));
}

TEST(HeaderBuilderForbiddenHeaderTest, AnEmptyNameIsAllowed)
{
    EXPECT_FALSE(HeaderBuilder::IsForbiddenHeader(""));
}
