#include <stdio.h>
#include <string>
#include <fcntl.h>

#include <XrdCeph/ceph_posix.h>
#include <XrdSys/XrdSysError.hh>
#include <XrdOuc/XrdOucEnv.hh>
#include <XrdVersion.hh>

#include "CephOss.hh"
#include "CephOssDir.hh"
#include "CephOssFile.hh"

extern "C"
{
  XrdOss*
  XrdOssGetStorageSystem(XrdOss* native_oss,
                         XrdSysLogger* Logger,
                         const char* config_fn,
                         const char* parms)
  {
    return new CephOss(parms);
  }
}

const char* CephOss::getPoolFromEnv(XrdOucEnv *env) {
  if (0 != env) {
    char* value = env->Get("pool");
    if (0 != value) {
      return value;
    }
  }
  return (char*)m_defaultPool.c_str();
}

CephOss::CephOss(const char* defaultPool) {
  if (0 == defaultPool) {
    m_defaultPool = "default";
  } else {
    m_defaultPool = defaultPool;
  }
}

CephOss::~CephOss() {
  ceph_posix_disconnect_all();
}

int CephOss::Chmod(const char *path, mode_t mode, XrdOucEnv *envP) {
  return -ENOTSUP;
}

int CephOss::Create(const char *tident, const char *path, mode_t access_mode,
                    XrdOucEnv &env, int Opts) {
  return -ENOTSUP;
}

int CephOss::Init(XrdSysLogger *logger, const char* configFn) { return 0; }

int CephOss::Mkdir(const char *path, mode_t mode, int mkpath, XrdOucEnv *envP) {
  return -ENOTSUP;
}

int CephOss::Remdir(const char *path, int Opts, XrdOucEnv *eP) {
  return -ENOTSUP;
}

int CephOss::Rename(const char *from,
                    const char *to,
                    XrdOucEnv *eP1,
                    XrdOucEnv *eP2) {
  return -ENOTSUP;
}

int CephOss::Stat(const char* path,
                  struct stat* buff,
                  int opts,
                  XrdOucEnv* env) {
  return ceph_posix_stat(getPoolFromEnv(env), path, buff);
}

int CephOss::StatFS(const char *path, char *buff, int &blen, XrdOucEnv *eP) {
  XrdOssVSInfo sP;
  int rc = StatVS(&sP, 0, 0);
  if (rc) {
    return rc;
  }
  int percentUsedSpace = (sP.Usage*100)/sP.Total;
  blen = snprintf(buff, blen, "%d %lld %d %d %lld %d",
                  1, sP.Free, percentUsedSpace, 0, 0LL, 0);
  return XrdOssOK;
}

int CephOss::StatVS(XrdOssVSInfo *sP, const char *sname, int updt) {
  int rc = ceph_posix_statfs(&(sP->Total), &(sP->Free));
  if (rc) {
    return rc;
  }
  sP->Large = sP->Total;
  sP->LFree = sP->Free;
  sP->Usage = sP->Total-sP->Free;
  sP->Extents = 1;
  return XrdOssOK;
}

int CephOss::Truncate (const char* path,
                       unsigned long long size,
                       XrdOucEnv* env) {
  return ceph_posix_truncate(getPoolFromEnv(env), path, size);
}

int CephOss::Unlink(const char *path, int Opts, XrdOucEnv *env) {
  return ceph_posix_unlink(getPoolFromEnv(env), path);
}

XrdOssDF* CephOss::newDir(const char *tident) {
  return dynamic_cast<XrdOssDF *>(new CephOssDir(this));
}

XrdOssDF* CephOss::newFile(const char *tident) {
  return dynamic_cast<XrdOssDF *>(new CephOssFile(this));
}

XrdVERSIONINFO(XrdOssGetStorageSystem, CephOss);
