//------------------------------------------------------------------------------
// Unit tests for XrdMonDiskCache: the on-failure disk cache that stores failed
// OpenSearch _bulk bodies and replays them oldest-first, surviving restarts.
//------------------------------------------------------------------------------

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "XrdApps/XrdMonCollect/XrdMonDiskCache.hh"

#include <gtest/gtest.h>

namespace
{
// A fresh, empty cache directory under the test's temp dir.
std::string freshDir(const std::string& name)
{
   std::string d = std::string(::testing::TempDir()) + "/" + name;
   // Best-effort clean: remove known files from a prior run.
   std::string cmd = "rm -rf '" + d + "'";
   if (system(cmd.c_str()) != 0) { /* ignore */ }
   return d;
}

// A replay callback that records the bodies it received and returns a fixed
// result (true = POST succeeded, false = sink still down).
struct Recorder
{
   std::vector<std::string> seen;
   bool result = true;
   bool operator()(const std::string& b) { seen.push_back(b); return result; }
};
}

TEST(XrdMonDiskCache, StoreThenReplayInFifoOrder)
{
   XrdMonDiskCache c(freshDir("dc-fifo"));
   std::string e;
   ASSERT_TRUE(c.Init(e)) << e;

   ASSERT_TRUE(c.Store("body-1", e)) << e;
   ASSERT_TRUE(c.Store("body-2", e)) << e;
   ASSERT_TRUE(c.Store("body-3", e)) << e;
   EXPECT_EQ(c.Files(), 3u);
   EXPECT_FALSE(c.Empty());
   EXPECT_EQ(c.Stored(), 3u);
   EXPECT_GT(c.Bytes(), 0u);

   Recorder rec;  // succeeds
   auto cb = [&](const std::string& b){ return rec(b); };
   EXPECT_EQ(c.ReplayOldest(cb, e), 1);
   EXPECT_EQ(c.ReplayOldest(cb, e), 1);
   EXPECT_EQ(c.ReplayOldest(cb, e), 1);
   EXPECT_EQ(c.ReplayOldest(cb, e), 0);   // nothing left

   ASSERT_EQ(rec.seen.size(), 3u);
   EXPECT_EQ(rec.seen[0], "body-1");      // oldest first
   EXPECT_EQ(rec.seen[1], "body-2");
   EXPECT_EQ(rec.seen[2], "body-3");
   EXPECT_TRUE(c.Empty());
   EXPECT_EQ(c.Files(), 0u);
   EXPECT_EQ(c.Bytes(), 0u);
   EXPECT_EQ(c.Replayed(), 3u);
}

TEST(XrdMonDiskCache, FailedReplayKeepsFile)
{
   XrdMonDiskCache c(freshDir("dc-keep"));
   std::string e;
   ASSERT_TRUE(c.Init(e)) << e;
   ASSERT_TRUE(c.Store("payload", e)) << e;

   Recorder down; down.result = false;    // sink still unavailable
   auto cbDown = [&](const std::string& b){ return down(b); };
   EXPECT_EQ(c.ReplayOldest(cbDown, e), -1);
   EXPECT_EQ(c.Files(), 1u);              // file is kept for a later attempt
   EXPECT_FALSE(c.Empty());
   EXPECT_EQ(c.Replayed(), 0u);

   Recorder up;                           // sink recovered
   auto cbUp = [&](const std::string& b){ return up(b); };
   EXPECT_EQ(c.ReplayOldest(cbUp, e), 1);
   EXPECT_EQ(up.seen.size(), 1u);
   EXPECT_EQ(up.seen[0], "payload");
   EXPECT_TRUE(c.Empty());
   EXPECT_EQ(c.Replayed(), 1u);
}

TEST(XrdMonDiskCache, SurvivesRestart)
{
   std::string dir = freshDir("dc-restart");
   std::string e;
   {  XrdMonDiskCache c(dir);
      ASSERT_TRUE(c.Init(e)) << e;
      ASSERT_TRUE(c.Store("a", e)) << e;
      ASSERT_TRUE(c.Store("b", e)) << e;
   }
   // A new instance on the same directory recovers the pending backlog.
   XrdMonDiskCache c2(dir);
   ASSERT_TRUE(c2.Init(e)) << e;
   EXPECT_EQ(c2.Files(), 2u);
   EXPECT_GT(c2.Bytes(), 0u);

   Recorder rec;
   auto cb = [&](const std::string& b){ return rec(b); };
   EXPECT_EQ(c2.ReplayOldest(cb, e), 1);
   EXPECT_EQ(c2.ReplayOldest(cb, e), 1);
   EXPECT_EQ(c2.ReplayOldest(cb, e), 0);
   ASSERT_EQ(rec.seen.size(), 2u);
   EXPECT_EQ(rec.seen[0], "a");           // order preserved across restart
   EXPECT_EQ(rec.seen[1], "b");
   EXPECT_TRUE(c2.Empty());
}

TEST(XrdMonDiskCache, InitRemovesStalePartials)
{
   std::string dir = freshDir("dc-tmp");
   std::string e;
   {  XrdMonDiskCache c(dir);
      ASSERT_TRUE(c.Init(e)) << e;        // creates the directory
   }
   // Leave a stale .tmp partial (as a crash mid-write would) and a good file.
   std::ofstream(dir + "/0000000000001-000000.ndjson.tmp") << "partial";
   {  XrdMonDiskCache c(dir);
      ASSERT_TRUE(c.Init(e)) << e;
      ASSERT_TRUE(c.Store("good", e)) << e;
   }
   // Re-open: the .tmp must have been removed; only the good body remains.
   XrdMonDiskCache c2(dir);
   ASSERT_TRUE(c2.Init(e)) << e;
   EXPECT_EQ(c2.Files(), 1u);
   Recorder rec;
   auto cb = [&](const std::string& b){ return rec(b); };
   EXPECT_EQ(c2.ReplayOldest(cb, e), 1);
   ASSERT_EQ(rec.seen.size(), 1u);
   EXPECT_EQ(rec.seen[0], "good");
}

TEST(XrdMonDiskCache, CustomSuffixStoredAndRescanned)
{
   std::string dir = freshDir("dc-suffix");
   std::string e;
   {  XrdMonDiskCache c(dir, ".frames");
      ASSERT_TRUE(c.Init(e)) << e;
      ASSERT_TRUE(c.Store("framed-bytes", e)) << e;
   }
   // A rescan with the same suffix recovers the body; the default-suffix
   // cache sharing the directory does not see it.
   XrdMonDiskCache same(dir, ".frames");
   ASSERT_TRUE(same.Init(e)) << e;
   EXPECT_EQ(same.Files(), 1u);

   XrdMonDiskCache other(dir);
   ASSERT_TRUE(other.Init(e)) << e;
   EXPECT_EQ(other.Files(), 0u);

   Recorder rec;
   auto cb = [&](const std::string& b){ return rec(b); };
   EXPECT_EQ(same.ReplayOldest(cb, e), 1);
   ASSERT_EQ(rec.seen.size(), 1u);
   EXPECT_EQ(rec.seen[0], "framed-bytes");
}

TEST(XrdMonDiskCache, MaxBytesDropsOldest)
{
   XrdMonDiskCache c(freshDir("dc-cap"));
   c.SetMaxBytes(10);
   std::string e;
   ASSERT_TRUE(c.Init(e)) << e;

   ASSERT_TRUE(c.Store("aaaa", e)) << e;    // 4 bytes
   ASSERT_TRUE(c.Store("bbbb", e)) << e;    // 8 bytes total
   EXPECT_EQ(c.Dropped(), 0u);
   ASSERT_TRUE(c.Store("cccc", e)) << e;    // would be 12: drops "aaaa"
   EXPECT_EQ(c.Dropped(), 1u);
   EXPECT_EQ(c.Files(), 2u);
   EXPECT_LE(c.Bytes(), 10u);

   // A body larger than what remains evicts everything older, then stores.
   ASSERT_TRUE(c.Store("ddddddddd", e)) << e;   // 9 bytes: drops both
   EXPECT_EQ(c.Dropped(), 3u);
   EXPECT_EQ(c.Files(), 1u);

   Recorder rec;
   auto cb = [&](const std::string& b){ return rec(b); };
   EXPECT_EQ(c.ReplayOldest(cb, e), 1);
   EXPECT_EQ(c.ReplayOldest(cb, e), 0);
   ASSERT_EQ(rec.seen.size(), 1u);
   EXPECT_EQ(rec.seen[0], "ddddddddd");     // only the newest survived
}
