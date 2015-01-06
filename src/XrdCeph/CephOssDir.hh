#ifndef __CEPH_OSS_DIR_HH__
#define __CEPH_OSS_DIR_HH__

#include <XrdOss/XrdOss.hh>
#include "CephOss.hh"

//------------------------------------------------------------------------------
//! This class implements XrdOssDF interface for usage with a CEPH storage.
//! It has a very restricted usage as the only valid path for opendir is '/'.
//! The reason is that ceph is an object store where you can only list all
//! objects, and that has no notion of hierarchy
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
