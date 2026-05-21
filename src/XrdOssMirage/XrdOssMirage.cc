#include "XrdOssMirage.hh"
#include "XrdOssMirageDir.hh"
#include "XrdOssMirageFile.hh"
#include "XrdOssMirageXAttr.hh"
#include "XrdVersion.hh"

#include "XrdSys/XrdSysFAttr.hh"

#include <mutex>

XrdVERSIONINFO(XrdOssGetStorageSystem, XrdOssMirage);

extern "C"
{
    XrdOss *XrdOssGetStorageSystem(XrdOss* native_oss, XrdSysLogger* lp, const char* config_fn, const char* parms)
    {
        return new XrdOssMirage;
    }
}

XrdOssDF *XrdOssMirage::newDir(const char *tident)
{
    return new XrdOssMirageDir;
}

XrdOssDF *XrdOssMirage::newFile(const char *tident)
{
    return new XrdOssMirageFile(*this);
}

int XrdOssMirage::Chmod(const char * path, mode_t mode, XrdOucEnv *envP)
{
    return -ENOTSUP;
}

int XrdOssMirage::Create(const char *tid, const char *path, mode_t mode, XrdOucEnv &env, int opts)
{
    const std::lock_guard lock(mutex);

    if (hasEntry(path))
    {
        if (opts & XRDOSS_new)
            return -EEXIST;  

        if (isEntryBeingWritten(path))
            return -EBUSY;
    }

    // preserve previous configuration but reset the size in case it already exists
    entries.try_emplace(path, std::make_shared<XrdOssMirageEntry>());
    entries[path]->size = 0;

    static std::once_flag xattr_injection_flag;
    std::call_once(xattr_injection_flag, [this]() noexcept
        {
            if (XrdOssMirageXAttr * const xattr = dynamic_cast<XrdOssMirageXAttr*>(XrdSysFAttr::Xat); xattr != nullptr)
                xattr->setOss(*this);
        });

    return XrdOssOK;
}

uint64_t XrdOssMirage::Features()
{
    return XRDOSS_HASNAIO | XRDOSS_HASFICL;
}

int XrdOssMirage::Init(XrdSysLogger *lp, const char *cfn)
{
    return XrdOssOK;
}

int XrdOssMirage::Mkdir(const char *path, mode_t mode, int mkpath, XrdOucEnv  *envP)
{
    return -ENOTSUP;
}

int XrdOssMirage::Remdir(const char *path, int Opts, XrdOucEnv *envP)
{
    return -ENOTSUP;
}

int XrdOssMirage::Rename(const char *oPath, const char *nPath, XrdOucEnv  *oEnvP, XrdOucEnv *nEnvP)
{
    const std::lock_guard lock(mutex);

    if (!hasEntry(oPath))
        return -ENOENT;

    if (hasEntry(nPath))
        return -EEXIST;

    entries[nPath] = std::move(entries[oPath]);
    entries.erase(oPath);

    return XrdOssOK;
}

int XrdOssMirage::Stat(const char *path, struct stat *buff, int opts, XrdOucEnv *envP)
{
    const std::lock_guard lock(mutex);

    if (!hasEntry(path))
        return -ENOENT;

    buff->st_size = entries[path]->size;

    return XrdOssOK;
}

int XrdOssMirage::Truncate(const char *path, unsigned long long fsize, XrdOucEnv *envP)
{
    const std::lock_guard lock(mutex);

    if (!hasEntry(path))
        return -ENOENT;

    if (isEntryBeingWritten(path))
        return -EBUSY;

    entries[path]->size = fsize;
    
    return XrdOssOK;
}

int XrdOssMirage::Unlink(const char *path, int Opts, XrdOucEnv *envP)
{
    const std::lock_guard lock(mutex);

    if (!hasEntry(path))
        return -ENOENT;

    entries.erase(path);

    return XrdOssOK;
}

std::optional<XrdOssMirageEntry> XrdOssMirage::getEntryRead(const char *path)
{
    const std::lock_guard lock(mutex);

    if (!hasEntry(path) || isEntryBeingWritten(path))
        return {};

    return *entries[path];
}

std::optional<XrdOssMirageEntryPtr> XrdOssMirage::getEntryWrite(const char *path)
{
    const std::lock_guard lock(mutex);

    if (!hasEntry(path) || isEntryBeingWritten(path))
        return {};

    return entries[path];    
}

bool XrdOssMirage::hasEntry(const char *path)
{
    return entries.contains(path);
}

bool XrdOssMirage::isEntryBeingWritten(const char *path)
{
    return entries[path].use_count() > 1;
}
