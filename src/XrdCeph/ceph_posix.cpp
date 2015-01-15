/*
 * This interface provides wrapper methods for using ceph through a POSIX API.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <radosstriper/libradosstriper.hpp>
#include <map>
#include <string>
#include <sys/xattr.h>
#include <time.h>
#include <XrdCeph/ceph_posix.h>

/// small structs to store file data, either for CEPH or for a local file
struct CephFile {
  std::string name;
  std::string pool;
  std::string userId;
};

struct CephFileRef : CephFile {
  int flags;
  mode_t mode;
  unsigned long long offset;
};

/// small struct for directory listing
struct DirIterator {
  librados::ObjectIterator m_iterator;
  librados::IoCtx *m_ioctx;
};

/// global variables holding stripers and ioCtxs for each ceph pool plus the cluster object
std::map<std::string, libradosstriper::RadosStriper*> g_radosStripers;
std::map<std::string, librados::IoCtx*> g_ioCtx;
librados::Rados* g_cluster = 0;
/// global variable holding a map of file descriptor to file reference
std::map<unsigned int, CephFileRef> g_fds;
/// global variable holding a list of files currently opened for write
std::multiset<std::string> g_filesOpenForWrite;
/// global variable remembering the next available file descriptor
unsigned int g_nextCephFd = 0;
/// global variables containing default userId and pool
std::string g_defaultUserId = "admin";
std::string g_defaultPool = "default";

/// global variable for the log function
static void (*g_logfunc) (char *, va_list argp) = 0;

static void logwrapper(char* format, ...) {
  if (0 == g_logfunc) return;
  va_list arg;
  va_start(arg, format);
  (*g_logfunc)(format, arg);
  va_end(arg);  
}

/// sets the default userId and pool
void ceph_posix_set_defaults(const char* value) {
  if (value) {
    std::string svalue = value;
    size_t atPos = svalue.find('@');
    if (std::string::npos == atPos) {
      g_defaultPool = svalue;
    } else {
      g_defaultUserId = svalue.substr(0, atPos);
      g_defaultPool = svalue.substr(atPos+1);
    }
  }
}

/// fill a ceph file struct from a path and an environment
static void fillCephFile(const char *path, XrdOucEnv *env, CephFile &file) {
  // Syntax of the given path is :
  //   [[userId@]pool:]<actual path>
  // in case userId or pool is not provided and env is not null
  // the entries cephUserId and cephPool of env will be used.
  // If env is null or no entry is found for what is missing,
  // defaults are applied. These defaults are initially set to
  // 'admin' and 'default' but can be changed via a call to
  // ceph_posix_set_defaults
  std::string spath = path;
  size_t colonPos = spath.find(':');
  if (std::string::npos == colonPos) {
    file.name = spath;
  } else {
    size_t atPos = spath.find('@');
    if (std::string::npos == atPos || atPos > colonPos) {
      file.pool = spath.substr(0, colonPos);
    } else {
      file.userId = spath.substr(0, atPos);
      file.pool = spath.substr(atPos+1, colonPos-atPos-1);
    }
  }
  if (file.userId.empty()) {
    if (0 != env) {
      char* cuser = env->Get("cephUserId");
      if (0 != cuser) {
        file.userId = cuser;
      }
    }
    if (file.userId.empty()) {
      file.userId = g_defaultUserId;
    }
  }
  if (file.pool.empty()) {
    if (0 != env) {
      char* cpool = env->Get("cephPool");
      if (0 != cpool) {
        file.pool = cpool;
      }
    }
    if (file.pool.empty()) {
      file.pool = g_defaultPool;
    }
  }
}

static CephFile getCephFile(const char *path, XrdOucEnv *env) {
  CephFile file;
  fillCephFile(path, env, file);
  return file;
}

static CephFileRef getCephFileRef(const char *path, XrdOucEnv *env, int flags,
                                  mode_t mode, unsigned long long offset) {
  CephFileRef fr;
  fillCephFile(path, env, fr);
  fr.flags = flags;
  fr.mode = mode;
  fr.offset = 0;
  return fr;
}

static libradosstriper::RadosStriper* getRadosStriper(const CephFile& file) {
  std::string userAtPool = file.userId + '@' + file.pool;
  std::map<std::string, libradosstriper::RadosStriper*>::iterator it =
    g_radosStripers.find(userAtPool);
  if (it == g_radosStripers.end()) {
    // we need to create a new radosStriper
    // Do we already have a cluster
    if (0 == g_cluster) {
      // create connection to cluster
      g_cluster = new librados::Rados;
      if (0 == g_cluster) {
        return 0;
      }
      int rc = g_cluster->init(file.userId.c_str());
      if (rc) {
        delete g_cluster;
        g_cluster = 0;
        return 0;
      }
      rc = g_cluster->conf_read_file(NULL);
      if (rc) {
        g_cluster->shutdown();
        delete g_cluster;
        g_cluster = 0;
        return 0;
      }
      g_cluster->conf_parse_env(NULL);
      rc = g_cluster->connect();
      if (rc) {
        g_cluster->shutdown();
        delete g_cluster;
        g_cluster = 0;
        return 0;
      }
    }
    // create IoCtx for our pool
    librados::IoCtx *ioctx = new librados::IoCtx;
    if (0 == ioctx) {
      g_cluster->shutdown();
      delete g_cluster;
      return 0;
    }
    int rc = g_cluster->ioctx_create(file.pool.c_str(), *ioctx);
    if (rc != 0) {
      g_cluster->shutdown();
      delete g_cluster;
      delete ioctx;
      return 0;
    }
    // create RadosStriper connection
    libradosstriper::RadosStriper *striper = new libradosstriper::RadosStriper;
    if (0 == striper) {
      g_cluster->shutdown();
      delete g_cluster;
      delete ioctx;
      return 0;
    }
    rc = libradosstriper::RadosStriper::striper_create(*ioctx, striper);
    if (rc != 0) {
      g_cluster->shutdown();
      delete g_cluster;
      delete ioctx;
      delete striper;
      return 0;
    }
    g_ioCtx.insert(std::pair<std::string, librados::IoCtx*>(userAtPool, ioctx));    
    it = g_radosStripers.insert(std::pair<std::string, libradosstriper::RadosStriper*>
                                (userAtPool, striper)).first;
  }
  return it->second;
}

static librados::IoCtx* getIoCtx(const CephFile& file) {
  libradosstriper::RadosStriper *striper = getRadosStriper(file);
  if (0 == striper) {
    return 0;
  }
  return g_ioCtx[file.pool];
}

void ceph_posix_disconnect_all() {
  for (std::map<std::string, libradosstriper::RadosStriper*>::iterator it =
         g_radosStripers.begin();
       it != g_radosStripers.end();
       it++) {
    delete it->second;
  }
  g_radosStripers.clear();
  for (std::map<std::string, librados::IoCtx*>::iterator it = g_ioCtx.begin();
       it != g_ioCtx.end();
       it++) {
    delete it->second;
  }
  g_ioCtx.clear();
  delete g_cluster;
}

void ceph_posix_set_logfunc(void (*logfunc) (char *, va_list argp)) {
  g_logfunc = logfunc;
};

int ceph_posix_open(XrdOucEnv* env, const char *pathname, int flags, mode_t mode) {
  logwrapper((char*)"ceph_open : fd %d associated to %s\n", g_nextCephFd, pathname);
  CephFileRef fr = getCephFileRef(pathname, env, flags, mode, 0);
  g_fds[g_nextCephFd] = fr;
  g_nextCephFd++;
  if (flags & O_RDWR) {
    g_filesOpenForWrite.insert(pathname);
  }
  return g_nextCephFd-1;
}

int ceph_posix_close(int fd) {
  std::map<unsigned int, CephFileRef>::iterator it = g_fds.find(fd);
  if (it != g_fds.end()) {
    logwrapper((char*)"ceph_close: closed fd %d\n", fd);
    if (it->second.flags & O_RDWR) {
      g_filesOpenForWrite.erase(g_filesOpenForWrite.find(it->second.name));
    }
    g_fds.erase(it);
    return 0;
  } else {
    return -EBADF;
  }
}

static off64_t lseek_compute_offset(CephFileRef &fr, off64_t offset, int whence) {
  switch (whence) {
  case SEEK_SET:
    fr.offset = offset;
    break;
  case SEEK_CUR:
    fr.offset += offset;
    break;
  default:
    return -EINVAL;
  }
  return 0;
}

off_t ceph_posix_lseek(int fd, off_t offset, int whence) {
  std::map<unsigned int, CephFileRef>::iterator it = g_fds.find(fd);
  if (it != g_fds.end()) {
    CephFileRef &fr = it->second;
    logwrapper((char*)"ceph_lseek: for fd %d, offset=%d, whence=%d\n", fd, offset, whence);
    return (off_t)lseek_compute_offset(fr, offset, whence);
  } else {
    return -EBADF;
  }
}

off64_t ceph_posix_lseek64(int fd, off64_t offset, int whence) {
  std::map<unsigned int, CephFileRef>::iterator it = g_fds.find(fd);
  if (it != g_fds.end()) {
    CephFileRef &fr = it->second;
    logwrapper((char*)"ceph_lseek64: for fd %d, offset=%d, whence=%d\n", fd, offset, whence);
    return lseek_compute_offset(fr, offset, whence);
  } else {
    return -EBADF;
  }
}

ssize_t ceph_posix_write(int fd, const void *buf, size_t count) {
  std::map<unsigned int, CephFileRef>::iterator it = g_fds.find(fd);
  if (it != g_fds.end()) {
    CephFileRef &fr = it->second;
    logwrapper((char*)"ceph_write: for fd %d, count=%d\n", fd, count);
    if ((fr.flags & O_RDWR) == 0) {
      return -EBADF;
    }
    libradosstriper::RadosStriper *striper = getRadosStriper(fr);
    if (0 == striper) {
      return -EINVAL;
    }
    ceph::bufferlist bl;
    bl.append((const char*)buf, count);
    int rc = striper->write(fr.name, bl, count, fr.offset);
    if (rc) return rc;
    fr.offset += count;
    return count;
  } else {
    return -EBADF;
  }
}

ssize_t ceph_posix_read(int fd, void *buf, size_t count) {
  std::map<unsigned int, CephFileRef>::iterator it = g_fds.find(fd);
  if (it != g_fds.end()) {
    CephFileRef &fr = it->second;
    logwrapper((char*)"ceph_read: for fd %d, count=%d\n", fd, count);
    if ((fr.flags & O_RDWR) != 0) {
      return -EBADF;
    }
    libradosstriper::RadosStriper *striper = getRadosStriper(fr);
    if (0 == striper) {
      return -EINVAL;
    }
    ceph::bufferlist bl;
    int rc = striper->read(fr.name, &bl, count, fr.offset);
    if (rc < 0) return rc;
    bl.copy(0, rc, (char*)buf);
    fr.offset += rc;
    return rc;
  } else {
    return -EBADF;
  }
}

int ceph_posix_fstat(int fd, struct stat *buf) {
  std::map<unsigned int, CephFileRef>::iterator it = g_fds.find(fd);
  if (it != g_fds.end()) {
    CephFileRef &fr = it->second;
    logwrapper((char*)"ceph_stat: fd %d\n", fd);
    // minimal stat : only size and times are filled
    // atime, mtime and ctime are set all to the same value
    // mode is set arbitrarily to 0666
    libradosstriper::RadosStriper *striper = getRadosStriper(fr);
    if (0 == striper) {
      return -EINVAL;
    }
    memset(buf, 0, sizeof(*buf));
    int rc = striper->stat(fr.name, (uint64_t*)&(buf->st_size), &(buf->st_atime));
    if (rc != 0) {
      return -rc;
    }
    buf->st_mtime = buf->st_atime;
    buf->st_ctime = buf->st_atime;
    buf->st_mode = 0666;
    return 0;
  } else {
    return -EBADF;
  }
}

int ceph_posix_stat(XrdOucEnv* env, const char *pathname, struct stat *buf) {
  logwrapper((char*)"ceph_stat : %s\n", pathname);
  // minimal stat : only size and times are filled
  // atime, mtime and ctime are set all to the same value
  // mode is set arbitrarily to 0666
  libradosstriper::RadosStriper *striper = getRadosStriper(getCephFile(pathname, env));
  if (0 == striper) {
    return -EINVAL;
  }
  memset(buf, 0, sizeof(*buf));
  int rc = striper->stat(pathname, (uint64_t*)&(buf->st_size), &(buf->st_atime));
  if (rc != 0) {
    // for non exiting file. Check that we did not open it for write recently
    // in that case, we return 0 size and current time
    if (-ENOENT == rc && g_filesOpenForWrite.find(pathname) != g_filesOpenForWrite.end()) {
      buf->st_size = 0;
      buf->st_atime = time(NULL);
    } else {
      return -rc;
    }
  }
  buf->st_mtime = buf->st_atime;
  buf->st_ctime = buf->st_atime;  
  buf->st_mode = 0666;
  return 0;
}

int ceph_posix_fstat64(int fd, struct stat64 *buf) {
  std::map<unsigned int, CephFileRef>::iterator it = g_fds.find(fd);
  if (it != g_fds.end()) {
    CephFileRef &fr = it->second;
      logwrapper((char*)"ceph_stat64: fd %d\n", fd);
      // minimal stat : only size and times are filled
      libradosstriper::RadosStriper *striper = getRadosStriper(fr);
      if (0 == striper) {
        return -EINVAL;
      }
      memset(buf, 0, sizeof(*buf));
      int rc = striper->stat(fr.name, (uint64_t*)&(buf->st_size), &(buf->st_atime));
      if (rc != 0) {
        return -rc;
      }
      buf->st_mtime = buf->st_atime;
      buf->st_ctime = buf->st_atime;  
      return 0;
  } else {
    return -EBADF;
  }
}

int ceph_posix_stat64(XrdOucEnv* env, const char *pathname, struct stat64 *buf) {
  logwrapper((char*)"ceph_stat : %s\n", pathname);
  // minimal stat : only size and times are filled
  libradosstriper::RadosStriper *striper = getRadosStriper(getCephFile(pathname, env));
  if (0 == striper) {
    return -EINVAL;
  }
  memset(buf, 0, sizeof(*buf));
  int rc = striper->stat(pathname, (uint64_t*)&(buf->st_size), &(buf->st_atime));
  if (rc != 0) {
    return -rc;
  }
  buf->st_mtime = buf->st_atime;
  buf->st_ctime = buf->st_atime;  
  return 0;
}

int ceph_posix_fsync(int fd) {
  std::map<unsigned int, CephFileRef>::iterator it = g_fds.find(fd);
  if (it != g_fds.end()) {
    logwrapper((char*)"ceph_sync: fd %d\n", fd);
    return 0;
  } else {
    return -EBADF;
  }
}

int ceph_posix_fcntl(int fd, int cmd, ... /* arg */ ) {
  std::map<unsigned int, CephFileRef>::iterator it = g_fds.find(fd);
  if (it != g_fds.end()) {
    CephFileRef &fr = it->second;
    logwrapper((char*)"ceph_fcntl: fd %d cmd=%d\n", fd, cmd);
    // minimal implementation
    switch (cmd) {
    case F_GETFL:
      return fr.mode;
    default:
      return -EINVAL;
    }
  } else {
    return -EBADF;
  }
}

static ssize_t ceph_posix_internal_getxattr(const CephFile &file, const char* name,
                                            void* value, size_t size) {
  libradosstriper::RadosStriper *striper = getRadosStriper(file);
  if (0 == striper) {
    return -EINVAL;
  }
  ceph::bufferlist bl;
  int rc = striper->getxattr(file.name, name, bl);
  if (rc) {
    return -rc;
  }
  bl.copy(0, size, (char*)value);
  return 0;
}  

ssize_t ceph_posix_getxattr(XrdOucEnv* env, const char* path,
                            const char* name, void* value,
                            size_t size) {
  logwrapper((char*)"ceph_getxattr: path %s name=%s\n", path, name);
  return ceph_posix_internal_getxattr(getCephFile(path, env), name, value, size);
}

ssize_t ceph_posix_fgetxattr(int fd, const char* name,
                             void* value, size_t size) {
  std::map<unsigned int, CephFileRef>::iterator it = g_fds.find(fd);
  if (it != g_fds.end()) {
    CephFileRef &fr = it->second;
    logwrapper((char*)"ceph_fgetxattr: fd %d name=%s\n", fd, name);
    return ceph_posix_internal_getxattr(fr, name, value, size);
  } else {
    return -EBADF;
  }
}

static ssize_t ceph_posix_internal_setxattr(const CephFile &file, const char* name,
                                            const void* value, size_t size, int flags) {
  libradosstriper::RadosStriper *striper = getRadosStriper(file);
  if (0 == striper) {
    return -EINVAL;
  }
  ceph::bufferlist bl;
  bl.append((const char*)value, size);
  int rc = striper->setxattr(file.name, name, bl);
  if (rc) {
    return -rc;
  }
  return 0;
}

ssize_t ceph_posix_setxattr(XrdOucEnv* env, const char* path,
                            const char* name, const void* value,
                            size_t size, int flags) {
  logwrapper((char*)"ceph_setxattr: path %s name=%s value=%s\n", path, name, value);
  return ceph_posix_internal_setxattr(getCephFile(path, env), name, value, size, flags);
}

int ceph_posix_fsetxattr(int fd,
                         const char* name, const void* value,
                         size_t size, int flags)  {
  std::map<unsigned int, CephFileRef>::iterator it = g_fds.find(fd);
  if (it != g_fds.end()) {
    CephFileRef &fr = it->second;
    logwrapper((char*)"ceph_fsetxattr: fd %d name=%s value=%s\n", fd, name, value);
    return ceph_posix_internal_setxattr(fr, name, value, size, flags);
  } else {
    return -EBADF;
  }
}

static int ceph_posix_internal_removexattr(const CephFile &file, const char* name) {
  libradosstriper::RadosStriper *striper = getRadosStriper(file);
  if (0 == striper) {
    return -EINVAL;
  }
  int rc = striper->rmxattr(file.name, name);
  if (rc) {
    return -rc;
  }
  return 0;
}

int ceph_posix_removexattr(XrdOucEnv* env, const char* path,
                           const char* name) {
  logwrapper((char*)"ceph_removexattr: path %s name=%s\n", path, name);
  return ceph_posix_internal_removexattr(getCephFile(path, env), name);
}

int ceph_posix_fremovexattr(int fd, const char* name) {
  std::map<unsigned int, CephFileRef>::iterator it = g_fds.find(fd);
  if (it != g_fds.end()) {
    CephFileRef &fr = it->second;
    logwrapper((char*)"ceph_fremovexattr: fd %d name=%s\n", fd, name);
    return ceph_posix_internal_removexattr(fr, name);
  } else {
    return -EBADF;
  }
}

static int ceph_posix_internal_listxattrs(const CephFile &file, XrdSysXAttr::AList **aPL, int getSz) {
  libradosstriper::RadosStriper *striper = getRadosStriper(file);
  if (0 == striper) {
    return -EINVAL;
  }
  // call ceph
  std::map<std::string, ceph::bufferlist> attrset;
  int rc = striper->getxattrs(file.name, attrset);
  if (rc) {
    return -rc;
  }
  // build result
  *aPL = 0;
  int maxSize = 0;
  for (std::map<std::string, ceph::bufferlist>::const_iterator it = attrset.begin();
       it != attrset.end();
       it++) {
    XrdSysXAttr::AList* newItem = (XrdSysXAttr::AList*)malloc(sizeof(XrdSysXAttr::AList)+it->first.size());
    newItem->Next = *aPL;
    newItem->Vlen = it->second.length();
    if (newItem->Vlen > maxSize) {
      maxSize = newItem->Vlen;
    }
    newItem->Nlen = it->first.size();
    strncpy(newItem->Name, it->first.c_str(), newItem->Vlen+1);
    *aPL = newItem;
  }
  if (getSz) {
    return 0;
  } else {
    return maxSize;
  }
}

int ceph_posix_listxattrs(XrdOucEnv* env, const char* path, XrdSysXAttr::AList **aPL, int getSz) {
  logwrapper((char*)"ceph_listxattrs: path %s\n", path);
  return ceph_posix_internal_listxattrs(getCephFile(path, env), aPL, getSz);
}

int ceph_posix_flistxattrs(int fd, XrdSysXAttr::AList **aPL, int getSz) {
  std::map<unsigned int, CephFileRef>::iterator it = g_fds.find(fd);
  if (it != g_fds.end()) {
    CephFileRef &fr = it->second;
    logwrapper((char*)"ceph_flistxattrs: fd %d\n", fd);
    return ceph_posix_internal_listxattrs(fr, aPL, getSz);
  } else {
    return -EBADF;
  }
}

void ceph_posix_freexattrlist(XrdSysXAttr::AList *aPL) {
  while (aPL) {
    free(aPL->Name);
    XrdSysXAttr::AList *cur = aPL;
    aPL = aPL->Next;
    free(cur);
  }
}

int ceph_posix_statfs(long long *totalSpace, long long *freeSpace) {
  logwrapper((char*)"ceph_posix_statfs\n");
  librados::cluster_stat_t result;
  int rc = g_cluster->cluster_stat(result);
  if (0 == rc) {
    *totalSpace = result.kb * 1024;
    *freeSpace = result.kb_avail * 1024;
  }
  return rc;
}

int ceph_posix_truncate(XrdOucEnv* env, const char *pathname, unsigned long long size) {
  logwrapper((char*)"ceph_posix_truncate : %s\n", pathname);
  // minimal stat : only size and times are filled
  CephFile file = getCephFile(pathname, env);  
  libradosstriper::RadosStriper *striper = getRadosStriper(file);
  if (0 == striper) {
    return -EINVAL;
  }
  return striper->trunc(file.name, size);
}

int ceph_posix_unlink(XrdOucEnv* env, const char *pathname) {
  logwrapper((char*)"ceph_posix_unlink : %s\n", pathname);
  // minimal stat : only size and times are filled
  CephFile file = getCephFile(pathname, env);
  libradosstriper::RadosStriper *striper = getRadosStriper(file);
  if (0 == striper) {
    return -EINVAL;
  }
  return striper->remove(file.name);
}

DIR* ceph_posix_opendir(XrdOucEnv* env, const char *pathname) {
  logwrapper((char*)"ceph_posix_opendir : %s\n", pathname);
  // only accept root dir, as there is no concept of dirs in object stores
  CephFile file = getCephFile(pathname, env);
  if (file.name.size() != 1 || file.name[0] != '/') {
    errno = -ENOENT;
    return 0;
  }
  librados::IoCtx *ioctx = getIoCtx(file);
  if (0 == ioctx) {
    errno = EINVAL;
    return 0;
  }
  DirIterator* res = new DirIterator();
  res->m_iterator = ioctx->objects_begin();
  res->m_ioctx = ioctx;
  return (DIR*)res;
}

int ceph_posix_readdir(DIR *dirp, char *buff, int blen) {
  librados::ObjectIterator &iterator = ((DirIterator*)dirp)->m_iterator;
  librados::IoCtx *ioctx = ((DirIterator*)dirp)->m_ioctx;
  while (iterator->first.compare(iterator->first.size()-17, 17, ".0000000000000000") &&
         iterator != ioctx->objects_end()) {
    iterator++;
  }
  if (iterator == ioctx->objects_end()) {
    buff[0] = 0;
  } else {
    int l = iterator->first.size()-17;
    if (l < blen) blen = l;
    strncpy(buff, iterator->first.c_str(), blen-1);
    buff[blen-1] = 0;
    iterator++;
  }
  return 0;
}

int ceph_posix_closedir(DIR *dirp) {
  delete ((DirIterator*)dirp);
  return 0;
}
