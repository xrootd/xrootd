#include "HeaderBuilderFixture.hh"

#include <gtest/gtest.h>

#include <string>

using namespace XrdClHttp;

namespace {

class ForbiddenEntry : public HeaderBuilderFixture {};

} // namespace

/******************************************************************************/
/*                 B u i l d :  f o r b i d d e n  e n t r y                  */
/******************************************************************************/

TEST_F(ForbiddenEntry, AnAuthorizationEntryIsRejected)
{
    const std::string spec{"authorization: Bearer token"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, AnAuthorizationEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Authorization: Bearer token"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, ATransferHeaderAuthorizationEntryIsRejected)
{
    const std::string spec{"transferheaderauthorization: Bearer token"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, ATransferHeaderAuthorizationEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"TransferHeaderAuthorization: Bearer token"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, AConnectionEntryIsRejected)
{
    const std::string spec{"connection: close"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, AConnectionEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Connection: close"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, AContentLengthEntryIsRejected)
{
    const std::string spec{"content-length: 0"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, AContentLengthEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Content-Length: 0"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, AnExpectEntryIsRejected)
{
    const std::string spec{"expect: 100-continue"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, AnExpectEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Expect: 100-continue"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, AHostEntryIsRejected)
{
    const std::string spec{"host: evil.example.com"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, AHostEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Host: evil.example.com"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, AKeepAliveEntryIsRejected)
{
    const std::string spec{"keep-alive: timeout=5"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, AKeepAliveEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Keep-Alive: timeout=5"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, AProxyAuthenticateEntryIsRejected)
{
    const std::string spec{"proxy-authenticate: Basic"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, AProxyAuthenticateEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Proxy-Authenticate: Basic"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, AProxyAuthorizationEntryIsRejected)
{
    const std::string spec{"proxy-authorization: Basic dXNlcg=="};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, AProxyAuthorizationEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Proxy-Authorization: Basic dXNlcg=="};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, AProxyConnectionEntryIsRejected)
{
    const std::string spec{"proxy-connection: close"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, AProxyConnectionEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Proxy-Connection: close"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, ATeEntryIsRejected)
{
    const std::string spec{"te: trailers"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, ATeEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"TE: trailers"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, ATrailerEntryIsRejected)
{
    const std::string spec{"trailer: Expires"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, ATrailerEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Trailer: Expires"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, ATransferEncodingEntryIsRejected)
{
    const std::string spec{"transfer-encoding: chunked"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, ATransferEncodingEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Transfer-Encoding: chunked"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, AnUpgradeEntryIsRejected)
{
    const std::string spec{"upgrade: websocket"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

TEST_F(ForbiddenEntry, AnUpgradeEntryIsRejectedWhenCapitalized)
{
    const std::string spec{"Upgrade: websocket"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}

// A forbidden entry must reject the whole request rather than silently dropping
// the offending header and sending the rest.
TEST_F(ForbiddenEntry, AForbiddenEntryRejectsTheWholeRequest)
{
    const std::string spec{"X-Good: value\nHost: evil.example.com\nX-Also-Good: value"};

    EXPECT_FALSE(HeaderBuilder::Build(spec, headers));
}
