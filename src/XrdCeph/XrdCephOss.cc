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

#include <stdio.h>
#include <string>
#include <fcntl.h>

#include "XrdCeph/XrdCephPosix.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucName2Name.hh"
#include "XrdOuc/XrdOucN2NLoader.hh"
#include "XrdVersion.hh"
#include "XrdCeph/XrdCephOss.hh"
#include "XrdCeph/XrdCephOssDir.hh"
#include "XrdCeph/XrdCephOssFile.hh"

XrdVERSIONINFO(XrdOssGetStorageSystem, XrdCephOss);

XrdSysError XrdCephEroute(0);
XrdOucTrace XrdCephTrace(&XrdCephEroute);

// log wrapping function to be used by ceph_posix interface
char g_logstring[1024];
static void logwrapper(char *format, va_list argp) {
  vsnprintf(g_logstring, 1024, format, argp);
  XrdCephEroute.Say(g_logstring);
}

/// pointer to library providing Name2Name interface. 0 be default
/// populated in case of ceph.namelib entry in the config file
/// used in XrdCephPosix
extern XrdOucName2Name *g_namelib;

extern "C"
{
  XrdOss*
  XrdOssGetStorageSystem(XrdOss* native_oss,
                         XrdSysLogger* lp,
                         const char* config_fn,
                         const char* parms)
  {
    // Do the herald thing
    XrdCephEroute.SetPrefix("ceph_");
    XrdCephEroute.logger(lp);
    XrdCephEroute.Say("++++++ CERN/IT-DSS XrdCeph");
    // set parameters
    try {
      ceph_posix_set_defaults(parms);
    } catch (std::exception &e) {
      XrdCephEroute.Say("CephOss loading failed with exception. Check the syntax of parameters : ", parms);
      return 0;
    }
    // deal with logging
    ceph_posix_set_logfunc(logwrapper);
    return new XrdCephOss(config_fn, XrdCephEroute);
  }
}

XrdCephOss::XrdCephOss(const char *configfn, XrdSysError &Eroute) {
  Configure(configfn, Eroute);
}

XrdCephOss::~XrdCephOss() {
  ceph_posix_disconnect_all();
}

// declared and used in XrdCephPosix.cc
extern unsigned int g_maxCephPoolIdx;
int XrdCephOss::Configure(const char *configfn, XrdSysError &Eroute) {
   int NoGo = 0;
   XrdOucEnv myEnv;
   XrdOucStream Config(&Eroute, getenv("XRDINSTANCE"), &myEnv, "=====> ");
   // If there is no config file, nothing to be done
   if (configfn && *configfn) {
     // Try to open the configuration file.
     int cfgFD;
     if ((cfgFD = open(configfn, O_RDONLY, 0)) < 0) {
       Eroute.Emsg("Config", errno, "open config file", configfn);
       return 1;
     }
     Config.Attach(cfgFD);
     // Now start reading records until eof.
     char *var;
     while((var = Config.GetMyFirstWord())) {
       if (!strncmp(var, "ceph.nbconnections", 18)) {
         var = Config.GetWord();
         if (var) {
           unsigned long value = strtoul(var, 0, 10);
           if (value > 0 and value <= 100) {
             g_maxCephPoolIdx = value;
           } else {
             Eroute.Emsg("Config", "Invalid value for ceph.nbconnections in config file (must be between 1 and 100)", configfn, var);
             return 1;
           }
         } else {
           Eroute.Emsg("Config", "Missing value for ceph.nbconnections in config file", configfn);
           return 1;
         }
       }
       if (!strncmp(var, "ceph.namelib", 12)) {
         var = Config.GetWord();
         if (var) {
           // Warn in case parameters were givne
           char parms[1040];
           if (!Config.GetRest(parms, sizeof(parms)) || parms[0]) {
             Eroute.Emsg("Config", "namelib parameters will be ignored");
           }
           // Load name lib
           XrdOucN2NLoader n2nLoader(&Eroute,configfn,NULL,NULL,NULL);
           g_namelib = n2nLoader.Load(var, XrdVERSIONINFOVAR(XrdOssGetStorageSystem), NULL);
           if (!g_namelib) {
             Eroute.Emsg("Config", "Unable to load library given in ceph.namelib : %s", var);
           }
         } else {
           Eroute.Emsg("Config", "Missing value for ceph.namelib in config file", configfn);
           return 1;
         }
       }
     }

     // Now check if any errors occured during file i/o
     int retc = Config.LastError();
     if (retc) {
       NoGo = Eroute.Emsg("Config", -retc, "read config file",
                          configfn);
     }
     Config.Close();
   }
   return NoGo;
}

int XrdCephOss::Chmod(const char *path, mode_t mode, XrdOucEnv *envP) {
  return -ENOTSUP;
}

int XrdCephOss::Create(const char *tident, const char *path, mode_t access_mode,
                    XrdOucEnv &env, int Opts) {
  return -ENOTSUP;
}

int XrdCephOss::Init(XrdSysLogger *logger, const char* configFn) { return 0; }

int XrdCephOss::Mkdir(const char *path, mode_t mode, int mkpath, XrdOucEnv *envP) {
  return -ENOTSUP;
}

int XrdCephOss::Remdir(const char *path, int Opts, XrdOucEnv *eP) {
  return -ENOTSUP;
}

int XrdCephOss::Rename(const char *from,
                    const char *to,
                    XrdOucEnv *eP1,
                    XrdOucEnv *eP2) {
  return -ENOTSUP;
}

int XrdCephOss::Stat(const char* path,
                  struct stat* buff,
                  int opts,
                  XrdOucEnv* env) {
  try {
    if (!strcmp(path, "/")) {
      // special case of a stat made by the locate interface
      // we intend to then list all files
      memset(buff, 0, sizeof(*buff));
      buff->st_mode = S_IFDIR | 0700;
      return 0;
    } else {
      return ceph_posix_stat(env, path, buff);
    }
  } catch (std::exception &e) {
    XrdCephEroute.Say("stat : invalid syntax in file parameters");
    return -EINVAL;
  }
}

int XrdCephOss::StatFS(const char *path, char *buff, int &blen, XrdOucEnv *eP) {
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

int XrdCephOss::StatVS(XrdOssVSInfo *sP, const char *sname, int updt) {
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

int XrdCephOss::Truncate (const char* path,
                          unsigned long long size,
                          XrdOucEnv* env) {
  try {
    return ceph_posix_truncate(env, path, size);
  } catch (std::exception &e) {
    XrdCephEroute.Say("truncate : invalid syntax in file parameters");
    return -EINVAL;
  }
}

int XrdCephOss::Unlink(const char *path, int Opts, XrdOucEnv *env) {
  try {
    return ceph_posix_unlink(env, path);
  } catch (std::exception &e) {
    XrdCephEroute.Say("unlink : invalid syntax in file parameters");
    return -EINVAL;
  }
}

XrdOssDF* XrdCephOss::newDir(const char *tident) {
  return new XrdCephOssDir(this);
}

XrdOssDF* XrdCephOss::newFile(const char *tident) {
  return new XrdCephOssFile(this);
}

