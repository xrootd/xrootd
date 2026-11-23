// Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
#include "XrdHttp/XrdHttpExtHandler.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucJson.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"

#include <gtest/gtest.h>
#include <algorithm>
#include <arpa/inet.h>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
using Json = nlohmann::json;
using NativeResponse = XrdHttpExtReq::NativeResponse;
constexpr char kId[] = "01234567-89ab-cdef-0123-456789abcdef";
constexpr char kDiscovery[] = "/.well-known/wlcg-tape-rest-api";
const std::string kStatus = std::string("/api/v1/stage/") + kId;
const std::string kRelease = std::string("/api/v1/release/") + kId;

// One transport per test. Native completion is deliberately deferred until the
// original XrdHttpExtReq has been destroyed, as in the real bridge lifecycle.
struct Transport {
  std::map<std::string, std::string> headers;
  std::string body;
  int reads = 0;
  int readSize = 0;
  bool readWait = false;
  int readResult = -2; // Default: expose the next bounded borrowed body view.
  size_t readOffset = 0;
  int chunkSize = INT_MAX;
  std::vector<int> readSizes;
  bool nullBuffer = false;
  int nativeCalls = 0;
  int nativeReturn = XrdHttpExtReq::Pending;
  ClientRequest native{};
  std::string payload;
  size_t maxResponse = 0;
  XrdHttpExtReq::NativeCallback callback;
  int sends = 0;
  int status = 0;
  bool standardReason = false;
  std::string responseHeaders;
  std::string responseBody;
  int sendReturn = 0;
  int starts = 0;
  bool keepAlive = true;
  long long responseLength = 0;
  int dataCalls = 0;
  int dataReturn = 0;
  std::string interim;
};
Transport transport;

// Factory tests must not leave the process environment changed for other tests.
struct SiteEnvironment {
  bool existed;
  std::string value;
  SiteEnvironment() : existed(std::getenv("XRDSITE") != nullptr),
    value(existed ? std::getenv("XRDSITE") : "") {}
  ~SiteEnvironment() {
    if (existed) setenv("XRDSITE", value.c_str(), 1);
    else unsetenv("XRDSITE");
  }
};
} // namespace

// These are link-time transport stubs, not a second adapter implementation.
// Do not link XrdHttpExtHandler.cc into this executable.
XrdHttpExtReq::XrdHttpExtReq(XrdHttpReq *, XrdHttpProtocol *protocol)
  : prot(protocol), headers(transport.headers), length(0), pmark(nullptr),
    mSciTag(0) {}

int XrdHttpExtReq::RunNative(const ClientRequest &request,
                            const std::string &payload,
                            NativeCallback callback, size_t maxResponse)
{
  ++transport.nativeCalls;
  transport.native = request;
  transport.payload = payload;
  transport.maxResponse = maxResponse;
  if (transport.nativeReturn == Pending)
    transport.callback = std::move(callback);
  return transport.nativeReturn;
}

int XrdHttpExtReq::SendSimpleResp(int code, const char *description,
                                const char *headers, const char *body,
                                long long length)
{
  ++transport.sends;
  transport.status = code;
  transport.standardReason = description == nullptr;
  transport.responseHeaders = headers ? headers : "";
  transport.responseBody = body ? std::string(body,
    length < 0 ? std::strlen(body) : static_cast<size_t>(length)) : "";
  return transport.sendReturn;
}

int XrdHttpExtReq::BuffgetData(int size, char **data, bool wait)
{
  ++transport.reads;
  transport.readSize = size;
  transport.readWait = wait;
  transport.readSizes.push_back(size);
  *data = transport.nullBuffer ? nullptr : &transport.body[transport.readOffset];
  if (transport.readResult != -2) return transport.readResult;
  const auto count = std::min({size, transport.chunkSize,
    static_cast<int>(transport.body.size() - transport.readOffset)});
  transport.readOffset += count;
  return count;
}

int XrdHttpExtReq::StartSimpleResp(int code, const char *description,
                                 const char *headers, long long length,
                                 bool keepalive)
{
  ++transport.starts;
  transport.keepAlive = keepalive;
  transport.responseLength = length;
  return SendSimpleResp(code, description, headers, nullptr, 0);
}

int XrdHttpExtReq::SendData(const char *body, int length)
{
  ++transport.dataCalls;
  if (transport.dataReturn < 0) return transport.dataReturn;
  if (transport.starts) transport.responseBody.append(body, length);
  else transport.interim.append(body, length);
  return transport.dataReturn;
}

namespace {
class TapeApiTest : public ::testing::Test {
protected:
  SiteEnvironment savedSite;
  XrdSysLogger logger;
  XrdSysError errors{&logger, "TapeApiTest"};
  XrdOucEnv environment;
  std::unique_ptr<XrdHttpExtHandler> handler;
  std::unique_ptr<XrdHttpExtReq> request;

  void SetUp() override {
    ASSERT_EQ(setenv("XRDSITE", "unit-test-site", 1), 0);
    environment.Put("xrd.prepare.profile", "v1");
    handler = Factory();
    ASSERT_NE(handler, nullptr);
  }

  std::unique_ptr<XrdHttpExtHandler> Factory(const char *options = nullptr) {
    return std::unique_ptr<XrdHttpExtHandler>(
      XrdHttpGetExtHandler(&errors, nullptr, options, &environment));
  }

  void Start(const std::string &verb, const std::string &path,
             const std::string &body = "") {
    request.reset();
    transport = Transport{};
    transport.body = body;
    request.reset(new XrdHttpExtReq(nullptr, nullptr));
    request->verb = verb;
    request->resource = path;
    request->length = body.size();
  }

  void ExpectPending() {
    ASSERT_EQ(handler->ProcessReq(*request), XrdHttpExtReq::Pending);
    EXPECT_EQ(transport.nativeCalls, 1);
    EXPECT_EQ(transport.sends, 0);
    EXPECT_EQ(ntohl(transport.native.header.dlen), transport.payload.size());
    EXPECT_EQ(transport.maxResponse, 4u * 1024 * 1024);
    ASSERT_TRUE(transport.callback);
  }

  void Complete(const std::string &body, NativeResponse::Kind kind = NativeResponse::Success,
                int code = 0) {
    ASSERT_TRUE(transport.callback);
    auto callback = std::move(transport.callback);
    request.reset();
    XrdHttpExtReq completion(nullptr, nullptr);
    NativeResponse response;
    response.kind = kind;
    response.code = code;
    response.data = body;
    EXPECT_EQ(callback(completion, response), transport.sendReturn);
    EXPECT_EQ(transport.sends, 1);
    EXPECT_TRUE(transport.standardReason);
  }

  void ExpectProblem(int status) {
    ASSERT_EQ(transport.status, status);
    EXPECT_EQ(transport.sends, 1);
    EXPECT_TRUE(transport.standardReason);
    EXPECT_EQ(transport.responseHeaders.substr(0, std::strlen("Content-Type: application/problem+json")), "Content-Type: application/problem+json");
    const auto problem = Json::parse(transport.responseBody);
    EXPECT_EQ(problem.at("status"), status);
    ASSERT_TRUE(problem.at("title").is_string());
    EXPECT_FALSE(problem.at("title").get<std::string>().empty());
  }

  void ExpectRejected(int status, bool close = false) {
    EXPECT_EQ(handler->ProcessReq(*request), close ? -1 : transport.sendReturn);
    EXPECT_EQ(transport.nativeCalls, 0);
    EXPECT_FALSE(transport.callback);
    ExpectProblem(status);
    if (close) {
      EXPECT_EQ(transport.starts, 1);
      EXPECT_FALSE(transport.keepAlive);
      EXPECT_EQ(transport.responseLength, transport.responseBody.size());
    }
  }
};

TEST_F(TapeApiTest, StageTranslatesMetadataAndOnlyForwardsAuthorization) {
  Start("POST", "/api/v1/stage", R"({"files":[
    {"path":"//data///one/","diskLifetime":"PT1H",
     "targetedMetadata":{"site":"value"},"ignored":"not forwarded"},
    {"path":"/data/two"}]})");
  request->headers["xrd-http-fullresource"] =
    "/api/v1/stage?ignored=value&authz=synthetic-token&xrd.prepare.request=spoofed";
  ExpectPending();
  EXPECT_EQ(ntohs(transport.native.header.requestid), kXR_prepare);
  EXPECT_EQ(transport.native.prepare.options, kXR_stage);
  EXPECT_EQ(ntohs(transport.native.prepare.optionX), 0);
  // Literal hex encodes only diskLifetime and targetedMetadata, not path or
  // unknown fields. This assertion does not reuse production encoding helpers.
  EXPECT_EQ(transport.payload,
    "/data/one?xrd.prepare.file="
    "7b226469736b4c69666574696d65223a2250543148222c2274617267657465644d65746164617461223a"
    "7b2273697465223a2276616c7565227d7d&authz=synthetic-token\n"
    "/data/two?xrd.prepare.file=7b7d&authz=synthetic-token\n");
  EXPECT_EQ(transport.reads, 1);
  EXPECT_EQ(transport.readSize, request->length);
  EXPECT_TRUE(transport.readWait);
  Complete(std::string(kId) + std::string(2, '\0'));
  EXPECT_EQ(transport.status, 201);
  EXPECT_EQ(Json::parse(transport.responseBody), Json({{"requestId", kId}}));
  EXPECT_EQ(transport.responseHeaders,
    "Content-Type: application/json\r\nLocation: " + kStatus);
}

TEST_F(TapeApiTest, StageDoesNotAddEmptyOrUnrelatedQueryParameters) {
  for (const auto &resource : {"/api/v1/stage", "/api/v1/stage?other=value",
                               "/api/v1/stage?authz="}) {
    SCOPED_TRACE(resource);
    Start("POST", "/api/v1/stage", R"({"files":[{"path":"/a"}]})");
    request->headers["xrd-http-fullresource"] = resource;
    ExpectPending();
    EXPECT_EQ(transport.payload, "/a?xrd.prepare.file=7b7d\n");
    Complete(kId);
    EXPECT_EQ(transport.status, 201);
  }
}

TEST_F(TapeApiTest, StatusUsesPrepareQueryAndPassesStructuredResponse) {
  Start("GET", kStatus);
  ExpectPending();
  EXPECT_EQ(ntohs(transport.native.header.requestid), kXR_query);
  EXPECT_EQ(ntohs(transport.native.query.infotype), kXR_QPrep);
  EXPECT_EQ(transport.payload, std::string(kId) + "\n");
  EXPECT_EQ(transport.reads, 0);
  const Json response = {{"id", kId}, {"createdAt", 100}, {"startedAt", 101},
    {"completedAt", 102}, {"files", Json::array({
    {{"path", "/a"}, {"state", "COMPLETED"}}})}};
  Complete(response.dump() + std::string(1, '\0'));
  EXPECT_EQ(transport.status, 200);
  EXPECT_EQ(Json::parse(transport.responseBody), response);
  EXPECT_EQ(transport.responseHeaders, "Content-Type: application/json");
}

TEST_F(TapeApiTest, CancelUsesPrepareCancelAndSelectedPaths) {
  Start("POST", kStatus + "/cancel", R"({"paths":["//a/","/b"]})");
  request->headers["xrd-http-fullresource"] = kStatus + "/cancel?authz=synthetic";
  ExpectPending();
  EXPECT_EQ(ntohs(transport.native.header.requestid), kXR_prepare);
  EXPECT_EQ(transport.native.prepare.options, kXR_cancel);
  EXPECT_EQ(ntohs(transport.native.prepare.optionX), 0);
  EXPECT_EQ(transport.payload, std::string(kId) + "\n/a?authz=synthetic\n/b?authz=synthetic\n");
  Complete("");
  EXPECT_EQ(transport.status, 200);
  EXPECT_TRUE(transport.responseBody.empty());
}

TEST_F(TapeApiTest, DeleteUsesReservedCancelRequestWithoutPaths) {
  Start("DELETE", kStatus);
  ExpectPending();
  EXPECT_EQ(ntohs(transport.native.header.requestid), kXR_prepare);
  EXPECT_EQ(transport.native.prepare.options, kXR_cancel);
  EXPECT_EQ(ntohs(transport.native.prepare.optionX), 0);
  EXPECT_EQ(transport.payload, std::string("@xrdprep-v1:delete:") + kId + "\n");
  EXPECT_EQ(transport.reads, 0);
  Complete("");
  EXPECT_EQ(transport.status, 200);
  EXPECT_TRUE(transport.responseBody.empty());
}

TEST_F(TapeApiTest, ReleaseUsesExtendedEvictAndRequestIdOnEveryPath) {
  Start("POST", kRelease, R"({"paths":["/a","/b"]})");
  request->headers["xrd-http-fullresource"] = kRelease + "?authz=synthetic&other=ignored";
  ExpectPending();
  EXPECT_EQ(ntohs(transport.native.header.requestid), kXR_prepare);
  EXPECT_EQ(transport.native.prepare.options, 0);
  EXPECT_EQ(ntohs(transport.native.prepare.optionX), kXR_evict);
  EXPECT_EQ(transport.payload,
    std::string("/a?xrd.prepare.request=") + kId + "&authz=synthetic\n" +
    "/b?xrd.prepare.request=" + kId + "&authz=synthetic\n");
  Complete("");
  EXPECT_EQ(transport.status, 200);
  EXPECT_TRUE(transport.responseBody.empty());
}

TEST_F(TapeApiTest, ArchiveInfoUsesReservedQueryAndReturnsArray) {
  Start("POST", "/api/v1/archiveinfo", R"({"paths":["/a","/b"]})");
  ExpectPending();
  EXPECT_EQ(ntohs(transport.native.header.requestid), kXR_query);
  EXPECT_EQ(ntohs(transport.native.query.infotype), kXR_QPrep);
  EXPECT_EQ(transport.payload, "@xrdprep-v1:archiveinfo\n/a\n/b\n");
  const Json response = Json::array({{{"path", "/a"}, {"onTape", true}}});
  Complete(response.dump() + std::string(2, '\0'));
  EXPECT_EQ(transport.status, 200);
  EXPECT_EQ(Json::parse(transport.responseBody), response);
}

TEST_F(TapeApiTest, MapsNativeErrorsToProblemResponses) {
  const std::pair<int, int> cases[] = {
    {kXR_NotAuthorized, 403}, {kXR_NotFound, 404},
    {kXR_ArgInvalid, 400}, {kXR_ArgMissing, 400}, {kXR_ArgTooLong, 413},
    {kXR_Unsupported, 501}, {kXR_overQuota, 429}, {kXR_inProgress, 503},
    {kXR_noserver, 503}, {kXR_Cancelled, 503}, {kXR_ServerError, 500},
    {123456, 500}
  };
  for (const auto &item : cases) {
    SCOPED_TRACE(item.first);
    Start("GET", kStatus);
    ExpectPending();
    Complete("backend \"error\"\nmessage", NativeResponse::Error, item.first);
    ExpectProblem(item.second);
    EXPECT_EQ(Json::parse(transport.responseBody).at("title"), "backend \"error\"\nmessage");
  }
}

TEST_F(TapeApiTest, NativeRedirectIsServiceUnavailable) {
  Start("GET", kStatus);
  ExpectPending();
  Complete("other.example", NativeResponse::Redirect);
  ExpectProblem(503);
}

TEST_F(TapeApiTest, UnavailableBridgeDoesNotPretendToBePending) {
  for (int rc : {-1, 0}) {
    SCOPED_TRACE(rc);
    Start("GET", kStatus);
    transport.nativeReturn = rc;
    transport.sendReturn = -7;
    EXPECT_EQ(handler->ProcessReq(*request), -7);
    EXPECT_EQ(transport.nativeCalls, 1);
    EXPECT_FALSE(transport.callback);
    ExpectProblem(503);
  }
}

TEST_F(TapeApiTest, RejectsInvalidBackendRequestIds) {
  const std::vector<std::string> ids = {"", "not-an-id", std::string(kId) + "\n",
    "01234567-89AB-CDEF-0123-456789ABCDEF", std::string(kId) + "/extra",
    std::string(kId) + "\r\nInjected: value", std::string(kId, 8) + '\0' + kId};
  for (const auto &id : ids) {
    SCOPED_TRACE(id);
    Start("POST", "/api/v1/stage", R"({"files":[{"path":"/a"}]})");
    ExpectPending();
    Complete(id);
    ExpectProblem(502);
  }
}

TEST_F(TapeApiTest, RejectsMalformedOrWrongShapeBackendJson) {
  for (bool archive : {false, true}) {
    const std::vector<std::string> bodies = {"", "{", "null", "42", "\"text\"",
      archive ? "{}" : "[]", "{} trailing"};
    for (const auto &body : bodies) {
      SCOPED_TRACE(archive);
      SCOPED_TRACE(body);
      Start(archive ? "POST" : "GET", archive ? "/api/v1/archiveinfo" : kStatus,
            archive ? R"({"paths":["/a"]})" : "");
      ExpectPending();
      Complete(body);
      ExpectProblem(502);
    }
  }
}

TEST_F(TapeApiTest, RejectsEmbeddedNullInBackendJson) {
  for (bool archive : {false, true}) {
    SCOPED_TRACE(archive);
    Start(archive ? "POST" : "GET", archive ? "/api/v1/archiveinfo" : kStatus,
          archive ? R"({"paths":["/a"]})" : "");
    ExpectPending();
    // A trailing native C-string terminator is allowed; an embedded NUL must
    // not let the JSON parser silently ignore subsequent non-JSON bytes.
    Complete(std::string(archive ? "[]\0junk" : "{}\0junk", 7));
    ExpectProblem(502);
  }
}

TEST_F(TapeApiTest, RejectsInvalidStageBodyBeforeCallingNative) {
  for (const auto &body : {"", "{", "null", "[]", "{}", R"({"files":{}})",
      R"({"files":[]})", R"({"files":["/a"]})", R"({"files":[{}]})",
      R"({"files":[{"path":12}]})", R"({"files":[{"path":""}]})"}) {
    SCOPED_TRACE(body);
    Start("POST", "/api/v1/stage", body);
    ExpectRejected(400);
  }
}

TEST_F(TapeApiTest, RejectsInvalidPathsBodiesOnAllPathsEndpoints) {
  for (const auto &path : {kStatus + "/cancel", kRelease, std::string("/api/v1/archiveinfo")}) {
    for (const auto &body : {"", "{", "null", "[]", "{}", R"({"paths":"/a"})",
        R"({"paths":[]})", R"({"paths":[1]})", R"({"paths":[""]})"}) {
      SCOPED_TRACE(path);
      SCOPED_TRACE(body);
      Start("POST", path, body);
      ExpectRejected(400);
    }
  }
}

TEST_F(TapeApiTest, RejectsUnsafePathsAndNormalizedDuplicates) {
  const std::vector<std::string> paths = {"relative", "/", "/a/../b", "/./a",
    "/a?authz=spoof", "/a#fragment", "/a\nb", "/a\rb", std::string("/a\0b", 4),
    std::string("/a") + char(127), "/" + std::string(1024, 'a')};
  for (bool stage : {false, true}) {
    for (const auto &path : paths) {
      SCOPED_TRACE(path);
      const Json body = stage ? Json({{"files", Json::array({{{"path", path}}})}})
                              : Json({{"paths", Json::array({path})}});
      Start("POST", stage ? "/api/v1/stage" : "/api/v1/archiveinfo", body.dump());
      ExpectRejected(400);
    }
    Start("POST", stage ? "/api/v1/stage" : "/api/v1/archiveinfo",
      stage ? R"({"files":[{"path":"/a"},{"path":"//a/"}]})"
            : R"({"paths":["/a","//a/"]})");
    ExpectRejected(400);
  }
}

TEST_F(TapeApiTest, RejectsInvalidMetadataAndOversizedMetadata) {
  const std::vector<Json> files = {
    {{"path", "/a"}, {"diskLifetime", 42}},
    {{"path", "/a"}, {"diskLifetime", ""}},
    {{"path", "/a"}, {"targetedMetadata", "text"}},
    {{"path", "/a"}, {"targetedMetadata", Json::array()}}
  };
  for (const auto &file : files) {
    SCOPED_TRACE(file.dump());
    Start("POST", "/api/v1/stage", Json({{"files", Json::array({file})}}).dump());
    ExpectRejected(400);
  }
  Start("POST", "/api/v1/stage", Json({{"files", Json::array({
    {{"path", "/a"}, {"targetedMetadata", {{"large", std::string(1024, 'x')}}}}
  })}}).dump());
  ExpectRejected(413);
}

TEST_F(TapeApiTest, EnforcesNativeBatchLimitAtFortyEightFiles) {
  for (bool stage : {false, true}) {
    for (size_t count : {48u, 49u}) {
      SCOPED_TRACE(stage);
      SCOPED_TRACE(count);
      Json files = Json::array();
      for (size_t i = 0; i < count; ++i) {
        const auto path = "/file" + std::to_string(i);
        files.push_back(stage ? Json({{"path", path}}) : Json(path));
      }
      Start("POST", stage ? "/api/v1/stage" : "/api/v1/archiveinfo",
        Json({{stage ? "files" : "paths", files}}).dump());
      if (count == 49) ExpectRejected(413);
      else {
        ExpectPending();
        Complete(stage ? kId : "[]");
        EXPECT_EQ(transport.status, stage ? 201 : 200);
      }
    }
  }
}

TEST_F(TapeApiTest, RejectsInvalidRequestIdsOnEveryIdEndpoint) {
  for (const auto &id : {"bad", "01234567-89AB-CDEF-0123-456789ABCDEF", "", "id/extra"}) {
    for (const auto &route : std::vector<std::pair<std::string, std::string>>{
        {"GET", std::string("/api/v1/stage/") + id},
        {"DELETE", std::string("/api/v1/stage/") + id},
        {"POST", std::string("/api/v1/stage/") + id + "/cancel"},
        {"POST", std::string("/api/v1/release/") + id}}) {
      SCOPED_TRACE(route.second);
      Start(route.first, route.second, R"({"paths":["/a"]})");
      ExpectRejected(404);
    }
  }
}

TEST_F(TapeApiTest, RejectsWrongMethodsAndUnknownPaths) {
  const std::pair<std::string, std::string> routes[] = {
    {"PUT", kDiscovery}, {"GET", "/api/v1/stage"}, {"POST", kStatus},
    {"GET", kStatus + "/cancel"}, {"GET", kRelease}, {"GET", "/api/v1/archiveinfo"}
  };
  for (const auto &route : routes) {
    SCOPED_TRACE(route.second);
    Start(route.first, route.second);
    ExpectRejected(405);
  }
  Start("GET", "/api/v1/unrelated");
  ExpectRejected(404);
}

TEST_F(TapeApiTest, RejectsNegativeAndOversizedLengthsBeforeReading) {
  for (long long length : {-1LL, 4LL * 1024 * 1024 + 1}) {
    Start("POST", "/api/v1/stage");
    request->length = length;
    ExpectRejected(length < 0 ? 400 : 413, true);
    EXPECT_EQ(transport.reads, 0);
  }
}

TEST_F(TapeApiTest, RejectsFailedOverlongAndNullBodyReads) {
  for (int result : {0, -1, 1000}) {
    SCOPED_TRACE(result);
    Start("POST", "/api/v1/stage", R"({"files":[{"path":"/a"}]})");
    transport.readResult = result;
    ExpectRejected(400, true);
    EXPECT_EQ(transport.reads, 1);
  }
  Start("POST", "/api/v1/stage", R"({"files":[{"path":"/a"}]})");
  transport.nullBuffer = true;
  ExpectRejected(400, true);
}

TEST_F(TapeApiTest, FactoryRequestSizeLimitAcceptsExactSize) {
  const std::string body = R"({"files":[{"path":"/a"}]})";
  handler = Factory(std::to_string(body.size()).c_str());
  ASSERT_NE(handler, nullptr);
  Start("POST", "/api/v1/stage", body);
  ExpectPending();
  Complete(kId);
  EXPECT_EQ(transport.status, 201);
  Start("POST", "/api/v1/stage", body + " ");
  ExpectRejected(413, true);
  EXPECT_EQ(transport.reads, 0);
}

TEST_F(TapeApiTest, ReadsPartialBodiesAndAcknowledgesContinueOnce) {
  Start("POST", "/api/v1/stage", R"({"files":[{"path":"/a"}]})");
  transport.chunkSize = 3;
  request->headers["ExPeCt"] = "100-CoNtInUe";
  ExpectPending();
  EXPECT_EQ(transport.payload, "/a?xrd.prepare.file=7b7d\n");
  EXPECT_GT(transport.reads, 1);
  EXPECT_EQ(transport.readOffset, transport.body.size());
  EXPECT_EQ(transport.dataCalls, 1);
  EXPECT_EQ(transport.interim, "HTTP/1.1 100 Continue\r\n\r\n");
  Complete(kId);
  EXPECT_EQ(transport.status, 201);
}

TEST_F(TapeApiTest, BoundsReadSizeAndRejectsTruncatedBody) {
  const std::string body = R"({"files":[{"path":"/a"}]})" + std::string(70000, ' ');
  Start("POST", "/api/v1/stage", body);
  ExpectPending();
  ASSERT_EQ(transport.readSizes.size(), 2u);
  EXPECT_EQ(transport.readSizes[0], 64 * 1024);
  EXPECT_EQ(transport.readSizes[1], body.size() - 64 * 1024);
  Complete(kId);
  Start("POST", "/api/v1/stage", body);
  ++request->length;
  ExpectRejected(400, true);
  EXPECT_EQ(transport.reads, 3);
}

TEST_F(TapeApiTest, RejectsTransferEncodingWithoutReadingOrSendingContinue) {
  for (const auto &verb : {"POST", "GET"}) {
    SCOPED_TRACE(verb);
    Start(verb, "/api/v1/stage", R"({"files":[{"path":"/a"}]})");
    request->headers["TrAnSfEr-EnCoDiNg"] = "chunked";
    request->headers["Expect"] = "100-continue";
    ExpectRejected(411, true);
    EXPECT_EQ(transport.reads, 0);
    EXPECT_TRUE(transport.interim.empty());
  }
}

TEST_F(TapeApiTest, FailedContinueOrErrorHeaderSendClosesConnection) {
  Start("POST", "/api/v1/stage", R"({"files":[{"path":"/a"}]})");
  request->headers["Expect"] = "100-continue";
  transport.dataReturn = -1;
  EXPECT_EQ(handler->ProcessReq(*request), -1);
  EXPECT_EQ(transport.status, 400);
  EXPECT_FALSE(transport.keepAlive);
  EXPECT_EQ(transport.nativeCalls, 0);
  EXPECT_EQ(transport.reads, 0);
  Start("POST", "/api/v1/stage");
  request->length = -1;
  transport.sendReturn = -1;
  EXPECT_EQ(handler->ProcessReq(*request), -1);
  EXPECT_EQ(transport.starts, 1);
  EXPECT_FALSE(transport.keepAlive);
  EXPECT_EQ(transport.dataCalls, 0);
  EXPECT_EQ(transport.nativeCalls, 0);
}

TEST_F(TapeApiTest, HeadErrorHasHeadersButNoBody) {
  for (long long length : {0LL, -1LL}) {
    Start("HEAD", kStatus);
    request->length = length;
    EXPECT_EQ(handler->ProcessReq(*request), length < 0 ? -1 : 0);
    EXPECT_EQ(transport.status, length < 0 ? 400 : 405);
    EXPECT_EQ(transport.starts, 1);
    EXPECT_EQ(transport.keepAlive, length == 0);
    EXPECT_GT(transport.responseLength, 0);
    EXPECT_TRUE(transport.responseBody.empty());
    EXPECT_EQ(transport.dataCalls, 0);
    EXPECT_EQ(transport.nativeCalls, 0);
  }
}

TEST_F(TapeApiTest, FactoryRequiresDurableProfileV1) {
  std::unique_ptr<XrdHttpExtHandler> missing(
    XrdHttpGetExtHandler(&errors, nullptr, nullptr, nullptr));
  EXPECT_EQ(missing, nullptr);
  for (const auto &profile : {"", "v0", "v2"}) {
    SCOPED_TRACE(profile);
    environment.Put("xrd.prepare.profile", profile);
    EXPECT_EQ(Factory(), nullptr);
  }
  XrdOucEnv empty;
  std::unique_ptr<XrdHttpExtHandler> unset(
    XrdHttpGetExtHandler(&errors, nullptr, nullptr, &empty));
  EXPECT_EQ(unset, nullptr);
}

TEST_F(TapeApiTest, FactoryRequiresNonemptySiteName) {
  ASSERT_EQ(unsetenv("XRDSITE"), 0);
  EXPECT_EQ(Factory(), nullptr);
  ASSERT_EQ(setenv("XRDSITE", "", 1), 0);
  EXPECT_EQ(Factory(), nullptr);
}

TEST_F(TapeApiTest, FactoryValidatesSizeOptions) {
  for (const auto &option : {"", "1", "1k", "4m", "4194304"}) {
    SCOPED_TRACE(option);
    EXPECT_NE(Factory(option), nullptr);
  }
  for (const auto &option : {"0", "-1", "nonsense", "4194305", "5m"}) {
    SCOPED_TRACE(option);
    EXPECT_EQ(Factory(option), nullptr);
  }
}

TEST_F(TapeApiTest, MatchesOnlyTapeRoutesAndDiscoveryDoesNotRequireBridge) {
  auto *bridge = dynamic_cast<XrdHttpExtHandlerBridge *>(handler.get());
  ASSERT_NE(bridge, nullptr);
  for (const auto &path : {std::string(kDiscovery), std::string("/api/v1/stage"),
      kStatus, kStatus + "/cancel", kRelease, std::string("/api/v1/archiveinfo")}) {
    SCOPED_TRACE(path);
    EXPECT_TRUE(handler->MatchesPath("GET", path.c_str()));
    EXPECT_TRUE(handler->MatchesPath("POST", path.c_str()));
    EXPECT_EQ(bridge->RequiresBridge("GET", path.c_str()), path != kDiscovery);
  }
  for (const auto &path : {"/", "/api/v1/stages", "/api/v1/release",
      "/api/v1/archiveinfo/extra", "/.well-known/wlcg-tape-rest-api/extra"}) {
    SCOPED_TRACE(path);
    EXPECT_FALSE(handler->MatchesPath("GET", path));
  }
  EXPECT_FALSE(handler->MatchesPath("GET", nullptr));
  EXPECT_FALSE(bridge->RequiresBridge("GET", nullptr));
}

TEST_F(TapeApiTest, DiscoveryUsesConfiguredSiteCaseInsensitiveHostAndProtocol) {
  for (const auto &protocol : {"http", "https"}) {
    SCOPED_TRACE(protocol);
    Start("GET", kDiscovery);
    request->headers["hOsT"] = "tape.example:8443";
    request->headers["xrd-http-prot"] = protocol;
    EXPECT_EQ(handler->ProcessReq(*request), 0);
    EXPECT_EQ(transport.nativeCalls, 0);
    EXPECT_EQ(transport.reads, 0);
    EXPECT_EQ(transport.sends, 1);
    EXPECT_EQ(transport.status, 200);
    EXPECT_TRUE(transport.standardReason);
    EXPECT_EQ(transport.responseHeaders, "Content-Type: application/json");
    const Json expected = {{"sitename", "unit-test-site"},
      {"endpoints", Json::array({{{"uri", std::string(protocol) + "://tape.example:8443/api/v1"},
                                {"version", "v1"}}})}};
    EXPECT_EQ(Json::parse(transport.responseBody), expected);
  }
}

TEST_F(TapeApiTest, DiscoveryRejectsMissingAndEmptyHost) {
  Start("GET", kDiscovery);
  request->headers["xrd-http-prot"] = "https";
  ExpectRejected(400);
  Start("GET", kDiscovery);
  request->headers["host"] = "";
  request->headers["xrd-http-prot"] = "https";
  ExpectRejected(400);
}
} // namespace

TEST_F(TapeApiTest, RejectsRawNullInEveryJsonRouteWithoutDispatch) {
  for (const auto &path : {std::string("/api/v1/stage"), kStatus + "/cancel",
                           kRelease, std::string("/api/v1/archiveinfo")}) {
    std::string body = path == "/api/v1/stage" ? R"({"files":[{"path":"/a"}]})" : R"({"paths":["/a"]})";
    body += '\0'; body += "ignored trailing bytes";
    Start("POST", path, body);
    ExpectRejected(400);
    EXPECT_EQ(transport.nativeCalls, 0);
  }
}

TEST_F(TapeApiTest, EscapedNullIsAllowedInApplicationMetadata) {
  Start("POST", "/api/v1/stage", R"({"files":[{"path":"/a","targetedMetadata":{"note":"before\u0000after"}}]})");
  ExpectPending();
}

TEST_F(TapeApiTest, MethodErrorsAdvertiseRouteMethodsIncludingHead) {
  const std::vector<std::pair<std::string, std::string>> routes = {
    {kDiscovery, "GET"}, {"/api/v1/stage", "POST"}, {kStatus, "GET, DELETE"},
    {kStatus + "/cancel", "POST"}, {kRelease, "POST"}, {"/api/v1/archiveinfo", "POST"}
  };
  for (const auto &route : routes) {
    for (const auto &verb : {"PUT", "HEAD"}) {
      Start(verb, route.first);
      EXPECT_EQ(handler->ProcessReq(*request), 0);
      EXPECT_EQ(transport.status, 405);
      EXPECT_NE(transport.responseHeaders.find("\r\nAllow: " + route.second), std::string::npos);
      EXPECT_EQ(transport.nativeCalls, 0);
      if (std::string(verb) == "HEAD") {
        EXPECT_TRUE(transport.responseBody.empty());
        EXPECT_GT(transport.responseLength, 0);
      } else EXPECT_FALSE(transport.responseBody.empty());
    }
  }
}
