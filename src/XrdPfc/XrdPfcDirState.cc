#include "XrdPfcDirState.hh"
#include "XrdPfcPathParseTools.hh"

#include <cstdlib>
#include <string>

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

      // Carry the last-access times up, as update_stats_and_usages() does.
      subdirs.update_last_times(dhere);
      subdirs.update_last_times(dsubdirs);
   }
}

//----------------------------------------------------------------------------
//! Upward propagate stats to parents, join last open/close timestamps,
//! apply deltas / stats to usages, and reset the deltas.
//! If reap_candidates is given, collect the empty leaves into it.

void DirState::update_stats_and_usages(std::vector<DirState*> *reap_candidates)
{
   m_RecursiveStBlocksFuzz = 0;
   for (auto & [name, sub_ds] : m_subdirs)
   {
      sub_ds.update_stats_and_usages(reap_candidates);

      m_RecursiveStBlocksFuzz += sub_ds.m_StBlocksFuzz + sub_ds.m_RecursiveStBlocksFuzz;

      m_recursive_subdir_stats.AddUp(sub_ds.m_recursive_subdir_stats);
      m_recursive_subdir_stats.AddUp(sub_ds.m_here_stats);
      // nothing to do for m_here_stats.

      // Join timestamps from daugters and their sub-daugter trees.
      m_recursive_subdir_usage.update_last_times(sub_ds.m_recursive_subdir_usage);
      m_recursive_subdir_usage.update_last_times(sub_ds.m_here_usage);

      // The parent clears a child's deltas once absorbed; the root's are cleared by
      // DataFsState::update_stats_and_usages().
      sub_ds.m_here_stats.Reset();
      sub_ds.m_recursive_subdir_stats.Reset();

      if (reap_candidates && sub_ds.is_empty_leaf())
         reap_candidates->push_back(&sub_ds);
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
//! No subdirs, files or open files, as of the last propagation, and no opens since.
//! m_NDirectories can undercount, so m_subdirs is checked as well.

bool DirState::is_empty_leaf() const
{
   return m_subdirs.empty() &&
          m_here_usage.m_NDirectories == 0 && m_here_usage.m_NFiles == 0 &&
          m_here_usage.m_NFilesOpen == 0 &&
          m_here_stats.m_NFilesOpened == 0 && m_here_stats.m_NFilesCreated == 0;
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
          // Since the last snapshot, this directory and below.
          m_sshot_stats.m_NumIos, m_sshot_stats.m_Duration,
          m_sshot_stats.m_BytesHit, m_sshot_stats.m_BytesMissed, m_sshot_stats.m_BytesBypassed,
          m_sshot_stats.m_BytesWritten);

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

void DataFsState::update_stats_and_usages(time_t last_update, bool collect_reap_candidates)
{
   if (collect_reap_candidates)
      m_reap_candidates.clear();
   m_root.update_stats_and_usages(collect_reap_candidates ? &m_reap_candidates : nullptr);
   m_root.m_here_stats.Reset();
   m_root.m_recursive_subdir_stats.Reset();
   m_usage_update_time = last_update;
   m_stats_reset_time  = last_update;
}

//----------------------------------------------------------------------------
//! Remove the empty leaves collected at the last snapshot, which reported them once
//! more, empty. The root is never a candidate, and a parent waits for the next
//! snapshot. Returns the leftover st_blocks taken out of the tree.

long long DataFsState::reap_empty_leaf_dirs(unlink_func unlink_foo)
{
   long long total_residue = 0;
   for (DirState *ds : m_reap_candidates)
   {
      // A pending close points at a DirState while m_NFilesOpen > 0.
      if ( ! ds->is_empty_leaf())
         continue;

      std::string dir_path;
      dir_path.reserve(1024);
      ds->generate_dir_path(dir_path);
      if (unlink_foo(dir_path) != 0)
         continue;

      // Return a leftover st_blocks through the recursive stats, where the ancestors
      // counted it, rather than lose it with the node. Keep the correction, and the
      // node's own fuzz, as the parent's.
      DirState *parent  = ds->m_parent;
      long long residue = ds->m_here_usage.m_StBlocks + ds->m_recursive_subdir_usage.m_StBlocks;
      parent->m_recursive_subdir_stats.m_StBlocksRemoved += residue;
      parent->m_StBlocksFuzz += std::llabs(residue) + ds->m_StBlocksFuzz;
      m_st_blocks_fuzz       += std::llabs(residue);
      total_residue          += residue;

      ++parent->m_here_stats.m_NDirectoriesRemoved;
      parent->m_subdirs.erase(parent->m_subdirs.find(ds->m_dir_name));
   }
   m_reap_candidates.clear();
   return total_residue;
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
          (long long)(m_usage_update_time - m_sshot_stats_reset_time), max_depth);

   m_root.dump_recursively("root", max_depth);
}
