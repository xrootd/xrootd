#ifndef __XRD_OSS_MIRAGE_WRAPPER_FILE_HH__
#define __XRD_OSS_MIRAGE_WRAPPER_FILE_HH__

#include "XrdOssMirage.hh"

#include <XrdOss/XrdOssWrapper.hh>

#include <cstdint>
#include <memory>

//
// Wraps a single file request. The active back end is chosen on Open() from
// the path and every subsequent call is forwarded to it. Until Open() decides
// otherwise the stacked file (the one the base class wraps) stays active.
//
class XrdOssMirageWrapperFile : public XrdOssWrapDF {
private:
    XrdOssMirage             &mirage;
    std::unique_ptr<XrdOssDF> wrapped_file;
    std::unique_ptr<XrdOssDF> mirage_file;
    XrdOssDF                 *active;

public:
    XrdOssMirageWrapperFile(XrdOssMirage &mirage, std::unique_ptr<XrdOssDF> wrapped_file, const char *tident);
    virtual ~XrdOssMirageWrapperFile() = default;

    virtual int     StatRet(struct stat *buff) override;
    virtual int     Fchmod(mode_t mode) override;
    virtual int     Fstat(struct stat *buf) override;
    virtual int     Fsync() override;
    virtual int     Fsync(XrdSfsAio *aiop) override;
    virtual int     Ftruncate(unsigned long long flen) override;
    virtual int     Open(const char *path, int Oflag, mode_t Mode, XrdOucEnv &env) override;
    virtual ssize_t pgRead (void* buffer, off_t offset, size_t rdlen, uint32_t* csvec, uint64_t opts) override;
    virtual int     pgRead (XrdSfsAio* aioparm, uint64_t opts) override;
    virtual ssize_t pgWrite(void* buffer, off_t offset, size_t wrlen, uint32_t* csvec, uint64_t opts) override;
    virtual int     pgWrite(XrdSfsAio* aioparm, uint64_t opts) override;
    virtual ssize_t Read(off_t offset, size_t size) override;
    virtual ssize_t Read(void *buffer, off_t offset, size_t size) override;
    virtual int     Read(XrdSfsAio *aiop) override;
    virtual ssize_t ReadRaw(void *buffer, off_t offset, size_t size) override;
    virtual ssize_t Write(const void *buffer, off_t offset, size_t size) override;
    virtual int     Write(XrdSfsAio *aiop) override;
    virtual int     Close(long long *retsz=0) override;
    virtual int     getFD() override;
};

#endif
