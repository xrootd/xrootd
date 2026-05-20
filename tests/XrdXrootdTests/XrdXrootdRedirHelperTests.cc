//------------------------------------------------------------------------------
// Unit tests for XrdXrootdRedirHelper.
//
// The helper is a thin adapter around XrdXrootdRedirPI, so the tests cover the
// boundary that the helper actually owns:
//   - the IsActive() / Init() arming/disarming;
//   - the tri-state translation of the plugin's return-string contract
//     (empty -> Unchanged, non-empty no '!' -> Replaced, '!'-prefixed -> Error
//     with the leading '!' stripped from the message);
//   - Redirect()'s dispatch on the sign of the port argument: the host[?cgi]
//     form (port >= 0) and the scheme://host[:port][/tail] URL form (port < 0);
//   - propagation of port mutations and the host[?cgi] target split Redirect()
//     does before forwarding host and cgi to the plugin;
//   - the scheme://host[:port][/tail] URL parsing ParseURL() owns, plus the
//     unparseable URLs Redirect() must short-circuit to Unchanged;
//   - the target XrdNetAddrInfo cache's expTime/refresh path, driven via the
//     test-only SetClockForTesting() seam (cache hit within ipHold, refresh
//     after expiry, and restoring the real clock).
//------------------------------------------------------------------------------

#undef NDEBUG

#include "XrdXrootd/XrdXrootdRedirHelper.hh"
#include "XrdXrootd/XrdXrootdRedirPI.hh"

#include "XrdNet/XrdNetAddr.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"

#include <gtest/gtest.h>
#include <ctime>
#include <string>

namespace {

//------------------------------------------------------------------------------
// Test-only fake clock backing XrdXrootdRedirHelper::SetClockForTesting().
// Tests write to gFakeNow to advance "time"; gFakeClock() is the function
// pointer the helper invokes in lieu of time(nullptr).
//------------------------------------------------------------------------------
time_t gFakeNow = 0;
time_t gFakeClock() { return gFakeNow; }


//------------------------------------------------------------------------------
// Programmable test double for XrdXrootdRedirPI.
//
// Returns whatever the test pre-programmed as `response` / `urlResponse`,
// optionally mutating the in/out port to simulate a plugin that asks for a
// different port. Records the inputs of the most recent invocation so tests
// can assert that the helper forwarded them verbatim.
//------------------------------------------------------------------------------
class FakeRedirPI : public XrdXrootdRedirPI
{
public:
   // Programmable Redirect() outputs.
   std::string  response;
   bool         shouldMutatePort = false;
   uint16_t     mutatedPort      = 0;

   // Programmable RedirectURL() output.
   std::string  urlResponse;

   // Recorded inputs of the most recent Redirect() call.
   std::string  seenTarget;
   std::string  seenCgi;
   uint16_t     seenPort = 0;
   int          calls    = 0;

   // Recorded inputs of the most recent RedirectURL() call.
   std::string  urlSeenHead, urlSeenTarget, urlSeenPort, urlSeenTail;
   int          rdrOptsSeen = 0;
   int          urlCalls    = 0;

   std::string Redirect(const char *Target, uint16_t &port,
                        const char *TCgi,
                        XrdNetAddrInfo &, XrdNetAddrInfo &) override
   {
      ++calls;
      seenTarget = Target ? Target : "";
      seenCgi    = TCgi   ? TCgi   : "";
      seenPort   = port;
      if (shouldMutatePort) port = mutatedPort;
      return response;
   }

   std::string RedirectURL(const char *urlHead, const char *Target,
                           const char *port,    const char *urlTail,
                           int &rdrOpts,
                           XrdNetAddrInfo &, XrdNetAddrInfo &) override
   {
      ++urlCalls;
      urlSeenHead   = urlHead ? urlHead : "";
      urlSeenTarget = Target  ? Target  : "";
      urlSeenPort   = port    ? port    : "";
      urlSeenTail   = urlTail ? urlTail : "";
      rdrOptsSeen   = rdrOpts;
      return urlResponse;
   }
};

class XrdXrootdRedirHelperTest : public ::testing::Test
{
protected:
   // logger and eDest are required by the helper's Init contract when a
   // plugin is supplied; they get exercised only on resolution failures,
   // which we don't trigger here.
   XrdSysLogger logger;
   XrdSysError  eDest{&logger, "test"};

   // We use a numeric IP literal as the redirect target throughout so that
   // XrdNetAddr::Set() inside LookupTarget() succeeds without any DNS
   // dependency. The same string is used for clientAddr to keep the test
   // hermetic.
   XrdNetAddr   clientAddr;
   FakeRedirPI  plugin;

   void SetUp() override
   {
      ASSERT_EQ(nullptr, clientAddr.Set("127.0.0.1", 4242));
      // Start each test with the helper disarmed and the real clock in place
      // so observed results come from the test, not from leftover state of a
      // previous test.
      XrdXrootdRedirHelper::Init(nullptr, nullptr, 0);
      XrdXrootdRedirHelper::SetClockForTesting(nullptr);
   }

   void TearDown() override
   {
      XrdXrootdRedirHelper::Init(nullptr, nullptr, 0);
      XrdXrootdRedirHelper::SetClockForTesting(nullptr);
   }

   void armWithPlugin()
   {
      XrdXrootdRedirHelper::Init(&plugin, &eDest, 8 * 60 * 60);
   }
};

//==============================================================================
// IsActive / Init
//==============================================================================

TEST_F(XrdXrootdRedirHelperTest, IsActiveReflectsInit)
{
   EXPECT_FALSE(XrdXrootdRedirHelper::IsActive());

   armWithPlugin();
   EXPECT_TRUE(XrdXrootdRedirHelper::IsActive());

   XrdXrootdRedirHelper::Init(nullptr, nullptr, 0);
   EXPECT_FALSE(XrdXrootdRedirHelper::IsActive());
}

//==============================================================================
// Redirect: host+port form (port >= 0)
//==============================================================================

TEST_F(XrdXrootdRedirHelperTest, RedirectWithoutPluginIsUnchanged)
{
   int         port = 1094;
   std::string outTarget = "untouched", errMsg = "untouched";

   auto outcome = XrdXrootdRedirHelper::Redirect(
       "127.0.0.1", port, clientAddr, outTarget, errMsg);

   EXPECT_EQ(XrdXrootdRedirHelper::Outcome::Unchanged, outcome);
   EXPECT_EQ(1094,         port);
   EXPECT_EQ("untouched",  outTarget);
   EXPECT_EQ("untouched",  errMsg);
   EXPECT_EQ(0,            plugin.calls);
}

TEST_F(XrdXrootdRedirHelperTest, RedirectEmptyPluginResponseIsUnchanged)
{
   armWithPlugin();
   plugin.response = "";

   int         port = 1094;
   std::string outTarget = "untouched", errMsg = "untouched";

   auto outcome = XrdXrootdRedirHelper::Redirect(
       "127.0.0.1", port, clientAddr, outTarget, errMsg);

   EXPECT_EQ(XrdXrootdRedirHelper::Outcome::Unchanged, outcome);
   EXPECT_EQ("untouched", outTarget);
   EXPECT_EQ("untouched", errMsg);
   EXPECT_EQ(1,           plugin.calls);
}

TEST_F(XrdXrootdRedirHelperTest, RedirectReplacedTargetIsReturnedToCaller)
{
   armWithPlugin();
   plugin.response = "alt.example.org";

   int         port = 1094;
   std::string outTarget, errMsg = "untouched";

   auto outcome = XrdXrootdRedirHelper::Redirect(
       "127.0.0.1", port, clientAddr, outTarget, errMsg);

   EXPECT_EQ(XrdXrootdRedirHelper::Outcome::Replaced, outcome);
   EXPECT_EQ("alt.example.org", outTarget);
   EXPECT_EQ("untouched",       errMsg);  // errMsg is for errors only
}

TEST_F(XrdXrootdRedirHelperTest, RedirectReplacedTargetCarriesCgi)
{
   armWithPlugin();
   plugin.response = "alt.example.org?lcgi=1";

   int         port = 1094;
   std::string outTarget, errMsg;

   auto outcome = XrdXrootdRedirHelper::Redirect(
       "127.0.0.1", port, clientAddr, outTarget, errMsg);

   EXPECT_EQ(XrdXrootdRedirHelper::Outcome::Replaced, outcome);
   EXPECT_EQ("alt.example.org?lcgi=1", outTarget);
}

TEST_F(XrdXrootdRedirHelperTest, RedirectPortMutationIsPropagated)
{
   armWithPlugin();
   plugin.response         = "alt.example.org";
   plugin.shouldMutatePort = true;
   plugin.mutatedPort      = 9999;

   int         port = 1094;
   std::string outTarget, errMsg;

   auto outcome = XrdXrootdRedirHelper::Redirect(
       "127.0.0.1", port, clientAddr, outTarget, errMsg);

   EXPECT_EQ(XrdXrootdRedirHelper::Outcome::Replaced, outcome);
   EXPECT_EQ(9999, port);
}

TEST_F(XrdXrootdRedirHelperTest, RedirectErrorStripsBangFromMessage)
{
   armWithPlugin();
   plugin.response = "!something broke";

   int         port = 1094;
   std::string outTarget = "untouched", errMsg;

   auto outcome = XrdXrootdRedirHelper::Redirect(
       "127.0.0.1", port, clientAddr, outTarget, errMsg);

   EXPECT_EQ(XrdXrootdRedirHelper::Outcome::Error, outcome);
   EXPECT_EQ("something broke", errMsg);
   EXPECT_EQ("untouched",       outTarget);  // outTarget is for replacements only
}

TEST_F(XrdXrootdRedirHelperTest, RedirectErrorEmptyMessageOnBareBang)
{
   armWithPlugin();
   plugin.response = "!";

   int         port = 1094;
   std::string outTarget, errMsg = "untouched";

   auto outcome = XrdXrootdRedirHelper::Redirect(
       "127.0.0.1", port, clientAddr, outTarget, errMsg);

   EXPECT_EQ(XrdXrootdRedirHelper::Outcome::Error, outcome);
   EXPECT_EQ("", errMsg);
}

TEST_F(XrdXrootdRedirHelperTest, RedirectForwardsHostPortAndCgi)
{
   armWithPlugin();
   plugin.response = "";  // outcome irrelevant here; we check the plugin's inputs

   int         port = 1094;
   std::string outTarget, errMsg;
   // Redirect() receives the target joined as host?cgi and must split it back
   // into the separate host and cgi arguments the plugin contract expects.
   XrdXrootdRedirHelper::Redirect("127.0.0.1?fcgi=1", port,
                                  clientAddr, outTarget, errMsg);

   EXPECT_EQ("127.0.0.1", plugin.seenTarget);
   EXPECT_EQ("?fcgi=1",   plugin.seenCgi);
   EXPECT_EQ(1094u,       plugin.seenPort);
   EXPECT_EQ(1,           plugin.calls);
   EXPECT_EQ(0,           plugin.urlCalls);  // host form must not call RedirectURL
}

TEST_F(XrdXrootdRedirHelperTest, RedirectTargetWithoutCgiYieldsEmptyCgi)
{
   armWithPlugin();
   plugin.response = "";

   int         port = 1094;
   std::string outTarget, errMsg;
   // A target with no '?' splits to an empty cgi for the plugin.
   XrdXrootdRedirHelper::Redirect("127.0.0.1", port,
                                  clientAddr, outTarget, errMsg);

   EXPECT_EQ("127.0.0.1", plugin.seenTarget);
   EXPECT_EQ("",          plugin.seenCgi);
}

TEST_F(XrdXrootdRedirHelperTest, RedirectUnchangedLeavesPortUntouched)
{
   armWithPlugin();
   plugin.response         = "";     // empty reply -> Unchanged
   plugin.shouldMutatePort = true;   // the plugin still writes the port...
   plugin.mutatedPort      = 9999;

   int         port = 1094;
   std::string outTarget, errMsg;

   auto outcome = XrdXrootdRedirHelper::Redirect(
       "127.0.0.1", port, clientAddr, outTarget, errMsg);

   // ...but Redirect must commit the plugin's port only on Replaced, so an
   // Unchanged outcome leaves the caller's port at its original value.
   EXPECT_EQ(XrdXrootdRedirHelper::Outcome::Unchanged, outcome);
   EXPECT_EQ(1094, port);
}

TEST_F(XrdXrootdRedirHelperTest, RedirectPortOutOfRangeIsUnchanged)
{
   armWithPlugin();
   plugin.response = "alt.example.org";  // would Replace if the plugin ran

   int         port = 70000;             // > UINT16_MAX: not a valid port
   std::string outTarget = "untouched", errMsg;

   auto outcome = XrdXrootdRedirHelper::Redirect(
       "127.0.0.1", port, clientAddr, outTarget, errMsg);

   // The out-of-range port is rejected before the plugin is consulted.
   EXPECT_EQ(XrdXrootdRedirHelper::Outcome::Unchanged, outcome);
   EXPECT_EQ("untouched", outTarget);
   EXPECT_EQ(0,           plugin.calls);
}

//==============================================================================
// Redirect: URL form (port < 0)
//
// A negative port selects the URL form: Redirect() parses the scheme://host
// URL itself and invokes the plugin's RedirectURL() entry point.  The port
// argument doubles as the plugin's rdrOpts and is left unmodified on return.
//==============================================================================

TEST_F(XrdXrootdRedirHelperTest, RedirectUrlWithoutPluginIsUnchanged)
{
   int         port      = -1;
   std::string outTarget = "untouched", errMsg = "untouched";

   auto outcome = XrdXrootdRedirHelper::Redirect(
       "xroot://127.0.0.1:1094/foo", port, clientAddr, outTarget, errMsg);

   EXPECT_EQ(XrdXrootdRedirHelper::Outcome::Unchanged, outcome);
   EXPECT_EQ("untouched", outTarget);
   EXPECT_EQ("untouched", errMsg);
   EXPECT_EQ(0,           plugin.urlCalls);
}

TEST_F(XrdXrootdRedirHelperTest, RedirectUrlReplacedTargetIsReturnedToCaller)
{
   armWithPlugin();
   plugin.urlResponse = "file:///mnt/data";

   int         port = -1;
   std::string outTarget, errMsg;
   auto outcome = XrdXrootdRedirHelper::Redirect(
       "xroot://127.0.0.1:1094/foo", port, clientAddr, outTarget, errMsg);

   EXPECT_EQ(XrdXrootdRedirHelper::Outcome::Replaced, outcome);
   EXPECT_EQ("file:///mnt/data", outTarget);
}

TEST_F(XrdXrootdRedirHelperTest, RedirectUrlErrorStripsBangFromMessage)
{
   armWithPlugin();
   plugin.urlResponse = "!nope";

   int         port = -1;
   std::string outTarget, errMsg;
   auto outcome = XrdXrootdRedirHelper::Redirect(
       "xroot://127.0.0.1:1094/foo", port, clientAddr, outTarget, errMsg);

   EXPECT_EQ(XrdXrootdRedirHelper::Outcome::Error, outcome);
   EXPECT_EQ("nope", errMsg);
}

TEST_F(XrdXrootdRedirHelperTest, RedirectUrlForwardsAllParts)
{
   armWithPlugin();
   plugin.urlResponse = "";  // outcome irrelevant; we check the plugin's inputs

   int         port = -7;    // negative: URL form; value doubles as rdrOpts
   std::string outTarget, errMsg;
   XrdXrootdRedirHelper::Redirect("xroot://127.0.0.1:1094/path?cgi=1",
                                  port, clientAddr, outTarget, errMsg);

   EXPECT_EQ("xroot://",    plugin.urlSeenHead);
   EXPECT_EQ("127.0.0.1",   plugin.urlSeenTarget);
   EXPECT_EQ("1094",        plugin.urlSeenPort);
   EXPECT_EQ("/path?cgi=1", plugin.urlSeenTail);
   EXPECT_EQ(-7,            plugin.rdrOptsSeen);  // port forwarded as rdrOpts
   EXPECT_EQ(-7,            port);                // URL form leaves port untouched
   EXPECT_EQ(1,             plugin.urlCalls);
   EXPECT_EQ(0,             plugin.calls);        // URL form must not call Redirect
}

TEST_F(XrdXrootdRedirHelperTest, RedirectUrlUnparseableUrlIsUnchanged)
{
   armWithPlugin();
   plugin.urlResponse = "file:///mnt/data";  // would Replace if the plugin ran

   int         port      = -1;
   std::string outTarget = "untouched", errMsg = "untouched";
   // No "://": Redirect() must skip the plugin entirely rather than hand it an
   // unparseable target.
   auto outcome = XrdXrootdRedirHelper::Redirect(
       "not-a-url", port, clientAddr, outTarget, errMsg);

   EXPECT_EQ(XrdXrootdRedirHelper::Outcome::Unchanged, outcome);
   EXPECT_EQ("untouched", outTarget);
   EXPECT_EQ(0,           plugin.urlCalls);
}

//==============================================================================
// ParseURL: scheme://host[:port][/tail] grammar
//
// ParseURL is a pure function (no plugin, cache or DNS), so these assert the
// split directly instead of observing it through the fake plugin.
//==============================================================================

TEST_F(XrdXrootdRedirHelperTest, ParseURLSplitsFullUrl)
{
   std::string head, host, port, tail;
   EXPECT_TRUE(XrdXrootdRedirHelper::ParseURL(
       "xroot://data.example.org:1094/path?cgi=1", head, host, port, tail));
   EXPECT_EQ("xroot://",         head);
   EXPECT_EQ("data.example.org", host);
   EXPECT_EQ("1094",             port);
   EXPECT_EQ("/path?cgi=1",      tail);
}

TEST_F(XrdXrootdRedirHelperTest, ParseURLWithoutPortYieldsEmptyPort)
{
   std::string head, host, port, tail;
   EXPECT_TRUE(XrdXrootdRedirHelper::ParseURL(
       "xroot://data.example.org/path", head, host, port, tail));
   EXPECT_EQ("data.example.org", host);
   EXPECT_EQ("",                 port);
   EXPECT_EQ("/path",            tail);
}

TEST_F(XrdXrootdRedirHelperTest, ParseURLWithoutTailYieldsEmptyTail)
{
   std::string head, host, port, tail;
   EXPECT_TRUE(XrdXrootdRedirHelper::ParseURL(
       "xroot://data.example.org:1094", head, host, port, tail));
   EXPECT_EQ("data.example.org", host);
   EXPECT_EQ("1094",             port);
   EXPECT_EQ("",                 tail);
}

TEST_F(XrdXrootdRedirHelperTest, ParseURLRejectsUrlWithoutScheme)
{
   std::string head, host, port, tail;
   EXPECT_FALSE(XrdXrootdRedirHelper::ParseURL(
       "data.example.org:1094/path", head, host, port, tail));
}

TEST_F(XrdXrootdRedirHelperTest, ParseURLRejectsTooShortAuthority)
{
   // "://" is present but the host[:port] before the first '/' is below the
   // minimum length, so ParseURL must reject the URL.
   std::string head, host, port, tail;
   EXPECT_FALSE(XrdXrootdRedirHelper::ParseURL(
       "xroot://x/path", head, host, port, tail));
}

//==============================================================================
// LookupTarget cache: refresh-after-expiry path
//
// Drives XrdXrootdRedirHelper::SetClockForTesting() to fast-forward past the
// configured ipHold so the cache's refresh branch fires.  Each test uses a
// distinct IPv4 literal so the process-global niMap entry is fresh and can't
// be hit by a cache entry left behind by an earlier test.
//==============================================================================

TEST_F(XrdXrootdRedirHelperTest, RefreshAfterExpiryStillReplaces)
{
   gFakeNow = 1'000'000;
   XrdXrootdRedirHelper::SetClockForTesting(&gFakeClock);
   XrdXrootdRedirHelper::Init(&plugin, &eDest, /*ipHold=*/60);
   plugin.response = "alt.example.org";

   int         port = 1094;
   std::string outTarget, errMsg;

   // First Redirect: cache populated; entry will be valid until t = 1'000'060.
   auto outcome1 = XrdXrootdRedirHelper::Redirect(
       "127.0.0.2", port, clientAddr, outTarget, errMsg);
   EXPECT_EQ(XrdXrootdRedirHelper::Outcome::Replaced, outcome1);
   EXPECT_EQ("alt.example.org", outTarget);
   EXPECT_EQ(1, plugin.calls);

   // Advance well past the entry's expTime: the helper must take the refresh
   // path inside LookupTarget.  We can't observe the netaddr re-resolve
   // directly, but we can assert that Redirect still produces correct outputs
   // and the plugin was invoked exactly once more.
   gFakeNow = 1'000'500;
   outTarget.clear();
   errMsg.clear();
   auto outcome2 = XrdXrootdRedirHelper::Redirect(
       "127.0.0.2", port, clientAddr, outTarget, errMsg);
   EXPECT_EQ(XrdXrootdRedirHelper::Outcome::Replaced, outcome2);
   EXPECT_EQ("alt.example.org", outTarget);
   EXPECT_EQ(2, plugin.calls);
}

TEST_F(XrdXrootdRedirHelperTest, CacheHitWithinIpHoldStillReplaces)
{
   gFakeNow = 2'000'000;
   XrdXrootdRedirHelper::SetClockForTesting(&gFakeClock);
   XrdXrootdRedirHelper::Init(&plugin, &eDest, /*ipHold=*/3600);
   plugin.response = "alt.example.org";

   int         port = 1094;
   std::string outTarget, errMsg;

   // First Redirect: caches "127.0.0.3" until t = 2'003'600.
   XrdXrootdRedirHelper::Redirect("127.0.0.3", port,
                                  clientAddr, outTarget, errMsg);
   ASSERT_EQ(1, plugin.calls);

   // Advance only within ipHold: refresh must NOT fire.  Same observable as
   // above (we still get Replaced) - this test exists mainly to guard the
   // companion path so any future regression where refresh triggers
   // unconditionally would still be caught by more invasive tooling.
   gFakeNow += 60;       // far less than 3600
   outTarget.clear();
   errMsg.clear();
   auto outcome = XrdXrootdRedirHelper::Redirect(
       "127.0.0.3", port, clientAddr, outTarget, errMsg);
   EXPECT_EQ(XrdXrootdRedirHelper::Outcome::Replaced, outcome);
   EXPECT_EQ("alt.example.org", outTarget);
   EXPECT_EQ(2, plugin.calls);
}

TEST_F(XrdXrootdRedirHelperTest, SetClockForTestingNullRestoresRealClock)
{
   // Install a fake clock, then immediately uninstall it: subsequent Redirect
   // must not crash or produce an invalid outcome under the real wall clock.
   gFakeNow = 3'000'000;
   XrdXrootdRedirHelper::SetClockForTesting(&gFakeClock);
   XrdXrootdRedirHelper::SetClockForTesting(nullptr);

   armWithPlugin();
   plugin.response = "alt.example.org";

   int         port = 1094;
   std::string outTarget, errMsg;
   auto outcome = XrdXrootdRedirHelper::Redirect(
       "127.0.0.4", port, clientAddr, outTarget, errMsg);
   EXPECT_EQ(XrdXrootdRedirHelper::Outcome::Replaced, outcome);
   EXPECT_EQ("alt.example.org", outTarget);
}

}  // namespace
