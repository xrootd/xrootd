#include "XrdSimulated.hh"

#include "XrdVersion.hh"

XrdVERSIONINFO(XrdOssGetStorageSystem, XrdSimulated);

extern "C"
{
    XrdOss *XrdOssGetStorageSystem(XrdOss* native_oss, XrdSysLogger* lp, const char* config_fn, const char* parms)
    {
        return new XrdSimulated;
    }
}

XrdOssDF *XrdSimulated::newDir(const char *tident)
{
    return nullptr;
}

XrdOssDF *XrdSimulated::newFile(const char *tident)
{
    return nullptr;
}

int XrdSimulated::Chmod(const char * path, mode_t mode, XrdOucEnv *envP)
{
    return 0;
}

int XrdSimulated::Create(const char *tid, const char *path, mode_t mode, XrdOucEnv &env, int opts)
{
    return 0;
}

int XrdSimulated::Init(XrdSysLogger *lp, const char *cfn)
{
    return 0;
}

int XrdSimulated::Mkdir(const char *path, mode_t mode, int mkpath, XrdOucEnv  *envP)
{
    return 0;
}

int XrdSimulated::Remdir(const char *path, int Opts, XrdOucEnv *envP)
{
    return 0;
}

int XrdSimulated::Rename(const char *oPath, const char *nPath, XrdOucEnv  *oEnvP, XrdOucEnv *nEnvP)
{
    return 0;
}

int XrdSimulated::Stat(const char *path, struct stat *buff, int opts, XrdOucEnv *envP)
{
    return 0;
}

int XrdSimulated::Truncate(const char *path, unsigned long long fsize, XrdOucEnv *envP)
{
    return 0;
}

int XrdSimulated::Unlink(const char *path, int Opts, XrdOucEnv *envP)
{
    return 0;
}
