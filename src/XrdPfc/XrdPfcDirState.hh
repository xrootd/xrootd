#ifndef __XRDPFC_DIRSTATE_HH__
#define __XRDPFC_DIRSTATE_HH__

#include "XrdPfcStats.hh"

#include <ctime>
#include <functional>
#include <map>
#include <string>


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
class PathTokenizer;

using unlink_func = std::function<int(const std::string&)>;

//==============================================================================
// Data-holding struct DirUsage -- complementary to Stats.
//==============================================================================

struct DirUsage
{
   time_t    m_LastOpenTime  = 0;
   time_t    m_LastCloseTime = 0;
   long long m_StBlocks      = 0;
   int       m_NFilesOpen    = 0;
   int       m_NFiles        = 0;
   int       m_NDirectories  = 0;

   DirUsage() = default;

   DirUsage(const DirUsage& s) = default;

   DirUsage& operator=(const DirUsage&) = default;

   DirUsage(const DirUsage &a, const DirUsage &b) :
      m_LastOpenTime  (std::max(a.m_LastOpenTime,  b.m_LastOpenTime)),
      m_LastCloseTime (std::max(a.m_LastCloseTime, b.m_LastCloseTime)),
      m_StBlocks      (a.m_StBlocks     + b.m_StBlocks),
      m_NFilesOpen    (a.m_NFilesOpen   + b.m_NFilesOpen),
      m_NFiles        (a.m_NFiles       + b.m_NFiles),
      m_NDirectories  (a.m_NDirectories + b.m_NDirectories)
   {}

   void update_from_stats(const DirStats& s)
   {
      m_StBlocks     += s.m_StBlocksAdded       - s.m_StBlocksRemoved;
      m_NFilesOpen   += s.m_NFilesOpened        - s.m_NFilesClosed;
      m_NFiles       += s.m_NFilesCreated       - s.m_NFilesRemoved;
      m_NDirectories += s.m_NDirectoriesCreated - s.m_NDirectoriesRemoved;
   }

   void update_last_times(const DirUsage& u)
   {
      m_LastOpenTime  = std::max(m_LastOpenTime,  u.m_LastOpenTime);
      m_LastCloseTime = std::max(m_LastCloseTime, u.m_LastCloseTime);
   }
};


//==============================================================================
// Base classes, shared between in-memory tree form and snap-shot vector form.
//==============================================================================

struct DirStateBase
{
   std::string  m_dir_name;

   DirStateBase() {}
   DirStateBase(const std::string &dname) : m_dir_name(dname) {}
};

struct DataFsStateBase
{
   time_t    m_usage_update_time = 0;
   time_t    m_stats_reset_time = 0;

   long long m_disk_total = 0; // In bytes, from Oss::StatVS() on space data
   long long m_disk_used  = 0; // ""
   long long m_file_usage = 0; // Calculate usage by data files in the cache
   long long m_meta_total = 0; // In bytes, from Oss::StatVS() on space meta
   long long m_meta_used  = 0; // ""
};


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
   void update_stats_and_usages(bool purge_empty_dirs, unlink_func unlink_foo);
   void reset_stats();
   void reset_sshot_stats();

   int count_dirs_to_level(int max_depth) const;

   void dump_recursively(const char *name, int max_depth) const;
};


//==============================================================================
// DataFsState
//==============================================================================

struct DataFsState : public DataFsStateBase
{
   DirState        m_root;
   time_t          m_sshot_stats_reset_time = 0;
   // time_t          m_purge_stats_reset_time = 0;

   DataFsState() : m_root() {}

   DirState* get_root() { return & m_root; }

   DirState* find_dirstate_for_lfn(const std::string& lfn, DirState **last_existing_dir = nullptr)
   {
      return m_root.find_path(lfn, -1, true, true, last_existing_dir);
   }

   void update_stats_and_usages(time_t last_update, bool purge_empty_dirs, unlink_func unlink_foo);
   void reset_stats(time_t last_update);
   void reset_sshot_stats(time_t last_update);
   // void reset_purge_stats();

   void dump_recursively(int max_depth) const;
};

}

#endif
