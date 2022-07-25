#ifndef __XRDPFC_CACHE_IO_HH__
#define __XRDPFC_CACHE_IO_HH__

class XrdSysTrace;

#include "XrdPfc.hh"
#include "XrdOuc/XrdOucCache.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdSys/XrdSysPthread.hh"
#include <XrdSys/XrdSysRAtomic.hh>

namespace XrdPfc
{
//----------------------------------------------------------------------------
//! Base cache-io class that implements some XrdOucCacheIO abstract methods.
//----------------------------------------------------------------------------
class IO : public XrdOucCacheIO
{
public:
   IO (XrdOucCacheIO *io, Cache &cache);

   //! Original data source.
   virtual XrdOucCacheIO *Base() { return m_io; }

   //! Original data source URL.
   const char *Path() override { return m_io->Path(); }

   using XrdOucCacheIO::Sync;
   int Sync() override { return 0; }

   using XrdOucCacheIO::Trunc;
   int Trunc(long long Offset) override { return -ENOTSUP; }

   using XrdOucCacheIO::Write;
   int Write(char *Buffer, long long Offset, int Length) override { return -ENOTSUP; }

   void Update(XrdOucCacheIO &iocp) override;

   // Detach is virtual from XrdOucCacheIO, here it is split
   // into abstract ioActive() and DetachFinalize().
   bool Detach(XrdOucCacheIOCD &iocdP) final;

   virtual bool ioActive()       = 0;
   virtual void DetachFinalize() = 0;

   const char*  GetLocation() { return m_io->Location(false); }
   XrdSysTrace* GetTrace()    { return m_cache.GetTrace(); }

   XrdOucCacheIO* GetInput();

protected:
   Cache       &m_cache;   //!< reference to Cache object
   const char  *m_traceID;

   const char*  GetPath()         { return m_io->Path(); }
   std::string  GetFilename()     { return XrdCl::URL(GetPath()).GetPath(); }
   const char*  RefreshLocation() { return m_io->Location(true);  }

   unsigned short ObtainReadSid() { return m_read_seqid++; }

   struct ReadReqRHCond : public ReadReqRH
   {
      XrdSysCondVar m_cond   {0};
      int           m_retval {0};

      using ReadReqRH::ReadReqRH;

      void Done(int result) override
      { m_cond.Lock(); m_retval = result; m_cond.Signal(); m_cond.UnLock(); }
   };

   RAtomic_int       m_active_read_reqs; //!< number of active read requests

private:
   XrdSys::RAtomic<XrdOucCacheIO*> m_io; //!< original data source
   RAtomic_ushort          m_read_seqid; //!< sequential read id (for logging)

   void SetInput(XrdOucCacheIO*);

   // Variables used by File to store IO-relates state. Managed under
   // File::m_state_cond mutex.
   friend class File;

   time_t m_attach_time       {0}; // Set by File::AddIO()
   int    m_active_prefetches {0};
   bool   m_allow_prefetching {true};
   bool   m_in_detach         {false};
};
}

#endif
