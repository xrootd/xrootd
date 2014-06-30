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
struct CephFileRef {
  std::string name;
  std::string pool;
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

/// global variable for the log function
static void (*g_logfunc) (char *, va_list argp) = 0;

static void logwrapper(char* format, ...) {
  if (0 == g_logfunc) return;
  va_list arg;
  va_start(arg, format);
  (*g_logfunc)(format, arg);
  va_end(arg);  
}

static libradosstriper::RadosStriper* getRadosStriper(std::string pool) {
  std::map<std::string, libradosstriper::RadosStriper*>::iterator it =
    g_radosStripers.find(pool);
  if (it == g_radosStripers.end()) {
    // we need to create a new radosStriper
    // Do we already have a cluster
    if (0 == g_cluster) {    
      // create connection to cluster
      g_cluster = new librados::Rados;
      if (0 == g_cluster) {
        return 0;
      }
      int rc = g_cluster->init(0);
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
    int rc = g_cluster->ioctx_create(pool.c_str(), *ioctx);
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
    g_ioCtx.insert(std::pair<std::string, librados::IoCtx*>(pool, ioctx));    
    it = g_radosStripers.insert(std::pair<std::string, libradosstriper::RadosStriper*>
                                (pool, striper)).first;
  }
  return it->second;
}

static librados::IoCtx* getIoCtx(std::string pool) {
  libradosstriper::RadosStriper *striper = getRadosStriper(pool);
  if (0 == striper) {
    return 0;
  }
  return g_ioCtx[pool];
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

int ceph_posix_open(const char *pool, const char *pathname, int flags, mode_t mode) {
  CephFileRef fr;
  logwrapper((char*)"ceph_open in pool %s: fd %d associated to %s\n", pool, g_nextCephFd, pathname);
  fr.pool = pool;
  fr.name = pathname;
  fr.flags = flags;
  fr.mode = mode;
  fr.offset = 0;
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
    libradosstriper::RadosStriper *striper = getRadosStriper(fr.pool);
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
    libradosstriper::RadosStriper *striper = getRadosStriper(fr.pool);
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
    libradosstriper::RadosStriper *striper = getRadosStriper(fr.pool);
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

int ceph_posix_stat(const char* pool, const char *pathname, struct stat *buf) {
  logwrapper((char*)"ceph_stat in pool %s: %s\n", pool, pathname);
  // minimal stat : only size and times are filled
  libradosstriper::RadosStriper *striper = getRadosStriper(pool);
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
  return 0;
}

int ceph_posix_fstat64(int fd, struct stat64 *buf) {
  std::map<unsigned int, CephFileRef>::iterator it = g_fds.find(fd);
  if (it != g_fds.end()) {
    CephFileRef &fr = it->second;
      logwrapper((char*)"ceph_stat64: fd %d\n", fd);
      // minimal stat : only size and times are filled
      libradosstriper::RadosStriper *striper = getRadosStriper(fr.pool);
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

int ceph_posix_stat64(const char* pool, const char *pathname, struct stat64 *buf) {
  logwrapper((char*)"ceph_stat in pool %s: %s\n", pool, pathname);
  // minimal stat : only size and times are filled
  libradosstriper::RadosStriper *striper = getRadosStriper(pool);
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

ssize_t ceph_posix_fgetxattr(int fd, const char* name, void* value, size_t size) {
  std::map<unsigned int, CephFileRef>::iterator it = g_fds.find(fd);
  if (it != g_fds.end()) {
    CephFileRef &fr = it->second;
    logwrapper((char*)"ceph_fgetxattr: fd %d name=%s\n", fd, name);
    libradosstriper::RadosStriper *striper = getRadosStriper(fr.pool);
    if (0 == striper) {
      return -EINVAL;
    }
    ceph::bufferlist bl;
    int rc = striper->getxattr(fr.name, name, bl);
    if (rc) {
      return -rc;
    }
    bl.copy(0, size, (char*)value);
    return 0;
  } else {
    return -EBADF;
  }
}

int ceph_posix_fsetxattr(int fd, const char* name, const void* value,
                         size_t size, int flags)  {
  std::map<unsigned int, CephFileRef>::iterator it = g_fds.find(fd);
  if (it != g_fds.end()) {
    CephFileRef &fr = it->second;
    logwrapper((char*)"ceph_fsetxattr: fd %d name=%s value=%s\n", fd, name, value);
    libradosstriper::RadosStriper *striper = getRadosStriper(fr.pool);
    if (0 == striper) {
      return -EINVAL;
    }
    ceph::bufferlist bl;
    bl.append((const char*)value, size);
    int rc = striper->setxattr(fr.name, name, bl);
    if (rc) {
      return -rc;
    }
    return 0;
  } else {
    return -EBADF;
  }
}

int ceph_posix_fremovexattr(int fd, const char* name) {
  std::map<unsigned int, CephFileRef>::iterator it = g_fds.find(fd);
  if (it != g_fds.end()) {
    CephFileRef &fr = it->second;
    logwrapper((char*)"ceph_removexattr: fd %d name=%s\n", fd, name);
    libradosstriper::RadosStriper *striper = getRadosStriper(fr.pool);
    if (0 == striper) {
      return -EINVAL;
    }
    int rc = striper->rmxattr(fr.name, name);
    if (rc) {
      return -rc;
    }
    return 0;
  } else {
    return -EBADF;
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

int ceph_posix_truncate(const char* pool, const char *pathname, unsigned long long size) {
  logwrapper((char*)"ceph_posix_truncate in pool %s: %s\n", pool, pathname);
  // minimal stat : only size and times are filled
  libradosstriper::RadosStriper *striper = getRadosStriper(pool);
  if (0 == striper) {
    return -EINVAL;
  }
  return striper->trunc(pathname, size);
}

int ceph_posix_unlink(const char* pool, const char *pathname) {
  logwrapper((char*)"ceph_posix_unlink in pool %s: %s\n", pool, pathname);
  // minimal stat : only size and times are filled
  libradosstriper::RadosStriper *striper = getRadosStriper(pool);
  if (0 == striper) {
    return -EINVAL;
  }
  return striper->remove(pathname);
}

DIR* ceph_posix_opendir(const char* pool) {
  librados::IoCtx *ioctx = getIoCtx(pool);
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
