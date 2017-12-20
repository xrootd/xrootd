#include "XrdFileCacheIO.hh"
#include "XrdFileCacheTrace.hh"

using namespace XrdFileCache;

IO::IO(XrdOucCacheIO2 *io, XrdOucCacheStats &stats, Cache &cache) :
   m_statsGlobal(stats), m_cache(cache), m_traceID("IO"), m_io(io)
{
   m_path = m_io->Path();
}

void IO::Update(XrdOucCacheIO2 &iocp)
{
   SetInput(&iocp);
   TRACE_PC(Info, const char* loc = m_io->Location(),
            "IO::Update() " << Path() << " location: " <<
            ((loc && loc[0] != 0) ? loc : "<not set>"));
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

