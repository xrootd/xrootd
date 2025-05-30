/******************************************************************************/
/*                                                                            */
/*                        X r d P o s i x M a p . c c                         */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#include <cerrno>
#include <sys/stat.h>

#include "XrdOuc/XrdOucECMsg.hh"
#include "XProtocol/XProtocol.hh"
#include "XrdPosix/XrdPosixMap.hh"
#include "XrdSfs/XrdSfsFlags.hh"
#include "XrdSys/XrdSysHeaders.hh"

#ifndef EAUTH
#define EAUTH EBADE
#endif

#ifndef ENOSR
#define ENOSR ENOSPC
#endif

#ifndef ECHRNG
#define ECHRNG EINVAL
#endif

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/
  
bool XrdPosixMap::Debug = false;

/******************************************************************************/
/*                            F l a g s 2 M o d e                             */
/******************************************************************************/
  
mode_t XrdPosixMap::Flags2Mode(dev_t *rdv, uint32_t flags)
{
   mode_t newflags = 0;

// Map the xroot flags to unix flags
//
   if (flags & XrdCl::StatInfo::XBitSet)       newflags |= S_IXUSR;
   if (flags & XrdCl::StatInfo::IsReadable)    newflags |= S_IRUSR;
   if (flags & XrdCl::StatInfo::IsWritable)    newflags |= S_IWUSR;
        if (flags & XrdCl::StatInfo::Other)    newflags |= S_IFBLK;
   else if (flags & XrdCl::StatInfo::IsDir)    newflags |= S_IFDIR;
   else                                        newflags |= S_IFREG;
   if (flags & XrdCl::StatInfo::POSCPending)   newflags |= XRDSFS_POSCPEND;
   if (rdv)
  {*rdv = 0;
   if (flags & XrdCl::StatInfo::Offline)       *rdv     |= XRDSFS_OFFLINE;
   if (flags & XrdCl::StatInfo::BackUpExists)  *rdv     |= XRDSFS_HASBKUP;
  }

   return newflags;
}

/******************************************************************************/
/*                             S t a t F i l l                                */
/******************************************************************************/

int XrdPosixMap::Entry2Buf(const XrdCl::DirectoryList::ListEntry &dirEnt, struct stat &buf, XrdOucECMsg &ecMsg)
{
   auto statInfo = dirEnt.GetStatInfo();
   if (!statInfo) {
      ecMsg.Set(EIO, "Directory entry is missing expected stat information");
      return EIO;
   }

   memset(&buf, '\0', sizeof(buf));
   buf.st_mode   = Flags2Mode(nullptr, statInfo->GetFlags());
   // Since the UID/GID isn't known by the client, when these are translated by
   // XrdXrootdProtocol::StatGen back to xroot protocol flags, they will get zero'd
   // out if only the user access mode is set (e.g., S_IRUSR).  Therefor, upgrade the
   // access mode in the mapping to "other" as well (e.g., S_ROTH).  This way, the
   // computed mode is the same for both the origin and the cache
   if (buf.st_mode & S_IRUSR) buf.st_mode |= S_IROTH;
   if (buf.st_mode & S_IWUSR) buf.st_mode |= S_IWOTH;
   if (buf.st_mode & S_IXUSR) buf.st_mode |= S_IXOTH;
   buf.st_mtime  = static_cast<time_t>(statInfo->GetModTime());
   buf.st_ctime  = buf.st_mtime;
   buf.st_size   = static_cast<size_t>(statInfo->GetSize());
   buf.st_ino    = static_cast<ino_t>(strtoll(statInfo->GetId().c_str(), 0, 10));
   buf.st_blocks = buf.st_size/512 + buf.st_size%512;
   // If the device is zero'd out, then the listing later is translated to being offline
   buf.st_dev    = 1;

   if (statInfo->ExtendedFormat())
      {buf.st_ctime = static_cast<time_t>(statInfo->GetChangeTime());
       buf.st_atime = static_cast<time_t>(statInfo->GetAccessTime());
      }

   return 0;
}
  
/******************************************************************************/
/* Private:                      m a p C o d e                                */
/******************************************************************************/

int XrdPosixMap::mapCode(int rc)
{
    switch(rc)
       {case XrdCl::errRetry:                return EAGAIN;       // Cl:001
        case XrdCl::errInvalidOp:            return EOPNOTSUPP;   // Cl:003
        case XrdCl::errConfig:               return ENOEXEC;      // Cl:006
        case XrdCl::errInvalidArgs:          return EINVAL;       // Cl:009
        case XrdCl::errInProgress:           return EINPROGRESS;  // Cl:010
        case XrdCl::errNotSupported:         return ENOTSUP;      // Cl:013
        case XrdCl::errDataError:            return EDOM;         // Cl:014
        case XrdCl::errNotImplemented:       return ENOSYS;       // Cl:015
        case XrdCl::errNoMoreReplicas:       return ENOSR;        // Cl:016
        case XrdCl::errInvalidAddr:          return EHOSTUNREACH; // Cl:101
        case XrdCl::errSocketError:          return ENOTSOCK;     // Cl:102
        case XrdCl::errSocketTimeout:        return ETIMEDOUT;    // Cl:103
        case XrdCl::errSocketDisconnected:   return ENOTCONN;     // Cl:104
        case XrdCl::errStreamDisconnect:     return ECONNRESET;   // Cl:107
        case XrdCl::errConnectionError:      return ECONNREFUSED; // Cl:108
        case XrdCl::errInvalidSession:       return ECHRNG;       // Cl:109
        case XrdCl::errTlsError:             return ENETRESET;    // Cl:110
        case XrdCl::errInvalidMessage:       return EPROTO;       // Cl:201
        case XrdCl::errHandShakeFailed:      return EPROTO;       // Cl:202
        case XrdCl::errLoginFailed:          return ECONNABORTED; // Cl:203
        case XrdCl::errAuthFailed:           return EAUTH;        // Cl:204
        case XrdCl::errQueryNotSupported:    return ENOTSUP;      // Cl:205
        case XrdCl::errOperationExpired:     return ETIME;        // Cl:206
        case XrdCl::errOperationInterrupted: return EINTR;        // Cl:207
        case XrdCl::errNoMoreFreeSIDs:       return ENOSR;        // Cl:301
        case XrdCl::errInvalidRedirectURL:   return ESPIPE;       // Cl:302
        case XrdCl::errInvalidResponse:      return EBADMSG;      // Cl:303
        case XrdCl::errNotFound:             return EIDRM;        // Cl:304
        case XrdCl::errCheckSumError:        return EILSEQ;       // Cl:305
        case XrdCl::errRedirectLimit:        return ELOOP;        // Cl:306
        default:                             break;
       }
   return ENOMSG;
}

/******************************************************************************/
/*                           M o d e 2 A c c e s s                            */
/******************************************************************************/
  
XrdCl::Access::Mode XrdPosixMap::Mode2Access(mode_t mode)
{  XrdCl::Access::Mode XMode = XrdCl::Access::None;

// Map the mode
//
   if (mode & S_IRUSR) XMode |= XrdCl::Access::UR;
   if (mode & S_IWUSR) XMode |= XrdCl::Access::UW;
   if (mode & S_IXUSR) XMode |= XrdCl::Access::UX;
   if (mode & S_IRGRP) XMode |= XrdCl::Access::GR;
   if (mode & S_IWGRP) XMode |= XrdCl::Access::GW;
   if (mode & S_IXGRP) XMode |= XrdCl::Access::GX;
   if (mode & S_IROTH) XMode |= XrdCl::Access::OR;
   if (mode & S_IXOTH) XMode |= XrdCl::Access::OX;
   return XMode;
}

/******************************************************************************/
/*                                R e s u l t                                 */
/******************************************************************************/
  
int XrdPosixMap::Result(const XrdCl::XRootDStatus &Status,
                        XrdOucECMsg& ecMsg, bool retneg1)
{
   int eNum;

// If all went well, return success
//
   if (Status.IsOK()) return 0;

// If this is an xrootd error then get the xrootd generated error
//
   if (Status.code == XrdCl::errErrorResponse)
      {ecMsg = Status.GetErrorMessage();
       eNum  = XProtocol::toErrno(Status.errNo);
      } else {
       ecMsg = Status.ToStr();
       eNum  = (Status.errNo ? Status.errNo : mapCode(Status.code));
      }

// Trace this if need be (we supress this for as we really need more info to
// make this messae useful like the opteration and path).
//
// if (eNum != ENOENT && !ecMsg.hasMsg() && Debug)
//    std::cerr <<"XrdPosix: " <<eText <<std::endl;

// Return
//
   ecMsg = errno = eNum;
   return (retneg1 ? -1 : -eNum);
}
