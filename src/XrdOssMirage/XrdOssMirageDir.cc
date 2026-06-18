#include "XrdOssMirageDir.hh"

int XrdOssMirageDir::Opendir(const char *path, XrdOucEnv &env)
{
    return -ENOTSUP;
}

int XrdOssMirageDir::Readdir(char *buff, int blen)
{
    return -ENOTSUP;
}

int XrdOssMirageDir::StatRet(struct stat *buff)
{
    return -ENOTSUP;
}

int XrdOssMirageDir::Close(long long *retsz)
{
    return -ENOTSUP;
}
