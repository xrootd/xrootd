#ifndef __XRDPFC_CACHE_IO_HH__
#define __XRDPFC_CACHE_IO_HH__

class XrdSysTrace;

#include "XrdPfc.hh"
#include "XrdOuc/XrdOucCache.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdSys/XrdSysPthread.hh"

#include <atomic>

namespace XrdPfc
{
//----------------------------------------------------------------------------
//! Base cache-io class that implements XrdOucCacheIO abstract methods.
//----------------------------------------------------------------------------
class IO : public XrdOucCacheIO
{
public:
   IO (XrdOucCacheIO *io, Cache &cache);

   //! Original data source.
   virtual XrdOucCacheIO *Base() { return m_io; }

   //! Original data source URL.
   virtual const char *Path() { return m_io.load(std::memory_order_relaxed)->Path(); }

   using XrdOucCacheIO::Sync;

   virtual int Sync() { return 0; }

   using XrdOucCacheIO::Trunc;

   virtual int Trunc(long long Offset) { return -ENOTSUP; }

   using XrdOucCacheIO::Write;

   virtual int Write(char *Buffer, long long Offset, int Length) { return -ENOTSUP; }

   virtual void Update(XrdOucCacheIO &iocp);

   // Detach is virtual from XrdOucCacheIO, here it is split
   // into abstract ioActive() and DetachFinalize().
   bool Detach(XrdOucCacheIOCD &iocdP) /* final */;

   virtual bool ioActive()       = 0;
   virtual void DetachFinalize() = 0;

   const char*  GetLocation() { return m_io.load(std::memory_order_relaxed)->Location(false); }
   XrdSysTrace* GetTrace()    { return m_cache.GetTrace(); }

   XrdOucCacheIO* GetInput();

protected:
   Cache       &m_cache;           //!< reference to Cache needed in detach
   const char  *m_traceID;

   const char*  GetPath()         { return m_io.load(std::memory_order_relaxed)->Path(); }
   std::string  GetFilename()     { return XrdCl::URL(GetPath()).GetPath(); }
   const char*  RefreshLocation() { return m_io.load(std::memory_order_relaxed)->Location(true);  }

private:
   std::atomic<XrdOucCacheIO*> m_io;                //!< original data source

   void         SetInput(XrdOucCacheIO*);
};
}

#endif
