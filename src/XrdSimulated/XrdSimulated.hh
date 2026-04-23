#ifndef __XRD_SIMULATED_HH__
#define __XRD_SIMULATED_HH__

#include "XrdSimulatedEntry.hh"

#include <XrdOss/XrdOss.hh>

#include <unordered_map>
#include <mutex>

class XrdSimulated : public XrdOss {
private:
    std::unordered_map<std::string, XrdSimulatedEntry> entries;
    std::mutex mutex;

public:
    XrdSimulated() = default;
    virtual ~XrdSimulated() = default;

    virtual XrdOssDF *newDir(const char *tident) override;
    virtual XrdOssDF *newFile(const char *tident) override;
    virtual int       Chmod(const char * path, mode_t mode, XrdOucEnv *envP=0) override;
    virtual int       Create(const char *tid, const char *path, mode_t mode, XrdOucEnv &env, int opts=0) override;
    virtual int       Init(XrdSysLogger *lp, const char *cfn) override;
    virtual int       Mkdir(const char *path, mode_t mode, int mkpath=0, XrdOucEnv  *envP=0) override;
    virtual int       Remdir(const char *path, int Opts=0, XrdOucEnv *envP=0) override;
    virtual int       Rename(const char *oPath, const char *nPath, XrdOucEnv  *oEnvP=0, XrdOucEnv *nEnvP=0) override;
    virtual int       Stat(const char *path, struct stat *buff, int opts=0, XrdOucEnv *envP=0) override;
    virtual int       Truncate(const char *path, unsigned long long fsize, XrdOucEnv *envP=0) override;
    virtual int       Unlink(const char *path, int Opts=0, XrdOucEnv *envP=0) override;
};

#endif
