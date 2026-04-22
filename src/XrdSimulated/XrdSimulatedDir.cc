#include "XrdSimulatedDir.hh"

int XrdSimulatedDir::Opendir(const char *path, XrdOucEnv &env)
{
    return 0;
}

int XrdSimulatedDir::Readdir(char *buff, int blen)
{
    return 0;
}

int XrdSimulatedDir::StatRet(struct stat *buff)
{
    return 0;
}

int XrdSimulatedDir::Close(long long *retsz)
{
    return 0;
}
