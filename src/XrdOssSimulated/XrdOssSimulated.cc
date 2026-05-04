#include "XrdOssSimulated.hh"
#include "XrdOssSimulatedDir.hh"
#include "XrdOssSimulatedFile.hh"
#include "XrdVersion.hh"

#include "XrdSys/XrdSysError.hh"

namespace XrdGlobal
{
    extern XrdSysError Log;
}

XrdVERSIONINFO(XrdOssGetStorageSystem, XrdOssSimulated);

extern "C"
{
    XrdOss *XrdOssGetStorageSystem(XrdOss* native_oss, XrdSysLogger* lp, const char* config_fn, const char* parms)
    {
        return new XrdOssSimulated;
    }
}

XrdOssDF *XrdOssSimulated::newDir(const char *tident)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    
    return new XrdOssSimulatedDir;
}

XrdOssDF *XrdOssSimulated::newFile(const char *tident)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    return new XrdOssSimulatedFile(this);
}

int XrdOssSimulated::Chmod(const char * path, mode_t mode, XrdOucEnv *envP)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return -ENOTSUP;
}

int XrdOssSimulated::Create(const char *tid, const char *path, mode_t mode, XrdOucEnv &env, int opts)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    std::lock_guard lock(mutex);

    if ((opts & XRDOSS_new) && entries.contains(path))
        return -EEXIST;

    entries[path] = std::move(XrdOssSimulatedEntry{});

    return XrdOssOK;
}

uint64_t XrdOssSimulated::Features()
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return XRDOSS_HASNAIO | XRDOSS_HASFICL;
}

int XrdOssSimulated::Init(XrdSysLogger *lp, const char *cfn)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return XrdOssOK;
}

int XrdOssSimulated::Mkdir(const char *path, mode_t mode, int mkpath, XrdOucEnv  *envP)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return -ENOTSUP;
}

int XrdOssSimulated::Remdir(const char *path, int Opts, XrdOucEnv *envP)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return -ENOTSUP;
}

int XrdOssSimulated::Rename(const char *oPath, const char *nPath, XrdOucEnv  *oEnvP, XrdOucEnv *nEnvP)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    std::lock_guard lock(mutex);

    if (!entries.contains(oPath))
        return -ENOENT;

    if (entries.contains(nPath))
        return -EEXIST;

    entries[nPath] = std::move(entries[oPath]);
    entries.erase(oPath);

    return XrdOssOK;
}

int XrdOssSimulated::Stat(const char *path, struct stat *buff, int opts, XrdOucEnv *envP)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    std::lock_guard lock(mutex);

    if (!entries.contains(path))
        return -ENOENT;

    buff->st_size = entries[path].size;

    return XrdOssOK;
}

int XrdOssSimulated::Truncate(const char *path, unsigned long long fsize, XrdOucEnv *envP)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    std::lock_guard lock(mutex);

    if (!entries.contains(path))
        return -ENOENT;

    entries[path].size = fsize;
    
    return XrdOssOK;
}

int XrdOssSimulated::Unlink(const char *path, int Opts, XrdOucEnv *envP)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    std::lock_guard lock(mutex);

    if (!entries.contains(path))
        return -ENOENT;

    entries.erase(path);

    return XrdOssOK;
}
