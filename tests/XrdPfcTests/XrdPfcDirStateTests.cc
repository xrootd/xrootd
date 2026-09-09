//------------------------------------------------------------------------------
// Tests for XrdPfc::DirState -- the per-directory usage accounting tree.
//
// The tree is maintained incrementally: every change is reported as a delta
// (DirStats) and folded into a running level (DirUsage), so a single unreported
// change is never corrected -- it drifts, forever, and propagates to every
// ancestor. Two consequences were live bugs:
//
//  - the directory count could drift negative, and
//    ResourceMonitor::perform_purge_check() passes it to vector::reserve(),
//    whose size_type is unsigned, so -1 became ~1.8e19 and threw
//    std::length_error, killing the resource-monitor thread (issue #2808);
//  - the empty-leaf reap could free a DirState that a pending file-close still
//    held a pointer to, which is reachable from the outside with
//    "xrdfs <cache> cache fevict" on a file that is still open.
//
// DirState has no dependency on the Cache singleton, so it is compiled in here
// the same way XrdPfcInfo.cc is.
//
// The event application below deliberately mirrors
// ResourceMonitor::process_queues(): that is the only producer of these deltas,
// and a test that invented its own accounting would pass while the real one
// drifted. If process_queues() changes, these helpers must change with it.
//------------------------------------------------------------------------------

#include "XrdPfc/XrdPfcDirState.hh"

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

using namespace XrdPfc;

namespace
{

//------------------------------------------------------------------------------
// A stand-in for the cache namespace, so unlink_foo() can behave like
// XrdOssSys::Unlink() does on a directory -- rmdir(), which refuses a
// non-empty one. Several of the bugs below were masked in production by
// exactly that refusal, so a test that let every unlink succeed would be
// testing the wrong thing.
//------------------------------------------------------------------------------

class FakeNamespace
{
public:
   std::set<std::string> m_files;

   void create(const std::string &lfn) { m_files.insert(lfn); }
   void remove(const std::string &lfn) { m_files.erase(lfn); }

   int rmdir(const std::string &dir) const
   {
      const std::string pfx = dir + "/";
      for (const auto &f : m_files)
         if (f.compare(0, pfx.size(), pfx) == 0)
            return -ENOTEMPTY;
      return 0;
   }

   unlink_func unlinker() { return [this](const std::string &d) { return rmdir(d); }; }
};

//------------------------------------------------------------------------------
// The accounting ResourceMonitor::process_queues() performs, and nothing more.
//------------------------------------------------------------------------------

class Accounting
{
public:
   DataFsState    m_fs;
   FakeNamespace  m_ns;
   time_t         m_now = 1000;

   //! Mirrors the m_file_open_q loop. existing_file is File::Open()'s
   //! data_existed, i.e. whether the data file was already on disk.
   void open(const std::string &lfn, bool existing_file)
   {
      DirState *last_existing = nullptr;
      DirState *ds = m_fs.find_dirstate_for_lfn(lfn, &last_existing);
      // The production accounting, called rather than re-implemented: a test
      // that copied this logic would pass while the real one drifted.
      note_file_open(ds, last_existing, existing_file);
      ds->m_here_usage.m_LastOpenTime = m_now;
   }

   //! Mirrors the m_file_close_q loop.
   void close(const std::string &lfn)
   {
      DirState *ds = m_fs.find_dirstate_for_lfn(lfn);
      ds->m_here_stats.m_NFilesClosed += 1;
      ds->m_here_usage.m_LastCloseTime = m_now;
   }

   //! Mirrors the m_file_purge_q3 loop, which does not create missing nodes.
   void purge(const std::string &lfn, long long st_blocks)
   {
      DirState *ds = m_fs.get_root()->find_path(lfn, -1, true, false);
      if ( ! ds) return;
      ds->m_here_stats.m_StBlocksRemoved += st_blocks;
      ds->m_here_stats.m_NFilesRemoved   += 1;
   }

   void add_blocks(const std::string &lfn, long long st_blocks)
   {
      m_fs.find_dirstate_for_lfn(lfn)->m_here_stats.m_StBlocksAdded += st_blocks;
   }

   //! One heart_beat() secondary-activity cycle.
   void beat(bool reap_empty_dirs)
   {
      ++m_now;
      m_fs.update_stats_and_usages(m_now, reap_empty_dirs, m_ns.unlinker());
      m_fs.reset_stats(m_now);
   }

   //! Cache a file the ordinary way: it appears, is opened, written, closed.
   void cache_file(const std::string &lfn, long long st_blocks)
   {
      m_ns.create(lfn);
      open(lfn, false);
      add_blocks(lfn, st_blocks);
      close(lfn);
   }

   DirState& root() { return *m_fs.get_root(); }

   //! The quantity perform_purge_check() reserves the purge-shot vector with.
   int n_dirs_from_usage()
   {
      return 1 + root().m_here_usage.m_NDirectories
               + root().m_recursive_subdir_usage.m_NDirectories;
   }
   //! The same count taken from the tree itself.
   int n_dirs_in_tree() { return root().count_dirs_to_level(9999); }

   DirState* find(const std::string &dir_path)
   {
      return root().find_path(dir_path, -1, false, false);
   }
};

} // namespace

//==============================================================================
// The directory count must agree with the tree (issue #2808)
//==============================================================================

TEST(DirStateCount, MatchesTreeAfterOrdinaryUse)
{
   Accounting a;
   a.cache_file("/a/b/c/f1", 8);
   a.cache_file("/a/b/f2", 8);
   a.cache_file("/d/f3", 8);
   a.beat(false);

   EXPECT_EQ(a.n_dirs_from_usage(), a.n_dirs_in_tree());
   EXPECT_EQ(a.n_dirs_in_tree(), 5);   // root, a, a/b, a/b/c, d
}

TEST(DirStateCount, MatchesTreeThroughReapOfAWholeChain)
{
   Accounting a;
   a.cache_file("/a/b/c/f1", 8);
   a.beat(false);
   ASSERT_EQ(a.n_dirs_from_usage(), a.n_dirs_in_tree());

   a.m_ns.remove("/a/b/c/f1");
   a.purge("/a/b/c/f1", 8);

   // The reap takes one level per cycle, so run enough of them to unwind the
   // chain, checking the invariant at every step.
   for (int i = 0; i < 5; ++i) {
      a.beat(true);
      EXPECT_EQ(a.n_dirs_from_usage(), a.n_dirs_in_tree()) << "after reap cycle " << i;
   }
   EXPECT_EQ(a.n_dirs_in_tree(), 1);   // back to the root alone
}

// The regression behind #2808: find_dirstate_for_lfn() creates the DirState
// chain whether or not the *file* is new, so an open of a file that already
// exists -- in a directory the tree has never seen, because the initial scan
// could not enter it -- used to create nodes with no matching
// m_NDirectoriesCreated. Their eventual removal was still recorded, so the
// count went one lower every reap and eventually negative.
TEST(DirStateCount, CountsNodesCreatedForAnAlreadyExistingFile)
{
   Accounting a;
   a.cache_file("/scanned/f1", 8);
   a.beat(false);
   ASSERT_EQ(a.n_dirs_from_usage(), a.n_dirs_in_tree());

   // A file already on disk, in a directory the tree does not know about.
   a.m_ns.create("/unscanned/sub/f2");
   a.open("/unscanned/sub/f2", true);
   a.close("/unscanned/sub/f2");
   a.beat(false);

   EXPECT_EQ(a.n_dirs_from_usage(), a.n_dirs_in_tree());
   EXPECT_EQ(a.n_dirs_in_tree(), 4);   // root, scanned, unscanned, unscanned/sub
}

TEST(DirStateCount, NeverGoesNegativeAcrossRepeatedUnscannedDirs)
{
   Accounting a;
   a.cache_file("/base/anchor", 8);
   a.beat(false);

   for (int i = 0; i < 8; ++i) {
      char lfn[64];
      snprintf(lfn, sizeof(lfn), "/base/d%d/f", i);
      a.m_ns.create(lfn);
      a.open(lfn, true);          // already on disk, node is new
      a.close(lfn);
      a.beat(false);
      a.m_ns.remove(lfn);
      a.purge(lfn, 0);
      a.beat(true);
      a.beat(true);

      EXPECT_EQ(a.n_dirs_from_usage(), a.n_dirs_in_tree()) << "iteration " << i;
      EXPECT_GE(a.n_dirs_from_usage(), 1) << "iteration " << i;
   }
}

//==============================================================================
// What the empty-leaf reap must not remove
//==============================================================================

TEST(DirStateReap, RemovesAGenuinelyEmptyLeaf)
{
   Accounting a;
   a.cache_file("/a/b/f1", 8);
   a.beat(false);
   ASSERT_NE(a.find("/a/b"), nullptr);

   a.m_ns.remove("/a/b/f1");
   a.purge("/a/b/f1", 8);
   a.beat(true);

   EXPECT_EQ(a.find("/a/b"), nullptr) << "an empty leaf should be reaped";
}

// The use-after-free. ResourceMonitor::AccessToken holds a bare DirState* from
// the time a file's open record is processed until its close record is, and the
// close handler dereferences it. Removing a file while it is still open --
// "xrdfs <cache> cache fevict", or a purge racing a client -- takes m_NFiles
// back to 0 while the File is alive, so the directory must stay put until the
// close has been accounted for.
TEST(DirStateReap, KeepsADirectoryWhoseFileIsStillOpen)
{
   Accounting a;
   a.m_ns.create("/a/b/f1");
   a.open("/a/b/f1", false);
   a.add_blocks("/a/b/f1", 8);
   a.beat(false);
   ASSERT_NE(a.find("/a/b"), nullptr);
   ASSERT_EQ(a.find("/a/b")->m_here_usage.m_NFilesOpen, 1);

   // Evicted underneath the open file: the data is gone, so rmdir would now
   // succeed and only the accounting can protect the node.
   a.m_ns.remove("/a/b/f1");
   a.purge("/a/b/f1", 8);
   a.beat(true);

   // Check survival *before* dereferencing: without the fix this node is gone,
   // and a test for a use-after-free should report that as a failure rather
   // than crash on its own null pointer.
   ASSERT_NE(a.find("/a/b"), nullptr)
      << "reaped a directory that a pending close still points at";
   EXPECT_EQ(a.find("/a/b")->m_here_usage.m_NFiles, 0)     << "the file is gone";
   EXPECT_EQ(a.find("/a/b")->m_here_usage.m_NFilesOpen, 1) << "but still counted open";

   // Account for the close without reaping, so the open count can be observed
   // reaching zero.
   a.close("/a/b/f1");
   a.beat(false);
   ASSERT_NE(a.find("/a/b"), nullptr);
   EXPECT_EQ(a.find("/a/b")->m_here_usage.m_NFilesOpen, 0);

   // Now it may go.
   a.beat(true);
   EXPECT_EQ(a.find("/a/b"), nullptr) << "should be reaped once nothing is open";
}

TEST(DirStateReap, KeepsADirectoryThatStillHasChildren)
{
   Accounting a;
   a.cache_file("/a/b/c/f1", 8);
   a.beat(false);

   // Drive m_NDirectories to 0 while a child node is still present, which is
   // the state m_subdirs.empty() has to catch. It was an assert() before, and
   // asserts are compiled out of release builds.
   // Remove the file from the namespace too, so rmdir() would succeed and the
   // m_subdirs.empty() clause is the only thing left protecting the node. With
   // the file still present, rmdir refusing would mask a missing guard.
   a.m_ns.remove("/a/b/c/f1");

   DirState *b = a.find("/a/b");
   ASSERT_NE(b, nullptr);
   ASSERT_FALSE(b->m_subdirs.empty());
   b->m_here_usage.m_NDirectories = 0;
   b->m_here_usage.m_NFiles       = 0;
   b->m_here_usage.m_NFilesOpen   = 0;

   a.beat(true);

   EXPECT_NE(a.find("/a/b"), nullptr) << "reaped a node that still had children";
   EXPECT_NE(a.find("/a/b/c"), nullptr) << "and took a live subtree with it";
}

TEST(DirStateReap, TakesOneLevelPerCycle)
{
   Accounting a;
   a.cache_file("/a/b/c/f1", 8);
   a.beat(false);
   a.m_ns.remove("/a/b/c/f1");
   a.purge("/a/b/c/f1", 8);

   a.beat(true);
   EXPECT_EQ(a.find("/a/b/c"), nullptr) << "the leaf goes first";
   EXPECT_NE(a.find("/a/b"), nullptr)   << "its parent waits for the next cycle";

   a.beat(true);
   EXPECT_EQ(a.find("/a/b"), nullptr);
   EXPECT_NE(a.find("/a"), nullptr);
}

// rmdir() refusing a non-empty directory is the backstop that kept several of
// these bugs from doing damage in production. Make sure the reap honours it.
TEST(DirStateReap, HonoursAFailedUnlink)
{
   Accounting a;
   a.cache_file("/a/b/f1", 8);
   a.beat(false);

   // The accounting believes the directory is empty, the namespace disagrees.
   a.purge("/a/b/f1", 8);       // note: the file is NOT removed from m_ns
   a.beat(true);

   EXPECT_NE(a.find("/a/b"), nullptr) << "reaped despite rmdir refusing";
}
