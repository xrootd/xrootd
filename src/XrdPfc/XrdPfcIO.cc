#include "XrdPfcIO.hh"
#include "XrdPfcTrace.hh"

using namespace XrdPfc;

IO::IO(XrdOucCacheIO *io, XrdOucCacheStats &stats, Cache &cache) :
   m_statsGlobal(stats), m_cache(cache), m_traceID("IO"), m_io(io)
{
   m_path = m_io->Path();
}

void IO::Update(XrdOucCacheIO &iocp)
{
   SetInput(&iocp);
   TRACE_PC(Info, const char* loc = m_io->Location(),
            "IO::Update() " << Path() << " location: " <<
            ((loc && loc[0] != 0) ? loc : "<not set>"));
}

void IO::SetInput(XrdOucCacheIO* x)
{
   XrdSysMutexHelper lock(&updMutex);
   m_io = x;
}

XrdOucCacheIO* IO::GetInput()
{
   XrdSysMutexHelper lock(&updMutex);
   return m_io;
}

