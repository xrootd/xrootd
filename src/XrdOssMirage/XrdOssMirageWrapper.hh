#ifndef __XRD_OSS_MIRAGE_WRAPPER_HH__
#define __XRD_OSS_MIRAGE_WRAPPER_HH__

#include "XrdOssMirage.hh"

#include <XrdOss/XrdOssWrapper.hh>

#include <memory>

const std::string &mirage_prefix(const char *parms = nullptr);
bool is_mirage_path(std::string_view path) noexcept;

class XrdOssMirageWrapper : public XrdOssWrapper {
private:
    XrdOssMirage            mirage;

    XrdOss &route(const char *path);

public:
    XrdOssMirageWrapper(XrdOss *curr_oss, XrdSysLogger *logger, const char *parms, XrdOucEnv *envP);
    virtual ~XrdOssMirageWrapper() = default;

    virtual XrdOssDF *newDir(const char *tident) override;
    virtual XrdOssDF *newFile(const char *tident) override;
    virtual int         Lfn2Pfn(const char *Path, char *buff, int blen) override;
    virtual const char *Lfn2Pfn(const char *Path, char *buff, int blen, int &rc) override;
    virtual int       Chmod(const char *path, mode_t mode, XrdOucEnv *envP=0) override;
    virtual int       Create(const char *tid, const char *path, mode_t mode, XrdOucEnv &env, int opts=0) override;
    virtual int       Mkdir(const char *path, mode_t mode, int mkpath=0, XrdOucEnv *envP=0) override;
    virtual int       Remdir(const char *path, int Opts=0, XrdOucEnv *envP=0) override;
    virtual int       Rename(const char *oPath, const char *nPath, XrdOucEnv *oEnvP=0, XrdOucEnv *nEnvP=0) override;
    virtual int       Stat(const char *path, struct stat *buff, int opts=0, XrdOucEnv *envP=0) override;
    virtual int       Truncate(const char *path, unsigned long long fsize, XrdOucEnv *envP=0) override;
    virtual int       Unlink(const char *path, int Opts=0, XrdOucEnv *envP=0) override;
};

#endif
