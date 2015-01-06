#ifndef __CEPH_OSS_HH__
#define __CEPH_OSS_HH__

#include <string>
#include <XrdOss/XrdOss.hh>

//------------------------------------------------------------------------------
//! This class implements XrdOss interface for usage with a CEPH storage.
//! It should be loaded via the ofs.osslib directive.
//!
//! This plugin is able to use any pool of ceph with any userId.
//! There are several ways to provide the pool and userId to be used for a given
//! operation. Here is the ordered list of possibilities.
//! First one defined wins :
//!   - the path can be prepended with userId and pool. Syntax is :
//!       [[userId@]pool:]<actual path>
//!   - the XrdOucEnv parameter, when existing, can have 'cephUserId' and/or
//!     'cephPool' entries
//!   - the ofs.osslib directive can provide an argument with format :
//!       [userID@]pool
//!   - default are 'admin' and 'default' for userId and pool respectively
//!
//! Note that the definition of a default via the ofs.osslib directive may
//! clash with one used in a ofs.xattrlib directive. In case both directives
//! have a default and they are different, the behavior is not defined.
//! In case one of the two only has a default, it will be applied for both plugins.
//------------------------------------------------------------------------------

class CephOss : public XrdOss {
public:
  CephOss();
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

};

#endif /* __CEPH_OSS_HH__ */
