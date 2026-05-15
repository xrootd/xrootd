#include "XrdOssSimulated.hh"
#include "XrdOssSimulatedDir.hh"
#include "XrdOssSimulatedFile.hh"
#include "XrdOssSimulatedXAttr.hh"
#include "XrdVersion.hh"

#include "XrdSys/XrdSysFAttr.hh"

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
    return new XrdOssSimulatedDir;
}

XrdOssDF *XrdOssSimulated::newFile(const char *tident)
{
    return new XrdOssSimulatedFile(*this);
}

int XrdOssSimulated::Chmod(const char * path, mode_t mode, XrdOucEnv *envP)
{
    return -ENOTSUP;
}

int XrdOssSimulated::Create(const char *tid, const char *path, mode_t mode, XrdOucEnv &env, int opts)
{
    const std::lock_guard lock(mutex);

    if ((opts & XRDOSS_new) && hasEntry(path))
        return -EEXIST;

    entries[path] = std::make_shared<XrdOssSimulatedEntry>();

    XrdOssSimulatedXAttr * const xattr = static_cast<XrdOssSimulatedXAttr*>(XrdSysFAttr::Xat);
    if (xattr != nullptr)
        xattr->setOss(*this);

    return XrdOssOK;
}

uint64_t XrdOssSimulated::Features()
{
    return XRDOSS_HASNAIO | XRDOSS_HASFICL;
}

int XrdOssSimulated::Init(XrdSysLogger *lp, const char *cfn)
{
    return XrdOssOK;
}

int XrdOssSimulated::Mkdir(const char *path, mode_t mode, int mkpath, XrdOucEnv  *envP)
{
    return -ENOTSUP;
}

int XrdOssSimulated::Remdir(const char *path, int Opts, XrdOucEnv *envP)
{
    return -ENOTSUP;
}

int XrdOssSimulated::Rename(const char *oPath, const char *nPath, XrdOucEnv  *oEnvP, XrdOucEnv *nEnvP)
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

int XrdOssSimulated::Stat(const char *path, struct stat *buff, int opts, XrdOucEnv *envP)
{
    const std::lock_guard lock(mutex);

    if (!hasEntry(path))
        return -ENOENT;

    buff->st_size = entries[path]->size;

    return XrdOssOK;
}

int XrdOssSimulated::Truncate(const char *path, unsigned long long fsize, XrdOucEnv *envP)
{
    const std::lock_guard lock(mutex);

    if (!hasEntry(path))
        return -ENOENT;

    if (isEntryBeingWritten(path))
        return -EBUSY;

    entries[path]->size = fsize;
    
    return XrdOssOK;
}

int XrdOssSimulated::Unlink(const char *path, int Opts, XrdOucEnv *envP)
{
    const std::lock_guard lock(mutex);

    if (!hasEntry(path))
        return -ENOENT;

    entries.erase(path);

    return XrdOssOK;
}

std::optional<XrdOssSimulatedEntry> XrdOssSimulated::getEntryRead(const char *path)
{
    const std::lock_guard lock(mutex);

    if (!hasEntry(path) || isEntryBeingWritten(path))
        return {};

    return *entries[path];
}

std::optional<XrdOssSimulatedEntryPtr> XrdOssSimulated::getEntryWrite(const char *path)
{
    const std::lock_guard lock(mutex);

    if (!hasEntry(path) || isEntryBeingWritten(path))
        return {};

    return entries[path];    
}

bool XrdOssSimulated::hasEntry(const char *path)
{
    return entries.contains(path);
}

bool XrdOssSimulated::isEntryBeingWritten(const char *path)
{
    return entries[path].use_count() > 1;
}
