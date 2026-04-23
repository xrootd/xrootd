#include "XrdSimulated.hh"
#include "XrdSimulatedDir.hh"
#include "XrdSimulatedFile.hh"
#include "XrdVersion.hh"

#include "XrdSys/XrdSysError.hh"

#include <mutex>
#include <shared_mutex>

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
    
    return new XrdSimulatedDir;
}

XrdOssDF *XrdSimulated::newFile(const char *tident)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    return new XrdSimulatedFile;
}

int XrdSimulated::Chmod(const char * path, mode_t mode, XrdOucEnv *envP)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    std::scoped_lock lock(mutex);

    if (!entries.contains(path))
        return -ENOENT;

    
    
    return XrdOssOK;
}

int XrdSimulated::Create(const char *tid, const char *path, mode_t mode, XrdOucEnv &env, int opts)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    std::scoped_lock lock(mutex);

    if ((opts & XRDOSS_new) && entries.contains(path))
        return -EEXIST;

    XrdSimulatedEntry entry {};

    entries[path] = std::move(entry);

    return XrdOssOK;
}

int XrdSimulated::Init(XrdSysLogger *lp, const char *cfn)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

int XrdSimulated::Mkdir(const char *path, mode_t mode, int mkpath, XrdOucEnv  *envP)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return XrdOssOK;
}

int XrdSimulated::Remdir(const char *path, int Opts, XrdOucEnv *envP)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return XrdOssOK;
}

int XrdSimulated::Rename(const char *oPath, const char *nPath, XrdOucEnv  *oEnvP, XrdOucEnv *nEnvP)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    std::scoped_lock lock(mutex);

    if (!entries.contains(oPath))
        return -ENOENT;

    if (entries.contains(nPath))
        return -EEXIST;

    std::unique_lock<std::shared_mutex> file_lock(*entries[oPath].mutex, std::try_to_lock);

    if (!file_lock.owns_lock())
        return -EBUSY;

    entries[nPath] = std::move(entries[oPath]);
    entries.erase(oPath);

    return XrdOssOK;
}

int XrdSimulated::Stat(const char *path, struct stat *buff, int opts, XrdOucEnv *envP)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    std::scoped_lock lock(mutex);

    if (!entries.contains(path))
        return -ENOENT;

    return XrdOssOK;
}

int XrdSimulated::Truncate(const char *path, unsigned long long fsize, XrdOucEnv *envP)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    std::scoped_lock lock(mutex);

    if (!entries.contains(path))
        return -ENOENT;

    std::unique_lock<std::shared_mutex> file_lock(*entries[path].mutex, std::try_to_lock);

    if (!file_lock.owns_lock())
        return -EBUSY;

    
    
    return XrdOssOK;
}

int XrdSimulated::Unlink(const char *path, int Opts, XrdOucEnv *envP)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    std::scoped_lock lock(mutex);

    if (!entries.contains(path))
        return -ENOENT;

    std::unique_lock<std::shared_mutex> file_lock(*entries[path].mutex, std::try_to_lock);

    if (!file_lock.owns_lock())
        return -EBUSY;

    entries.erase(path);

    return XrdOssOK;
}
