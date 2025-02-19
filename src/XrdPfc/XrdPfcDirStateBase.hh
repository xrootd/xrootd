#ifndef __XRDPFC_DIRSTATEBASE_HH__
#define __XRDPFC_DIRSTATEBASE_HH__

#include <XrdPfc/XrdPfcStats.hh>
#include <ctime>
#include <string>

namespace XrdPfc
{

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
// Base classes, shared between in-memory tree form and snap-shot vector forms.
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

   long long m_disk_total = 0; // In bytes, from Oss::StatVS() on space data
   long long m_disk_used  = 0; // ""
   long long m_file_usage = 0; // Calculate usage by data files in the cache
   long long m_meta_total = 0; // In bytes, from Oss::StatVS() on space meta
   long long m_meta_used  = 0; // ""
};

}

#endif
