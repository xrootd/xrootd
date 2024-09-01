#include "XrdPfc.hh"
#include "XrdPfcDirStatePurgeshot.hh"
#include "XrdPfcResourceMonitor.hh"
#include "XrdPfcFPurgeState.hh"
#include "XrdPfcPurgePin.hh"
#include "XrdPfcTrace.hh"

#include "XrdOss/XrdOss.hh"

#include <sys/time.h>

namespace
{
   XrdSysTrace* GetTrace() { return XrdPfc::Cache::GetInstance().GetTrace(); }
   const char *m_traceID = "ResourceMonitor";
}

//==============================================================================
// OldStylePurgeDriver
//==============================================================================
namespace XrdPfc
{

long long UnlinkPurgeStateFilesInMap(FPurgeState& purgeState, long long bytes_to_remove, const std::string& root_path)
{
   static const char *trc_pfx = "UnlinkPurgeStateFilesInMap ";

   struct stat fstat;
   int         protected_cnt = 0;
   int         deleted_file_count = 0;
   long long   deleted_st_blocks = 0;
   long long   protected_st_blocks = 0;
   long long   st_blocks_to_remove = (bytes_to_remove >> 9) + 1ll;
   

   const auto &cache = Cache::TheOne();
   auto &resmon = Cache::ResMon();
   auto &oss = *cache.GetOss();

   TRACE(Info, trc_pfx << "Started, root_path = " << root_path << ", bytes_to_remove = " << bytes_to_remove);

   // Loop over map and remove files with oldest values of access time.
   for (FPurgeState::map_i it = purgeState.refMap().begin(); it != purgeState.refMap().end(); ++it)
   {
      // Finish when enough space has been freed but not while age-based purging is in progress.
      // Those files are marked with time-stamp = 0.
      if (st_blocks_to_remove <= 0 && it->first != 0)
      {
         break;
      }

      std::string &infoPath = it->second.path;
      std::string  dataPath = infoPath.substr(0, infoPath.size() - Info::s_infoExtensionLen);

      if (cache.IsFileActiveOrPurgeProtected(dataPath))
      {
         ++protected_cnt;
         protected_st_blocks += it->second.nStBlocks;
         TRACE(Debug, trc_pfx << "File is active or purge-protected: " << dataPath << " size: " << 512ll * it->second.nStBlocks);
         continue;
      }

      // remove info file
      if (oss.Stat(infoPath.c_str(), &fstat) == XrdOssOK)
      {
         oss.Unlink(infoPath.c_str());
         TRACE(Dump, trc_pfx << "Removed file: '" << infoPath << "' size: " << 512ll * fstat.st_size);
      }
      else
      {
         TRACE(Error, trc_pfx << "Can't locate file " << dataPath);
      }

      // remove data file
      if (oss.Stat(dataPath.c_str(), &fstat) == XrdOssOK)
      {
         st_blocks_to_remove -= it->second.nStBlocks;
         deleted_st_blocks   += it->second.nStBlocks;
         ++deleted_file_count;

         oss.Unlink(dataPath.c_str());
         TRACE(Dump, trc_pfx << "Removed file: '" << dataPath << "' size: " << 512ll * it->second.nStBlocks << ", time: " << it->first);

         resmon.register_file_purge(dataPath, it->second.nStBlocks);
      }
   }
   if (protected_cnt > 0)
   {
      TRACE(Info, trc_pfx << "Encountered " << protected_cnt << " protected files, sum of their size: " << 512ll * protected_st_blocks);
   }

   TRACE(Info, trc_pfx << "Finished, removed " << deleted_file_count << " data files, removed total size " << 512ll * deleted_st_blocks)

   return deleted_st_blocks;
}

// -------------------------------------------------------------------------------------

void OldStylePurgeDriver(DataFsPurgeshot &ps)
{
   static const char *trc_pfx = "OldStylePurgeDriver ";
   const auto &cache = Cache::TheOne();
   const auto &conf  = Cache::Conf();
   auto &oss = *cache.GetOss();

   time_t purge_start = time(0);
   
   /////////////////////////////////////////////////////////////
   /// PurgePin 
   /////////////////////////////////////////////////////////////
   PurgePin *purge_pin = cache.GetPurgePin();
   long long std_blocks_removed_by_pin = 0;
   if (purge_pin)
   {   
      // set dir stat for each path and calculate nBytes to recover for each path
      // return total bytes to recover within the plugin
      long long clearVal = purge_pin->GetBytesToRecover(ps);
      if (clearVal)
      {
         TRACE(Debug, "PurgePin remove total " << clearVal << " bytes");
         PurgePin::list_t &dpl = purge_pin->refDirInfos();
         // iterate through the plugin paths
         for (PurgePin::list_i ppit = dpl.begin(); ppit != dpl.end(); ++ppit)
         {
            TRACE(Debug, trc_pfx << "PurgePin scanning dir " << ppit->path.c_str() << " to remove " << ppit->nBytesToRecover << " bytes");

            FPurgeState fps(ppit->nBytesToRecover, oss);
            bool scan_ok = fps.TraverseNamespace(ppit->path.c_str());
            if ( ! scan_ok) {
               TRACE(Warning, trc_pfx << "purge-pin scan of directory failed for " << ppit->path);
               continue;
            } 
            
            fps.MoveListEntriesToMap();
            std_blocks_removed_by_pin += UnlinkPurgeStateFilesInMap(fps, ppit->nBytesToRecover, ppit->path);
         }
      }
   }

   /////////////////////////////////////////////////////////////
   /// Default purge
   /////////////////////////////////////////////////////////////

   // check if the default pargue is still needed after purge pin
   long long pin_removed_bytes = std_blocks_removed_by_pin * 512ll;
   long long default_purge_blocks_removed = 0;
   if (ps.m_bytes_to_remove > pin_removed_bytes)
   {
      // init default purge
      long long bytes_to_remove = ps.m_bytes_to_remove - pin_removed_bytes;
      FPurgeState purgeState(2 * bytes_to_remove, oss); // prepare twice more volume than required

      if (ps.m_age_based_purge)
      {
         purgeState.setMinTime(time(0) - conf.m_purgeColdFilesAge);
      }
      if (conf.is_uvkeep_purge_in_effect())
      {
         purgeState.setUVKeepMinTime(time(0) - conf.m_cs_UVKeep);
      }

      // Make a map of file paths, sorted by access time.
      bool scan_ok = purgeState.TraverseNamespace("/");
      if (!scan_ok)
      {
         TRACE(Error, trc_pfx << "default purge namespace traversal failed at top-directory, this should not happen.");
         return;
      }

      TRACE(Debug, trc_pfx << "default purge usage measured from cinfo files " << purgeState.getNBytesTotal() << " bytes.");

      purgeState.MoveListEntriesToMap();
      default_purge_blocks_removed = UnlinkPurgeStateFilesInMap(purgeState, bytes_to_remove, "/");
   }

   // print the total summary
   /////////////////////////////////////////////////
   int purge_duration = time(0) - purge_start;
   long long total_bytes_removed = (default_purge_blocks_removed + std_blocks_removed_by_pin) * 512ll;
   TRACE(Info, trc_pfx << "Finished, removed total size " << total_bytes_removed << ", purge duration " << purge_duration);
}

} // end namespace XrdPfc
