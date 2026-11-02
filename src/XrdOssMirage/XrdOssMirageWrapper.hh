#ifndef __XRD_OSS_MIRAGE_WRAPPER_HH__
#define __XRD_OSS_MIRAGE_WRAPPER_HH__

#include "XrdOssMirage.hh"
#include "XrdOssMirageWrapperBase.hh"

#include <XrdOss/XrdOssWrapper.hh>

#include <memory>
#include <string_view>

class XrdOssMirageWrapper : public XrdOssWrapper, public XrdOssMirageWrapperBase {
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
    virtual int       Reloc(const char *tident, const char *path, const char *cgName, const char *anchor=0) override;
    virtual int       Remdir(const char *path, int Opts=0, XrdOucEnv *envP=0) override;
    virtual int       Rename(const char *oPath, const char *nPath, XrdOucEnv *oEnvP=0, XrdOucEnv *nEnvP=0) override;
    virtual int       Stat(const char *path, struct stat *buff, int opts=0, XrdOucEnv *envP=0) override;
    virtual int       StatFS(const char *path, char *buff, int &blen, XrdOucEnv *envP=0) override;
    virtual int       StatPF(const char *path, struct stat *buff, int opts) override;
    virtual int       StatPF(const char *path, struct stat *buff) override;
    virtual int       StatXA(const char *path, char *buff, int &blen, XrdOucEnv *envP=0) override;
    virtual int       StatXP(const char *path, unsigned long long &attr, XrdOucEnv *envP=0) override;
    virtual int       Truncate(const char *path, unsigned long long fsize, XrdOucEnv *envP=0) override;
    virtual int       Unlink(const char *path, int Opts=0, XrdOucEnv *envP=0) override;
    virtual void      Connect(XrdOucEnv &env) override;
    virtual void      Disc(XrdOucEnv &env) override;
    virtual void      EnvInfo(XrdOucEnv *envP) override;
    virtual uint64_t  Features() override;
    virtual bool      getErrMsg(std::string &eText) override;
    virtual int       StatLS(XrdOucEnv &env, const char *path, char *buff, int &blen) override;
};

#endif
