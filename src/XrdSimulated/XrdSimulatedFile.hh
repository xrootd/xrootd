#ifndef __XRD_SIMULATED_FILE_HH__
#define __XRD_SIMULATED_FILE_HH__

#include <XrdOss/XrdOss.hh>

class XrdSimulatedFile : public XrdOssDF {
public:
    XrdSimulatedFile() = default;
    virtual ~XrdSimulatedFile() = default;

    virtual int     StatRet(struct stat *buff) override;
    virtual int     Clone(XrdOssDF& srcFile) override;
    virtual int     Clone(const std::vector<XrdOucCloneSeg> &cVec) override;
    virtual int     Fchmod(mode_t mode) override;
    virtual void    Flush() override;
    virtual int     Fstat(struct stat *buf) override;
    virtual int     Fsync() override;
    virtual int     Fsync(XrdSfsAio *aiop) override;
    virtual int     Ftruncate(unsigned long long flen) override;
    virtual off_t   getMmap(void **addr) override;
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
};

#endif
