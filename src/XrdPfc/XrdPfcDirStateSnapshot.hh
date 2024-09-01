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
   DataFsSnapshot(const DataFsStateBase &b, time_t sshot_stats_reset_time) :
     DataFsStateBase(b),
     m_sshot_stats_reset_time(sshot_stats_reset_time)
   {}

   // Import of data into vector form is implemented in ResourceMonitor
   // in order to avoid dependence of this struct on DirState.

   void write_json_file(const std::string &fname, XrdOss& oss, bool include_preamble);
   void dump();
};

}

#endif
