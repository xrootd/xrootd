#include "XrdOssSimulatedDir.hh"

int XrdOssSimulatedDir::Opendir(const char *path, XrdOucEnv &env)
{
    return -ENOTSUP;
}

int XrdOssSimulatedDir::Readdir(char *buff, int blen)
{
    return -ENOTSUP;
}

int XrdOssSimulatedDir::StatRet(struct stat *buff)
{
    return -ENOTSUP;
}

int XrdOssSimulatedDir::Close(long long *retsz)
{
    return -ENOTSUP;
}
