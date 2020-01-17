#ifndef __XRDPFC_CACHE_IO_HH__
#define __XRDPFC_CACHE_IO_HH__

class XrdSysTrace;

#include "XrdPfc.hh"
#include "XrdOuc/XrdOucCache.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdSys/XrdSysPthread.hh"

namespace XrdPfc
{
//----------------------------------------------------------------------------
//! Base cache-io class that implements XrdOucCacheIO abstract methods.
//----------------------------------------------------------------------------
class IO : public XrdOucCacheIO
{
public:
   IO (XrdOucCacheIO *io, XrdOucCacheStats &stats, Cache &cache);

   //! Original data source.
   virtual XrdOucCacheIO *Base() { return m_io; }

   //! Original data source URL.
   virtual const char *Path() { return m_io->Path(); }

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

   XrdSysTrace* GetTrace() { return m_cache.GetTrace(); }

   XrdOucCacheIO* GetInput();

protected:
   XrdOucCacheStats &m_statsGlobal;     //!< reference to Cache statistics
   Cache            &m_cache;           //!< reference to Cache needed in detach

   const char  *m_traceID;
   std::string  m_path;
   const char*  GetPath() { return m_path.c_str(); }

private:
   XrdOucCacheIO  *m_io;                //!< original data source
   XrdSysMutex     updMutex;
   void SetInput(XrdOucCacheIO*);
};
}

#endif

