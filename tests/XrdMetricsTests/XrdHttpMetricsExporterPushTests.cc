//------------------------------------------------------------------------------
// End-to-end test of the XrdHttpMetricsExporter HTTP transport. It stands up a
// loopback HTTP receiver, serializes a small registry, and pushes it through
// the same XrdHttpMetricsExporterPush::Send() helper the plugin's background
// loops use, then asserts the request the receiver actually saw: the verb,
// path, Content-Type, and that the serialized body arrived intact. This closes
// the gap left by the serializer unit tests, which cover the document but not
// the curl wiring (method/content-type/url) or the wire round-trip.
//
// Compiled only when libcurl is available (same condition as the plugin's push
// support); the registry/serializer pieces come from XrdUtils.
//------------------------------------------------------------------------------

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cctype>
#include <cstring>
#include <string>
#include <thread>

#include <curl/curl.h>

#include "XrdHttpMetricsExporter/XrdHttpMetricsExporterPush.hh"
#include "XrdMetrics/XrdMetricsConfig.hh"
#include "XrdMetrics/XrdMetricsRegistry.hh"
#include "XrdMetrics/XrdMetricsSerializer.hh"

#include <gtest/gtest.h>

using namespace XrdMetrics;

namespace
{
const char *kPromType = "Content-Type: text/plain; version=0.0.4; charset=utf-8";
const char *kOtelType = "Content-Type: application/json";

bool contains(const std::string& hay, const std::string& needle)
{
   return hay.find(needle) != std::string::npos;
}

std::string lower(std::string s)
{
   for (char& c : s) c = (char)std::tolower((unsigned char)c);
   return s;
}

// A one-shot loopback HTTP/1.1 receiver: binds an ephemeral port on 127.0.0.1,
// accepts a single connection on a background thread, reads the full request
// (headers + Content-Length body), replies 200, and exposes what it saw.
struct MockServer
{
   int          listenFd = -1;
   int          port     = 0;
   std::thread  th;
   std::string  reqLine;   // e.g. "POST /v1/metrics HTTP/1.1"
   std::string  headers;   // header block, lower-cased
   std::string  body;      // request body
   bool         got       = false;

   void start()
   {
      listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
      ASSERT_GE(listenFd, 0);
      int one = 1;
      ::setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      addr.sin_port = 0;   // ephemeral
      ASSERT_EQ(::bind(listenFd, (sockaddr*)&addr, sizeof addr), 0);
      ASSERT_EQ(::listen(listenFd, 1), 0);

      socklen_t len = sizeof addr;
      ASSERT_EQ(::getsockname(listenFd, (sockaddr*)&addr, &len), 0);
      port = ntohs(addr.sin_port);

      // Bound accept() so a failed client can never hang the test.
      timeval tv{5, 0};
      ::setsockopt(listenFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

      th = std::thread([this]{ serveOne(); });
   }

   void serveOne()
   {
      int fd = ::accept(listenFd, nullptr, nullptr);
      if (fd < 0) return;
      timeval tv{5, 0};
      ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

      std::string buf;
      char tmp[4096];
      size_t hdrEnd = std::string::npos;
      long   clen   = -1;
      for (;;)
         {ssize_t n = ::recv(fd, tmp, sizeof tmp, 0);
          if (n <= 0) break;
          buf.append(tmp, (size_t)n);
          if (hdrEnd == std::string::npos)
             {size_t p = buf.find("\r\n\r\n");
              if (p != std::string::npos)
                 {hdrEnd = p + 4;
                  std::string h = lower(buf.substr(0, p));
                  size_t cl = h.find("content-length:");
                  if (cl != std::string::npos)
                     clen = std::atol(h.c_str() + cl + 15);
                 }
             }
          if (hdrEnd != std::string::npos)
             {size_t have = buf.size() - hdrEnd;
              if (clen < 0 || have >= (size_t)clen) break;
             }
         }

      const char *resp = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
      ssize_t w = ::send(fd, resp, std::strlen(resp), 0);
      (void)w;
      ::close(fd);

      if (hdrEnd != std::string::npos)
         {size_t eol = buf.find("\r\n");
          reqLine = buf.substr(0, eol);
          headers = lower(buf.substr(0, hdrEnd));
          body    = buf.substr(hdrEnd);
          got     = true;
         }
   }

   std::string url(const std::string& path) const
   {
      return "http://127.0.0.1:" + std::to_string(port) + path;
   }

   ~MockServer()
   {
      if (th.joinable()) th.join();
      if (listenFd >= 0) ::close(listenFd);
   }
};
}

TEST(MetricsExporterPush, OtelPostRoundTrip)
{
   Collector collector("xrootd");
   collector.subsystem("sched").counter<std::uint64_t>("jobs_total", "jobs scheduled")
                      += 7;
   std::string payload;
   OtelJsonSerializer ser(payload, "xrootd");
   collector.serialize(ser);

   MockServer srv;
   srv.start();
   const std::string url = srv.url("/v1/metrics");

   curl_global_init(CURL_GLOBAL_DEFAULT);
   CURL *curl = curl_easy_init();
   ASSERT_NE(curl, nullptr);
   CURLcode rc = XrdHttpMetricsExporterPush::Send(curl, url, kOtelType,
                                                  /*put=*/false, payload);
   curl_easy_cleanup(curl);
   srv.th.join();

   EXPECT_EQ(rc, CURLE_OK);
   ASSERT_TRUE(srv.got);
   EXPECT_EQ(srv.reqLine, "POST /v1/metrics HTTP/1.1");
   EXPECT_TRUE(contains(srv.headers, "content-type: application/json"));
   EXPECT_TRUE(contains(srv.body, "\"resourceMetrics\""));
   EXPECT_TRUE(contains(srv.body, "\"name\":\"xrootd_sched_jobs_total\""));
   EXPECT_EQ(srv.body, payload);   // body delivered byte-for-byte
}

TEST(MetricsExporterPush, PushgatewayPutRoundTrip)
{
   Collector collector("xrootd");
   collector.subsystem("sched").counter<std::uint64_t>("jobs_total", "jobs scheduled")
                      += 4;
   std::string payload;
   PrometheusTextSerializer ser(payload);
   collector.serialize(ser);

   MockServer srv;
   srv.start();
   const std::string url = XrdMetrics::Config::PushgatewayURL(
      "http://127.0.0.1:" + std::to_string(srv.port), "xrootd", "node1");

   curl_global_init(CURL_GLOBAL_DEFAULT);
   CURL *curl = curl_easy_init();
   ASSERT_NE(curl, nullptr);
   CURLcode rc = XrdHttpMetricsExporterPush::Send(curl, url, kPromType,
                                                  /*put=*/true, payload);
   curl_easy_cleanup(curl);
   srv.th.join();

   EXPECT_EQ(rc, CURLE_OK);
   ASSERT_TRUE(srv.got);
   EXPECT_EQ(srv.reqLine,
             "PUT /metrics/job/xrootd/instance/node1 HTTP/1.1");
   EXPECT_TRUE(contains(srv.headers, "content-type: text/plain; version=0.0.4"));
   EXPECT_TRUE(contains(srv.body, "xrootd_sched_jobs_total"));
   EXPECT_EQ(srv.body, payload);
}
