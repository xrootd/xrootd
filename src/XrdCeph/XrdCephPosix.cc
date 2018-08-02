//------------------------------------------------------------------------------
// Copyright (c) 2014-2015 by European Organization for Nuclear Research (CERN)
// Author: Sebastien Ponce <sebastien.ponce@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

/*
 * This interface provides wrapper methods for using ceph through a POSIX API.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <radosstriper/libradosstriper.hpp>
#include <map>
#include <stdexcept>
#include <string>
#include <sstream>
#if !defined(__FreeBSD__)
#include <sys/xattr.h>
#endif
#include <time.h>
#include <limits>
#include <pthread.h>
#include "XrdSfs/XrdSfsAio.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdOuc/XrdOucName2Name.hh"
#include "XrdSys/XrdSysPlatform.hh"

#include "XrdCeph/XrdCephPosix.hh"

/// small structs to store file metadata
struct CephFile {
  std::string name;
  std::string pool;
  std::string userId;
  unsigned int nbStripes;
  unsigned long long stripeUnit;
  unsigned long long objectSize;
};

struct CephFileRef : CephFile {
  int flags;
  mode_t mode;
  unsigned long long offset;
  unsigned rdcount;
  unsigned wrcount;
};

/// small struct for directory listing
struct DirIterator {
  librados::NObjectIterator m_iterator;
  librados::IoCtx *m_ioctx;
};

/// small struct for aio API callbacks
struct AioArgs {
  AioArgs(XrdSfsAio* a, AioCB *b, size_t n, ceph::bufferlist *_bl=0) :
    aiop(a), callback(b), nbBytes(n), bl(_bl) {}
  XrdSfsAio* aiop;
  AioCB *callback;
  size_t nbBytes;
  ceph::bufferlist *bl;
};

/// global variables holding stripers/ioCtxs/cluster objects
/// Note that we have a pool of them to circumvent the limitation
/// of having a single objecter/messenger per IoCtx
typedef std::map<std::string, libradosstriper::RadosStriper*> StriperDict;
std::vector<StriperDict> g_radosStripers;
typedef std::map<std::string, librados::IoCtx*> IOCtxDict;
std::vector<IOCtxDict> g_ioCtx;
std::vector<librados::Rados*> g_cluster;
/// mutex protecting the striper and ioctx maps
XrdSysMutex g_striper_mutex;
/// index of current Striper/IoCtx to be used
unsigned int g_cephPoolIdx = 0;
/// size of the Striper/IoCtx pool, defaults to 1
/// may be overwritten in the configuration file
/// (See XrdCephOss::configure)
unsigned int g_maxCephPoolIdx = 1;
/// pointer to library providing Name2Name interface. 0 be default
/// populated in case of ceph.namelib entry in the config file in XrdCephOss
XrdOucName2Name *g_namelib = 0;

/// global variable holding a list of files currently opened for write
std::multiset<std::string> g_filesOpenForWrite;
/// global variable holding a map of file descriptor to file reference
std::map<unsigned int, CephFileRef> g_fds;
/// global variable remembering the next available file descriptor
unsigned int g_nextCephFd = 0;
/// mutex protecting the map of file descriptors and the openForWrite multiset
XrdSysMutex g_fd_mutex;
/// mutex protecting initialization of ceph clusters
XrdSysMutex g_init_mutex;

/// Accessor to next ceph pool index
/// Note that this is not thread safe, but we do not care
/// as we only want a rough load balancing
unsigned int getCephPoolIdxAndIncrease() {
  if (g_radosStripers.size() == 0) {
    // make sure we do not have a race condition here
    XrdSysMutexHelper lock(g_init_mutex);
    // double check now that we have the lock
    if (g_radosStripers.size() == 0) {
      // initialization phase : allocate corresponding places in the vectors
      for (unsigned int i = 0; i < g_maxCephPoolIdx; i++) {
        g_radosStripers.push_back(StriperDict());
        g_ioCtx.push_back(IOCtxDict());
        g_cluster.push_back(0);
      }
    }
  }
  unsigned int res = g_cephPoolIdx;
  unsigned nextValue = g_cephPoolIdx+1;
  if (nextValue >= g_maxCephPoolIdx) {
    nextValue = 0;
  }
  g_cephPoolIdx = nextValue;
  return res;
}

/// check whether a file is open for write
bool isOpenForWrite(std::string& name) {
  XrdSysMutexHelper lock(g_fd_mutex);
  return g_filesOpenForWrite.find(name) != g_filesOpenForWrite.end();
}

/// look for a FileRef from its file descriptor
CephFileRef* getFileRef(int fd) {
  XrdSysMutexHelper lock(g_fd_mutex);
  std::map<unsigned int, CephFileRef>::iterator it = g_fds.find(fd);
  if (it != g_fds.end()) {
    return &(it->second);
  } else {
    return 0;
  }
}

/// deletes a FileRef from the global table of file descriptors
void deleteFileRef(int fd, const CephFileRef &fr) {
  XrdSysMutexHelper lock(g_fd_mutex);
  if (fr.flags & (O_WRONLY|O_RDWR)) {
    g_filesOpenForWrite.erase(g_filesOpenForWrite.find(fr.name));
  }
  std::map<unsigned int, CephFileRef>::iterator it = g_fds.find(fd);
  if (it != g_fds.end()) {
    g_fds.erase(it);
  }
}

/**
 * inserts a new FileRef into the global table of file descriptors
 * and return the associated file descriptor
 */
int insertFileRef(CephFileRef &fr) {
  XrdSysMutexHelper lock(g_fd_mutex);
  g_fds[g_nextCephFd] = fr;
  g_nextCephFd++;
  if (fr.flags & (O_WRONLY|O_RDWR)) {
    g_filesOpenForWrite.insert(fr.name);
  }
  return g_nextCephFd-1;
}

/// global variable containing defaults for CephFiles
CephFile g_defaultParams = { "",
                             "default",        // default pool
                             "admin",          // default user
                             1,                // default nbStripes
                             4 * 1024 * 1024,  // default stripeUnit : 4 MB
                             4 * 1024 * 1024}; // default objectSize : 4 MB

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

/// simple integer parsing, to be replaced by std::stoll when C++11 can be used
static unsigned long long int stoull(const std::string &s) {
  char* end;
  errno = 0;
  unsigned long long int res = strtoull(s.c_str(), &end, 10);
  if (0 != *end) {
    throw std::invalid_argument(s);
  }
  if (ERANGE == errno) {
    throw std::out_of_range(s);
  }
  return res;
}

/// simple integer parsing, to be replaced by std::stoi when C++11 can be used
static unsigned int stoui(const std::string &s) {
  char* end;
  errno = 0;
  unsigned long int res = strtoul(s.c_str(), &end, 10);
  if (0 != *end) {
    throw std::invalid_argument(s);
  }
  if (ERANGE == errno || res > std::numeric_limits<unsigned int>::max()) {
    throw std::out_of_range(s);
  }
  return (unsigned int)res;
}

/// fills the userId of a ceph file struct from a string and an environment
/// returns position of first character after the userId
static int fillCephUserId(const std::string &params, XrdOucEnv *env, CephFile &file) {
  // default
  file.userId = g_defaultParams.userId;
  // parsing
  size_t atPos = params.find('@');
  if (std::string::npos != atPos) {
    file.userId = params.substr(0, atPos);
    return atPos+1;
  } else {
    if (0 != env) {
      char* cuser = env->Get("cephUserId");
      if (0 != cuser) {
        file.userId = cuser;
      }
    }
    return 0;
  }
}

/// fills the pool of a ceph file struct from a string and an environment
/// returns position of first character after the pool
static int fillCephPool(const std::string &params, unsigned int offset, XrdOucEnv *env, CephFile &file) {
  // default
  file.pool = g_defaultParams.pool;
  // parsing
  size_t comPos = params.find(',', offset);
  if (std::string::npos == comPos) {
    if (params.size() == offset) {
      if (NULL != env) {
        char* cpool = env->Get("cephPool");
        if (0 != cpool) {
          file.pool = cpool;
        }
      }
    } else {
      file.pool = params.substr(offset);
    }
    return params.size();
  } else {
    file.pool = params.substr(offset, comPos-offset);
    return comPos+1;
  }
}

/// fills the nbStriped of a ceph file struct from a string and an environment
/// returns position of first character after the nbStripes
// this may raise std::invalid_argument and std::out_of_range
static int fillCephNbStripes(const std::string &params, unsigned int offset, XrdOucEnv *env, CephFile &file) {
  // default
  file.nbStripes = g_defaultParams.nbStripes;
  // parsing
  size_t comPos = params.find(',', offset);
  if (std::string::npos == comPos) {
    if (params.size() == offset) {
      if (NULL != env) {
        char* cNbStripes = env->Get("cephNbStripes");
        if (0 != cNbStripes) {
          file.nbStripes = stoui(cNbStripes);
        }
      }
    } else {
      file.nbStripes = stoui(params.substr(offset));
    }
    return params.size();
  } else {
    file.nbStripes = stoui(params.substr(offset, comPos-offset));
    return comPos+1;
  }
}

/// fills the stripeUnit of a ceph file struct from a string and an environment
/// returns position of first character after the stripeUnit
// this may raise std::invalid_argument and std::out_of_range
static int fillCephStripeUnit(const std::string &params, unsigned int offset, XrdOucEnv *env, CephFile &file) {
  // default
  file.stripeUnit = g_defaultParams.stripeUnit;
  // parsing
  size_t comPos = params.find(',', offset);
  if (std::string::npos == comPos) {
    if (params.size() == offset) {
      if (NULL != env) {
        char* cStripeUnit = env->Get("cephStripeUnit");
        if (0 != cStripeUnit) {
          file.stripeUnit = ::stoull(cStripeUnit);
        }
      }
    } else {
      file.stripeUnit = ::stoull(params.substr(offset));
    }
    return params.size();
  } else {
    file.stripeUnit = ::stoull(params.substr(offset, comPos-offset));
    return comPos+1;
  }
}

/// fills the objectSize of a ceph file struct from a string and an environment
/// returns position of first character after the objectSize
// this may raise std::invalid_argument and std::out_of_range
static void fillCephObjectSize(const std::string &params, unsigned int offset, XrdOucEnv *env, CephFile &file) {
  // default
  file.objectSize = g_defaultParams.objectSize;
  // parsing
  if (params.size() == offset) {
    if (NULL != env) {
      char* cObjectSize = env->Get("cephObjectSize");
      if (0 != cObjectSize) {
        file.objectSize = ::stoull(cObjectSize);
      }
    }
  } else {
    file.objectSize = ::stoull(params.substr(offset));
  }
}

/// fill the parameters of a ceph file struct (all but name) from a string and an environment
/// see fillCephFile for the detailed syntax
void fillCephFileParams(const std::string &params, XrdOucEnv *env, CephFile &file) {
  // parse the params one by one
  unsigned int afterUser = fillCephUserId(params, env, file);
  unsigned int afterPool = fillCephPool(params, afterUser, env, file);
  unsigned int afterNbStripes = fillCephNbStripes(params, afterPool, env, file);
  unsigned int afterStripeUnit = fillCephStripeUnit(params, afterNbStripes, env, file);
  fillCephObjectSize(params, afterStripeUnit, env, file);
}

/// sets the default userId, pool and file layout
/// syntax is [user@]pool[,nbStripes[,stripeUnit[,objectSize]]]
/// may throw std::invalid_argument or std::out_of_range in case of error
void ceph_posix_set_defaults(const char* value) {
  if (value) {
    CephFile newdefault;
    fillCephFileParams(value, NULL, newdefault);
    g_defaultParams = newdefault;
  }
}

/// converts a logical filename to physical one if needed
void translateFileName(std::string &physName, std::string logName){
  if (0 != g_namelib) {
    char physCName[MAXPATHLEN+1];
    int retc = g_namelib->lfn2pfn(logName.c_str(), physCName, sizeof(physCName));
    if (retc) {
      logwrapper((char*)"ceph_namelib : failed to translate %s using namelib plugin, using it as is", logName.c_str());
      physName = logName;
    } else {
      physName = physCName;
    }
  } else {
    physName = logName;
  }
}

/// fill a ceph file struct from a path and an environment
void fillCephFile(const char *path, XrdOucEnv *env, CephFile &file) {
  // Syntax of the given path is :
  //   [[userId@]pool[,nbStripes[,stripeUnit[,objectSize]]]:]<actual path>
  // for the missing parts, if env is not null the entries
  // cephUserId, cephPool, cephNbStripes, cephStripeUnit, and cephObjectSize
  // of env will be used.
  // If env is null or no entry is found for what is missing, defaults are
  // applied. These defaults are initially set to 'admin', 'default', 1, 4MB and 4MB
  // but can be changed via a call to ceph_posix_set_defaults
  std::string spath = path;
  size_t colonPos = spath.find(':');
  if (std::string::npos == colonPos) {
    // deal with name translation
    translateFileName(file.name, spath);
    fillCephFileParams("", env, file);
  } else {
    translateFileName(file.name, spath.substr(colonPos+1));
    fillCephFileParams(spath.substr(0, colonPos), env, file);
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
  fr.rdcount = 0;
  fr.wrcount = 0;
  return fr;
}

inline librados::Rados* checkAndCreateCluster(unsigned int cephPoolIdx,
                                              std::string userId = g_defaultParams.userId) {
  if (0 == g_cluster[cephPoolIdx]) {
    // create connection to cluster
    librados::Rados *cluster = new librados::Rados;
    if (0 == cluster) {
      return 0;
    }
    int rc = cluster->init(userId.c_str());
    if (rc) {
      logwrapper((char*)"checkAndCreateCluster : cluster init failed");
      delete cluster;
      return 0;
    }
    rc = cluster->conf_read_file(NULL);
    if (rc) {
      logwrapper((char*)"checkAndCreateCluster : cluster read config failed, rc = %d", rc);
      cluster->shutdown();
      delete cluster;
      return 0;
    }
    cluster->conf_parse_env(NULL);
    rc = cluster->connect();
    if (rc) {
      logwrapper((char*)"checkAndCreateCluster : cluster connect failed, rc = %d", rc);
      cluster->shutdown();
      delete cluster;
      return 0;
    }
    g_cluster[cephPoolIdx] = cluster;
  }
  return g_cluster[cephPoolIdx];
}

int checkAndCreateStriper(unsigned int cephPoolIdx, std::string &userAtPool, const CephFile& file) {
  StriperDict &sDict = g_radosStripers[cephPoolIdx];
  StriperDict::iterator it = sDict.find(userAtPool);
  if (it == sDict.end()) {
    // we need to create a new radosStriper
    // Get a cluster
    librados::Rados* cluster = checkAndCreateCluster(cephPoolIdx, file.userId);
    if (0 == cluster) {
      logwrapper((char*)"checkAndCreateStriper : checkAndCreateCluster failed");
      return 0;
    }
    // create IoCtx for our pool
    librados::IoCtx *ioctx = new librados::IoCtx;
    if (0 == ioctx) {
      logwrapper((char*)"checkAndCreateStriper : IoCtx instantiation failed");
      cluster->shutdown();
      delete cluster;
      g_cluster[cephPoolIdx] = 0;
      return 0;
    }
    int rc = g_cluster[cephPoolIdx]->ioctx_create(file.pool.c_str(), *ioctx);
    if (rc != 0) {
      logwrapper((char*)"checkAndCreateStriper : ioctx_create failed, rc = %d", rc);
      cluster->shutdown();
      delete cluster;
      g_cluster[cephPoolIdx] = 0;
      delete ioctx;
      return 0;
    }
    // create RadosStriper connection
    libradosstriper::RadosStriper *striper = new libradosstriper::RadosStriper;
    if (0 == striper) {
      logwrapper((char*)"checkAndCreateStriper : RadosStriper instantiation failed");
      delete ioctx;
      cluster->shutdown();
      delete cluster;
      g_cluster[cephPoolIdx] = 0;
      return 0;
    }
    rc = libradosstriper::RadosStriper::striper_create(*ioctx, striper);
    if (rc != 0) {
      logwrapper((char*)"checkAndCreateStriper : striper_create failed, rc = %d", rc);
      delete striper;
      delete ioctx;
      cluster->shutdown();
      delete cluster;
      g_cluster[cephPoolIdx] = 0;
      return 0;
    }
    // setup layout
    rc = striper->set_object_layout_stripe_count(file.nbStripes);
    if (rc != 0) {
      logwrapper((char*)"checkAndCreateStriper : invalid nbStripes %d", file.nbStripes);
      delete striper;
      delete ioctx;
      cluster->shutdown();
      delete cluster;
      g_cluster[cephPoolIdx] = 0;
      return 0;
    }
    rc = striper->set_object_layout_stripe_unit(file.stripeUnit);
    if (rc != 0) {
      logwrapper((char*)"checkAndCreateStriper : invalid stripeUnit %d (must be non 0, multiple of 64K)", file.stripeUnit);
      delete striper;
      delete ioctx;
      cluster->shutdown();
      delete cluster;
      g_cluster[cephPoolIdx] = 0;
      return 0;
    }
    rc = striper->set_object_layout_object_size(file.objectSize);
    if (rc != 0) {
      logwrapper((char*)"checkAndCreateStriper : invalid objectSize %d (must be non 0, multiple of stripe_unit)", file.objectSize);
      delete striper;
      delete ioctx;
      cluster->shutdown();
      delete cluster;
      g_cluster[cephPoolIdx] = 0;
      return 0;
    }
    IOCtxDict & ioDict = g_ioCtx[cephPoolIdx];
    ioDict.insert(std::pair<std::string, librados::IoCtx*>(userAtPool, ioctx));
    sDict.insert(std::pair<std::string, libradosstriper::RadosStriper*>
                 (userAtPool, striper)).first;
  }
  return 1;
} 

static libradosstriper::RadosStriper* getRadosStriper(const CephFile& file) {
  XrdSysMutexHelper lock(g_striper_mutex);
  std::stringstream ss;
  ss << file.userId << '@' << file.pool << ',' << file.nbStripes << ','
     << file.stripeUnit << ',' << file.objectSize;
  std::string userAtPool = ss.str();
  unsigned int cephPoolIdx = getCephPoolIdxAndIncrease();
  if (checkAndCreateStriper(cephPoolIdx, userAtPool, file) == 0) {
    logwrapper((char*)"getRadosStriper : checkAndCreateStriper failed");
    return 0;
  }
  return g_radosStripers[cephPoolIdx][userAtPool];
}

static librados::IoCtx* getIoCtx(const CephFile& file) {
  XrdSysMutexHelper lock(g_striper_mutex);
  std::stringstream ss;
  ss << file.userId << '@' << file.pool << ',' << file.nbStripes << ','
     << file.stripeUnit << ',' << file.objectSize;
  std::string userAtPool = ss.str();
  unsigned int cephPoolIdx = getCephPoolIdxAndIncrease();
  if (checkAndCreateStriper(cephPoolIdx, userAtPool, file) == 0) {
    return 0;
  }
  return g_ioCtx[cephPoolIdx][userAtPool];
}

void ceph_posix_disconnect_all() {
  XrdSysMutexHelper lock(g_striper_mutex);
  for (unsigned int i= 0; i < g_maxCephPoolIdx; i++) {
    for (StriperDict::iterator it2 = g_radosStripers[i].begin();
         it2 != g_radosStripers[i].end();
         it2++) {
      delete it2->second;
    }
    for (IOCtxDict::iterator it2 = g_ioCtx[i].begin();
         it2 != g_ioCtx[i].end();
         it2++) {
      delete it2->second;
    }
    delete g_cluster[i];
  }
  g_radosStripers.clear();
  g_ioCtx.clear();
  g_cluster.clear();
}

void ceph_posix_set_logfunc(void (*logfunc) (char *, va_list argp)) {
  g_logfunc = logfunc;
};

static int ceph_posix_internal_truncate(const CephFile &file, unsigned long long size);

int ceph_posix_open(XrdOucEnv* env, const char *pathname, int flags, mode_t mode) {
  CephFileRef fr = getCephFileRef(pathname, env, flags, mode, 0);
  int fd = insertFileRef(fr);
  logwrapper((char*)"ceph_open: fd %d associated to %s", fd, pathname);
  // in case of O_CREAT and O_EXCL, we should complain if the file exists
  // in case of O_READ, the file has to exist
  if (((flags & O_CREAT) && (flags & O_EXCL)) || ((flags&O_ACCMODE) == O_RDONLY)) {
    libradosstriper::RadosStriper *striper = getRadosStriper(fr);
    if (0 == striper) {
      deleteFileRef(fd, fr);
      return -EINVAL;
    }
    struct stat buf;
    int rc = striper->stat(fr.name, (uint64_t*)&(buf.st_size), &(buf.st_atime));
    if ((flags&O_ACCMODE) == O_RDONLY) {
      if (rc) {
        deleteFileRef(fd, fr);
        return rc;
      }
    } else if (rc != -ENOENT) {
      deleteFileRef(fd, fr);
      if (0 == rc) return -EEXIST;
      return rc;
    }
  }
  // in case of O_TRUNC, we should truncate the file
  if (flags & O_TRUNC) {
    int rc = ceph_posix_internal_truncate(fr, 0);
    // fail only if file exists and cannot be truncated
    if (rc < 0 && rc != -ENOENT) {
      deleteFileRef(fd, fr);
      return rc;
    }
  }
  return fd;
}

int ceph_posix_close(int fd) {
  CephFileRef* fr = getFileRef(fd);
  if (fr) {
    logwrapper((char*)"ceph_close: closed fd %d for file %s, read ops count %d, write ops count %d",
               fd, fr->name.c_str(), fr->rdcount, fr->wrcount);
    deleteFileRef(fd, *fr);
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
  return fr.offset;
}

off_t ceph_posix_lseek(int fd, off_t offset, int whence) {
  CephFileRef* fr = getFileRef(fd);
  if (fr) {
    logwrapper((char*)"ceph_lseek: for fd %d, offset=%lld, whence=%d", fd, offset, whence);
    return (off_t)lseek_compute_offset(*fr, offset, whence);
  } else {
    return -EBADF;
  }
}

off64_t ceph_posix_lseek64(int fd, off64_t offset, int whence) {
  CephFileRef* fr = getFileRef(fd);
  if (fr) {
    logwrapper((char*)"ceph_lseek64: for fd %d, offset=%lld, whence=%d", fd, offset, whence);
    return lseek_compute_offset(*fr, offset, whence);
  } else {
    return -EBADF;
  }
}

ssize_t ceph_posix_write(int fd, const void *buf, size_t count) {
  CephFileRef* fr = getFileRef(fd);
  if (fr) {
    logwrapper((char*)"ceph_write: for fd %d, count=%d", fd, count);
    if ((fr->flags & (O_WRONLY|O_RDWR)) == 0) {
      return -EBADF;
    }
    libradosstriper::RadosStriper *striper = getRadosStriper(*fr);
    if (0 == striper) {
      return -EINVAL;
    }
    ceph::bufferlist bl;
    bl.append((const char*)buf, count);
    int rc = striper->write(fr->name, bl, count, fr->offset);
    if (rc) return rc;
    fr->offset += count;
    fr->wrcount++;
    return count;
  } else {
    return -EBADF;
  }
}

ssize_t ceph_posix_pwrite(int fd, const void *buf, size_t count, off64_t offset) {
  CephFileRef* fr = getFileRef(fd);
  if (fr) {
    // TODO implement proper logging level for this plugin - this should be only debug
    //logwrapper((char*)"ceph_write: for fd %d, count=%d", fd, count);
    if ((fr->flags & (O_WRONLY|O_RDWR)) == 0) {
      return -EBADF;
    }
    libradosstriper::RadosStriper *striper = getRadosStriper(*fr);
    if (0 == striper) {
      return -EINVAL;
    }
    ceph::bufferlist bl;
    bl.append((const char*)buf, count);
    int rc = striper->write(fr->name, bl, count, offset);
    if (rc) return rc;
    fr->wrcount++;
    return count;
  } else {
    return -EBADF;
  }
}

static void ceph_aio_write_complete(rados_completion_t c, void *arg) {
  AioArgs *awa = reinterpret_cast<AioArgs*>(arg);
  size_t rc = rados_aio_get_return_value(c);
  awa->callback(awa->aiop, rc == 0 ? awa->nbBytes : rc);
  delete(awa);
}

ssize_t ceph_aio_write(int fd, XrdSfsAio *aiop, AioCB *cb) {
  CephFileRef* fr = getFileRef(fd);
  if (fr) {
    // get the parameters from the Xroot aio object
    size_t count = aiop->sfsAio.aio_nbytes;
    const char *buf = (const char*)aiop->sfsAio.aio_buf;
    size_t offset = aiop->sfsAio.aio_offset;
    // TODO implement proper logging level for this plugin - this should be only debug
    //logwrapper((char*)"ceph_aio_write: for fd %d, count=%d", fd, count);
    if ((fr->flags & (O_WRONLY|O_RDWR)) == 0) {
      return -EBADF;
    }
    // get the striper object
    libradosstriper::RadosStriper *striper = getRadosStriper(*fr);
    if (0 == striper) {
      return -EINVAL;
    }
    // prepare a bufferlist around the given buffer
    ceph::bufferlist bl;
    bl.append(buf, count);
    // get the poolIdx to use
    int cephPoolIdx = getCephPoolIdxAndIncrease();
    // Get the cluster to use
    librados::Rados* cluster = checkAndCreateCluster(cephPoolIdx);
    if (0 == cluster) {
      return -EINVAL;
    }
    // prepare a ceph AioCompletion object and do async call
    AioArgs *args = new AioArgs(aiop, cb, count);
    librados::AioCompletion *completion =
      cluster->aio_create_completion(args, ceph_aio_write_complete, NULL);
    // do the write
    int rc = striper->aio_write(fr->name, completion, bl, count, offset);
    completion->release();
    return rc;
  } else {
    return -EBADF;
  }
}

ssize_t ceph_posix_read(int fd, void *buf, size_t count) {
  CephFileRef* fr = getFileRef(fd);
  if (fr) {
    // TODO implement proper logging level for this plugin - this should be only debug
    //logwrapper((char*)"ceph_read: for fd %d, count=%d", fd, count);
    if ((fr->flags & O_WRONLY) != 0) {
      return -EBADF;
    }
    libradosstriper::RadosStriper *striper = getRadosStriper(*fr);
    if (0 == striper) {
      return -EINVAL;
    }
    ceph::bufferlist bl;
    int rc = striper->read(fr->name, &bl, count, fr->offset);
    if (rc < 0) return rc;
    bl.copy(0, rc, (char*)buf);
    fr->offset += rc;
    fr->rdcount++;
    return rc;
  } else {
    return -EBADF;
  }
}

ssize_t ceph_posix_pread(int fd, void *buf, size_t count, off64_t offset) {
  CephFileRef* fr = getFileRef(fd);
  if (fr) {
    // TODO implement proper logging level for this plugin - this should be only debug
    //logwrapper((char*)"ceph_read: for fd %d, count=%d", fd, count);
    if ((fr->flags & O_WRONLY) != 0) {
      return -EBADF;
    }
    libradosstriper::RadosStriper *striper = getRadosStriper(*fr);
    if (0 == striper) {
      return -EINVAL;
    }
    ceph::bufferlist bl;
    int rc = striper->read(fr->name, &bl, count, offset);
    if (rc < 0) return rc;
    bl.copy(0, rc, (char*)buf);
    fr->rdcount++;
    return rc;
  } else {
    return -EBADF;
  }
}

static void ceph_aio_read_complete(rados_completion_t c, void *arg) {
  AioArgs *awa = reinterpret_cast<AioArgs*>(arg);
  size_t rc = rados_aio_get_return_value(c);
  if (awa->bl) {
    if (rc > 0) {
      awa->bl->copy(0, rc, (char*)awa->aiop->sfsAio.aio_buf);
    }
    delete awa->bl;
    awa->bl = 0;
  }
  awa->callback(awa->aiop, rc == 0 ? awa->nbBytes : rc);
  delete(awa);
}

ssize_t ceph_aio_read(int fd, XrdSfsAio *aiop, AioCB *cb) {
  CephFileRef* fr = getFileRef(fd);
  if (fr) {
    // get the parameters from the Xroot aio object
    size_t count = aiop->sfsAio.aio_nbytes;
    size_t offset = aiop->sfsAio.aio_offset;
    // TODO implement proper logging level for this plugin - this should be only debug
    //logwrapper((char*)"ceph_aio_read: for fd %d, count=%d", fd, count);
    if ((fr->flags & O_WRONLY) != 0) {
      return -EBADF;
    }
    // get the striper object
    libradosstriper::RadosStriper *striper = getRadosStriper(*fr);
    if (0 == striper) {
      return -EINVAL;
    }
    // prepare a bufferlist to receive data
    ceph::bufferlist *bl = new ceph::bufferlist();
    // get the poolIdx to use
    int cephPoolIdx = getCephPoolIdxAndIncrease();
    // Get the cluster to use
    librados::Rados* cluster = checkAndCreateCluster(cephPoolIdx);
    if (0 == cluster) {
      return -EINVAL;
    }
    // prepare a ceph AioCompletion object and do async call
    AioArgs *args = new AioArgs(aiop, cb, count, bl);
    librados::AioCompletion *completion =
      cluster->aio_create_completion(args, ceph_aio_read_complete, NULL);
    // do the read
    int rc = striper->aio_read(fr->name, completion, bl, count, offset);
    completion->release();
    return rc;
  } else {
    return -EBADF;
  }
}

int ceph_posix_fstat(int fd, struct stat *buf) {
  CephFileRef* fr = getFileRef(fd);
  if (fr) {
    logwrapper((char*)"ceph_stat: fd %d", fd);
    // minimal stat : only size and times are filled
    // atime, mtime and ctime are set all to the same value
    // mode is set arbitrarily to 0666 | S_IFREG
    libradosstriper::RadosStriper *striper = getRadosStriper(*fr);
    if (0 == striper) {
      logwrapper((char*)"ceph_stat: getRadosStriper failed");
      return -EINVAL;
    }
    memset(buf, 0, sizeof(*buf));
    int rc = striper->stat(fr->name, (uint64_t*)&(buf->st_size), &(buf->st_atime));
    if (rc != 0) {
      return -rc;
    }
    buf->st_mtime = buf->st_atime;
    buf->st_ctime = buf->st_atime;
    buf->st_mode = 0666 | S_IFREG;
    return 0;
  } else {
    return -EBADF;
  }
}

int ceph_posix_stat(XrdOucEnv* env, const char *pathname, struct stat *buf) {
  logwrapper((char*)"ceph_stat: %s", pathname);
  // minimal stat : only size and times are filled
  // atime, mtime and ctime are set all to the same value
  // mode is set arbitrarily to 0666 | S_IFREG
  CephFile file = getCephFile(pathname, env);
  libradosstriper::RadosStriper *striper = getRadosStriper(file);
  if (0 == striper) {
    return -EINVAL;
  }
  memset(buf, 0, sizeof(*buf));
  int rc = striper->stat(file.name, (uint64_t*)&(buf->st_size), &(buf->st_atime));
  if (rc != 0) {
    // for non existing file. Check that we did not open it for write recently
    // in that case, we return 0 size and current time
    if (-ENOENT == rc && isOpenForWrite(file.name)) {
      buf->st_size = 0;
      buf->st_atime = time(NULL);
    } else {
      return -rc;
    }
  }
  buf->st_mtime = buf->st_atime;
  buf->st_ctime = buf->st_atime;
  buf->st_mode = 0666 | S_IFREG;
  return 0;
}

int ceph_posix_fsync(int fd) {
  CephFileRef* fr = getFileRef(fd);
  if (fr) {
    logwrapper((char*)"ceph_sync: fd %d", fd);
    return 0;
  } else {
    return -EBADF;
  }
}

int ceph_posix_fcntl(int fd, int cmd, ... /* arg */ ) {
  CephFileRef* fr = getFileRef(fd);
  if (fr) {
    logwrapper((char*)"ceph_fcntl: fd %d cmd=%d", fd, cmd);
    // minimal implementation
    switch (cmd) {
    case F_GETFL:
      return fr->mode;
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
  if (rc < 0) return rc;
  size_t returned_size = (size_t)rc<size?rc:size;
  bl.copy(0, returned_size, (char*)value);
  return returned_size;
}

ssize_t ceph_posix_getxattr(XrdOucEnv* env, const char* path,
                            const char* name, void* value,
                            size_t size) {
  logwrapper((char*)"ceph_getxattr: path %s name=%s", path, name);
  return ceph_posix_internal_getxattr(getCephFile(path, env), name, value, size);
}

ssize_t ceph_posix_fgetxattr(int fd, const char* name,
                             void* value, size_t size) {
  CephFileRef* fr = getFileRef(fd);
  if (fr) {
    logwrapper((char*)"ceph_fgetxattr: fd %d name=%s", fd, name);
    return ceph_posix_internal_getxattr(*fr, name, value, size);
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
  logwrapper((char*)"ceph_setxattr: path %s name=%s value=%s", path, name, value);
  return ceph_posix_internal_setxattr(getCephFile(path, env), name, value, size, flags);
}

int ceph_posix_fsetxattr(int fd,
                         const char* name, const void* value,
                         size_t size, int flags)  {
  CephFileRef* fr = getFileRef(fd);
  if (fr) {
    logwrapper((char*)"ceph_fsetxattr: fd %d name=%s value=%s", fd, name, value);
    return ceph_posix_internal_setxattr(*fr, name, value, size, flags);
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
  logwrapper((char*)"ceph_removexattr: path %s name=%s", path, name);
  return ceph_posix_internal_removexattr(getCephFile(path, env), name);
}

int ceph_posix_fremovexattr(int fd, const char* name) {
  CephFileRef* fr = getFileRef(fd);
  if (fr) {
    logwrapper((char*)"ceph_fremovexattr: fd %d name=%s", fd, name);
    return ceph_posix_internal_removexattr(*fr, name);
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
  logwrapper((char*)"ceph_listxattrs: path %s", path);
  return ceph_posix_internal_listxattrs(getCephFile(path, env), aPL, getSz);
}

int ceph_posix_flistxattrs(int fd, XrdSysXAttr::AList **aPL, int getSz) {
  CephFileRef* fr = getFileRef(fd);
  if (fr) {
    logwrapper((char*)"ceph_flistxattrs: fd %d", fd);
    return ceph_posix_internal_listxattrs(*fr, aPL, getSz);
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
  logwrapper((char*)"ceph_posix_statfs");
  // get the poolIdx to use
  int cephPoolIdx = getCephPoolIdxAndIncrease();
  // Get the cluster to use
  librados::Rados* cluster = checkAndCreateCluster(cephPoolIdx);
  if (0 == cluster) {
    return -EINVAL;
  }
  // call ceph stat
  librados::cluster_stat_t result;
  int rc = cluster->cluster_stat(result);
  if (0 == rc) {
    *totalSpace = result.kb * 1024;
    *freeSpace = result.kb_avail * 1024;
  }
  return rc;
}

static int ceph_posix_internal_truncate(const CephFile &file, unsigned long long size) {
  libradosstriper::RadosStriper *striper = getRadosStriper(file);
  if (0 == striper) {
    return -EINVAL;
  }
  return striper->trunc(file.name, size);
}

int ceph_posix_ftruncate(int fd, unsigned long long size) {
  CephFileRef* fr = getFileRef(fd);
  if (fr) {
    logwrapper((char*)"ceph_posix_ftruncate: fd %d, size %d", fd, size);
    return ceph_posix_internal_truncate(*fr, size);
  } else {
    return -EBADF;
  }
}

int ceph_posix_truncate(XrdOucEnv* env, const char *pathname, unsigned long long size) {
  logwrapper((char*)"ceph_posix_truncate : %s", pathname);
  // minimal stat : only size and times are filled
  CephFile file = getCephFile(pathname, env);
  return ceph_posix_internal_truncate(file, size);
}

int ceph_posix_unlink(XrdOucEnv* env, const char *pathname) {
  logwrapper((char*)"ceph_posix_unlink : %s", pathname);
  // minimal stat : only size and times are filled
  CephFile file = getCephFile(pathname, env);
  libradosstriper::RadosStriper *striper = getRadosStriper(file);
  if (0 == striper) {
    return -EINVAL;
  }
  return striper->remove(file.name);
}

DIR* ceph_posix_opendir(XrdOucEnv* env, const char *pathname) {
  logwrapper((char*)"ceph_posix_opendir : %s", pathname);
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
  res->m_iterator = ioctx->nobjects_begin();
  res->m_ioctx = ioctx;
  return (DIR*)res;
}

int ceph_posix_readdir(DIR *dirp, char *buff, int blen) {
  librados::NObjectIterator &iterator = ((DirIterator*)dirp)->m_iterator;
  librados::IoCtx *ioctx = ((DirIterator*)dirp)->m_ioctx;
  while (iterator->get_oid().compare(iterator->get_oid().size()-17, 17, ".0000000000000000") &&
         iterator != ioctx->nobjects_end()) {
    iterator++;
  }
  if (iterator == ioctx->nobjects_end()) {
    buff[0] = 0;
  } else {
    int l = iterator->get_oid().size()-17;
    if (l < blen) blen = l;
    strncpy(buff, iterator->get_oid().c_str(), blen-1);
    buff[blen-1] = 0;
    iterator++;
  }
  return 0;
}

int ceph_posix_closedir(DIR *dirp) {
  delete ((DirIterator*)dirp);
  return 0;
}
