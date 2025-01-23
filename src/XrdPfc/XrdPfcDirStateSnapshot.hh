#ifndef __XRDPFC_DIRSTATESNAPSHOT_HH__
#define __XRDPFC_DIRSTATESNAPSHOT_HH__

#include "XrdPfcDirState.hh"

#include <vector>

class XrdOss;


//==============================================================================
// Structs for DirState export in vector form
//==============================================================================

namespace XrdPfc
{

// For usage / stat reporting

struct DirStateElement : public DirStateBase
{
   DirStats m_stats;
   DirUsage m_usage;

   int m_parent = -1;
   int m_daughters_begin = -1, m_daughters_end = -1;

   DirStateElement() {}
   DirStateElement(const DirState &b, int parent) :
     DirStateBase(b),
     m_stats(b.m_sshot_stats),
     m_usage(b.m_here_usage, b.m_recursive_subdir_usage),
     m_parent(parent)
   {}
};

struct DataFsSnapshot : public DataFsStateBase
{
   std::vector<DirStateElement> m_dir_states;
   time_t                       m_sshot_stats_reset_time = 0;

   DataFsSnapshot() {}
   DataFsSnapshot(const DataFsState &b) :
     DataFsStateBase(b),
     m_sshot_stats_reset_time(b.m_sshot_stats_reset_time)
   {}

   // Import of data into vector form is implemented in ResourceMonitor
   // in order to avoid dependence of this struct on DirState.

   void write_json_file(const std::string &fname, XrdOss& oss, bool include_preamble);
   void dump();
};

// For purge planning & execution

struct DirPurgeElement : public DirStateBase
{
   DirUsage m_usage;

   int m_parent = -1;
   int m_daughters_begin = -1, m_daughters_end = -1;

   DirPurgeElement() {}
   DirPurgeElement(const DirState &b, int parent) :
     DirStateBase(b),
     m_usage(b.m_here_usage, b.m_recursive_subdir_usage),
     m_parent(parent)
   {}
};

struct DataFsPurgeshot : public DataFsStateBase
{
   long long m_bytes_to_remove = 0;
   long long m_estimated_writes_from_writeq = 0;

   bool m_space_based_purge = false;
   bool m_age_based_purge = false;

   std::vector<DirPurgeElement> m_dir_vec;
   // could have parallel vector of DirState* ... or store them in the DirPurgeElement.
   // requires some interlock / ref-counting with the source tree.
   // or .... just block DirState removal for the duration of the purge :) Yay.

   DataFsPurgeshot() {}
   DataFsPurgeshot(const DataFsState &b) :
     DataFsStateBase(b)
   {}

  int find_dir_entry_from_tok(int entry, PathTokenizer &pt, int pos, int *last_existing_entry) const;

  int find_dir_entry_for_dir_path(const std::string &dir_path) const;

  const DirUsage* find_dir_usage_for_dir_path(const std::string &dir_path) const;
};

}

#endif
