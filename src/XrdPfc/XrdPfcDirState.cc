#include "XrdPfcDirState.hh"
#include "XrdPfcPathParseTools.hh"

#include <string>
#include <cassert>

using namespace XrdPfc;

//----------------------------------------------------------------------------
//! Constructor
//----------------------------------------------------------------------------
DirState::DirState() : m_parent(0), m_depth(0)
{}

//----------------------------------------------------------------------------
//! Constructor
//! @param DirState parent directory
//----------------------------------------------------------------------------
DirState::DirState(DirState *parent) :
   m_parent(parent),
   m_depth(m_parent->m_depth + 1)
{}

//----------------------------------------------------------------------------
//! Constructor
//! @param parent parent DirState object
//! @param dname  name of this directory only, no slashes, no extras.
//----------------------------------------------------------------------------
DirState::DirState(DirState *parent, const std::string &dname) :
  DirStateBase(dname),
  m_parent(parent),
  m_depth(m_parent->m_depth + 1)
{}

//----------------------------------------------------------------------------
//! Internal function called from find_dir or find_path_tok
//! @param dir subdir name
//----------------------------------------------------------------------------
DirState *DirState::create_child(const std::string &dir)
{
   std::pair<DsMap_i, bool> ir = m_subdirs.insert(std::make_pair(dir, DirState(this, dir)));
   return &ir.first->second;
}

//----------------------------------------------------------------------------
//! Internal function called from find_path
//! @param dir subdir name
//----------------------------------------------------------------------------
DirState *DirState::find_path_tok(PathTokenizer &pt, int pos, bool create_subdirs,
                                  DirState **last_existing_dir)
{
   if (pos == pt.get_n_dirs())
      return this;

   DirState *ds = nullptr;

   DsMap_i i = m_subdirs.find(pt.m_dirs[pos]);

   if (i != m_subdirs.end())
   {
      ds = &i->second;
      if (last_existing_dir)
         *last_existing_dir = ds;
   }
   else if (create_subdirs)
   {
      ds = create_child(pt.m_dirs[pos]);
   }

   if (ds)
      return ds->find_path_tok(pt, pos + 1, create_subdirs, last_existing_dir);

   return nullptr;
}

//----------------------------------------------------------------------------
//! Recursive function to find DirState with given absolute dir path
//! @param path full path to parse
//! @param max_depth directory depth to which to descend (value < 0 means full descent)
//! @param parse_as_lfn
//! @param create_subdirs
DirState *DirState::find_path(const std::string &path, int max_depth, bool parse_as_lfn,
                              bool create_subdirs, DirState **last_existing_dir)
{
   PathTokenizer pt(path, max_depth, parse_as_lfn);

   if (last_existing_dir)
      *last_existing_dir = this;

   return find_path_tok(pt, 0, create_subdirs, last_existing_dir);
}

//----------------------------------------------------------------------------
//! Non recursive function to find an entry in this directory only.
//! @param dir subdir name @param bool create the subdir in this DirsStat
//! @param create_subdirs if true and the dir is not found, a new DirState
//!        child is created
DirState *DirState::find_dir(const std::string &dir,
                             bool create_subdirs)
{
   DsMap_i i = m_subdirs.find(dir);

   if (i != m_subdirs.end())
      return &i->second;

   if (create_subdirs)
      return create_child(dir);

   return nullptr;
}

//----------------------------------------------------------------------------
//! Parent-recursively height-first append '/' and directory names to the
//! result string.
//! Returns total number of added characters.
int DirState::generate_dir_path(std::string &result)
{
   int n_chars = 0;
   if (m_parent) {
      n_chars += m_parent->generate_dir_path(result) + 1 + m_dir_name.length();
      result += '/';
      result += m_dir_name;
   }
   return n_chars;
}

//----------------------------------------------------------------------------
//! Propagate usages to parents after initial directory scan.
//! Called from ResourceMonitor::perform_initial_scan()
//----------------------------------------------------------------------------
void DirState::upward_propagate_initial_scan_usages()
{
   DirUsage &here    = m_here_usage;
   DirUsage &subdirs = m_recursive_subdir_usage;

   for (auto & [name, daughter] : m_subdirs)
   {
      daughter.upward_propagate_initial_scan_usages();

      DirUsage &dhere    = daughter.m_here_usage;
      DirUsage &dsubdirs = daughter.m_recursive_subdir_usage;

      here.m_NDirectories += 1;

      subdirs.m_StBlocks     += dhere.m_StBlocks     + dsubdirs.m_StBlocks;
      subdirs.m_NFiles       += dhere.m_NFiles       + dsubdirs.m_NFiles;
      subdirs.m_NDirectories += dhere.m_NDirectories + dsubdirs.m_NDirectories;
   }
}

//----------------------------------------------------------------------------
//! Upward propagate stats to parents, join last open/close timestamps, and
//! apply deltas / stats to usages.
//!
//! Additionally, manage removal of empty leaf directories and destruction
//! of corresponding leaf DirState objects.

void DirState::update_stats_and_usages(bool purge_empty_dirs, unlink_func unlink_foo)
{
   // Hmmh.
   DsMap_i i = m_subdirs.begin();
   while (i != m_subdirs.end())
   {
      DirState &sub_ds = i->second;

      sub_ds.update_stats_and_usages(purge_empty_dirs, unlink_foo);

      m_recursive_subdir_stats.AddUp(sub_ds.m_recursive_subdir_stats);
      m_recursive_subdir_stats.AddUp(sub_ds.m_here_stats);
      // nothing to do for m_here_stats.

      // Join timestamps from daugters and their sub-daugter trees.
      m_recursive_subdir_usage.update_last_times(sub_ds.m_recursive_subdir_usage);
      m_recursive_subdir_usage.update_last_times(sub_ds.m_here_usage);

      // Clean empty leaf directories. Only do one level at a time.
      // Note that root will never get purged.
      bool increment_iter = true;
      if (purge_empty_dirs && sub_ds.m_here_stats.m_NDirectoriesRemoved == 0 &&
          sub_ds.m_here_usage.m_NDirectories == 0 && sub_ds.m_here_usage.m_NFiles == 0)
      {
         assert(sub_ds.m_subdirs.empty());

         std::string dir_path;
         dir_path.reserve(1024);
         sub_ds.generate_dir_path(dir_path);

         if (unlink_foo(dir_path) == 0) {
            DsMap_i j = i++;
            m_subdirs.erase(j);
            ++m_here_stats.m_NDirectoriesRemoved;
            increment_iter = false;
         }
      }
      if (increment_iter)
         ++i;
   }

   // Apply current deltas to sshot_stats (and, if eventually needed, purge_stats).
   m_sshot_stats.AddUp(m_recursive_subdir_stats);
   m_sshot_stats.AddUp(m_here_stats);

   // Apply deltas / stats to usages.
   m_here_usage.update_from_stats(m_here_stats);
   m_recursive_subdir_usage.update_from_stats(m_recursive_subdir_stats);

   // Once we have m_report_stats and m_purge_stats, update them here.
}

//----------------------------------------------------------------------------
//! Reset current transaction deltas / stats.
//! Called from ResourceMonitor::heart_beat()
//----------------------------------------------------------------------------
void DirState::reset_stats()
{
   for (DsMap_i i = m_subdirs.begin(); i != m_subdirs.end(); ++i)
   {
      i->second.reset_stats();
   }
   m_here_stats.Reset();
   m_recursive_subdir_stats.Reset();
}

void DirState::reset_sshot_stats()
{
   for (DsMap_i i = m_subdirs.begin(); i != m_subdirs.end(); ++i)
   {
      i->second.reset_sshot_stats();
   }
   m_sshot_stats.Reset();
}

int DirState::count_dirs_to_level(int max_depth) const
{
   int n_dirs = 1;
   if (m_depth < max_depth)
   {
      for (auto & [name, ds] : m_subdirs)
      {
         n_dirs += ds.count_dirs_to_level(max_depth);
      }
   }
   return n_dirs;
}

//----------------------------------------------------------------------------
//! Recursive print of statistics. Called if defined in pfc configuration.
//!
//----------------------------------------------------------------------------
void DirState::dump_recursively(const char *name, int max_depth) const
{
   printf("%*d %s usage_here=%lld usage_sub=%lld usage_total=%lld num_ios=%d duration=%d b_hit=%lld b_miss=%lld b_byps=%lld b_wrtn=%lld\n",
          2 + 2 * m_depth, m_depth, name,
          512 * m_here_usage.m_StBlocks, 512 * m_recursive_subdir_usage.m_StBlocks,
          512 * (m_here_usage.m_StBlocks + m_recursive_subdir_usage.m_StBlocks),
          // XXXXX here_stats or sum up? or both?
          m_here_stats.m_NumIos, m_here_stats.m_Duration,
          m_here_stats.m_BytesHit, m_here_stats.m_BytesMissed, m_here_stats.m_BytesBypassed,
          m_here_stats.m_BytesWritten);

   if (m_depth < max_depth)
   {
      for (auto & [name, ds] : m_subdirs)
      {
         ds.dump_recursively(name.c_str(), max_depth);
      }
   }
}


//==============================================================================
// DataFsState
//==============================================================================

void DataFsState::update_stats_and_usages(time_t last_update, bool purge_empty_dirs, unlink_func unlink_foo)
{
   m_root.update_stats_and_usages(purge_empty_dirs, unlink_foo);
   m_usage_update_time = last_update;
}

void DataFsState::reset_stats(time_t last_update)
{
   m_root.reset_stats();
   m_stats_reset_time = last_update;
}

void DataFsState::reset_sshot_stats(time_t last_update)
{
   m_root.reset_sshot_stats();
   m_sshot_stats_reset_time = last_update;
}

void DataFsState::dump_recursively(int max_depth) const
{
   if (max_depth < 0)
      max_depth = 4096;

   printf("DataFsState::dump_recursively delta_t = %lld, max_dump_depth = %d\n",
          (long long)(m_usage_update_time - m_stats_reset_time), max_depth);

   m_root.dump_recursively("root", max_depth);
}
