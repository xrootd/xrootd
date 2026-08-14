#include "XrdClHttp/XrdClHttpHeaderBuilder.hh"

#include <gtest/gtest.h>

using namespace XrdClHttp;

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
