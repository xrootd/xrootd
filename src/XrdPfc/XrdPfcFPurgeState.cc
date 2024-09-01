#include "XrdPfcFPurgeState.hh"
#include "XrdPfcFsTraversal.hh"
#include "XrdPfcInfo.hh"
#include "XrdPfc.hh"
#include "XrdPfcTrace.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOss/XrdOssAt.hh"

// Temporary, extensive purge tracing
// #define TRACE_PURGE(x) TRACE(Debug, x)
// #define TRACE_PURGE(x) std::cout << "PURGE " << x << "\n"
#define TRACE_PURGE(x)

using namespace XrdPfc;

const char *FPurgeState::m_traceID = "Purge";

//----------------------------------------------------------------------------
//! Constructor.
//----------------------------------------------------------------------------
FPurgeState::FPurgeState(long long iNBytesReq, XrdOss &oss) :
   m_oss(oss),
   m_nStBlocksReq((iNBytesReq >> 9) + 1ll), m_nStBlocksAccum(0), m_nStBlocksTotal(0),
   m_tMinTimeStamp(0), m_tMinUVKeepTimeStamp(0)
{

}

//----------------------------------------------------------------------------
//! Move remaing entires to the member map.
//! This is used for cold files and for files collected from purge plugin (really?).
//----------------------------------------------------------------------------
void FPurgeState::MoveListEntriesToMap()
{
   for (list_i i = m_flist.begin(); i != m_flist.end(); ++i)
   {
      m_fmap.insert(std::make_pair(i->time, *i));
   }
   m_flist.clear();
}

//----------------------------------------------------------------------------
//! Open info file. Look at the UV stams and last access time.
//! Store the file in sorted map or in a list.s
//! @param fname name of cache-info file
//! @param Info object
//! @param stat of the given file
//!
//----------------------------------------------------------------------------
void FPurgeState::CheckFile(const FsTraversal &fst, const char *fname, time_t atime, struct stat &fstat)
{
   long long nblocks = fstat.st_blocks;
   // TRACE(Dump, trc_pfx << "FPurgeState::CheckFile checking " << fname << " accessTime  " << atime);

   m_nStBlocksTotal += nblocks;

   // Could remove aged-out / uv-keep-failed files here ... or in the calling function that
   // can aggreagate info for all files in the directory.

   // For now keep using 0 time as this is used in the purge loop to make sure we continue even if enough
   // disk-space has been freed.

   if (m_tMinTimeStamp > 0 && atime < m_tMinTimeStamp)
   {
      m_flist.push_back(PurgeCandidate(fst.m_current_path, fname, nblocks, 0));
      m_nStBlocksAccum += nblocks;
   }
   else if (m_nStBlocksAccum < m_nStBlocksReq || (!m_fmap.empty() && atime < m_fmap.rbegin()->first))
   {
      m_fmap.insert(std::make_pair(atime, PurgeCandidate(fst.m_current_path, fname, nblocks, atime)));
      m_nStBlocksAccum += nblocks;

      // remove newest files from map if necessary
      while (!m_fmap.empty() && m_nStBlocksAccum - m_fmap.rbegin()->second.nStBlocks >= m_nStBlocksReq)
      {
         m_nStBlocksAccum -= m_fmap.rbegin()->second.nStBlocks;
         m_fmap.erase(--(m_fmap.rbegin().base()));
      }
   }
}

void FPurgeState::ProcessDirAndRecurse(FsTraversal &fst)
{
   for (auto it = fst.m_current_files.begin(); it != fst.m_current_files.end(); ++it)
   {
        // Check if the file is currently opened / purge-protected is done before unlinking of the file.
      const std::string &f_name = it->first;
      const std::string  i_name = f_name + Info::s_infoExtension;

      // XXX Note, the initial scan now uses stat information only!

      if (! it->second.has_both()) {
         // cinfo or data file is missing.  What do we do? Erase?
         // Should really be checked in some other "consistency" traversal.
         continue;
      }

      time_t atime = it->second.stat_cinfo.st_mtime;
      CheckFile(fst, i_name.c_str(), atime, it->second.stat_data);

      // Protected top-directories are skipped.
   }

   std::vector<std::string> dirs;
   dirs.swap(fst.m_current_dirs);
   for (auto &dname : dirs)
   {
      if (fst.cd_down(dname))
      {
        ProcessDirAndRecurse(fst);
        fst.cd_up();
      }
   }
}

bool FPurgeState::TraverseNamespace(const char *root_path)
{
   bool success_p = true;

   FsTraversal fst(m_oss);
   fst.m_protected_top_dirs.insert("pfc-stats"); // XXXX This should come from config. Also: N2N?
                                                 // Also ... this onoly applies to /, not any root_path
   if (fst.begin_traversal(root_path))
   {
      ProcessDirAndRecurse(fst);
   }
   else
   {
      // Fail startup, can't open /.
      success_p = false;
   }
   fst.end_traversal();

   return success_p;
}

/*
void FPurgeState::UnlinkInfoAndData(const char *fname, long long nblocks, XrdOssDF *iOssDF)
{
   fname[fname_len - m_info_ext_len] = 0;
   if (nblocks > 0)
   {
      if ( ! Cache.GetInstance().IsFileActiveOrPurgeProtected(dataPath))
      {
         m_n_purged++;
         m_bytes_purged += nblocks;
      } else
      {
         m_n_purge_protected++;
         m_bytes_purge_protected += nblocks;
         m_dir_state->add_usage_purged(nblocks);
         // XXXX should also tweak other stuff?
         fname[fname_len - m_info_ext_len] = '.';
         return;
      }
   }
   m_oss_at.Unlink(*iOssDF, fname);
   fname[fname_len - m_info_ext_len] = '.';
   m_oss_at.Unlink(*iOssDF, fname);
}
*/
