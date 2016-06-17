#include "XrdFileCacheIO.hh"
#include "XrdFileCacheTrace.hh"
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
   TRACE(Info, "IO::Update() " << Path() << " location: " <<
         (Location() && Location()[0] != 0) ? Location() : "<not set>");
}

void IO::SetInput(XrdOucCacheIO2* x)
{
   XrdSysMutexHelper lock(&updMutex);
   m_io = x;
}

XrdOucCacheIO2* IO::GetInput()
{
   XrdSysMutexHelper lock(&updMutex);
   return m_io;
}
