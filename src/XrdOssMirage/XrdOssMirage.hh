#ifndef __XRD_OSS_MIRAGE_HH__
#define __XRD_OSS_MIRAGE_HH__

#include "XrdOssMirageEntry.hh"

#include <XrdOss/XrdOss.hh>

#include <optional>
#include <unordered_map>

class XrdOssMirage : public XrdOss {
private:
    std::unordered_map<std::string, XrdOssMirageEntryPtr> entries;
    std::mutex mutex;

    bool has_entry(const char *path);
    bool is_entry_being_written(const char *path);

public:
    XrdOssMirage() = default;
    virtual ~XrdOssMirage() = default;

    virtual XrdOssDF *newDir(const char *tident) override;
    virtual XrdOssDF *newFile(const char *tident) override;
    virtual int       Chmod(const char * path, mode_t mode, XrdOucEnv *envP=0) override;
    virtual int       Create(const char *tid, const char *path, mode_t mode, XrdOucEnv &env, int opts=0) override;
    virtual uint64_t  Features() override;
    virtual int       Init(XrdSysLogger *lp, const char *cfn) override;
    virtual int       Mkdir(const char *path, mode_t mode, int mkpath=0, XrdOucEnv  *envP=0) override;
    virtual int       Remdir(const char *path, int Opts=0, XrdOucEnv *envP=0) override;
    virtual int       Rename(const char *oPath, const char *nPath, XrdOucEnv  *oEnvP=0, XrdOucEnv *nEnvP=0) override;
    virtual int       Stat(const char *path, struct stat *buff, int opts=0, XrdOucEnv *envP=0) override;
    virtual int       Truncate(const char *path, unsigned long long fsize, XrdOucEnv *envP=0) override;
    virtual int       Unlink(const char *path, int Opts=0, XrdOucEnv *envP=0) override;

    std::optional<XrdOssMirageEntry>     get_entry_read(const char *path);
    std::optional<XrdOssMirageEntryPtr>  get_entry_write(const char *path);
};

#endif
