#include "XrdSimulated.hh"
#include "XrdVersion.hh"

#include "XrdSys/XrdSysError.hh"

namespace XrdGlobal
{
    extern XrdSysError Log;
}

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
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return nullptr;
}

XrdOssDF *XrdSimulated::newFile(const char *tident)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return nullptr;
}

int XrdSimulated::Chmod(const char * path, mode_t mode, XrdOucEnv *envP)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

int XrdSimulated::Create(const char *tid, const char *path, mode_t mode, XrdOucEnv &env, int opts)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

int XrdSimulated::Init(XrdSysLogger *lp, const char *cfn)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

int XrdSimulated::Mkdir(const char *path, mode_t mode, int mkpath, XrdOucEnv  *envP)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

int XrdSimulated::Remdir(const char *path, int Opts, XrdOucEnv *envP)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

int XrdSimulated::Rename(const char *oPath, const char *nPath, XrdOucEnv  *oEnvP, XrdOucEnv *nEnvP)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

int XrdSimulated::Stat(const char *path, struct stat *buff, int opts, XrdOucEnv *envP)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

int XrdSimulated::Truncate(const char *path, unsigned long long fsize, XrdOucEnv *envP)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

int XrdSimulated::Unlink(const char *path, int Opts, XrdOucEnv *envP)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}
