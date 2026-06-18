#include "XrdOssMirage.hh"
#include "XrdOssMirageDir.hh"
#include "XrdOssMirageFile.hh"
#include "XrdOssMirageXAttr.hh"
#include "XrdVersion.hh"

#include "XrdSys/XrdSysFAttr.hh"

XrdVERSIONINFO(XrdOssGetStorageSystem, XrdOssMirage);

extern "C"
{
    XrdOss *XrdOssGetStorageSystem(XrdOss* native_oss, XrdSysLogger* lp, const char* config_fn, const char* parms)
    {
        return static_cast<XrdOss *>(new XrdOssMirage);
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

    if (has_entry(path))
    {
        if (opts & XRDOSS_new)
            return -EEXIST;  

        if (is_entry_being_written(path))
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
    return XRDOSS_HASNAIO;
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

    if (!has_entry(oPath))
        return -ENOENT;

    if (has_entry(nPath))
        return -EEXIST;

    entries[nPath] = std::move(entries[oPath]);
    entries.erase(oPath);

    return XrdOssOK;
}

int XrdOssMirage::Stat(const char *path, struct stat *buff, int opts, XrdOucEnv *envP)
{
    const std::lock_guard lock(mutex);

    if (!has_entry(path))
        return -ENOENT;

    *buff = {};
    buff->st_size = entries[path]->size;

    return XrdOssOK;
}

int XrdOssMirage::Truncate(const char *path, unsigned long long fsize, XrdOucEnv *envP)
{
    const std::lock_guard lock(mutex);

    if (!has_entry(path))
        return -ENOENT;

    if (is_entry_being_written(path))
        return -EBUSY;

    entries[path]->size = fsize;
    
    return XrdOssOK;
}

int XrdOssMirage::Unlink(const char *path, int Opts, XrdOucEnv *envP)
{
    const std::lock_guard lock(mutex);

    if (!has_entry(path))
        return -ENOENT;

    entries.erase(path);

    return XrdOssOK;
}

std::optional<XrdOssMirageEntry> XrdOssMirage::get_entry_read(const char *path)
{
    const std::lock_guard lock(mutex);

    if (!has_entry(path) || is_entry_being_written(path))
        return {};

    return *entries[path];
}

std::optional<XrdOssMirageEntryPtr> XrdOssMirage::get_entry_write(const char *path)
{
    const std::lock_guard lock(mutex);

    if (!has_entry(path) || is_entry_being_written(path))
        return {};

    return entries[path];    
}

bool XrdOssMirage::has_entry(const char *path)
{
    return entries.find(path) != entries.end();
}

bool XrdOssMirage::is_entry_being_written(const char *path)
{
    return entries[path].use_count() > 1;
}
