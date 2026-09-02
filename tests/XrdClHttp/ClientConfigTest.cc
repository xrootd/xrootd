/******************************************************************************/
/* Copyright (C) 2026, XRootD Collaboration                                  */
/******************************************************************************/

#include "XrdClHttp/XrdClHttpOps.hh"

#include <gtest/gtest.h>

using namespace XrdClHttp;

TEST(ClientConfig, StripsOnlyClientParameters) {
    HttpClientConfig config;
    std::string client_query;
    auto cleaned = ExtractHttpClientConfig(
        "davs://example/data?X-Amz-Signature=abc+def==&"
        "xrdcl.http.clientcert=%2Ftmp%2Fproxy&empty=",
        config, &client_query);

    EXPECT_EQ(cleaned,
        "davs://example/data?X-Amz-Signature=abc+def==&empty=");
    EXPECT_EQ(config.client_cert, "/tmp/proxy");
    EXPECT_EQ(client_query, "xrdcl.http.clientcert=%2Ftmp%2Fproxy");
}

TEST(ClientConfig, ParsesAuthenticationAndTlsPolicy) {
    HttpClientConfig config;
    auto cleaned = ExtractHttpClientConfig(
        "https://example/data?xrdcl.http.bearertokenfile=%2Ftmp%2Ftoken&"
        "xrdcl.http.cafile=%2Ftmp%2Fca.pem&xrdcl.http.noverify=1&"
        "xrdcl.http.noauth=true&xrdcl.authctx=identity",
        config);

    EXPECT_EQ(cleaned, "https://example/data");
    EXPECT_EQ(config.bearer_token_file, "/tmp/token");
    EXPECT_EQ(config.ca_file, "/tmp/ca.pem");
    EXPECT_TRUE(config.no_verify);
    EXPECT_TRUE(config.no_auth);
}
