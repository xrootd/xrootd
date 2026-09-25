#undef NDEBUG

#include "XrdHttpTpc/XrdHttpTpcUtils.hh"
#include "XrdHttpTpc/XrdHttpTpcTPC.hh"
#include "XrdHttpTpc/XrdHttpTpcTPCCheck.hh"

#include <exception>
#include <gtest/gtest.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <map>
#include <string>
#include <vector>

using namespace testing;

class XrdHttpTpcTests : public Test {};

TEST(XrdHttpTpcTests, prepareOpenURLTest) {
  std::string resource = "/eos/test/file.txt";
  std::map<std::string,std::string> empty {};
  {
    // Nothing to set in the openURL
    std::map<std::string, std::string> headers {{"Test","Test"}};
    XrdHttpTpcUtils::PrepareOpenURLParams params{resource,headers, empty,empty};
    auto openURL = XrdHttpTpcUtils::prepareOpenURL(params);

    ASSERT_EQ(resource + "?" + TPC::TPCHandler::OSS_TASK_OPAQUE.data(),openURL);
  }

  {
    // If authz= was put in the opaque of the resource (and therefore put in the xrd-http-query header),
    // Then the authorization header should have been set and no "authz" should be found in the opaque of
    // the open URL
    std::map<std::string, std::string> headers {{"xrd-http-query","authz=test&scitag.flow=144"}};
    XrdHttpTpcUtils::PrepareOpenURLParams params{resource,headers, empty,empty};
    auto openURL = XrdHttpTpcUtils::prepareOpenURL(params);

    ASSERT_NE(resource,openURL);
    ASSERT_TRUE(headers.find("Authorization") != headers.end());
    ASSERT_TRUE(openURL.find("&authz") == std::string::npos);
    ASSERT_TRUE(openURL.find("&scitag.flow") != std::string::npos);
    ASSERT_TRUE(openURL.find("cks.type") == std::string::npos);
    ASSERT_TRUE(openURL.find(std::string("?") + TPC::TPCHandler::OSS_TASK_OPAQUE.data()) != std::string::npos);
  }

  {
    // If authz= was put in the opaque of the resource (and therefore put in the xrd-http-query header),
    // and if the the authorization header is provided, we should not override the provided authorization header
    std::map<std::string, std::string> headers {{"xrd-http-query","authz=test&scitag.flow=144"},{"Authorization","abcd"}};
    XrdHttpTpcUtils::PrepareOpenURLParams params{resource,headers, empty,empty};
    auto openURL = XrdHttpTpcUtils::prepareOpenURL(params);

    ASSERT_NE(resource,openURL);
    ASSERT_TRUE(openURL.find("&authz") == std::string::npos);
    ASSERT_TRUE(openURL.find("&scitag.flow") != std::string::npos);
    ASSERT_EQ("abcd",headers["Authorization"]);
  }

  {
    // Some hdr2cgi has been configured, we should find them in the opaque of the openURL
    std::map<std::string, std::string> headers {{"xrd-http-query","authz=test&test1=test2"},{"Scitag","144"},{"lowercase_header","test1"} };
    std::map<std::string, std::string> hdr2cgi {{"SciTag","scitag.flow"},{"LOWERCASE_HEADER","lowercase"}};
    std::map<std::string, std::string> reprDigest {{"adler32","adler32val"},{"sha256","sha256val"}};
    XrdHttpTpcUtils::PrepareOpenURLParams params{resource,headers,hdr2cgi,reprDigest};
    auto openURL = XrdHttpTpcUtils::prepareOpenURL(params);

    ASSERT_TRUE(openURL.find("&test1=test2") != std::string::npos);
    ASSERT_TRUE(openURL.find("&scitag.flow=144") != std::string::npos);
    ASSERT_TRUE(openURL.find("&lowercase=test1") != std::string::npos);
    ASSERT_TRUE(openURL.find("&cks.type=adler32&cks.value=adler32val") != std::string::npos);
    ASSERT_TRUE(openURL.find(std::string("?") + TPC::TPCHandler::OSS_TASK_OPAQUE.data()) != std::string::npos);
  }
}

/* A COPY whose two ends designate the same file truncates it before anything is
 * read back from it, so it has to be rejected. Whether the remote end is this
 * very server is told by the Host header, by the addresses it resolves to, or
 * by the address the request came in on.
 */

static const std::string copy_path = "/srvdata/tpc/file.ref";
static const std::string copy_host = "myinstance.cern.ch";

struct CopyCase {
  std::string remoteURL;
  std::string requestPath;
  std::string requestHost;
  bool isCopyOntoItself;
  std::string description;
};

/* Cases the Host header alone decides, the socket being left unknown. */

static const CopyCase host_header_cases[] = {
  { "https://" + copy_host + copy_path, copy_path, copy_host, true,
    "a file copied onto itself" },
  { "https://" + copy_host + copy_path, copy_path + ".copy", copy_host, false,
    "another file of the same server" },
  { "https://myinstance1.cern.ch" + copy_path, copy_path, "myinstance2.cern.ch", false,
    "the same path on two servers of the same instance" },
  { "https://MyInstance.CERN.ch" + copy_path, copy_path, copy_host, true,
    "a host name is not case sensitive" },
  { "https://" + copy_host + ":443" + copy_path, copy_path, copy_host, true,
    "the default port of https, written out" },
  { "http://" + copy_host + copy_path, copy_path, copy_host + ":80", true,
    "the default port of http, written out in the header" },
  { "http://" + copy_host + ":443" + copy_path, copy_path, copy_host, false,
    "the default port of a scheme is not the default port of the other one" },
  { "https://" + copy_host + ":8443" + copy_path, copy_path, copy_host, false,
    "a server on another port is named with it on both sides" },
  { "https://localhost:10951" + copy_path, copy_path, "localhost:10952", false,
    "two instances of the same host are told apart by their port" },
  { "https://localhost:10951" + copy_path, copy_path, "localhost:10951", true,
    "two ends of the same instance of a host" },
  { "https://" + copy_host + copy_path + "?authz=Bearer%20token", copy_path, copy_host,
    true, "a query string is not part of the identity of a file" },
  { "https://" + copy_host + "/srvdata/tpc/a%20file.ref", "/srvdata/tpc/a file.ref",
    copy_host, true, "a quoted path" },
  { "https://" + copy_host + "/" + copy_path, copy_path, copy_host, true,
    "duplicate slashes, which XrdHttpReq::parseResource() squashes" },
  { "https://user:password@" + copy_host + copy_path, copy_path, copy_host, true,
    "userinfo is not part of the identity of a server" },
  { "https://" + copy_host + copy_path, copy_path, "", false,
    "an HTTP/1.0 request, which carries no Host header" },
  { "not-a-url", copy_path, copy_host, false,
    "a URL with no scheme" },
  { "https://", copy_path, copy_host, false,
    "a URL with no authority" },
};

TEST(XrdHttpTpcTests, isCopyOntoItselfHostHeader)
{
  for (const CopyCase &test : host_header_cases)
    ASSERT_EQ(test.isCopyOntoItself,
              XrdHttpTpcTPCCheck::isCopyOntoItself(test.remoteURL, test.requestPath,
                                                   test.requestHost, -1))
      << test.description;
}

/* Cases decided by the addresses the Host header resolves to, the socket being
 * left unknown and the two names being different.
 */

TEST(XrdHttpTpcTests, isCopyOntoItselfHostAddresses)
{
  const std::vector<CopyCase> address_cases = {
    { "https://127.0.0.1:10951" + copy_path, copy_path, "localhost:10951", true,
      "an address of the host named by the Host header" },
    { "https://localhost:10951" + copy_path, copy_path, "127.0.0.1:10951", true,
      "a name resolving to the address in the Host header" },
    { "https://127.0.0.1:10952" + copy_path, copy_path, "localhost:10951", false,
      "the same address on another port is another server" },
    { "https://127.0.0.1:10951" + copy_path, copy_path + ".copy", "localhost:10951",
      false, "another file of the same server" },
  };

  for (const CopyCase &test : address_cases)
    ASSERT_EQ(test.isCopyOntoItself,
              XrdHttpTpcTPCCheck::isCopyOntoItself(test.remoteURL, test.requestPath,
                                                   test.requestHost, -1))
      << test.description;
}

/* A connection to a listening socket of the loopback interface, so that the
 * address comparison can be exercised against a socket the kernel knows about.
 */

class LoopbackConnection {
public:
  LoopbackConnection() {
    mListen = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0; // any free port

    socklen_t addrLen = sizeof(addr);
    if (bind(mListen, reinterpret_cast<sockaddr *>(&addr), addrLen) ||
        listen(mListen, 1) ||
        getsockname(mListen, reinterpret_cast<sockaddr *>(&addr), &addrLen))
      return;

    mPort = ntohs(addr.sin_port);

    mClient = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(mClient, reinterpret_cast<sockaddr *>(&addr), addrLen))
      return;

    mServer = accept(mListen, nullptr, nullptr);
  }

  ~LoopbackConnection() {
    for (int fd : {mServer, mClient, mListen})
      if (fd >= 0) close(fd);
  }

  /// The socket the connection was accepted on, as a request comes in on
  int serverSocket() const { return mServer; }
  int port() const { return mPort; }

private:
  int mListen = -1;
  int mClient = -1;
  int mServer = -1;
  int mPort = 0;
};

/* Cases the address of the request decides, no Host header being sent. */

TEST(XrdHttpTpcTests, isCopyOntoItselfRequestAddress)
{
  LoopbackConnection connection;
  ASSERT_GE(connection.serverSocket(), 0);

  const std::string url =
    "https://127.0.0.1:" + std::to_string(connection.port()) + copy_path;
  const std::string other_port =
    "https://127.0.0.1:" + std::to_string(connection.port() + 1) + copy_path;

  const std::vector<CopyCase> address_cases = {
    { url, copy_path, "", true,
      "the address and the port the request came in on" },
    { other_port, copy_path, "", false,
      "the same address on another port is another server" },
    { url, copy_path + ".copy", "", false,
      "another file of the same server" },
  };

  for (const CopyCase &test : address_cases)
    ASSERT_EQ(test.isCopyOntoItself,
              XrdHttpTpcTPCCheck::isCopyOntoItself(test.remoteURL, test.requestPath,
                                                   test.requestHost,
                                                   connection.serverSocket()))
      << test.description;
}

/* tpc.dfs: the servers of a domain share our filesystem, so the same path on any
 * of them is the same file.
 */

TEST(XrdHttpTpcTests, isValidDomain)
{
  for (const char *domain : {"example.org", "srv-01.example.org", "localhost", "org"})
    ASSERT_TRUE(XrdHttpTpcTPCCheck::isValidDomain(domain)) << domain;

  for (const char *domain : {"", ".example.org", "example.org.", "example..org",
                             "*.example.org", "https://example.org", "example.org:1094",
                             "srv_1.example.org", "[::1]", "192.168.1.0", "10.0.0.0/8",
                             "Example.org", "example org"})
    ASSERT_FALSE(XrdHttpTpcTPCCheck::isValidDomain(domain)) << domain;
}

TEST(XrdHttpTpcTests, isInDomain)
{
  for (const char *host : {"example.org", "srv.example.org", "a.b.example.org",
                           "SRV.EXAMPLE.ORG", "srv.example.org."})
    ASSERT_TRUE(XrdHttpTpcTPCCheck::isInDomain(host, "example.org")) << host;

  for (const char *host : {"badexample.org", "example.org.bad.net", "org", "",
                           "example.com"})
    ASSERT_FALSE(XrdHttpTpcTPCCheck::isInDomain(host, "example.org")) << host;

  ASSERT_FALSE(XrdHttpTpcTPCCheck::isInDomain("srv.example.org", ""));
}

struct DfsCase {
  std::string remoteURL;
  std::string requestPath;
  std::string dfsDomain;
  bool isCopyOntoItself;
  std::string description;
};

TEST(XrdHttpTpcTests, isCopyOntoItselfSharedFilesystem)
{
  // Neither the name nor the address of srv1 is the one of the remote end
  const std::string host = "srv1.example.org";

  const std::vector<DfsCase> dfs_cases = {
    { "https://srv2.example.org" + copy_path, copy_path, "example.org", true,
      "another server of the domain" },
    { "https://SRV2.Example.ORG:1094" + copy_path, copy_path, "example.org", true,
      "a host name is not case sensitive, the port does not matter" },
    { "https://srv2.example.org" + copy_path, copy_path + ".copy", "example.org",
      false, "another file of the shared filesystem" },
    { "https://srv2.example.org" + copy_path, copy_path, "", false,
      "no tpc.dfs, no shared filesystem" },
    { "https://srv.other.invalid" + copy_path, copy_path, "example.org", false,
      "a name out of the domain which does not resolve" },
    { "https://127.0.0.1:1094" + copy_path, copy_path, "localhost", true,
      "an address whose name is in the domain" },
    { "https://127.0.0.1:1094" + copy_path, copy_path, "example.org", false,
      "an address whose name is out of the domain" },
  };

  for (const DfsCase &test : dfs_cases) {
    std::string errMsg;
    ASSERT_EQ(test.isCopyOntoItself,
              XrdHttpTpcTPCCheck::isCopyOntoItself(test.remoteURL, test.requestPath,
                                                   host, -1, test.dfsDomain, &errMsg))
      << test.description;
    ASSERT_EQ(test.isCopyOntoItself, !errMsg.empty()) << test.description;
    // All of them have a name, the unnamed address has its own test
    ASSERT_EQ(std::string::npos, errMsg.find("has no name")) << test.description;
  }
}

TEST(XrdHttpTpcTests, isCopyOntoItselfUnnamedAddress)
{
  // 192.0.2.0/24 is reserved for documentation, so it has no name: whether it
  // shares our filesystem cannot be told, and the COPY is rejected.
  std::string errMsg;
  ASSERT_TRUE(XrdHttpTpcTPCCheck::isCopyOntoItself("https://192.0.2.1" + copy_path,
                                                   copy_path, "srv1.example.org", -1,
                                                   "example.org", &errMsg));
  ASSERT_NE(std::string::npos, errMsg.find("has no name"));
}

/* Overwrite: F on a push: the destination must be known to be missing. */

TEST(XrdHttpTpcTests, isPushAllowed)
{
  // Overwriting is allowed, whatever the HEAD request found
  for (int status : {200, 404, 403, 500, -1})
    ASSERT_TRUE(XrdHttpTpcTPCCheck::isPushAllowed(true, status)) << status;

  ASSERT_TRUE(XrdHttpTpcTPCCheck::isPushAllowed(false, 404));

  for (int status : {200, 201, 204}) {
    std::string errMsg;
    ASSERT_FALSE(XrdHttpTpcTPCCheck::isPushAllowed(false, status, &errMsg)) << status;
    ASSERT_NE(std::string::npos, errMsg.find("already exists")) << status;
  }

  for (int status : {302, 401, 403, 410, 500, -1}) {
    std::string errMsg;
    ASSERT_FALSE(XrdHttpTpcTPCCheck::isPushAllowed(false, status, &errMsg)) << status;
    ASSERT_NE(std::string::npos, errMsg.find("'Overwrite: F' was supplied")) << status;
    ASSERT_NE(std::string::npos, errMsg.find("could not be verified")) << status;
  }
}
