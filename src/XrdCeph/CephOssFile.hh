#ifndef __CEPH_OSS_FILE_HH__
#define __CEPH_OSS_FILE_HH__

#include <XrdOss/XrdOss.hh>
#include "CephOss.hh"

class CephOssFile : public XrdOssDF {

public:

  CephOssFile(CephOss *cephoss);
  virtual ~CephOssFile() {};
  virtual int Open(const char *path, int flags, mode_t mode, XrdOucEnv &env);
  virtual int Close(long long *retsz=0);
  virtual ssize_t Read(off_t offset, size_t blen);
  virtual ssize_t Read(void *buff, off_t offset, size_t blen);
  virtual int     Read(XrdSfsAio *aoip);
  virtual ssize_t ReadRaw(void *, off_t, size_t);
  virtual int Fstat(struct stat *buff);
  virtual ssize_t Write(const void *buff, off_t offset, size_t blen);
  virtual int Fsync(void);

private:

  int m_fd;
  CephOss *m_cephOss;

};

#endif /* __CEPH_OSS_FILE_HH__ */
