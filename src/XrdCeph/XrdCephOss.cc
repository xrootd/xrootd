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

#include <chrono>
#include <stdio.h>
#include <string>
#include <fcntl.h>
#include <limits.h>

#include "XrdVersion.hh"
#include "XrdCeph/XrdCephOss.hh"
#include "XrdCeph/XrdCephOssDir.hh"
#include "XrdCeph/XrdCephOssFile.hh"
#include "XrdCeph/XrdCephPosix.hh"
#include "XrdCeph/XrdCephOssBufferedFile.hh"
#include "XrdCeph/XrdCephOssReadVFile.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucName2Name.hh"
#include "XrdOuc/XrdOucN2NLoader.hh"

XrdVERSIONINFO(XrdOssGetStorageSystem, XrdCephOss);

XrdSysError XrdCephEroute(0);
XrdOucTrace XrdCephTrace(&XrdCephEroute);

/// timestamp output for logging messages
static std::string ts() {
    std::time_t t = std::time(nullptr);
    char mbstr[50];
    std::strftime(mbstr, sizeof(mbstr), "%y%m%d %H:%M:%S ", std::localtime(&t));
    return std::string(mbstr);
}

// log wrapping function to be used by ceph_posix interface
char g_logstring[1024];
static void logwrapper(char *format, va_list argp) {
  vsnprintf(g_logstring, 1024, format, argp);
  XrdCephEroute.Say(ts().c_str(), g_logstring);
}

/// pointer to library providing Name2Name interface. 0 be default
/// populated in case of ceph.namelib entry in the config file
/// used in XrdCephPosix
extern XrdOucName2Name *g_namelib;


/// converts a logical filename to physical one if needed
void m_translateFileName(std::string &physName, std::string logName){
  if (0 != g_namelib) {
    char physCName[MAXPATHLEN+1];
    int retc = g_namelib->lfn2pfn(logName.c_str(), physCName, sizeof(physCName));
    if (retc) {
      XrdCephEroute.Say(__FUNCTION__, " - failed to translate '", logName.c_str(), "' using namelib plugin, using it as is");
      physName = logName;
    } else {
      XrdCephEroute.Say(__FUNCTION__, " - translated '", logName.c_str(), "' to '", physCName, "'");
      physName = physCName;
    }
  } else {
    physName = logName;
  }
}

/**
 * Get an integer numeric value from an extended attribute attached to an object
 *
 * @brief Retrieve an integer-value extended attribute.
 * @param path the object ID containing the attribute
 * @param attrName the name of the attribute to retrieve
 * @param maxAttrLen the largest number of characters to handle
 * @return value of the attibute, -EINVAL if not valid integer, or -ENOMEM
 *
 * Implementation:
 * Ian Johnson, ian.johnson@stfc.ac.uk, 2022
 *
 */

ssize_t getNumericAttr(const char* const path, const char* attrName, const int maxAttrLen)
{

  ssize_t retval;
  char *attrValue = (char*)malloc(maxAttrLen+1);
  if (NULL == attrValue) {
    return -ENOMEM;
  }

  ssize_t attrLen = ceph_posix_getxattr((XrdOucEnv*)NULL, path, attrName, attrValue, maxAttrLen);

  if (attrLen <= 0) {
    retval = -EINVAL;
  } else {
    attrValue[attrLen] = '\0';
    char *endPointer = (char *)NULL;
    retval = strtoll(attrValue, &endPointer, 10);
  }

  if (NULL != attrValue) {
    free(attrValue);
  }
  
  return retval;

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
extern unsigned int g_cephAioWaitThresh;

int XrdCephOss::Configure(const char *configfn, XrdSysError &Eroute) {
   int NoGo = 0;
   XrdOucEnv myEnv;
   XrdOucStream Config(&Eroute, getenv("XRDINSTANCE"), &myEnv, "=====> ");
   //disable posc  
   XrdOucEnv::Export("XRDXROOTD_NOPOSC", "1");
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
           std::string libname = var;
           // Warn in case parameters were givne
           char parms[1040];
           bool hasParms{false};
           if (!Config.GetRest(parms, sizeof(parms)) || parms[0]) {
              hasParms = true;
           }
           // Load name lib
           XrdOucN2NLoader  n2nLoader(&Eroute,configfn,(hasParms?parms:""),NULL,NULL);
           g_namelib = n2nLoader.Load(libname.c_str(), XrdVERSIONINFOVAR(XrdOssGetStorageSystem), NULL);
           if (!g_namelib) {
             Eroute.Emsg("Config", "Unable to load library given in ceph.namelib : %s", var);
           }
         } else {
           Eroute.Emsg("Config", "Missing value for ceph.namelib in config file ", configfn);
           return 1;
         }
       }

       int pread_flag_set = !strncmp(var, "ceph.usedefaultpreadalg", 24);
       int readv_flag_set = !strncmp(var, "ceph.usedefaultreadvalg", 24);
       if (pread_flag_set or readv_flag_set) {
         var = Config.GetWord();
         if (var) {
           char* endptr;
           long value = strtol(var, &endptr, 10);
           if ((value == 0 || value == 1) && (var != endptr)) {
             if (pread_flag_set) {
               m_useDefaultPreadAlg = value;
             } else if(readv_flag_set) {
               m_useDefaultReadvAlg = value;
             } else {
               Eroute.Emsg("Config", "Bug encountered during parsing", var);
             }
           } else {
             Eroute.Emsg("Config", "Invalid value for ceph.usedefault* in config file -- must be 0 or 1, got", var);
             return 1;
           }
         } else {
           Eroute.Emsg("Config", "Missing value for ceph.usedefault* in config file");
           return 1; 
         }
       }

       if (!strncmp(var, "ceph.aiowaitthresh", 19)) {
         var = Config.GetWord();
         if (var) {
           unsigned long value = strtoul(var, 0, 10);
           if ((value > 0) && (value < INT_MAX)){
             g_cephAioWaitThresh = value;
           } else {
             Eroute.Emsg("Config", "Invalid value for ceph.aiowaitthresh:", var);
           }
         } else {
           Eroute.Emsg("Config", "Missing value for ceph.aiowaitthresh in config file");
           return 1; 
         }
       }
     
        if (!strncmp(var, "ceph.usebuffer", 14)) { // allowable values: 0, 1
         var = Config.GetWord();
         if (var) {
           unsigned long value = strtoul(var, 0, 10);
           if (value <= 1) {
             m_configBufferEnable = value;
             Eroute.Emsg("Config", "ceph.usebuffer",std::to_string(m_configBufferEnable).c_str());
           } else {
             Eroute.Emsg("Config", "Invalid value for ceph.usebuffer in config file (must be 0 or 1)", configfn, var);
             return 1;
           }
         } else {
           Eroute.Emsg("Config", "Missing value for ceph.usebuffer in config file", configfn);
           return 1;
         }
       } // usebuffer
        if (!strncmp(var, "ceph.buffersize", 15)) { // size in bytes
         var = Config.GetWord();
         if (var) {
           unsigned long value = strtoul(var, 0, 10);
           if (value > 0 and value <= 1000000000L) {
             m_configBufferSize = value;
            Eroute.Emsg("Config", "ceph.buffersize", std::to_string(m_configBufferSize).c_str() ); 
           } else {
             Eroute.Emsg("Config", "Invalid value for ceph.buffersize in config file; enter in bytes (no units)", configfn, var);
             return 1;
           }
         } else {
           Eroute.Emsg("Config", "Missing value for ceph.buffersize in config file", configfn);
           return 1;
         }
       } // buffersize
        if (!strncmp(var, "ceph.buffermaxpersimul", 22)) { // size in bytes
         var = Config.GetWord();
         if (var) {
           unsigned long value = strtoul(var, 0, 10);
           if (value > 0 and value <= 1000000000L) {
             m_configMaxSimulBufferCount = value;
            Eroute.Emsg("Config", "ceph.buffermaxpersimul", std::to_string(m_configMaxSimulBufferCount).c_str() ); 
           } else {
             Eroute.Emsg("Config", "Invalid value for ceph.buffermaxpersimul in config file; enter in bytes (no units)", configfn, var);
             return 1;
           }
         } else {
           Eroute.Emsg("Config", "Missing value for ceph.buffermaxpersimul in config file", configfn);
           return 1;
         }
       } // buffersize

          if (!strncmp(var, "ceph.usereadv", 13)) { // allowable values: 0, 1
         var = Config.GetWord();
         if (var) {
           unsigned long value = strtoul(var, 0, 10);
           if (value <= 1) {
             m_configReadVEnable = value;
             Eroute.Emsg("Config", "ceph.usereadvalg",std::to_string(m_configBufferEnable).c_str());
           } else {
             Eroute.Emsg("Config", "Invalid value for ceph.usereadv in config file (must be 0 or 1)", configfn, var);
             return 1;
           }
         } else {
           Eroute.Emsg("Config", "Missing value for ceph.usereadv in config file", configfn);
           return 1;
         }
       } // usereadv
       if (!strncmp(var, "ceph.readvalgname", 17)) {
         var = Config.GetWord();
        // Eroute.Emsg("Config", "readvalgname readvalgname readvalgname readvalgname", var);
         if (var) {
           // Warn in case parameters were givne
           char parms[1040];
           if (!Config.GetRest(parms, sizeof(parms)) || parms[0]) {
             Eroute.Emsg("Config", "readvalgname parameters will be ignored");
           }
          m_configReadVAlgName = var;
         } else {
           Eroute.Emsg("Config", "Missing value for ceph.readvalgname in config file", configfn);
           return 1;
         }
       }
       if (!strncmp(var, "ceph.bufferiomode", 17)) {
         var = Config.GetWord();
         if (var) {
           // Warn in case parameters were givne
           char parms[1040];
           if (!Config.GetRest(parms, sizeof(parms)) || parms[0]) {
             Eroute.Emsg("Config", "readvalgname parameters will be ignored");
           }
          m_configBufferIOmode = var; // allowed values would be aio, io
         } else {
           Eroute.Emsg("Config", "Missing value for ceph.bufferiomode in config file", configfn);
           return 1;
         }
       }

       if (!strcmp(var, "ceph.reportingpools")) {
         var = Config.GetWord();
         if (var) {
           m_configPoolnames = var;
         } else {
           Eroute.Emsg("Config", "Missing value for ceph.reportingpools in config file", configfn);
           return 1; 
         }
       }       
     } // while

     // Now check if any errors occurred during file i/o

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

//SCS - lie to posix-assuming clients about directories [fixes brittleness in GFAL2]
int XrdCephOss::Mkdir(const char *path, mode_t mode, int mkpath, XrdOucEnv *envP) {
  return 0;
}

//SCS - lie to posix-assuming clients about directories [fixes brittleness in GFAL2]
int XrdCephOss::Remdir(const char *path, int Opts, XrdOucEnv *eP) {
  return 0;
}

int XrdCephOss::Rename(const char *from,
                    const char *to,
                    XrdOucEnv *eP1,
                    XrdOucEnv *eP2) {
  return -ENOTSUP;
}

/**
 *

 * @brief Extract a pool name (string before the first colon ':') from an object ID.
 * @param (in) possPool the object ID
 * @return pool name or unchanged object ID
 *
 * Implementation:
 * Ian Johnson		STFC RAL, ian.johnson@stfc.ac.uk, 2022 
 *
 */

std::string extractPool(std::string possPool) {

   std::string pool;
   auto colonPos = possPool.find_first_of(':');

   if (colonPos > 0) {
     pool = possPool.substr(0, colonPos);
   } else {
     pool = possPool;
   }
   return pool;
}


/**
 *
 * Populate a struct stat* with information on an object ID.
 * Determine whether the request relates to a pool name for disk space reporting via
 * StatLS. If not, handle an object path or the notional root element "/"
 *
 * @brief Return status information for an object ID.
 * @param (in) path the object ID
 * @param (out) buff receive the status information
 * @param (in) opts not used
 * @param (in) env not used
 * 
 * Implementation of enhancements:
 * Jyothish Thomas	STFC RAL, jyothish.thomas@stfc.ac.uk, 2022
 * Ian Johnson		STFC RAL, ian.johnson@stfc.ac.uk, 2022, 2023
 * 
 */


int XrdCephOss::Stat(const char* path,
                  struct stat* buff,
                  int opts,
                  XrdOucEnv* env) {
  
  XrdCephEroute.Say(__FUNCTION__, " path = ", path);

  std::string spath {path};
  m_translateFileName(spath,path);

  if (spath.back() == '/') { // Request to stat the root 


#ifdef STAT_TRACE
    XrdCephEroute.Say(__FUNCTION__, " - fake a return for stat'ing root element '/'");
#endif


    // special case of a stat made by the locate interface
    // we intend to then list all files 
    
    memset(buff, 0, sizeof(*buff));

    buff->st_mode = S_IFDIR|S_IRWXU;
    buff->st_dev = 1;
    buff->st_ino = 1;

    return XrdOssOK;
   
  } 

  if (spath.find_first_of(":") == spath.length()-1) { // Request to stat just the pool name

#ifdef STAT_TRACE
    XrdCephEroute.Say(__FUNCTION__, "Found request to stat pool name");
#endif

    spath.pop_back(); // remove colon from pool name
    if (m_configPoolnames.find(spath) != std::string::npos)  { // Support 'locate' for spaceinfo
#ifdef STAT_TRACE  
      XrdCephEroute.Say(__FUNCTION__, " - preparing spaceinfo report for '", path, "'");
#endif
      return XrdOssOK; // Only requires a status code, do not need to fill contents in struct stat
    } else {
      XrdCephEroute.Say(__FUNCTION__, " - cannot find pool '", path, "' in ceph.reportingpools");
      return -EINVAL;
    }


  } else if (ceph_posix_stat(env, path, buff) == 0) { // Found object ID 

#ifdef STAT_TRACE
    XrdCephEroute.Say(__FUNCTION__, " - found object ", spath.c_str(), " via ceph_posix_stat");
#endif
    return XrdOssOK;

  } else {

#ifdef STAT_TRACE
    XrdCephEroute.Say(__FUNCTION__, " - cannot find object '", spath.c_str(), "'");
#endif
    return -ENOENT;

  }


}



int XrdCephOss::StatFS(const char *path, char *buff, int &blen, XrdOucEnv *eP) {

#ifdef STAT_TRACE  
  XrdCephEroute.Say(__FUNCTION__, " path = ", path);
#endif
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

#ifdef STAT_TRACE
  XrdCephEroute.Say(__FUNCTION__, " path = ", sname);
#endif
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

int formatStatLSResponse(char *buff, int &blen, const char* cgroup, long long totalSpace, 
  long long usedSpace, long long freeSpace, long long quota, long long maxFreeChunk)
{
  return snprintf(buff, blen, "oss.cgroup=%s&oss.space=%lld&oss.free=%lld&oss.maxf=%lld&oss.used=%lld&oss.quota=%lld",
                                     cgroup,       totalSpace,    freeSpace,    maxFreeChunk, usedSpace,    quota);
}

/**
 *
 * Handle a request for the amount of space used in a Ceph pool
 * 
 * @brief Report on disk space use in this pool.
 * @param (in) env not used
 * @param (in) path name of the pool
 * @param (out) buff location for string containing OSS key-value pairs for disk space used, free, etc
 * @param (out) blen set to length of buff
 *
 * Implementation:
 * Jyothish Thomas	STFC RAL, jyothish.thomas@stfc.ac.uk, 2022
 * Ian Johnson		STFC RAL, ian.johnson@stfc.ac.uk, 2022, 2023
 *
 */



int XrdCephOss::StatLS(XrdOucEnv &env, const char *charPath, char *buff, int &blen)
{
  XrdCephEroute.Say(__FUNCTION__, " incoming path = ", charPath); 

  std::string path(charPath);
  path = extractPool(path);  
  std::string spath {path};
 
  m_translateFileName(spath,path);

//
// Following test is now redundant as we take the substring up to colonPos
//

  if (spath.back() == ':') {
    spath.pop_back();
  }
  if (m_configPoolnames.find(spath) == std::string::npos) {
    XrdCephEroute.Say("Can't report on ", spath.c_str());
    return -EINVAL;
  }

  long long usedSpace, totalSpace, freeSpace;

  if (ceph_posix_stat_pool(spath.c_str(), &usedSpace) != 0) {
      XrdCephEroute.Say("Failed to get used space in pool ", spath.c_str());
      return -EINVAL;
  }

  // Construct the object path
  std::string spaceInfoPath =  spath + ":" +  (const char *)"__spaceinfo__";
  totalSpace = getNumericAttr(spaceInfoPath.c_str(), "total_space", 24);
  if (totalSpace < 0) {
    XrdCephEroute.Say("Could not get 'total_space' attribute from ", spaceInfoPath.c_str());
    return -EINVAL;
  }

//
// Figure for 'usedSpace' already accounts for Erasure Coding overhead
//


  freeSpace = totalSpace - usedSpace;
  blen = formatStatLSResponse(buff, blen, 
    spath.c_str(),       /* "oss.cgroup" */ 
    totalSpace, /* "oss.space"  */
    usedSpace,  /* "oss.used"   */
    freeSpace,  /* "oss.free"   */
    totalSpace, /* "oss.quota"  */
    freeSpace   /* "oss.maxf"   */);
#ifdef STAT_TRACE
  XrdCephEroute.Say(__FUNCTION__, "space info = \n", buff);
#endif
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

  // Depending on the configuration settings stack up the underlying 
  // XrdCephOssFile instance with decorator objects for readV and Buffering requests

  XrdCephOssFile* xrdCephOssDF =  new XrdCephOssFile(this);
  
  if (m_configReadVEnable) {
    xrdCephOssDF = new XrdCephOssReadVFile(this,xrdCephOssDF,m_configReadVAlgName);
  }

  if (m_configBufferEnable) {
    xrdCephOssDF = new XrdCephOssBufferedFile(this,xrdCephOssDF, m_configBufferSize, 
                                              m_configBufferIOmode, m_configMaxSimulBufferCount);
  }


  return xrdCephOssDF;
}

