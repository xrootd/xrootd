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

#include "XrdVersion.hh"
#include "XrdCeph/XrdCephPosix.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdCeph/XrdCephXAttr.hh"

#include "XrdCks/XrdCksData.hh"
#include <string.h>

//#include "XrdCeph/XrdCephGlobals.hh"

FILE *g_cksLogFile;

char *ts_rfc3339() {

    std::time_t now = std::time({});
    char timeString[std::size("yyyy-mm-dd hh:mm:ss")];
    std::strftime(std::data(timeString), std::size(timeString),
                  "%F %TZ", std::gmtime(&now));
    return strdup(timeString);
}


XrdSysError XrdCephXattrEroute(0);
XrdOucTrace XrdCephXattrTrace(&XrdCephXattrEroute);

extern "C"
{
  XrdSysXAttr*
  XrdSysGetXAttrObject(XrdSysError  *errP,
                       const char   *config_fn,
                       const char   *parms)
  {
    // Do the herald thing
    XrdCephXattrEroute.SetPrefix("cephxattr_");
    XrdCephXattrEroute.logger(errP->logger());
    XrdCephXattrEroute.Say("++++++ CERN/IT-DSS XrdCephXattr");
    // set parameters
    try {
      ceph_posix_set_defaults(parms);
    } catch (std::exception &e) {
      XrdCephXattrEroute.Say("CephXattr loading failed with exception. Check the syntax of parameters : ", parms);
      return 0;
    }

    const char *cksLogFilename = "/var/log/xrootd/checksums/checksums.log";
    g_cksLogFile = fopen(cksLogFilename, "a");
    if (NULL == g_cksLogFile) {
           XrdCephXattrEroute.Emsg("Config cannot open file for logging checksum values and pathnames: ", cksLogFilename);
       } else {
           XrdCephXattrEroute.Emsg("Opened file for logging checksum values and pathname: ", cksLogFilename);
    }
    return new XrdCephXAttr();
  }
}
extern bool g_storeStreamedAdler32;

constexpr char hex2ascii(char nibble)   { return (0<= nibble && nibble<=9) ? nibble+'0' : nibble-10+'a'; }
constexpr char hiNibble(uint8_t hexbyte) { return (hexbyte & 0xf0) >> 4; }
constexpr char loNibble(uint8_t hexbyte) { return (hexbyte & 0x0f); }

constexpr char *hexbytes2ascii(const char bytes[], const unsigned int length){

  char asciiVal[2*length+1] {};
  for (unsigned int i = 0, j = 0; i < length; i++) {

     const uint8_t hexbyte = bytes[i];
     asciiVal[j++] = hex2ascii(hiNibble(hexbyte));
     asciiVal[j++] = hex2ascii(loNibble(hexbyte));

  }
  return strdup(asciiVal);
}

XrdCephXAttr::XrdCephXAttr() {}

XrdCephXAttr::~XrdCephXAttr() {}

int XrdCephXAttr::Del(const char *Aname, const char *Path, int fd) {
  try {
    return ceph_posix_removexattr(0, Path, Aname);
  } catch (std::exception &e) {
    XrdCephXattrEroute.Say("Del : invalid syntax in file parameters", Path);
    return -EINVAL;
  }
}

void XrdCephXAttr::Free(AList *aPL) {
  ceph_posix_freexattrlist(aPL);
}

int XrdCephXAttr::Get(const char *Aname, void *Aval, int Avsz,
                   const char *Path,  int fd) {
  if (fd >= 0) {
    return ceph_posix_fgetxattr(fd, Aname, Aval, Avsz);
  } else {
    try {
      return ceph_posix_getxattr(0, Path, Aname, Aval, Avsz);
    } catch (std::exception &e) {
      XrdCephXattrEroute.Say("Get : invalid syntax in file parameters", Path);
      return -EINVAL;
    }
  }
}

int XrdCephXAttr::List(AList **aPL, const char *Path, int fd, int getSz) {
  if (fd > 0) {
    return ceph_posix_flistxattrs(fd, aPL, getSz);
  } else {
    try {
      return ceph_posix_listxattrs(0, Path, aPL, getSz);
    } catch (std::exception &e) {
      XrdCephXattrEroute.Say("List : invalid syntax in file parameters", Path);
      return -EINVAL;
    }
  }
}

int XrdCephXAttr::Set(const char *Aname, const void *Aval, int Avsz,
                      const char *Path,  int fd,  int isNew) {

  int rc = 0;

  if (fd >= 0) {
    rc = ceph_posix_fsetxattr(fd, Aname, Aval, Avsz, 0);
  } else {
    try {
      rc = ceph_posix_setxattr(0, Path, Aname, Aval, Avsz, 0);
    } catch (std::exception &e) {
      XrdCephXattrEroute.Say("Set : invalid syntax in file parameters", Path);
     rc = -EINVAL;
    }
  }

  if (0 == rc && !strcmp(Aname, "XrdCks.adler32") ) { 


      auto *cks = (XrdCksData *)Aval;
      auto cksAscii = (const char*)hexbytes2ascii(cks->Value, cks->Length);
      XrdCephXattrEroute.Say("readback checksum = ", cksAscii);
      fprintf(g_cksLogFile, "%s,%s,%s,%s,%s\n", ts_rfc3339(), Path, "readback", "adler32", cksAscii);
      fflush(g_cksLogFile);
/*
      char cksBuff[8+1];//      sprintf(cksBuff, "%08x", (uint8_t)*(cks->Value));// Produces "00000084"
      fflush(g_cksLogFile);
*/
  }

  return rc;
}

XrdVERSIONINFO(XrdSysGetXAttrObject, XrdCephXAttr);
