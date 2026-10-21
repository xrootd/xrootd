#ifndef __XRDPFC_DIRSTATE_HH__
#define __XRDPFC_DIRSTATE_HH__

#include "XrdPfcStats.hh"
#include "XrdPfcDirStateBase.hh"

#include <ctime>
#include <functional>
#include <map>
#include <string>
#include <vector>


//==============================================================================
// Manifest:
//------------------------------------------------------------------------------
// - Data-holding struct DirUsage -- complementary to Stats.
// - Base classes for DirState and DataFsState, shared between in-memory
//   tree form and snap-shot vector form.
// - Forward declatation of structs for DirState export in vector form:
//   - struct DirStateElement \_ for stats and usages snapshot
//   - struct DataFsSnapshot  /
//   - struct DirPurgeElement \_ for purge snapshot
//   - struct DataFsPurgeshot /
//   Those are in another file so the object file can be included in the
//   dedicated binary for processing of the binary dumps.
// - class DirState -- state of a directory, including current delta-stats.
// - class DataFSState -- manager of the DirState tree, starting from root (as in "/").
//
// Structs for DirState export in vector form (DirStateElement and DataFsSnapshot)
// are declared in XrdPfcDirStateSnapshot.hh.

//==============================================================================

namespace XrdPfc
{
struct PathTokenizer;

using unlink_func = std::function<int(const std::string&)>;

//==============================================================================
// Structs for DirState export in vector form
//==============================================================================

struct DirStateElement;
struct DataFsSnapshot;

struct DirPurgeElement;
struct DataFsPurgeshot;


//==============================================================================
// DirState
//==============================================================================

struct DirState : public DirStateBase
{
   typedef std::map<std::string, DirState> DsMap_t;
   typedef DsMap_t::iterator               DsMap_i;

   DirStats     m_here_stats;
   DirStats     m_recursive_subdir_stats;

   DirUsage     m_here_usage;
   DirUsage     m_recursive_subdir_usage;

   // This should be optional, only if needed and only up to some max level.
   // Preferably stored in some extrnal vector (as AccessTokens are) and indexed from here.
   DirStats     m_sshot_stats; // here + subdir, reset after sshot dump
   // DirStats     m_purge_stats;  // here + subdir, running avg., as per purge params

   //! Accumulated magnitude of st_blocks corrections made at this node, never reset;
   //! a reaped child's is carried here. A diagnostic of accounting drift.
   long long    m_StBlocksFuzz = 0;
   long long    m_RecursiveStBlocksFuzz = 0; //!< sum over the subdirs, set by propagation

   DirState    *m_parent = nullptr;
   DsMap_t      m_subdirs;
   int          m_depth;
   bool         m_scanned = false; // set to true after files in this directory are scanned.

   void init();

   DirState* create_child(const std::string &dir);

   DirState* find_path_tok(PathTokenizer &pt, int pos, bool create_subdirs,
                           DirState **last_existing_dir = nullptr);

   // --- public part ---

   DirState();

   DirState(DirState *parent);

   DirState(DirState *parent, const std::string& dname);

   DirState* get_parent() { return m_parent; }

   DirState* find_path(const std::string &path, int max_depth, bool parse_as_lfn, bool create_subdirs,
                       DirState **last_existing_dir = nullptr);

   DirState* find_dir(const std::string &dir, bool create_subdirs);

   int generate_dir_path(std::string &result);

   // initial scan support
   void upward_propagate_initial_scan_usages();

   // stat & usages updates / management
   void update_stats_and_usages(std::vector<DirState*> *reap_candidates = nullptr);
   bool is_empty_leaf() const;
   void reset_sshot_stats();

   int count_dirs_to_level(int max_depth) const;

   void dump_recursively(const char *name, int max_depth) const;
};


//! Count the DirState nodes a lookup created, from node up to last_existing.
inline void note_created_dirs(DirState *node, DirState *last_existing)
{
   for (DirState *p = node; p != last_existing; )
   {
      p = p->get_parent();
      p->m_here_stats.m_NDirectoriesCreated += 1;
   }
}

//! Account one file open. Created directories are counted even when the data
//! file already existed (#2808).
inline void note_file_open(DirState *node, DirState *last_existing, bool existing_file)
{
   node->m_here_stats.m_NFilesOpened += 1;
   note_created_dirs(node, last_existing);
   if ( ! existing_file)
      node->m_here_stats.m_NFilesCreated += 1;
}


//==============================================================================
// DataFsState
//==============================================================================

struct DataFsState : public DataFsStateBase
{
   DirState        m_root;
   time_t          m_stats_reset_time = 0;
   time_t          m_sshot_stats_reset_time = 0;
   // time_t          m_purge_stats_reset_time = 0;
   long long       m_st_blocks_fuzz = 0; // all corrections; equals the root's subtree fuzz
   std::vector<DirState*> m_reap_candidates; // empty leaves at the last snapshot's propagation;
                                             // only the reap erases DirStates

   DataFsState() : m_root() {}

   void init_stat_reset_times(time_t t) { m_stats_reset_time = m_sshot_stats_reset_time = t; }

   DirState* get_root() { return & m_root; }

   DirState* find_dirstate_for_lfn(const std::string& lfn, DirState **last_existing_dir = nullptr)
   {
      return m_root.find_path(lfn, -1, true, true, last_existing_dir);
   }

   void update_stats_and_usages(time_t last_update, bool collect_reap_candidates);
   long long reap_empty_leaf_dirs(unlink_func unlink_foo); // returns the residue
   void reset_sshot_stats(time_t last_update);
   // void reset_purge_stats();

   void dump_recursively(int max_depth) const;
};

}

#endif
