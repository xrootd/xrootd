#ifndef __XRDPFC_DIRSTATEPURGESHOT_HH__
#define __XRDPFC_DIRSTATEPURGESHOT_HH__

#include "XrdPfc/XrdPfcDirStateBase.hh"
#include "XrdPfc/XrdPfcPathParseTools.hh"

namespace XrdPfc
{
struct DirPurgeElement : public DirStateBase
{
   DirUsage m_usage;

   int m_parent = -1;
   int m_daughters_begin = -1, m_daughters_end = -1;

   DirPurgeElement() {}
   DirPurgeElement(const DirStateBase &b, const DirUsage &here_usage, const DirUsage &subdir_usage, int parent) :
     DirStateBase(b),
     m_usage(here_usage, subdir_usage),
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
   DataFsPurgeshot(const DataFsStateBase &b) :
     DataFsStateBase(b)
   {}

  int find_dir_entry_from_tok(int entry, PathTokenizer &pt, int pos, int *last_existing_entry) const;

  int find_dir_entry_for_dir_path(const std::string &dir_path) const;

  const DirUsage* find_dir_usage_for_dir_path(const std::string &dir_path) const;
};


inline int DataFsPurgeshot::find_dir_entry_from_tok(int entry, PathTokenizer &pt, int pos, int *last_existing_entry) const
{
   if (pos == pt.get_n_dirs())
      return entry;

   const DirPurgeElement &dpe = m_dir_vec[entry];
   for (int i = dpe.m_daughters_begin; i != dpe.m_daughters_end; ++i)
   {
      if (m_dir_vec[i].m_dir_name == pt.get_dir(pos)) {
         return find_dir_entry_from_tok(i, pt, pos + 1, last_existing_entry);
      }
   }
   if (last_existing_entry)
      *last_existing_entry = entry;
   return -1;
}

inline int DataFsPurgeshot::find_dir_entry_for_dir_path(const std::string &dir_path) const
{
   PathTokenizer pt(dir_path, -1, false);
   return find_dir_entry_from_tok(0, pt, 0, nullptr);
}

inline const DirUsage* DataFsPurgeshot::find_dir_usage_for_dir_path(const std::string &dir_path) const
{
   int entry = find_dir_entry_for_dir_path(dir_path);
   return entry >= 0 ? &m_dir_vec[entry].m_usage : nullptr;
}

}

#endif
