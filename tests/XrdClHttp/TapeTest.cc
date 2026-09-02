/******************************************************************************/
/*                                                                            */
/*                    X r d C l H t t p T a p e T e s t . c c                 */
/*                                                                            */
/* (c) 2026 by the XRootD Collaboration                                       */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
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
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/******************************************************************************/

#include "XrdClHttp/XrdClHttpTape.hh"
#include "XrdClHttp/XrdClHttpUtil.hh"
#include "XrdOuc/XrdOucJson.hh"

#include <XrdCl/XrdClDefaultEnv.hh>
#include <XrdCl/XrdClURL.hh>

#include <gtest/gtest.h>

#include <cerrno>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

using Json = nlohmann::json;

namespace
{
using CurlHandle = std::unique_ptr<CURL, void (*)(CURL *)>;

const std::string kStorageUrl =
  "https://storage.example.org:8443/store/file";
const std::string kTapeEndpoint = "https://tape.example.org/api/v1";

std::string DiscoveryResponse(const std::string &uri = kTapeEndpoint,
                              const std::string &version = "v1")
{
  Json response;
  response["sitename"] = "test-site";
  response["endpoints"] = Json::array({
    {{"uri", uri}, {"version", version}}
  });
  return response.dump();
}

void ExpectOk(const XrdCl::XRootDStatus &status)
{
  EXPECT_TRUE(status.IsOK()) << status.ToStr();
}

void StartAndDiscover(XrdClHttp::TapeOperation &operation,
                      CURL *curl,
                      XrdClHttp::TapeHttpRequest &request)
{
  ExpectOk(operation.Start(request));
  EXPECT_EQ(request.method, XrdClHttp::HttpVerb::GET);
  EXPECT_EQ(request.url,
    "https://storage.example.org:8443/.well-known/wlcg-tape-rest-api");

  std::string response;
  bool complete = true;
  ExpectOk(operation.Advance(curl, 200, DiscoveryResponse(), request,
                             response, complete));
  EXPECT_FALSE(complete);
}
}

TEST(XrdClHttpUtility, SelectsFirstAvailableConfiguredAuthentication)
{
  EXPECT_TRUE(XrdClHttp::ShouldUseBearerToken("", true, true));
  EXPECT_FALSE(XrdClHttp::ShouldUseBearerToken("gsi,ztn", true, true));
  EXPECT_TRUE(XrdClHttp::ShouldUseBearerToken("ztn,gsi", true, true));
  EXPECT_TRUE(XrdClHttp::ShouldUseBearerToken("gsi, ztn", false, true));
  EXPECT_FALSE(XrdClHttp::ShouldUseBearerToken("ztn,gsi", true, false));
  EXPECT_FALSE(XrdClHttp::ShouldUseBearerToken("unix", false, true));
}

TEST(XrdClHttpUtility, InjectsBearerTokenThroughCommonAuthentication)
{
  auto env = XrdCl::DefaultEnv::GetEnv();
  ASSERT_NE(env, nullptr);
  ASSERT_TRUE(env->PutString("BearerToken", "test-token"));
  ASSERT_TRUE(env->PutInt("HttpDisableX509", 0));
  ASSERT_TRUE(env->PutString("HttpClientCertFile", "/tmp/test-cert"));

  ASSERT_EQ(setenv("XrdSecPROTOCOL", "ztn,gsi", 1), 0);
  XrdCl::URL url("https://storage.example.org/store/file");
  std::vector<std::pair<std::string, std::string>> headers;
  XrdClHttp::InjectBearerToken(url, headers);
  ASSERT_EQ(headers.size(), 1u);
  EXPECT_EQ(headers[0].first, "Authorization");
  EXPECT_EQ(headers[0].second, "Bearer test-token");

  headers = {{"Authorization", "Bearer explicit-token"}};
  XrdClHttp::InjectBearerToken(url, headers);
  ASSERT_EQ(headers.size(), 1u);
  EXPECT_EQ(headers[0].second, "Bearer explicit-token");

  XrdCl::URL authzUrl(
    "https://storage.example.org/store/file?authz=explicit-token");
  headers.clear();
  XrdClHttp::InjectBearerToken(authzUrl, headers);
  EXPECT_TRUE(headers.empty());

  ASSERT_EQ(setenv("XrdSecPROTOCOL", "gsi,ztn", 1), 0);
  headers.clear();
  XrdClHttp::InjectBearerToken(url, headers);
  EXPECT_TRUE(headers.empty());
}

TEST(XrdClHttpUtility, AddsSelectedBearerTokenOnce)
{
  std::vector<std::pair<std::string, std::string>> headers;
  XrdClHttp::AddBearerTokenHeader(headers, "", false, "test-token");
  ASSERT_EQ(headers.size(), 1);
  EXPECT_EQ(headers[0].first, "Authorization");
  EXPECT_EQ(headers[0].second, "Bearer test-token");

  XrdClHttp::AddBearerTokenHeader(headers, "ztn", false, "test-token");
  EXPECT_EQ(headers.size(), 1);
}

TEST(XrdClHttpUtility, PreservesExplicitAuthorizationHeader)
{
  std::vector<std::pair<std::string, std::string>> headers{
    {"authorization", "Bearer explicit-token"}
  };
  XrdClHttp::AddBearerTokenHeader(headers, "", false, "environment-token");

  ASSERT_EQ(headers.size(), 1);
  EXPECT_EQ(headers[0].second, "Bearer explicit-token");
}

TEST(XrdClHttpUtility, ReportsClientX509ConfigurationFailure)
{
  EXPECT_TRUE(XrdClHttp::SetClientX509(nullptr, "", "", nullptr));
  EXPECT_FALSE(XrdClHttp::SetClientX509(
    nullptr, "/tmp/client-cert.pem", "", nullptr));
}

TEST(TapeRestApi, DiscoversSupportedEndpoint)
{
  XrdCl::Buffer arg;
  arg.FromString("tape.discover");
  XrdClHttp::TapeOperation operation(
    kStorageUrl, XrdCl::QueryCode::Opaque, arg);

  XrdClHttp::TapeHttpRequest request;
  ExpectOk(operation.Start(request));

  std::string response;
  bool complete = false;
  ExpectOk(operation.Advance(nullptr, 200, DiscoveryResponse(), request,
                             response, complete));
  EXPECT_TRUE(complete);
  const Json result = Json::parse(response);
  EXPECT_EQ(result["uri"], kTapeEndpoint);
  EXPECT_EQ(result["version"], "v1");
  EXPECT_EQ(result["sitename"], "test-site");
}

TEST(TapeRestApi, BuildsStageRequestAndParsesResponse)
{
  CurlHandle curl(XrdClHttp::GetHandle(false), curl_easy_cleanup);
  ASSERT_NE(curl, nullptr);
  XrdClHttp::TapeOperation operation(
    kStorageUrl,
    {R"(xrdclhttp.tape.stage:{"path":"/store/file",)"
     R"("diskLifetime":"PT1H","targetedMetadata":)"
     R"({"test-site":{"activity":"analysis"}}})"},
    XrdCl::PrepareFlags::Stage);

  XrdClHttp::TapeHttpRequest request;
  StartAndDiscover(operation, curl.get(), request);
  EXPECT_EQ(request.method, XrdClHttp::HttpVerb::POST);
  EXPECT_EQ(request.url, kTapeEndpoint + "/stage");
  const Json body = Json::parse(request.body);
  ASSERT_EQ(body["files"].size(), 1u);
  EXPECT_EQ(body["files"][0]["path"], "/store/file");
  EXPECT_EQ(body["files"][0]["diskLifetime"], "PT1H");
  EXPECT_EQ(body["files"][0]["targetedMetadata"]["test-site"]["activity"],
            "analysis");

  std::string response;
  bool complete = false;
  ExpectOk(operation.Advance(curl.get(), 201,
    R"({"requestId":"request-1"})", request, response, complete));
  EXPECT_TRUE(complete);
  EXPECT_EQ(response, "request-1");

  XrdClHttp::TapeOperation emptyResponseOperation(
    kStorageUrl, {"/store/file"}, XrdCl::PrepareFlags::Stage);
  StartAndDiscover(emptyResponseOperation, curl.get(), request);
  EXPECT_FALSE(emptyResponseOperation.Advance(curl.get(), 201,
    R"({"requestId":""})", request, response, complete).IsOK());
}

TEST(TapeRestApi, BuildsStageStatusRequestAndParsesResponse)
{
  CurlHandle curl(XrdClHttp::GetHandle(false), curl_easy_cleanup);
  ASSERT_NE(curl, nullptr);
  XrdCl::Buffer arg;
  arg.FromString("request/1?# x");
  XrdClHttp::TapeOperation operation(
    kStorageUrl, XrdCl::QueryCode::Prepare, arg);

  XrdClHttp::TapeHttpRequest request;
  StartAndDiscover(operation, curl.get(), request);
  EXPECT_EQ(request.method, XrdClHttp::HttpVerb::GET);
  EXPECT_EQ(request.url,
            kTapeEndpoint + "/stage/request%2F1%3F%23%20x");

  std::string response;
  bool complete = false;
  ExpectOk(operation.Advance(curl.get(), 200,
    R"({"id":"request/1?# x","createdAt":1,"startedAt":2,)"
    R"("completedAt":3,"files":[{"path":"/store/file",)"
    R"("state":"COMPLETED","startedAt":2,"finishedAt":3}]})",
    request, response, complete));
  EXPECT_TRUE(complete);
  const Json status = Json::parse(response);
  EXPECT_EQ(status["id"], "request/1?# x");
  EXPECT_EQ(status["files"][0]["state"], "COMPLETED");
  EXPECT_FALSE(status["files"][0].contains("onDisk"));
}

TEST(TapeRestApi, NormalizesOptionalStageStatusFields)
{
  CurlHandle curl(XrdClHttp::GetHandle(false), curl_easy_cleanup);
  ASSERT_NE(curl, nullptr);
  XrdCl::Buffer arg;
  arg.FromString("request-1");
  XrdClHttp::TapeOperation operation(
    kStorageUrl, XrdCl::QueryCode::Prepare, arg);

  XrdClHttp::TapeHttpRequest request;
  StartAndDiscover(operation, curl.get(), request);

  std::string response;
  bool complete = false;
  ExpectOk(operation.Advance(curl.get(), 200,
    R"({"id":"request-1","createdAt":0,"startedAt":1,)"
    R"("extra":"ignored","files":[)"
    R"({"path":"//store///disk","onDisk":false,"error":"",)"
    R"("extra":"ignored"},)"
    R"({"path":"/store/queued","state":"","startedAt":2,)"
    R"("finishedAt":3},)"
    R"({"path":"/store/slow","state":"STARTED","error":"slow"}]})",
    request, response, complete));
  EXPECT_TRUE(complete);

  const Json status = Json::parse(response);
  EXPECT_EQ(status["createdAt"], 0);
  EXPECT_EQ(status["startedAt"], 1);
  EXPECT_FALSE(status.contains("completedAt"));
  EXPECT_FALSE(status.contains("extra"));
  ASSERT_EQ(status["files"].size(), 3u);
  EXPECT_EQ(status["files"][0]["path"], "/store/disk");
  EXPECT_EQ(status["files"][0]["onDisk"], false);
  EXPECT_FALSE(status["files"][0].contains("error"));
  EXPECT_FALSE(status["files"][0].contains("extra"));
  EXPECT_FALSE(status["files"][1].contains("state"));
  EXPECT_EQ(status["files"][1]["startedAt"], 2);
  EXPECT_EQ(status["files"][1]["finishedAt"], 3);
  EXPECT_EQ(status["files"][2]["state"], "STARTED");
  EXPECT_EQ(status["files"][2]["error"], "slow");
}

TEST(TapeRestApi, RejectsMalformedStageStatusFields)
{
  CurlHandle curl(XrdClHttp::GetHandle(false), curl_easy_cleanup);
  ASSERT_NE(curl, nullptr);
  XrdCl::Buffer arg;
  arg.FromString("request-1");

  const auto expectInvalid = [&](const std::string &body)
  {
    XrdClHttp::TapeOperation operation(
      kStorageUrl, XrdCl::QueryCode::Prepare, arg);
    XrdClHttp::TapeHttpRequest request;
    StartAndDiscover(operation, curl.get(), request);

    std::string response;
    bool complete = false;
    const auto status = operation.Advance(
      curl.get(), 200, body, request, response, complete);
    EXPECT_FALSE(status.IsOK());
    EXPECT_EQ(status.code, XrdCl::errInvalidResponse);
    EXPECT_EQ(status.errNo, EBADMSG);
    EXPECT_TRUE(complete);
  };

  expectInvalid(
    R"({"id":"request-1","createdAt":1,"startedAt":2,)"
    R"("files":[{"path":"/store/file","error":3}]})");
  expectInvalid(
    R"({"id":"request-1","createdAt":1,"startedAt":2,)"
    R"("files":[{"path":"/store/file","startedAt":3}]})");
  expectInvalid(
    R"({"id":"request-1","createdAt":1,"startedAt":2,)"
    R"("completedAt":"later","files":[]})");
  expectInvalid(
    R"({"id":"request-1","createdAt":-1,"startedAt":2,"files":[]})");
  expectInvalid(
    R"({"id":"request-1","createdAt":1,"startedAt":2,)"
    R"("files":[{"path":"/store/file","state":"STARTED",)"
    R"("startedAt":-1}]})");
  expectInvalid(
    R"({"id":"request-1","createdAt":1,"startedAt":2,)"
    R"("files":["/store/file"]})");
  expectInvalid(
    R"({"id":"request-1","createdAt":1,"startedAt":2,)"
    R"("files":[{"path":"","onDisk":false}]})");
}

TEST(TapeRestApi, BuildsCancelReleaseAndDeleteRequests)
{
  CurlHandle curl(XrdClHttp::GetHandle(false), curl_easy_cleanup);
  ASSERT_NE(curl, nullptr);

  XrdClHttp::TapeOperation cancel(kStorageUrl,
    {"request-1", "/store/file"}, XrdCl::PrepareFlags::Cancel);
  XrdClHttp::TapeHttpRequest request;
  StartAndDiscover(cancel, curl.get(), request);
  EXPECT_EQ(request.method, XrdClHttp::HttpVerb::POST);
  EXPECT_EQ(request.url, kTapeEndpoint + "/stage/request-1/cancel");
  EXPECT_EQ(Json::parse(request.body)["paths"][0], "/store/file");
  std::string response;
  bool complete = false;
  ExpectOk(cancel.Advance(curl.get(), 201, "", request, response, complete));
  EXPECT_TRUE(complete);

  XrdClHttp::TapeOperation release(kStorageUrl,
    {"request-1", "/store/file"}, XrdCl::PrepareFlags::Evict);
  StartAndDiscover(release, curl.get(), request);
  EXPECT_EQ(request.method, XrdClHttp::HttpVerb::POST);
  EXPECT_EQ(request.url, kTapeEndpoint + "/release/request-1");
  complete = false;
  ExpectOk(release.Advance(curl.get(), 202, "", request, response, complete));
  EXPECT_TRUE(complete);

  XrdCl::Buffer arg;
  arg.FromString("tape.stage_delete\nrequest-1");
  XrdClHttp::TapeOperation deletion(
    kStorageUrl, XrdCl::QueryCode::Opaque, arg);
  StartAndDiscover(deletion, curl.get(), request);
  EXPECT_EQ(request.method, XrdClHttp::HttpVerb::DELETE);
  EXPECT_EQ(request.url, kTapeEndpoint + "/stage/request-1");
  complete = false;
  ExpectOk(deletion.Advance(curl.get(), 204, "", request, response, complete));
  EXPECT_TRUE(complete);
}

TEST(TapeRestApi, NormalizesTapePaths)
{
  CurlHandle curl(XrdClHttp::GetHandle(false), curl_easy_cleanup);
  ASSERT_NE(curl, nullptr);
  XrdClHttp::TapeOperation operation(
    kStorageUrl, {"//store///file"}, XrdCl::PrepareFlags::Stage);

  XrdClHttp::TapeHttpRequest request;
  StartAndDiscover(operation, curl.get(), request);
  EXPECT_EQ(Json::parse(request.body)["files"][0]["path"], "/store/file");
}

TEST(TapeRestApi, BuildsAndParsesArchiveInfo)
{
  CurlHandle curl(XrdClHttp::GetHandle(false), curl_easy_cleanup);
  ASSERT_NE(curl, nullptr);
  XrdCl::Buffer arg;
  arg.FromString("tape.archiveinfo\n"
    "https://storage.example.org:8443/store/file\n"
    "https://storage.example.org:8443/store/missing");
  XrdClHttp::TapeOperation operation(
    kStorageUrl, XrdCl::QueryCode::Opaque, arg);

  XrdClHttp::TapeHttpRequest request;
  StartAndDiscover(operation, curl.get(), request);
  EXPECT_EQ(request.method, XrdClHttp::HttpVerb::POST);
  EXPECT_EQ(request.url, kTapeEndpoint + "/archiveinfo");
  ASSERT_EQ(Json::parse(request.body)["paths"].size(), 2u);

  std::string response;
  bool complete = false;
  ExpectOk(operation.Advance(curl.get(), 200,
    R"([{"path":"/store/missing","error":"not found"},)"
    R"({"path":"/store/file","locality":"DISK_AND_TAPE"}])",
    request, response, complete));
  EXPECT_TRUE(complete);
  const Json result = Json::parse(response);
  EXPECT_EQ(result[0]["path"], "/store/file");
  EXPECT_EQ(result[0]["url"],
    "https://storage.example.org:8443/store/file");
  EXPECT_EQ(result[0]["locality"], "DISK_AND_TAPE");
  EXPECT_EQ(result[1]["path"], "/store/missing");
  EXPECT_EQ(result[1]["url"],
    "https://storage.example.org:8443/store/missing");
  EXPECT_EQ(result[1]["error"], "not found");
}

TEST(TapeRestApi, NormalizesArchiveInfoLocalitiesAndErrors)
{
  CurlHandle curl(XrdClHttp::GetHandle(false), curl_easy_cleanup);
  ASSERT_NE(curl, nullptr);
  XrdCl::Buffer arg;
  arg.FromString("tape.archiveinfo\n"
    "https://storage.example.org:8443/store/lower\n"
    "https://storage.example.org:8443/store/unknown\n"
    "https://storage.example.org:8443/store/bad-error\n"
    "https://storage.example.org:8443/store/missing\n"
    "https://storage.example.org:8443/store/empty-error\n"
    "https://storage.example.org:8443/store/empty-locality");
  XrdClHttp::TapeOperation operation(
    kStorageUrl, XrdCl::QueryCode::Opaque, arg);

  XrdClHttp::TapeHttpRequest request;
  StartAndDiscover(operation, curl.get(), request);
  std::string response;
  bool complete = false;
  ExpectOk(operation.Advance(curl.get(), 200,
    R"([{"path":"/store/lower","locality":"disk_and_tape"},)"
    R"({"path":"/store/unknown","locality":"CLOUD"},)"
    R"({"path":"/store/bad-error","error":3},)"
    R"({"path":"/store/empty-error","error":""},)"
    R"({"path":"/store/empty-locality","locality":""}])",
    request, response, complete));
  EXPECT_TRUE(complete);

  const Json result = Json::parse(response);
  ASSERT_EQ(result.size(), 6u);
  EXPECT_EQ(result[0]["locality"], "DISK_AND_TAPE");
  EXPECT_EQ(result[1]["locality"], "UNKNOWN");
  EXPECT_EQ(result[2]["error"], "error field is not a string");
  EXPECT_EQ(result[3]["error"],
            "missing response item for path=/store/missing");
  EXPECT_EQ(result[4]["locality"], "UNKNOWN");
  EXPECT_EQ(result[5]["error"], "locality attribute missing");
}

TEST(TapeRestApi, RejectsUnsupportedDiscoveryResponses)
{
  XrdCl::Buffer arg;
  arg.FromString("tape.discover");
  XrdClHttp::TapeOperation operation(
    kStorageUrl, XrdCl::QueryCode::Opaque, arg);
  XrdClHttp::TapeHttpRequest request;
  ExpectOk(operation.Start(request));

  std::string response;
  bool complete = false;
  EXPECT_FALSE(operation.Advance(nullptr, 200,
    DiscoveryResponse(kTapeEndpoint, "v2"), request, response,
    complete).IsOK());

  XrdClHttp::TapeOperation malformedVersion(
    kStorageUrl, XrdCl::QueryCode::Opaque, arg);
  ExpectOk(malformedVersion.Start(request));
  EXPECT_FALSE(malformedVersion.Advance(nullptr, 200,
    DiscoveryResponse(kTapeEndpoint, "v1beta"), request, response,
    complete).IsOK());

  XrdClHttp::TapeOperation badScheme(
    kStorageUrl, XrdCl::QueryCode::Opaque, arg);
  ExpectOk(badScheme.Start(request));
  EXPECT_FALSE(badScheme.Advance(nullptr, 200,
    DiscoveryResponse("ftp://example.org/api/v1"), request, response,
    complete).IsOK());
}

TEST(TapeRestApi, RejectsNativeXRootdSchemes)
{
  XrdCl::Buffer arg;
  arg.FromString("tape.discover");
  for(const char *scheme : {"root", "roots", "xroot", "xroots"})
  {
    XrdClHttp::TapeOperation operation(
      std::string(scheme) + "://storage.example.org:1094/store/file",
      XrdCl::QueryCode::Opaque, arg);
    XrdClHttp::TapeHttpRequest request;
    const auto status = operation.Start(request);
    EXPECT_FALSE(status.IsOK());
    EXPECT_EQ(status.code, XrdCl::errInvalidArgs);
    EXPECT_EQ(status.errNo, EINVAL);
  }
}

TEST(TapeRestApi, RejectsArchiveInfoAcrossStorageEndpoints)
{
  XrdCl::Buffer arg;
  arg.FromString("tape.archiveinfo\n"
    "https://storage.example.org:8443/store/file\n"
    "https://other.example.org/store/file");
  XrdClHttp::TapeOperation operation(
    kStorageUrl, XrdCl::QueryCode::Opaque, arg);
  XrdClHttp::TapeHttpRequest request;
  EXPECT_FALSE(operation.Start(request).IsOK());
}

TEST(TapeRestApi, RejectsArchiveInfoOutsideStorageEndpoint)
{
  XrdCl::Buffer arg;
  arg.FromString("tape.archiveinfo\n"
    "https://other.example.org/store/file");
  XrdClHttp::TapeOperation operation(
    kStorageUrl, XrdCl::QueryCode::Opaque, arg);
  XrdClHttp::TapeHttpRequest request;
  EXPECT_FALSE(operation.Start(request).IsOK());
}

TEST(TapeRestApi, RejectsAmbiguousPrepareFlags)
{
  XrdClHttp::TapeOperation mixed(kStorageUrl, {"/store/file"},
    XrdCl::PrepareFlags::Stage | XrdCl::PrepareFlags::Cancel);
  XrdClHttp::TapeHttpRequest request;
  EXPECT_FALSE(mixed.Start(request).IsOK());

  XrdClHttp::TapeOperation unsupported(kStorageUrl, {"/store/file"},
    XrdCl::PrepareFlags::Stage | XrdCl::PrepareFlags::WriteMode);
  EXPECT_FALSE(unsupported.Start(request).IsOK());
}

TEST(TapeRestApi, RejectsMalformedInputs)
{
  XrdClHttp::TapeOperation emptyStage(kStorageUrl,
    std::vector<std::string>{},
    XrdCl::PrepareFlags::Stage);
  XrdClHttp::TapeHttpRequest request;
  EXPECT_FALSE(emptyStage.Start(request).IsOK());

  XrdClHttp::TapeOperation malformedStage(kStorageUrl,
    {"xrdclhttp.tape.stage:not-json"}, XrdCl::PrepareFlags::Stage);
  EXPECT_FALSE(malformedStage.Start(request).IsOK());

  XrdClHttp::TapeOperation missingStagePath(kStorageUrl,
    {R"(xrdclhttp.tape.stage:{"diskLifetime":"PT1H"})"},
    XrdCl::PrepareFlags::Stage);
  EXPECT_FALSE(missingStagePath.Start(request).IsOK());

  XrdClHttp::TapeOperation invalidStageMetadata(kStorageUrl,
    {R"(xrdclhttp.tape.stage:{"path":"/store/file",)"
     R"("targetedMetadata":["analysis"]})"},
    XrdCl::PrepareFlags::Stage);
  EXPECT_FALSE(invalidStageMetadata.Start(request).IsOK());

  XrdClHttp::TapeOperation incompleteCancel(kStorageUrl,
    {"request-1"}, XrdCl::PrepareFlags::Cancel);
  EXPECT_FALSE(incompleteCancel.Start(request).IsOK());

  XrdClHttp::TapeOperation emptyCancelRequest(kStorageUrl,
    {"", "/store/file"}, XrdCl::PrepareFlags::Cancel);
  EXPECT_FALSE(emptyCancelRequest.Start(request).IsOK());

  XrdClHttp::TapeOperation lineBreakReleaseRequest(kStorageUrl,
    {"request\n1", "/store/file"}, XrdCl::PrepareFlags::Evict);
  EXPECT_FALSE(lineBreakReleaseRequest.Start(request).IsOK());

  XrdCl::Buffer arg;
  arg.FromString("request-1\nextra");
  XrdClHttp::TapeOperation malformedQuery(
    kStorageUrl, XrdCl::QueryCode::Prepare, arg);
  EXPECT_FALSE(malformedQuery.Start(request).IsOK());

  arg.FromString("");
  XrdClHttp::TapeOperation emptyPrepareQuery(
    kStorageUrl, XrdCl::QueryCode::Prepare, arg);
  EXPECT_FALSE(emptyPrepareQuery.Start(request).IsOK());

  arg.FromString("request\r1");
  XrdClHttp::TapeOperation carriageReturnQuery(
    kStorageUrl, XrdCl::QueryCode::Prepare, arg);
  EXPECT_FALSE(carriageReturnQuery.Start(request).IsOK());

  arg.FromString("tape.unknown_command");
  XrdClHttp::TapeOperation unknown(
    kStorageUrl, XrdCl::QueryCode::Opaque, arg);
  EXPECT_FALSE(unknown.Start(request).IsOK());

  arg.FromString("");
  XrdClHttp::TapeOperation emptyOpaque(
    kStorageUrl, XrdCl::QueryCode::Opaque, arg);
  EXPECT_FALSE(emptyOpaque.Start(request).IsOK());

  arg.FromString("tape.archiveinfo\n");
  XrdClHttp::TapeOperation emptyArchiveInfo(
    kStorageUrl, XrdCl::QueryCode::Opaque, arg);
  EXPECT_FALSE(emptyArchiveInfo.Start(request).IsOK());

  arg.FromString("tape.archiveinfo\n/store/file\r");
  XrdClHttp::TapeOperation carriageReturnArchiveInfo(
    kStorageUrl, XrdCl::QueryCode::Opaque, arg);
  EXPECT_FALSE(carriageReturnArchiveInfo.Start(request).IsOK());

  arg.FromString("tape.stage_delete\nrequest-1\nextra");
  XrdClHttp::TapeOperation extraDeleteArgument(
    kStorageUrl, XrdCl::QueryCode::Opaque, arg);
  EXPECT_FALSE(extraDeleteArgument.Start(request).IsOK());

  arg.FromString("tape.stage_delete\nrequest\r1");
  XrdClHttp::TapeOperation carriageReturnDelete(
    kStorageUrl, XrdCl::QueryCode::Opaque, arg);
  EXPECT_FALSE(carriageReturnDelete.Start(request).IsOK());
}

TEST(TapeRestApi, RejectsConflictingStageStatusFields)
{
  CurlHandle curl(XrdClHttp::GetHandle(false), curl_easy_cleanup);
  ASSERT_NE(curl, nullptr);
  XrdCl::Buffer arg;
  arg.FromString("request-1");
  XrdClHttp::TapeOperation operation(
    kStorageUrl, XrdCl::QueryCode::Prepare, arg);
  XrdClHttp::TapeHttpRequest request;
  StartAndDiscover(operation, curl.get(), request);

  std::string response;
  bool complete = false;
  const auto status = operation.Advance(curl.get(), 200,
    R"({"id":"request-1","createdAt":1,"startedAt":1,)"
    R"("files":[{"path":"/store/file","onDisk":true,)"
    R"("state":"COMPLETED"}]})",
    request, response, complete);
  EXPECT_FALSE(status.IsOK());
  EXPECT_EQ(status.errNo, EBADMSG);
}

TEST(TapeRestApi, PreservesProblemResponseDetails)
{
  EXPECT_EQ(XrdClHttp::TapeProblemResponse(400,
    R"({"title":"invalid request","detail":"missing files"})"),
    "HTTP 400: invalid request - missing files");

  const std::string response = XrdClHttp::TapeProblemResponse(
    500, std::string(5000, 'x'));
  EXPECT_EQ(response, "HTTP 500: " + std::string(5000, 'x'));

  EXPECT_EQ(XrdClHttp::TapeProblemResponse(400, "not-json"),
            "HTTP 400: not-json");
}
