#ifndef __XRDPFC_PURGEPLG_HH__
#define __XRDPFC_PURGEPLG_HH__

#include <string>
#include <vector>

namespace XrdPfc
{
struct DataFsPurgeshot;
struct DirUsage;

//----------------------------------------------------------------------------
//! Base class for reguesting directory space to obtain.
//----------------------------------------------------------------------------
class PurgePin
{
public:
   struct DirInfo
   {
      std::string path;
      long long nBytesQuota{0};
      long long nBytesToRecover{0};

      // internal use by the Cache purge thread. to be revisited, maybe an access token is more appropriate.
      const DirUsage* dirUsage{nullptr};
   };

   typedef std::vector<DirInfo> list_t;
   typedef list_t::iterator list_i;

protected:
   list_t m_list;

public:
   virtual ~PurgePin() {}


   //---------------------------------------------------------------------
   //! 
   //!
   //! @return total number of bytes
   //---------------------------------------------------------------------
   virtual bool CallPeriodically() { return true; };


   //---------------------------------------------------------------------
   //! Provide erase information from directory statistics
   //!
   //! @param & XrdPfc::DirState vector, exported from the tree version.
   //          To be revisited -- can have a multi-step approach where
   //          cache periodically sends udates.
   //!
   //! @return total number of bytes
   //---------------------------------------------------------------------
   virtual long long GetBytesToRecover(const DataFsPurgeshot&) = 0;

   //------------------------------------------------------------------------------
   //! Parse configuration arguments.
   //!
   //! @param params configuration parameters
   //!
   //! @return status of configuration
   //------------------------------------------------------------------------------
   virtual bool ConfigPurgePin(const char* params)  // ?? AMT should this be abstract
   {
      (void) params;
      return true;
   }

   //-----------------------------------------------
   //!
   //!  Get quotas for the given paths. Used in the XrdPfc:Cache::Purge() thread.
   //!
   //------------------------------------------------------------------------------
   list_t &refDirInfos() { return m_list; }
};
}

#endif
