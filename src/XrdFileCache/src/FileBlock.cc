#include <string>
#include <sstream>


#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucCache.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdSys/XrdSysError.hh"
#include <XrdSys/XrdSysPthread.hh>
#include "CacheStats.hh"

#include <fcntl.h>

#include "FileBlock.hh"
#include "Factory.hh"
#include "Context.hh"
#include "IO.hh"


using namespace XrdFileCache;

void *
PrefetchRunnerBl(void * prefetch_void)
{
    Prefetch *prefetch = static_cast<Prefetch*>(prefetch_void);
    prefetch->Run();
    return NULL;
}

FileBlock::FileBlock(off_t off, int blocksize, XrdOucCacheIO*  io):
    m_diskDF(0),
    m_prefetch(0),
    m_offset0(off),
    m_io(io)
{
    XrdOucEnv myEnv;
    std::string fname;
    IO::getFilePathFromURL(io->Path(), fname);
    std::stringstream ss;
    ss << Factory::GetInstance().GetTempDirectory() << fname;
    char offExt[64];
    sprintf(&offExt[0],"_%lld", m_offset0 );
    ss << &offExt[0];
    fname = ss.str();

    if (1) {
        aMsgIO(kDebug, io, "FileBlock::FileBlock(), create Prefetch.");
        m_prefetch = new Prefetch(*io, fname, m_offset0, blocksize);
        pthread_t tid;
        XrdSysThread::Run(&tid, PrefetchRunnerBl, (void *)m_prefetch, 0, "XrdHdfsCache Prefetcher");
    }
}

FileBlock::~FileBlock()
{
    delete m_prefetch;
}

int FileBlock::Read(char *buff, long long off, int size)
{
        aMsg(kDebug, "FileBlock::Read() read off0 =  %d",  m_offset0);
        ssize_t retval = 0;
        aMsg(kDebug,"FileBlock::Read() from prefetch");

        retval = m_prefetch->Read(buff , off -m_offset0, size);
        return retval;
}

