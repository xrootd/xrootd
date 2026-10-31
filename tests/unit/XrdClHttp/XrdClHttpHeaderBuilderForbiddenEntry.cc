#include "HeaderBuilderFixture.hh"

#include <gtest/gtest.h>

#include <string>

using namespace XrdClHttp;

namespace {

class HeaderBuilderForbiddenEntryTest : public HeaderBuilderFixture {};

} // namespace

/******************************************************************************/
/*                 B u i l d :  f o r b i d d e n  e n t r y                  */
/******************************************************************************/

TEST_F(HeaderBuilderForbiddenEntryTest, AConnectionEntryIsRejected)
{
    const std::string spec{"connection: close"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderForbiddenEntryTest, AConnectionEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Connection: close"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderForbiddenEntryTest, AContentLengthEntryIsRejected)
{
    const std::string spec{"content-length: 0"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderForbiddenEntryTest, AContentLengthEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Content-Length: 0"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderForbiddenEntryTest, AnExpectEntryIsRejected)
{
    const std::string spec{"expect: 100-continue"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderForbiddenEntryTest, AnExpectEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Expect: 100-continue"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderForbiddenEntryTest, AHostEntryIsRejected)
{
    const std::string spec{"host: evil.example.com"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderForbiddenEntryTest, AHostEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Host: evil.example.com"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderForbiddenEntryTest, AKeepAliveEntryIsRejected)
{
    const std::string spec{"keep-alive: timeout=5"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderForbiddenEntryTest, AKeepAliveEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Keep-Alive: timeout=5"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderForbiddenEntryTest, AProxyAuthenticateEntryIsRejected)
{
    const std::string spec{"proxy-authenticate: Basic"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderForbiddenEntryTest, AProxyAuthenticateEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Proxy-Authenticate: Basic"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderForbiddenEntryTest, AProxyAuthorizationEntryIsRejected)
{
    const std::string spec{"proxy-authorization: Basic dXNlcg=="};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderForbiddenEntryTest, AProxyAuthorizationEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Proxy-Authorization: Basic dXNlcg=="};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderForbiddenEntryTest, AProxyConnectionEntryIsRejected)
{
    const std::string spec{"proxy-connection: close"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderForbiddenEntryTest, AProxyConnectionEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Proxy-Connection: close"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderForbiddenEntryTest, ATeEntryIsRejected)
{
    const std::string spec{"te: trailers"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderForbiddenEntryTest, ATeEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"TE: trailers"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderForbiddenEntryTest, ATrailerEntryIsRejected)
{
    const std::string spec{"trailer: Expires"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderForbiddenEntryTest, ATrailerEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Trailer: Expires"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderForbiddenEntryTest, ATransferEncodingEntryIsRejected)
{
    const std::string spec{"transfer-encoding: chunked"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderForbiddenEntryTest, ATransferEncodingEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Transfer-Encoding: chunked"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderForbiddenEntryTest, AnUpgradeEntryIsRejected)
{
    const std::string spec{"upgrade: websocket"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(HeaderBuilderForbiddenEntryTest, AnUpgradeEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Upgrade: websocket"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

// A forbidden entry must reject the whole request rather than silently dropping
// the offending header and sending the rest.
TEST_F(HeaderBuilderForbiddenEntryTest, AForbiddenEntryRejectsTheWholeRequest)
{
    const std::string spec{"X-Good: value\nHost: evil.example.com\nX-Also-Good: value"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}
