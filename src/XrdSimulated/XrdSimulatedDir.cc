#include "XrdSimulatedDir.hh"
#include "XrdSys/XrdSysError.hh"

namespace XrdGlobal
{
    extern XrdSysError Log;
}

int XrdSimulatedDir::Opendir(const char *path, XrdOucEnv &env)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

int XrdSimulatedDir::Readdir(char *buff, int blen)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

int XrdSimulatedDir::StatRet(struct stat *buff)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

int XrdSimulatedDir::Close(long long *retsz)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}
