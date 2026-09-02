/******************************************************************************/
/* Copyright (C) 2026 by European Organization for Nuclear Research (CERN)   */
/*                                                                            */
/* This file is part of the XrdClHttp client plugin for XRootD.               */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/******************************************************************************/

#include "XrdClHttp/XrdClHttpToken.hh"

#include <gtest/gtest.h>

#include <limits>
#include <string>
#include <vector>

namespace {

std::string MacaroonResponseOfSize(std::size_t size)
{
    const std::string prefix = "{\"macaroon\":\"";
    const std::string suffix = "\"}";
    if (size <= prefix.size() + suffix.size()) return {};
    return prefix +
        std::string(size - prefix.size() - suffix.size(), 'x') + suffix;
}

} // namespace

TEST(HttpToken, AcceptsHttpsUrl)
{
    std::string normalized;
    EXPECT_TRUE(XrdClHttp::NormalizeTokenUrl(
        "https://storage.example/eos/file?opaque=value", normalized));
    EXPECT_EQ(normalized,
              "https://storage.example/eos/file?opaque=value");
}

TEST(HttpToken, NormalizesDavsUrlToHttps)
{
    std::string normalized;
    EXPECT_TRUE(XrdClHttp::NormalizeTokenUrl(
        "davs://storage.example:8443/eos/file", normalized));
    EXPECT_EQ(normalized, "https://storage.example:8443/eos/file");

    EXPECT_TRUE(XrdClHttp::NormalizeTokenUrl(
        "HTTPS://storage.example/eos/file", normalized));
    EXPECT_EQ(normalized, "https://storage.example/eos/file");
}

TEST(HttpToken, RejectsInsecureOrMalformedUrl)
{
    const std::vector<std::string> invalid = {
        "http://storage.example/eos/file",
        "dav://storage.example/eos/file",
        "root://storage.example//eos/file",
        "file:///tmp/file",
        "https:///missing-host",
        "https://:8443/missing-host",
        "https://storage.example:/empty-port",
        "https://storage.example:abc/non-numeric-port",
        "https://storage.example:65536/out-of-range-port",
        "https://[::1]suffix/malformed-bracket-suffix",
        "https://[::1]:abc/non-numeric-ipv6-port",
        "https://::1/unbracketed-ipv6",
        "https://storage.example/path with space",
        "https://storage.example/path\nhttp://redirect.example/"
    };

    for (const auto &input : invalid) {
        std::string normalized = "stale-url";
        EXPECT_FALSE(XrdClHttp::NormalizeTokenUrl(input, normalized))
            << input;
        EXPECT_TRUE(normalized.empty()) << input;
    }
}

TEST(HttpToken, ParsesReadDefaults)
{
    XrdClHttp::TokenRequest request;
    std::string error;

    ASSERT_TRUE(XrdClHttp::ParseTokenRequest(
        R"({"path":"/store/file","validity":60,"write":false,"activities":[]})",
        request, error)) << error;
    EXPECT_EQ(request.path, "/store/file");
    EXPECT_EQ(request.validity, 60U);
    EXPECT_FALSE(request.write);
    EXPECT_EQ(request.activities,
              (std::vector<std::string>{"LIST", "DOWNLOAD"}));
}

TEST(HttpToken, ParsesWriteDefaults)
{
    XrdClHttp::TokenRequest request;
    std::string error;

    ASSERT_TRUE(XrdClHttp::ParseTokenRequest(
        R"({"path":"/rw","validity":15,"write":true,"activities":[]})",
        request, error)) << error;
    EXPECT_EQ(request.path, "/rw");
    EXPECT_EQ(request.validity, 15U);
    EXPECT_TRUE(request.write);
    EXPECT_EQ(request.activities,
              (std::vector<std::string>{
                  "LIST", "DOWNLOAD", "MANAGE", "UPLOAD", "DELETE"}));
}

TEST(HttpToken, PreservesCustomActivities)
{
    XrdClHttp::TokenRequest request;
    std::string error;

    ASSERT_TRUE(XrdClHttp::ParseTokenRequest(
        R"({"path":"/custom?opaque=value","validity":5,"write":true,"activities":["READ_METADATA","LIST"]})",
        request, error)) << error;
    EXPECT_EQ(request.path, "/custom?opaque=value");
    EXPECT_EQ(request.activities,
              (std::vector<std::string>{"READ_METADATA", "LIST"}));
}

TEST(HttpToken, ParsesIssuer)
{
    XrdClHttp::TokenRequest request;
    std::string error;

    ASSERT_TRUE(XrdClHttp::ParseTokenRequest(
        R"({"path":"/file","issuer":"https://issuer.example/base"})",
        request, error)) << error;
    EXPECT_EQ(request.issuer, "https://issuer.example/base");

    ASSERT_TRUE(XrdClHttp::ParseTokenRequest(
        R"({"path":"/file"})", request, error)) << error;
    EXPECT_TRUE(request.issuer.empty());
}

TEST(HttpToken, RejectsInvalidQueryFields)
{
    const std::vector<std::string> invalid = {
        R"({})",
        R"({"path":""})",
        R"({"path":"relative/file"})",
        R"({"path":42})",
        R"({"path":"/file","validity":-1})",
        R"({"path":"/file","validity":"60"})",
        R"({"path":"/file","issuer":""})",
        R"({"path":"/file","issuer":42})",
        R"({"path":"/file","write":"yes"})",
        R"({"path":"/file","activities":"LIST"})",
        R"({"path":"/file","activities":[""]})",
        R"({"path":"/file","activities":["LIST",7]})"
    };

    for (const auto &input : invalid) {
        XrdClHttp::TokenRequest request;
        std::string error;
        EXPECT_FALSE(XrdClHttp::ParseTokenRequest(input, request, error))
            << input;
        EXPECT_FALSE(error.empty()) << input;
    }
}

TEST(HttpToken, BuildsIssuerDiscoveryUrls)
{
    std::string url;

    ASSERT_TRUE(XrdClHttp::BuildOAuthAuthorizationServerUrl(
        "https://issuer.example", url));
    EXPECT_EQ(url,
              "https://issuer.example/.well-known/oauth-authorization-server");

    ASSERT_TRUE(XrdClHttp::BuildOAuthAuthorizationServerUrl(
        "https://issuer.example/tenant/one", url));
    EXPECT_EQ(url,
              "https://issuer.example/.well-known/"
              "oauth-authorization-server/tenant/one");

    ASSERT_TRUE(XrdClHttp::BuildOpenIdConfigurationUrl(
        "https://issuer.example", url));
    EXPECT_EQ(url,
              "https://issuer.example/.well-known/openid-configuration");

    ASSERT_TRUE(XrdClHttp::BuildOpenIdConfigurationUrl(
        "davs://issuer.example:8443/tenant/one/", url));
    EXPECT_EQ(url,
              "https://issuer.example:8443/tenant/one/"
              ".well-known/openid-configuration");

    ASSERT_TRUE(XrdClHttp::BuildOAuthAuthorizationServerUrl(
        "https://issuer.example/tenant?ignored=yes#fragment", url));
    EXPECT_EQ(url,
              "https://issuer.example/.well-known/"
              "oauth-authorization-server/tenant");

    ASSERT_TRUE(XrdClHttp::BuildOpenIdConfigurationUrl(
        "https://issuer.example?ignored=yes#fragment", url));
    EXPECT_EQ(url,
              "https://issuer.example/.well-known/openid-configuration");
}

TEST(HttpToken, RejectsInsecureIssuerDiscoveryUrls)
{
    const std::vector<std::string> invalid = {
        "http://issuer.example",
        "root://issuer.example",
        "https:///missing-host",
        "https://user:password@issuer.example"
    };

    for (const auto &issuer : invalid) {
        std::string url = "stale-url";
        EXPECT_FALSE(XrdClHttp::BuildOAuthAuthorizationServerUrl(issuer, url))
            << issuer;
        EXPECT_TRUE(url.empty()) << issuer;
        url = "stale-url";
        EXPECT_FALSE(XrdClHttp::BuildOpenIdConfigurationUrl(issuer, url))
            << issuer;
        EXPECT_TRUE(url.empty()) << issuer;
    }
}

TEST(HttpToken, BuildsGfalCompatibleReadRequest)
{
    EXPECT_EQ(XrdClHttp::BuildMacaroonRequest(
                  60, {"LIST", "DOWNLOAD"}),
              "{\"caveats\": [\"activity:LIST,DOWNLOAD\"], "
              "\"validity\": \"PT60M\"}");
}

TEST(HttpToken, BuildsGfalCompatibleWriteRequest)
{
    EXPECT_EQ(XrdClHttp::BuildMacaroonRequest(
                  10, {"LIST", "DOWNLOAD", "MANAGE", "UPLOAD", "DELETE"}),
              "{\"caveats\": "
              "[\"activity:LIST,DOWNLOAD,MANAGE,UPLOAD,DELETE\"], "
              "\"validity\": \"PT10M\"}");
}

TEST(HttpToken, BuildsGfalCompatibleCustomRequest)
{
    EXPECT_EQ(XrdClHttp::BuildMacaroonRequest(
                  5, {"READ_METADATA", "LIST"}),
              "{\"caveats\": [\"activity:READ_METADATA,LIST\"], "
              "\"validity\": \"PT5M\"}");
}

TEST(HttpToken, EscapesCustomActivitiesInJson)
{
    EXPECT_EQ(XrdClHttp::BuildMacaroonRequest(1, {"LIST\"INJECT"}),
              "{\"caveats\": [\"activity:LIST\\\"INJECT\"], "
              "\"validity\": \"PT1M\"}");
}

TEST(HttpToken, BuildsSciTokensRequest)
{
    EXPECT_EQ(XrdClHttp::BuildSciTokensRequest(),
              "grant_type=client_credentials");
}

TEST(HttpToken, BuildsGfalCompatibleOAuthMacaroonRequest)
{
    std::string body;
    std::string error;
    ASSERT_TRUE(XrdClHttp::BuildOAuthMacaroonRequest(
        "/eos/pilot/file", 5, {"LIST", "DOWNLOAD"}, body, error))
        << error;
    EXPECT_EQ(body,
              "grant_type=client_credentials&expire_in=300&scopes="
              "LIST%3A%2Feos%2Fpilot%2Ffile%20"
              "DOWNLOAD%3A%2Feos%2Fpilot%2Ffile");
    EXPECT_TRUE(error.empty());
}

TEST(HttpToken, PercentEncodesOAuthMacaroonScopes)
{
    std::string body;
    std::string error;
    ASSERT_TRUE(XrdClHttp::BuildOAuthMacaroonRequest(
        "/eos/a b+~%?opaque=ignored", 0, {"READ_METADATA"}, body,
        error)) << error;
    EXPECT_EQ(body,
              "grant_type=client_credentials&expire_in=0&scopes="
              "READ_METADATA%3A%2Feos%2Fa%20b%2B~%25");

    ASSERT_TRUE(XrdClHttp::BuildOAuthMacaroonRequest(
        "/eos/fragment-only#ignored", 1, {"LIST"}, body, error))
        << error;
    EXPECT_EQ(body,
              "grant_type=client_credentials&expire_in=60&scopes="
              "LIST%3A%2Feos%2Ffragment-only");
}

TEST(HttpToken, RejectsOAuthMacaroonValidityOverflow)
{
    const auto maximum_minutes =
        std::numeric_limits<std::uint64_t>::max() / 60;
    std::string body;
    std::string error;

    ASSERT_TRUE(XrdClHttp::BuildOAuthMacaroonRequest(
        "/file", maximum_minutes, {"LIST"}, body, error)) << error;
    EXPECT_NE(body.find("expire_in=" +
                        std::to_string(maximum_minutes * 60)),
              std::string::npos);

    body = "stale-body";
    EXPECT_FALSE(XrdClHttp::BuildOAuthMacaroonRequest(
        "/file", maximum_minutes + 1, {"LIST"}, body, error));
    EXPECT_TRUE(body.empty());
    EXPECT_NE(error.find("too large"), std::string::npos);
}

TEST(HttpToken, RejectsRelativeOAuthMacaroonScopePath)
{
    std::string body = "stale-body";
    std::string error;
    EXPECT_FALSE(XrdClHttp::BuildOAuthMacaroonRequest(
        "relative/file", 5, {"LIST"}, body, error));
    EXPECT_TRUE(body.empty());
    EXPECT_FALSE(error.empty());
}

TEST(HttpToken, ParsesGenericJsonStringResponse)
{
    std::string value;
    std::string error;
    ASSERT_TRUE(XrdClHttp::ParseJsonStringResponse(
        R"({"token_endpoint":"https://issuer.example/token"})",
        "token_endpoint", value, error)) << error;
    EXPECT_EQ(value, "https://issuer.example/token");

    ASSERT_TRUE(XrdClHttp::ParseJsonStringResponse(
        R"({"access_token":"issued-token"})", "access_token", value,
        error)) << error;
    EXPECT_EQ(value, "issued-token");
}

TEST(HttpToken, RejectsInvalidGenericJsonStringResponse)
{
    const std::vector<std::string> invalid = {
        R"({})",
        R"({"access_token":""})",
        R"({"access_token":null})",
        R"({"access_token":42})",
        "not-json",
        ""
    };

    for (const auto &input : invalid) {
        std::string value = "stale-value";
        std::string error;
        EXPECT_FALSE(XrdClHttp::ParseJsonStringResponse(
            input, "access_token", value, error)) << input;
        EXPECT_TRUE(value.empty()) << input;
        EXPECT_FALSE(error.empty()) << input;
        if (!input.empty()) {
            EXPECT_EQ(error.find(input), std::string::npos) << input;
        }
    }
}

TEST(HttpToken, ParsesMacaroonResponse)
{
    std::string token;
    std::string error;
    ASSERT_TRUE(XrdClHttp::ParseMacaroonResponse(
        R"({"expires_in":3600,"macaroon":"issued-token"})", token,
        error)) << error;
    EXPECT_EQ(token, "issued-token");
    EXPECT_TRUE(error.empty());
}

TEST(HttpToken, RejectsMissingOrEmptyMacaroon)
{
    const std::vector<std::string> invalid = {
        R"({})",
        R"({"macaroon":""})",
        R"({"macaroon":null})",
        R"({"macaroon":42})",
        ""
    };

    for (const auto &input : invalid) {
        std::string token = "stale-token";
        std::string error;
        EXPECT_FALSE(XrdClHttp::ParseMacaroonResponse(input, token, error))
            << input;
        EXPECT_TRUE(token.empty()) << input;
        EXPECT_FALSE(error.empty()) << input;
    }
}

TEST(HttpToken, AcceptsResponseJustBelowMaximumSize)
{
    const std::string response = MacaroonResponseOfSize(
        XrdClHttp::kMaxTokenResponseSize - 1);
    ASSERT_EQ(response.size(), XrdClHttp::kMaxTokenResponseSize - 1);

    std::string token;
    std::string error;
    ASSERT_TRUE(XrdClHttp::ParseMacaroonResponse(response, token, error))
        << error;
    EXPECT_FALSE(token.empty());
}

TEST(HttpToken, RejectsResponseAtMaximumSize)
{
    const std::string response = MacaroonResponseOfSize(
        XrdClHttp::kMaxTokenResponseSize);
    ASSERT_EQ(response.size(), XrdClHttp::kMaxTokenResponseSize);

    std::string token;
    std::string error;
    EXPECT_FALSE(XrdClHttp::ParseMacaroonResponse(response, token, error));
    EXPECT_TRUE(token.empty());
    EXPECT_NE(error.find("maximum size"), std::string::npos);
}

TEST(HttpToken, DoesNotIncludeResponseBodyInErrors)
{
    const std::string marker = "sensitive-response-body";
    std::string token;
    std::string error;

    EXPECT_FALSE(XrdClHttp::ParseMacaroonResponse(marker, token, error));
    EXPECT_EQ(error.find(marker), std::string::npos);

    EXPECT_FALSE(XrdClHttp::ParseMacaroonResponse(
        "{\"secret\":\"" + marker + "\"}", token, error));
    EXPECT_EQ(error.find(marker), std::string::npos);
}
