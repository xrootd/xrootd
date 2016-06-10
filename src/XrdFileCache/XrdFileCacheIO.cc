#include "XrdFileCacheIO.hh"
#include "XrdSys/XrdSysAtomics.hh"
#include "XrdPosix/XrdPosixFile.hh"

using namespace XrdFileCache;

IO::IO(XrdOucCacheIO2 *io, XrdOucCacheStats &stats, Cache &cache):
m_statsGlobal(stats), m_cache(cache), m_traceID("IO"), m_io(io)
{
   m_path = m_io->Path();
}

void IO::Update(XrdOucCacheIO2 &iocp)
{
   SetInput(&iocp);
}


void IO::SetInput(XrdOucCacheIO2* x)
{
   updMutex.Lock();
   m_io = x;
   updMutex.UnLock();
}

XrdOucCacheIO2* IO::GetInput()
{
   AtomicBeg(updMutex);
   return m_io;
   AtomicEnd(updMutex);
}
