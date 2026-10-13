//------------------------------------------------------------------------------
// Tests for XrdPfc::DirState. The Accounting helpers mirror
// ResourceMonitor::process_queues(); keep them in step with it.
//------------------------------------------------------------------------------

#include "XrdPfc/XrdPfcDirState.hh"

#include <gtest/gtest.h>

#include <cerrno>
#include <cstdio>
#include <set>
#include <string>

using namespace XrdPfc;

namespace
{

// Like rmdir(), refuses a non-empty directory.
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

class Accounting
{
public:
   DataFsState    m_fs;
   FakeNamespace  m_ns;
   time_t         m_now = 1000;

   //! existing_file: the data file was already on disk.
   void open(const std::string &lfn, bool existing_file)
   {
      DirState *last_existing = nullptr;
      DirState *ds = m_fs.find_dirstate_for_lfn(lfn, &last_existing);
      note_file_open(ds, last_existing, existing_file);
      ds->m_here_usage.m_LastOpenTime = m_now;
   }

   void close(const std::string &lfn)
   {
      DirState *ds = m_fs.find_dirstate_for_lfn(lfn);
      ds->m_here_stats.m_NFilesClosed += 1;
      ds->m_here_usage.m_LastCloseTime = m_now;
   }

   //! Like the purge loop, does not create missing nodes.
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

   // One level per reap cycle.
   for (int i = 0; i < 5; ++i) {
      a.beat(true);
      EXPECT_EQ(a.n_dirs_from_usage(), a.n_dirs_in_tree()) << "after reap cycle " << i;
   }
   EXPECT_EQ(a.n_dirs_in_tree(), 1);
}

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
      a.open(lfn, true);
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
// What the empty-leaf reap must not remove (issue #2934)
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

TEST(DirStateReap, KeepsADirectoryWhoseFileIsStillOpen)
{
   Accounting a;
   a.m_ns.create("/a/b/f1");
   a.open("/a/b/f1", false);
   a.add_blocks("/a/b/f1", 8);
   a.beat(false);
   ASSERT_NE(a.find("/a/b"), nullptr);
   ASSERT_EQ(a.find("/a/b")->m_here_usage.m_NFilesOpen, 1);

   // Evicted while open: rmdir would succeed, only the accounting protects it.
   a.m_ns.remove("/a/b/f1");
   a.purge("/a/b/f1", 8);
   a.beat(true);

   ASSERT_NE(a.find("/a/b"), nullptr)
      << "reaped a directory that a pending close still points at";
   EXPECT_EQ(a.find("/a/b")->m_here_usage.m_NFiles, 0)     << "the file is gone";
   EXPECT_EQ(a.find("/a/b")->m_here_usage.m_NFilesOpen, 1) << "but still counted open";

   a.close("/a/b/f1");
   a.beat(false);
   ASSERT_NE(a.find("/a/b"), nullptr);
   EXPECT_EQ(a.find("/a/b")->m_here_usage.m_NFilesOpen, 0);

   a.beat(true);
   EXPECT_EQ(a.find("/a/b"), nullptr) << "should be reaped once nothing is open";
}

TEST(DirStateReap, KeepsADirectoryThatStillHasChildren)
{
   Accounting a;
   a.cache_file("/a/b/c/f1", 8);
   a.beat(false);

   // Undercount m_NDirectories with a child still present, and let rmdir succeed,
   // so only the m_subdirs.empty() check protects the node.
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

TEST(DirStateReap, HonoursAFailedUnlink)
{
   Accounting a;
   a.cache_file("/a/b/f1", 8);
   a.beat(false);

   // The accounting says empty, the namespace does not.
   a.purge("/a/b/f1", 8);
   a.beat(true);

   EXPECT_NE(a.find("/a/b"), nullptr) << "reaped despite rmdir refusing";
}
