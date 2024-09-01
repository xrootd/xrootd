#include "XrdPfc.hh"
#include "XrdPfcPurgePin.hh"
#include "XrdPfcDirStateSnapshot.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOuca2x.hh"

#include <fcntl.h>

class XrdPfcPurgeQuota : public XrdPfc::PurgePin
{
   XrdSysError *log;
public:
   XrdPfcPurgeQuota() : log(XrdPfc::Cache::GetInstance().GetLog()) {}

   //----------------------------------------------------------------------------
   //! Set directory statistics
   //----------------------------------------------------------------------------
   void InitDirStatesForLocalPaths(const XrdPfc::DataFsPurgeshot &purge_shot)
   {
      for (list_i it = m_list.begin(); it != m_list.end(); ++it)
      {
         it->dirUsage = purge_shot.find_dir_usage_for_dir_path(it->path);
      }
   }

   //----------------------------------------------------------------------------
   //! Provide bytes to erase from dir quota listed in a text file
   //----------------------------------------------------------------------------
   long long GetBytesToRecover(const XrdPfc::DataFsPurgeshot &purge_shot) override
   {
      // setup diskusage for each dir path
      InitDirStatesForLocalPaths(purge_shot);

      long long totalToRemove = 0;
      // get bytes to remove
      for (list_i it = m_list.begin(); it != m_list.end(); ++it)
      {
         if (it->dirUsage == nullptr)
         {
            log->Emsg("PurgeQuotaPin--GetBytesToRecover", "directory not found:", it->path.c_str());
            continue;
         }
         long long cv = 512ll * it->dirUsage->m_StBlocks - it->nBytesQuota;
         if (cv > 0)
            it->nBytesToRecover = cv;
         else
            it->nBytesToRecover = 0;

         totalToRemove += it->nBytesToRecover;
      }

      return totalToRemove;
   }

   //----------------------------------------------------------------------------
   //! Provide bytes to erase from dir quota listed in a text file
   //----------------------------------------------------------------------------
   bool ConfigPurgePin(const char *parms) override
   {
      // retrive configuration file name
      if (!parms || !parms[0] || (strlen(parms) == 0))
      {
         log->Emsg("ConfigPurgePin", "Quota file not specified.");
         return false;
      }
      log->Emsg("ConfigPurgePin", "Using directory list", parms);

      //  parse the file to get directory quotas
      const char *config_filename = parms;
      const char *theINS = getenv("XRDINSTANCE");
      XrdOucEnv myEnv;
      XrdOucStream Config(log, theINS, &myEnv, "=====> PurgeQuota ");

      int fd;
      if ((fd = open(config_filename, O_RDONLY, 0)) < 0)
      {
         log->Emsg("ConfigPurgePin() can't open configuration file ", config_filename);
      }

      Config.Attach(fd);
      static const char *cvec[] = {"*** pfc purge plugin :", 0};
      Config.Capture(cvec);

      char *var;
      while ((var = Config.GetMyFirstWord()))
      {
         std::string dirpath = var;
         const char *val;

         if (!(val = Config.GetWord()))
         {
            log->Emsg("PurgeQuota plugin", "quota not specified");
            continue;
         }

         std::string tmpc = val;
         long long quota = 0;
         if (::isalpha(*(tmpc.rbegin())))
         {
            if (XrdOuca2x::a2sz(*log, "Error getting quota", tmpc.c_str(), &quota))
            {
               continue;
            }
         }
         else
         {
            if (XrdOuca2x::a2ll(*log, "Error getting quota", tmpc.c_str(), &quota))
            {
               continue;
            }
         }

         DirInfo d;
         d.path = dirpath;
         d.nBytesQuota = quota;
         m_list.push_back(d);
      }

      return true;
   }
};

/******************************************************************************/
/*                          XrdPfcGetPurgePin                                 */
/******************************************************************************/

// Return a purge object to use.
extern "C"
{
   XrdPfc::PurgePin *XrdPfcGetPurgePin(XrdSysError &)
   {
      return new XrdPfcPurgeQuota();
   }
}
