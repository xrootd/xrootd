#ifndef __XRD_OSS_MIRAGE_WRAPPER_DIR_HH__
#define __XRD_OSS_MIRAGE_WRAPPER_DIR_HH__

#include "XrdOssMirage.hh"

#include <XrdOss/XrdOssWrapper.hh>

#include <memory>

//
// Wraps a single directory request, routing on Opendir() in the same way
// XrdOssMirageWrapperFile does on Open().
//
class XrdOssMirageWrapperDir : public XrdOssWrapDF {
private:
    XrdOssMirage             &mirage;
    std::unique_ptr<XrdOssDF> wrapped_dir;
    std::unique_ptr<XrdOssDF> mirage_dir;
    XrdOssDF                 *active;

public:
    XrdOssMirageWrapperDir(XrdOssMirage &mirage, std::unique_ptr<XrdOssDF> wrapped_dir, const char *tident);
    virtual ~XrdOssMirageWrapperDir() = default;

    virtual int Opendir(const char *path, XrdOucEnv &env) override;
    virtual int Readdir(char *buff, int blen) override;
    virtual int StatRet(struct stat *buff) override;
    virtual int Close(long long *retsz=0) override;
};

#endif
