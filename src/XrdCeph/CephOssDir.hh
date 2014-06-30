#ifndef __CEPH_OSS_DIR_HH__
#define __CEPH_OSS_DIR_HH__

#include <XrdOss/XrdOss.hh>
#include "CephOss.hh"

class CephOssDir : public XrdOssDF {

public:

  CephOssDir(CephOss *cephoss);
  virtual ~CephOssDir() {};
  virtual int Opendir(const char *, XrdOucEnv &);
  virtual int Readdir(char *buff, int blen);
  virtual int Close(long long *retsz=0);

private:

  DIR *m_dirp;
  CephOss *m_cephOss;

};

#endif /* __CEPH_OSS_DIR_HH__ */
