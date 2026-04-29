#include "XrdOssSimulatedDir.hh"
#include "XrdSys/XrdSysError.hh"

namespace XrdGlobal
{
    extern XrdSysError Log;
}

int XrdOssSimulatedDir::Opendir(const char *path, XrdOucEnv &env)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return -ENOTSUP;
}

int XrdOssSimulatedDir::Readdir(char *buff, int blen)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return -ENOTSUP;
}

int XrdOssSimulatedDir::StatRet(struct stat *buff)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return -ENOTSUP;
}

int XrdOssSimulatedDir::Close(long long *retsz)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return -ENOTSUP;
}
