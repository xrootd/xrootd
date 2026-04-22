#ifndef __XRD_SIMULATED_DIR_HH__
#define __XRD_SIMULATED_DIR_HH__

#include <XrdOss/XrdOss.hh>

class XrdSimulatedDir : public XrdOssDF {
public:
    XrdSimulatedDir() = default;
    virtual ~XrdSimulatedDir() = default;

    virtual int Opendir(const char *path, XrdOucEnv &env) override;
    virtual int Readdir(char *buff, int blen) override;
    virtual int StatRet(struct stat *buff) override;
    virtual int Close(long long *retsz=0) override;
};

#endif
