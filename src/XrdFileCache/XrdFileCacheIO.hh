#ifndef __XRDFILECACHE_CACHE_IO_HH__
#define __XRDFILECACHE_CACHE_IO_HH__

class XrdSysTrace;

#include "XrdFileCache.hh"
#include "XrdOuc/XrdOucCache2.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdSys/XrdSysPthread.hh"

namespace XrdFileCache
{
//----------------------------------------------------------------------------
//! Base cache-io class that implements XrdOucCacheIO abstract methods.
//----------------------------------------------------------------------------
class IO : public XrdOucCacheIO2
{
public:
   IO (XrdOucCacheIO2 *io, XrdOucCacheStats &stats, Cache &cache);

   //! Original data source.
   virtual XrdOucCacheIO *Base() { return m_io; }

   //! Original data source URL.
   virtual const char *Path() { return m_io->Path(); }

   using XrdOucCacheIO2::Sync;

   virtual int Sync() { return 0; }

   using XrdOucCacheIO2::Trunc;

   virtual int Trunc(long long Offset) { return -ENOTSUP; }

   using XrdOucCacheIO2::Write;

   virtual int Write(char *Buffer, long long Offset, int Length) { return -ENOTSUP; }

   virtual void Update(XrdOucCacheIO2 &iocp);

   XrdSysTrace* GetTrace() { return m_cache.GetTrace(); }

   XrdOucCacheIO2* GetInput();

protected:
   XrdOucCacheStats &m_statsGlobal;     //!< reference to Cache statistics
   Cache            &m_cache;           //!< reference to Cache needed in detach

   const char  *m_traceID;
   std::string  m_path;
   const char*  GetPath() { return m_path.c_str(); }

private:
   XrdOucCacheIO2 *m_io;                //!< original data source
   XrdSysMutex     updMutex;
   void SetInput(XrdOucCacheIO2*);
};
}

#endif

