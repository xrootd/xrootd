#include <stdio.h>
#include <string>
#include <fcntl.h>

#include "XrdCeph/XrdCephPosix.h"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdVersion.hh"
#include "XrdCeph/XrdCephOss.hh"
#include "XrdCeph/XrdCephOssDir.hh"
#include "XrdCeph/XrdCephOssFile.hh"

XrdSysError CephEroute(0);
XrdOucTrace CephTrace(&CephEroute);

// log wrapping function to be used by ceph_posix interface
char g_logstring[1024];
static void logwrapper(char *format, va_list argp) {
  vsnprintf(g_logstring, 1024, format, argp);
  CephEroute.Say(g_logstring);
}

extern "C"
{
  XrdOss*
  XrdOssGetStorageSystem(XrdOss* native_oss,
                         XrdSysLogger* lp,
                         const char* config_fn,
                         const char* parms)
  {
    // Do the herald thing
    CephEroute.SetPrefix("ceph_");
    CephEroute.logger(lp);
    CephEroute.Say("++++++ CERN/IT-DSS XrdCeph");
    // set parameters
    try {
      ceph_posix_set_defaults(parms);
    } catch (std::exception e) {
      CephEroute.Say("CephOss loading failed with exception. Check the syntax of parameters : ", parms);
      return 0;
    }
    // deal with logging
    ceph_posix_set_logfunc(logwrapper);
    return new CephOss();
  }
}

CephOss::CephOss() {}

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
  try {
    return ceph_posix_stat(env, path, buff);
  } catch (std::exception e) {
    CephEroute.Say("stat : invalid syntax in file parameters");
    return -EINVAL;
  }
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
  try {
    return ceph_posix_truncate(env, path, size);
  } catch (std::exception e) {
    CephEroute.Say("truncate : invalid syntax in file parameters");
    return -EINVAL;
  }
}

int CephOss::Unlink(const char *path, int Opts, XrdOucEnv *env) {
  try {
    return ceph_posix_unlink(env, path);
  } catch (std::exception e) {
    CephEroute.Say("unlink : invalid syntax in file parameters");
    return -EINVAL;
  }
}

XrdOssDF* CephOss::newDir(const char *tident) {
  return dynamic_cast<XrdOssDF *>(new CephOssDir(this));
}

XrdOssDF* CephOss::newFile(const char *tident) {
  return dynamic_cast<XrdOssDF *>(new CephOssFile(this));
}

XrdVERSIONINFO(XrdOssGetStorageSystem, CephOss);
