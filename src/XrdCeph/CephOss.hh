#ifndef __CEPH_OSS_HH__
#define __CEPH_OSS_HH__

#include <string>
#include <XrdOss/XrdOss.hh>

class CephOss : public XrdOss {
public:
  CephOss(const char* defaultPool);
  virtual ~CephOss();

  virtual int     Chmod(const char *, mode_t mode, XrdOucEnv *eP=0);
  virtual int     Create(const char *, const char *, mode_t, XrdOucEnv &, int opts=0);
  virtual int     Init(XrdSysLogger *, const char*);
  virtual int     Mkdir(const char *, mode_t mode, int mkpath=0, XrdOucEnv *eP=0);
  virtual int     Remdir(const char *, int Opts=0, XrdOucEnv *eP=0);
  virtual int     Rename(const char *, const char *, XrdOucEnv *eP1=0, XrdOucEnv *eP2=0);
  virtual int     Stat(const char *, struct stat *, int opts=0, XrdOucEnv *eP=0);
  virtual int     StatFS(const char *path, char *buff, int &blen, XrdOucEnv *eP=0);
  virtual int     StatVS(XrdOssVSInfo *sP, const char *sname=0, int updt=0);
  virtual int     Truncate(const char *, unsigned long long, XrdOucEnv *eP=0);
  virtual int     Unlink(const char *path, int Opts=0, XrdOucEnv *eP=0);
  virtual XrdOssDF *newDir(const char *tident);
  virtual XrdOssDF *newFile(const char *tident);

  /// get the pool to be used from the environment
  const char* getPoolFromEnv(XrdOucEnv* env);

private:

  /// default pool to be used when an object is opened without a pool
  std::string m_defaultPool;
  
};

#endif /* __CEPH_OSS_HH__ */
