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
#include "XrdClHttp/XrdClHttpFilesystem.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdOuc/XrdOucJson.hh"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <sstream>
#include <string>
#include <stdexcept>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using Json = nlohmann::json;

namespace
{
struct HttpRequest
{
  std::string method;
  std::string path;
  std::string body;
};

class UniqueFd
{
  public:
    explicit UniqueFd(int fd = -1) : pFd(fd) {}
    ~UniqueFd() { Reset(); }

    UniqueFd(const UniqueFd &) = delete;
    UniqueFd &operator=(const UniqueFd &) = delete;

    UniqueFd(UniqueFd &&other) noexcept : pFd(other.Release()) {}

    UniqueFd &operator=(UniqueFd &&other) noexcept
    {
      if(this != &other) Reset(other.Release());
      return *this;
    }

    int Get() const { return pFd; }
    explicit operator bool() const { return pFd >= 0; }

    int Release()
    {
      const int fd = pFd;
      pFd = -1;
      return fd;
    }

    void Reset(int fd = -1)
    {
      if(pFd >= 0) ::close(pFd);
      pFd = fd;
    }

  private:
    int pFd = -1;
};

class TapeHttpServer
{
  public:
    enum class DiscoveryMode
    {
      Valid,
      Unsupported,
      UnsupportedScheme
    };

    explicit TapeHttpServer(DiscoveryMode mode = DiscoveryMode::Valid)
      : pDiscoveryMode(mode)
    {
      pListenFd.Reset(::socket(AF_INET, SOCK_STREAM, 0));
      if(!pListenFd) throw std::runtime_error("socket failed");

      int reuse = 1;
      ::setsockopt(pListenFd.Get(), SOL_SOCKET, SO_REUSEADDR, &reuse,
                   sizeof(reuse));

      sockaddr_in address;
      std::memset(&address, 0, sizeof(address));
      address.sin_family = AF_INET;
      address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      address.sin_port = 0;

      if(::bind(pListenFd.Get(), reinterpret_cast<sockaddr *>(&address),
                sizeof(address)) < 0)
      {
        throw std::runtime_error("bind failed");
      }

      if(::listen(pListenFd.Get(), 16) < 0)
      {
        throw std::runtime_error("listen failed");
      }

      socklen_t length = sizeof(address);
      if(::getsockname(pListenFd.Get(), reinterpret_cast<sockaddr *>(&address),
                       &length) < 0)
      {
        throw std::runtime_error("getsockname failed");
      }
      pPort = ntohs(address.sin_port);
      pThread = std::thread(&TapeHttpServer::Serve, this);
    }

    ~TapeHttpServer()
    {
      Stop();
    }

    std::string BaseUrl() const
    {
      std::ostringstream out;
      out << "http://127.0.0.1:" << pPort;
      return out.str();
    }

    std::vector<HttpRequest> Requests() const
    {
      std::lock_guard<std::mutex> lock(pMutex);
      return pRequests;
    }

  private:
    static std::string HeaderValue(const std::string &headers,
                                   const std::string &name)
    {
      const std::string lowerName = "\r\n" + name + ":";
      std::string lowerHeaders = headers;
      std::transform(lowerHeaders.begin(), lowerHeaders.end(),
                     lowerHeaders.begin(), [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                     });

      const std::size_t position = lowerHeaders.find(lowerName);
      if(position == std::string::npos) return "";

      std::size_t valueStart = position + lowerName.size();
      while(valueStart < headers.size() && headers[valueStart] == ' ')
      {
        ++valueStart;
      }
      const std::size_t valueEnd = headers.find("\r\n", valueStart);
      return headers.substr(valueStart, valueEnd - valueStart);
    }

    static HttpRequest ParseRequest(const std::string &raw)
    {
      HttpRequest request;
      const std::size_t lineEnd = raw.find("\r\n");
      std::istringstream line(raw.substr(0, lineEnd));
      std::string version;
      line >> request.method >> request.path >> version;

      const std::size_t bodyStart = raw.find("\r\n\r\n");
      if(bodyStart != std::string::npos)
      {
        request.body = raw.substr(bodyStart + 4);
      }
      return request;
    }

    std::string DiscoveryBody() const
    {
      std::ostringstream body;
      body << R"({"sitename":"test-site","endpoints":[)";
      if(pDiscoveryMode == DiscoveryMode::Valid)
      {
        body << R"({"uri":")" << BaseUrl()
             << R"(/api/v1","version":"v1"})";
      }
      else if(pDiscoveryMode == DiscoveryMode::Unsupported)
      {
        body << R"({"uri":")" << BaseUrl()
             << R"(/api/v2","version":"v2"})";
      }
      else
      {
        body << R"({"uri":"ftp://example.org/api/v1","version":"v1"})";
      }
      body << "]}";
      return body.str();
    }

    std::string ResponseBody(const HttpRequest &request, int &status) const
    {
      status = 200;
      if(request.method == "GET"
         && request.path == "/.well-known/wlcg-tape-rest-api")
      {
        return DiscoveryBody();
      }

      if(request.method == "POST" && request.path == "/api/v1/stage")
      {
        status = 201;
        return R"({"requestId":"request-1"})";
      }

      if(request.method == "GET"
         && request.path == "/api/v1/stage/request-1")
      {
        return R"({"id":"request-1","createdAt":1,"startedAt":2,)"
               R"("files":[{"path":"/store/file","onDisk":true,)"
               R"("state":"STARTED","startedAt":3}]})";
      }

      if(request.method == "POST"
         && request.path == "/api/v1/stage/request-1/cancel")
      {
        return "{}";
      }

      if(request.method == "DELETE"
         && request.path == "/api/v1/stage/request-1")
      {
        return "{}";
      }

      if(request.method == "POST"
         && request.path == "/api/v1/release/request-1")
      {
        return "{}";
      }

      if(request.method == "POST" && request.path == "/api/v1/archiveinfo")
      {
        return R"([{"path":"/store/file","locality":"DISK_AND_TAPE"},)"
               R"({"path":"/store/missing","error":"not found"}])";
      }

      status = 404;
      return R"({"title":"not found"})";
    }

    static void SendResponse(int fd, int status, const std::string &body)
    {
      const char *reason = status == 201 ? "Created" :
                           status == 404 ? "Not Found" : "OK";
      std::ostringstream response;
      response << "HTTP/1.1 " << status << ' ' << reason << "\r\n"
               << "Content-Type: application/json\r\n"
               << "Content-Length: " << body.size() << "\r\n"
               << "Connection: close\r\n\r\n"
               << body;
      const std::string data = response.str();
      ::send(fd, data.data(), data.size(), 0);
    }

    void HandleConnection(int fd)
    {
      std::string raw;
      char buffer[4096];
      std::size_t expectedSize = std::string::npos;
      while(true)
      {
        const ssize_t count = ::recv(fd, buffer, sizeof(buffer), 0);
        if(count <= 0) break;
        raw.append(buffer, static_cast<std::size_t>(count));

        const std::size_t headerEnd = raw.find("\r\n\r\n");
        if(headerEnd != std::string::npos && expectedSize == std::string::npos)
        {
          const std::string headers = "\r\n" + raw.substr(0, headerEnd + 2);
          const std::string contentLength =
            HeaderValue(headers, "content-length");
          expectedSize = headerEnd + 4;
          if(!contentLength.empty())
          {
            expectedSize += static_cast<std::size_t>(
              std::strtoul(contentLength.c_str(), nullptr, 10));
          }
        }
        if(expectedSize != std::string::npos && raw.size() >= expectedSize)
        {
          break;
        }
      }

      HttpRequest request = ParseRequest(raw);
      {
        std::lock_guard<std::mutex> lock(pMutex);
        pRequests.push_back(request);
      }

      int status = 200;
      const std::string body = ResponseBody(request, status);
      SendResponse(fd, status, body);
    }

    void Serve()
    {
      while(!pStopped)
      {
        UniqueFd connection(::accept(pListenFd.Get(), nullptr, nullptr));
        if(!connection)
        {
          if(pStopped || errno == EBADF || errno == EINVAL) break;
          continue;
        }
        HandleConnection(connection.Get());
      }
    }

    void Stop()
    {
      bool expected = false;
      if(!pStopped.compare_exchange_strong(expected, true)) return;
      if(pListenFd) ::shutdown(pListenFd.Get(), SHUT_RDWR);
      pListenFd.Reset();
      if(pThread.joinable()) pThread.join();
    }

    DiscoveryMode pDiscoveryMode;
    UniqueFd pListenFd;
    unsigned short pPort = 0;
    std::thread pThread;
    std::atomic<bool> pStopped{false};
    mutable std::mutex pMutex;
    std::vector<HttpRequest> pRequests;
};

const HttpRequest *FindRequest(const std::vector<HttpRequest> &requests,
                               const std::string &method,
                               const std::string &path)
{
  for(const auto &request : requests)
  {
    if(request.method == method && request.path == path)
    {
      return &request;
    }
  }
  return nullptr;
}

void ExpectOk(const XrdCl::XRootDStatus &status)
{
  EXPECT_TRUE(status.IsOK()) << status.ToString();
}

}

TEST(TapeRestApi, DiscoversSupportedEndpoint)
{
  TapeHttpServer server;

  std::string uri;
  std::string version;
  std::string sitename;
  const auto status = XrdClHttp::TapeDiscover(server.BaseUrl() + "/store/file",
                                             5, uri, version, sitename);

  ExpectOk(status);
  EXPECT_EQ(uri, server.BaseUrl() + "/api/v1");
  EXPECT_EQ(version, "v1");
  EXPECT_EQ(sitename, "test-site");

  const std::vector<HttpRequest> requests = server.Requests();
  ASSERT_EQ(requests.size(), 1u);
  EXPECT_EQ(requests[0].method, "GET");
  EXPECT_EQ(requests[0].path, "/.well-known/wlcg-tape-rest-api");
}

TEST(TapeRestApi, RunsStageLifecycle)
{
  TapeHttpServer server;
  const std::string url = server.BaseUrl() + "/store/file";

  std::string requestId;
  ExpectOk(XrdClHttp::TapeStage(
    url, {{{url, "", "3600", R"({"activity":"analysis"})"}}}, 5,
    requestId));
  EXPECT_EQ(requestId, "request-1");

  std::string stageStatusJson;
  ExpectOk(XrdClHttp::TapeStageStatus(url, requestId, 5, stageStatusJson));
  const Json stageStatus = Json::parse(stageStatusJson);
  EXPECT_EQ(stageStatus["id"], "request-1");
  ASSERT_EQ(stageStatus["files"].size(), 1u);
  EXPECT_EQ(stageStatus["files"][0]["path"], "/store/file");
  EXPECT_EQ(stageStatus["files"][0]["onDisk"], true);

  ExpectOk(XrdClHttp::TapeStageCancel(url, requestId, {url}, 5));
  ExpectOk(XrdClHttp::TapeStageDelete(url, requestId, 5));
  ExpectOk(XrdClHttp::TapeRelease(url, requestId, {url}, 5));

  const std::vector<HttpRequest> requests = server.Requests();
  const HttpRequest *stage = FindRequest(requests, "POST", "/api/v1/stage");
  ASSERT_NE(stage, nullptr);
  const Json stageBody = Json::parse(stage->body);
  ASSERT_EQ(stageBody["files"].size(), 1u);
  EXPECT_EQ(stageBody["files"][0]["path"], "/store/file");
  EXPECT_EQ(stageBody["files"][0]["diskLifetime"], "3600");
  EXPECT_EQ(stageBody["files"][0]["targetedMetadata"]["activity"], "analysis");

  const HttpRequest *cancel =
    FindRequest(requests, "POST", "/api/v1/stage/request-1/cancel");
  ASSERT_NE(cancel, nullptr);
  EXPECT_EQ(Json::parse(cancel->body)["paths"][0], "/store/file");

  EXPECT_NE(FindRequest(requests, "DELETE", "/api/v1/stage/request-1"),
            nullptr);

  const HttpRequest *release =
    FindRequest(requests, "POST", "/api/v1/release/request-1");
  ASSERT_NE(release, nullptr);
  EXPECT_EQ(Json::parse(release->body)["paths"][0], "/store/file");
}

TEST(TapeRestApi, PrepareAcceptsStructuredStageEntries)
{
  TapeHttpServer server;
  XrdClHttp::Filesystem filesystem(
    server.BaseUrl(), nullptr, XrdCl::DefaultEnv::GetLog());

  ExpectOk(filesystem.Prepare(
    {R"(xrdclhttp.tape.stage:{"path":"/store/file","diskLifetime":"7200",)"
     R"("targetedMetadata":{"activity":"analysis"}})"},
    XrdCl::PrepareFlags::Stage, 0, nullptr, 5));

  const std::vector<HttpRequest> requests = server.Requests();
  const HttpRequest *stage = FindRequest(requests, "POST", "/api/v1/stage");
  ASSERT_NE(stage, nullptr);
  const Json stageBody = Json::parse(stage->body);
  ASSERT_EQ(stageBody["files"].size(), 1u);
  EXPECT_EQ(stageBody["files"][0]["path"], "/store/file");
  EXPECT_EQ(stageBody["files"][0]["diskLifetime"], "7200");
  EXPECT_EQ(stageBody["files"][0]["targetedMetadata"]["activity"],
            "analysis");
}

TEST(TapeRestApi, RejectsMalformedStructuredStageEntries)
{
  TapeHttpServer server;
  XrdClHttp::Filesystem filesystem(
    server.BaseUrl(), nullptr, XrdCl::DefaultEnv::GetLog());

  EXPECT_FALSE(filesystem.Prepare(
    {"xrdclhttp.tape.stage:not-json"},
    XrdCl::PrepareFlags::Stage, 0, nullptr, 5).IsOK());
  EXPECT_FALSE(filesystem.Prepare(
    {R"(xrdclhttp.tape.stage:{"diskLifetime":"7200"})"},
    XrdCl::PrepareFlags::Stage, 0, nullptr, 5).IsOK());
  EXPECT_FALSE(filesystem.Prepare(
    {R"(xrdclhttp.tape.stage:{"path":"/store/file",)"
     R"("targetedMetadata":["analysis"]})"},
    XrdCl::PrepareFlags::Stage, 0, nullptr, 5).IsOK());

  EXPECT_TRUE(server.Requests().empty());
}

TEST(TapeRestApi, QueriesArchiveInfo)
{
  TapeHttpServer server;
  std::string responseJson;

  ExpectOk(XrdClHttp::TapeArchiveInfo(
    {server.BaseUrl() + "/store/file",
     server.BaseUrl() + "/store/missing"}, 5, responseJson));

  const Json response = Json::parse(responseJson);
  ASSERT_EQ(response.size(), 2u);
  EXPECT_EQ(response[0]["url"], server.BaseUrl() + "/store/file");
  EXPECT_EQ(response[0]["path"], "/store/file");
  EXPECT_EQ(response[0]["locality"], "DISK_AND_TAPE");
  EXPECT_EQ(response[1]["url"], server.BaseUrl() + "/store/missing");
  EXPECT_EQ(response[1]["path"], "/store/missing");
  EXPECT_EQ(response[1]["error"], "not found");

  const std::vector<HttpRequest> requests = server.Requests();
  const HttpRequest *archiveInfo =
    FindRequest(requests, "POST", "/api/v1/archiveinfo");
  ASSERT_NE(archiveInfo, nullptr);
  const Json body = Json::parse(archiveInfo->body);
  ASSERT_EQ(body["paths"].size(), 2u);
  EXPECT_EQ(body["paths"][0], "/store/file");
  EXPECT_EQ(body["paths"][1], "/store/missing");
}

TEST(TapeRestApi, EncodesRequestIdsInUrls)
{
  TapeHttpServer server;
  const std::string url = server.BaseUrl() + "/store/file";
  const std::string requestId =
    std::string("request/1?# x") + static_cast<char>(0xff);
  const std::string encodedRequestId = "request%2F1%3F%23%20x%FF";

  std::string stageStatusJson;
  EXPECT_FALSE(XrdClHttp::TapeStageStatus(
    url, requestId, 5, stageStatusJson).IsOK());
  EXPECT_FALSE(XrdClHttp::TapeStageCancel(
    url, requestId, {url}, 5).IsOK());
  EXPECT_FALSE(XrdClHttp::TapeStageDelete(
    url, requestId, 5).IsOK());
  EXPECT_FALSE(XrdClHttp::TapeRelease(
    url, requestId, {url}, 5).IsOK());

  const std::vector<HttpRequest> requests = server.Requests();
  EXPECT_NE(FindRequest(requests, "GET",
            "/api/v1/stage/" + encodedRequestId), nullptr);
  EXPECT_NE(FindRequest(requests, "POST",
            "/api/v1/stage/" + encodedRequestId + "/cancel"), nullptr);
  EXPECT_NE(FindRequest(requests, "DELETE",
            "/api/v1/stage/" + encodedRequestId), nullptr);
  EXPECT_NE(FindRequest(requests, "POST",
            "/api/v1/release/" + encodedRequestId), nullptr);
}

TEST(TapeRestApi, RejectsUnsupportedDiscoveryEndpoint)
{
  TapeHttpServer server(TapeHttpServer::DiscoveryMode::Unsupported);

  std::string uri;
  std::string version;
  std::string sitename;
  const auto status = XrdClHttp::TapeDiscover(server.BaseUrl() + "/store/file",
                                             5, uri, version, sitename);

  EXPECT_FALSE(status.IsOK());
}

TEST(TapeRestApi, RejectsDiscoveryEndpointWithUnsupportedScheme)
{
  TapeHttpServer server(TapeHttpServer::DiscoveryMode::UnsupportedScheme);

  std::string uri;
  std::string version;
  std::string sitename;
  const auto status = XrdClHttp::TapeDiscover(server.BaseUrl() + "/store/file",
                                             5, uri, version, sitename);

  EXPECT_FALSE(status.IsOK());
}

TEST(TapeRestApi, RejectsArchiveInfoAcrossStorageEndpoints)
{
  TapeHttpServer server;
  std::string responseJson;

  const auto status = XrdClHttp::TapeArchiveInfo(
    {server.BaseUrl() + "/store/file",
     "http://127.0.0.1:1/store/other"}, 5, responseJson);

  EXPECT_FALSE(status.IsOK());
  EXPECT_EQ(responseJson, "[]");

  const std::vector<HttpRequest> requests = server.Requests();
  EXPECT_EQ(FindRequest(requests, "POST", "/api/v1/archiveinfo"), nullptr);
}

TEST(TapeRestApi, RejectsAmbiguousPrepareFlags)
{
  XrdClHttp::Filesystem filesystem(
    "http://127.0.0.1:1", nullptr, XrdCl::DefaultEnv::GetLog());

  const auto mixed = filesystem.Prepare(
    {"/store/file"},
    XrdCl::PrepareFlags::Stage | XrdCl::PrepareFlags::Cancel,
    0, nullptr, 0);
  EXPECT_FALSE(mixed.IsOK());

  const auto unsupported = filesystem.Prepare(
    {"/store/file"},
    XrdCl::PrepareFlags::Stage | XrdCl::PrepareFlags::WriteMode,
    0, nullptr, 0);
  EXPECT_FALSE(unsupported.IsOK());
}

TEST(TapeRestApi, RejectsMalformedOpaqueTapePayloads)
{
  XrdClHttp::Filesystem filesystem(
    "http://127.0.0.1:1", nullptr, XrdCl::DefaultEnv::GetLog());

  XrdCl::Buffer buffer;
  buffer.FromString("request-1\nextra");
  EXPECT_FALSE(filesystem.Query(
    XrdCl::QueryCode::Prepare, buffer, nullptr, 0).IsOK());

  buffer.FromString("tape.archiveinfo\n");
  EXPECT_FALSE(filesystem.Query(
    XrdCl::QueryCode::Opaque, buffer, nullptr, 0).IsOK());

  buffer.FromString("tape.stage_delete\nrequest-1\nextra");
  EXPECT_FALSE(filesystem.Query(
    XrdCl::QueryCode::Opaque, buffer, nullptr, 0).IsOK());
}
