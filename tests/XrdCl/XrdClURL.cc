#undef NDEBUG

#include <XrdCl/XrdClURL.hh>
#include "XrdSys/XrdSysPlatform.hh"

#include <gtest/gtest.h>

#include <climits>
#include <cstdlib>

using namespace testing;

// Test that XrdCl::URL conforms to RFC1738 (https://www.rfc-editor.org/rfc/rfc1738)

class URLTest : public ::testing::Test {};

TEST(URLTest, LocalURLs)
{
  char url[MAXPATHLEN];
  for (const auto& protocol : { "", "file://" }) {
    for (const auto& path : { "/dev", "/dev/", "/dev/null" }) {
      for (const auto& params : { "", "?param=value", "?param1=value1&param2=value2" }) {
        snprintf(url, sizeof(url), "%s%s%s", protocol, path, params);

        XrdCl::URL local_url(url);

        EXPECT_TRUE(local_url.IsValid()) << "URL " << url << " is invalid" << std::endl;

        EXPECT_EQ(local_url.GetProtocol(), "file");

        EXPECT_EQ(local_url.GetHostId(), "localhost");
        EXPECT_EQ(local_url.GetHostName(), "localhost");

        const char *resolved_path = realpath(path, nullptr);
        snprintf(url, sizeof(url), "%s%s", resolved_path, params);

        EXPECT_STREQ(local_url.GetPath().c_str(), resolved_path);
        EXPECT_STREQ(local_url.GetPathWithParams().c_str(), url);
        EXPECT_STREQ(local_url.GetParamsAsString().c_str(), params);

        free((void*)resolved_path);
      }
    }
  }
}

TEST(URLTest, RemoteURLs)
{
  char url[MAXPATHLEN];
  char path_params[MAXPATHLEN];
  for (const char *protocol : { "", "http", "root", "https", "roots" }) {
    int default_port = *protocol == 'h' ? (strlen(protocol) == 4 ? 80 : 443) : 1094;
    for (const char *user : { "", "alice", "bob", "user_123", "xrootd" }) {
      for (const char *password : { "", "abc ABC 123", "symbols \\~`!#$%^&*()_-+={[}]|:;'\"<,>.?" }) {
        for (const char *host : { "localhost", "[::1]", "127.0.0.1", "eospilot.cern.ch" }) {
          for (const char *port : { "", "-1", "1094", "9999", "65535" }) {
            for (const char *path : { "", "/", "/data", "/data/", "/data/file.dat", "/data//file" }) {
              for (const char *params : { "", "?param=value", "?param1=value1&param2=value2" }) {
                snprintf(url, sizeof(url), "%s%s%s%s%s%s%s%s%s%s%s%s",
                  protocol, *protocol ? "://" : "",
                  // TODO: allow empty user and/or password in the login part
                  user, *user && *password ? ":" : "", *user ? password : "", *user ? "@" : "",
                  // TODO: accept URLs with empty path and non-empty parameters
                  host, *port ? ":" : "", port, *params && !*path ? "/" : "", path, params);
                snprintf(path_params, sizeof(path_params), "%s%s", *path == '/' ? path+1 : path, params);

                XrdCl::URL remote_url(url);

                EXPECT_TRUE(remote_url.IsValid()) << "URL " << url << " is invalid" << std::endl;
                EXPECT_EQ(remote_url.GetPort(), *port ? atoi(port) : default_port);
                EXPECT_STREQ(remote_url.GetProtocol().c_str(), *protocol ? protocol : "root");
                EXPECT_STREQ(remote_url.GetPassword().c_str(), *user && *password ? password : "");
                EXPECT_STREQ(remote_url.GetHostName().c_str(), host);
                EXPECT_STREQ(remote_url.GetPath().c_str(), *path == '/' ? path+1 : path);
                EXPECT_STREQ(remote_url.GetParamsAsString().c_str(), params);
                EXPECT_STREQ(remote_url.GetPathWithParams().c_str(), path_params);
              }
            }
          }
        }
      }
    }
  }

  XrdCl::URL complex_url(
    /* protocol, login, host, and port */
    "root://xrootd:fxG}+u;B@lxfsra02a08.cern.ch:9999/"
    /* path */
    "/eos/dev/SMWZd3pdExample_NTUP_SMWZ.526666._000073.root.1?"
    /* parameters */
    "&cap.sym=sfdDqALWo3W3tWUJ2O5XwQ5GG8U="
    "&cap.msg=eGj/mh+9TrecFBAZBNr/nLau4p0kjlEOjc1JC+9DVjL1Tq+g"
      "eBCIz/kKs261mnL4dJeUu6r25acCn4vhyp8UKyL1cVmmnyBnjqe6tz28q"
      "FO2#0fQHrHf6Z9N0MNhw1fplYjpGeNwFH2jQSfSo24zSZKGa/PKClGYnX"
    "&mgm.loginid=766877e6-9874-11e1-a77f-003048cf8cd8"
    "&mgm.replicaindex=0"
    "&mgm.replicahead=1"
  );

  EXPECT_TRUE(complex_url.IsValid());
  EXPECT_EQ(complex_url.GetPort(), 9999);
  EXPECT_STREQ(complex_url.GetProtocol().c_str(), "root");
  EXPECT_STREQ(complex_url.GetUserName().c_str(), "xrootd");
  EXPECT_STREQ(complex_url.GetPassword().c_str(), "fxG}+u;B");
  EXPECT_STREQ(complex_url.GetHostName().c_str(), "lxfsra02a08.cern.ch");
  EXPECT_STREQ(complex_url.GetPath().c_str(), "/eos/dev/SMWZd3pdExample_NTUP_SMWZ.526666._000073.root.1");
  EXPECT_EQ(complex_url.GetParams().size(), 5);

  auto params = complex_url.GetParams();

  EXPECT_EQ(params["cap.sym"], "sfdDqALWo3W3tWUJ2O5XwQ5GG8U=");
  EXPECT_EQ(params["mgm.loginid"], "766877e6-9874-11e1-a77f-003048cf8cd8");
  EXPECT_EQ(params["mgm.replicaindex"], "0");
  EXPECT_EQ(params["mgm.replicahead"], "1");
}

TEST(URLTest, InvalidURLs)
{
  const char *invalid_urls[] = {
    "root://",
    "://asds",
    "root:////path?param1=val1&param2=val2",
    "root://@//path?param1=val1&param2=val2",
    "root://:@//path?param1=val1&param2=val2",
    "root://asd@://path?param1=val1&param2=val2"
    "root://user1:passwd1host1:123//path?param1=val1&param2=val2",
    "root://user1:passwd1@host1:asd//path?param1=val1&param2=val2",
  };

  for (const auto& url : invalid_urls)
    EXPECT_FALSE(XrdCl::URL(url).IsValid()) << "URL " << url << " is not invalid" << std::endl;
}

