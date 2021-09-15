/******************************************************************************/
/*                                                                            */
/*                      X r d P o s i x A d m i n . c c                       */
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
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "XrdNet/XrdNetAddr.hh"
#include "XrdPosix/XrdPosixAdmin.hh"
#include "XrdPosix/XrdPosixMap.hh"
  
/******************************************************************************/
/*                                F a n O u t                                 */
/******************************************************************************/
  
XrdCl::URL *XrdPosixAdmin::FanOut(int &num)
{
   XrdCl::XRootDStatus            xStatus;
   XrdCl::LocationInfo           *info = 0;
   XrdCl::LocationInfo::Iterator  it;
   XrdCl::URL                    *uVec;
   XrdNetAddr netLoc;
   const char *hName;
   int i;

// Make sure admin is ok
//
   if (!isOK()) return 0;

// Issue the deep locate and verify that all went well
//
   xStatus = Xrd.DeepLocate(Url.GetPathWithParams(),XrdCl::OpenFlags::None,info);
   if (!xStatus.IsOK())
      {num = XrdPosixMap::Result(xStatus, false);
       return 0;
      }

// Allocate an array large enough to hold this information
//
   if (!(i = info->GetSize())) {delete info; return 0;}
   uVec = new XrdCl::URL[i];

// Now start filling out the array
//
   num = 0;
   for( it = info->Begin(); it != info->End(); ++it )
      {if (!netLoc.Set(it->GetAddress().c_str()) && (hName = netLoc.Name()))
          {std::string hString(hName);
           uVec[num] = Url;
           uVec[num].SetHostName(hString);
           uVec[num].SetPort(netLoc.Port());
           num++;
          }
      }

// Make sure we can return something;
//
   delete info;
   if (!num) {delete [] uVec; return 0;}
   return uVec;
}
  
/******************************************************************************/
/*                                 Q u e r y                                  */
/******************************************************************************/

int XrdPosixAdmin::Query(XrdCl::QueryCode::Code reqCode, void *buff, int bsz)
{
  XrdCl::Buffer reqBuff, *rspBuff = 0;

// Make sure we are OK
//
  if (!isOK()) return -1;

// Get argument
//
   reqBuff.FromString(Url.GetPathWithParams());

// Issue the query
//
   if (!XrdPosixMap::Result(Xrd.Query(reqCode, reqBuff, rspBuff)))
      {uint32_t rspSz = rspBuff->GetSize();
       char *rspbuff = rspBuff->GetBuffer();
       if (rspbuff && rspSz)
          {// if the string is null-terminated decrement the size
           if ( !(rspbuff[rspSz - 1]) ) --rspSz;
           if (bsz >= (int)rspSz + 1)
              {strncpy((char *)buff, rspbuff, rspSz);
               ((char*)buff)[rspSz] = 0; // make sure it is null-terminated
               delete rspBuff;
               return static_cast<int>(rspSz + 1);
              } else errno = ERANGE;
          } else errno = EFAULT;
      }

// Return error
//
   delete rspBuff;
   return -1;
}
  
/******************************************************************************/
/*                                  S t a t                                   */
/******************************************************************************/
  
bool XrdPosixAdmin::Stat(mode_t *flags, time_t *mtime)
{
   XrdCl::XRootDStatus xStatus;
   XrdCl::StatInfo    *sInfo = 0;

// Make sure admin is ok
//
   if (!isOK()) return false;

// Issue the stat and verify that all went well
//
   xStatus = Xrd.Stat(Url.GetPathWithParams(), sInfo);
   if (!xStatus.IsOK())
      {XrdPosixMap::Result(xStatus);
       delete sInfo;
       return false;
      }

// Return wanted data
//
   if (flags) *flags = XrdPosixMap::Flags2Mode(0, sInfo->GetFlags());
   if (mtime) *mtime = static_cast<time_t>(sInfo->GetModTime());

// Delete our status information and return final result
//
   delete sInfo;
   return true;
}

/******************************************************************************/
  
bool XrdPosixAdmin::Stat(struct stat &Stat)
{
   XrdCl::XRootDStatus xStatus;
   XrdCl::StatInfo    *sInfo = 0;

// Make sure admin is ok
//
   if (!isOK()) return false;

// Issue the stat and verify that all went well
//
   xStatus = Xrd.Stat(Url.GetPathWithParams(), sInfo);
   if (!xStatus.IsOK())
      {XrdPosixMap::Result(xStatus);
       delete sInfo;
       return false;
      }

// Return the data
//
   Stat.st_size   = static_cast<size_t>(sInfo->GetSize());
   Stat.st_blocks = Stat.st_size/512 + Stat.st_size%512;
   Stat.st_ino    = static_cast<ino_t>(strtoll(sInfo->GetId().c_str(), 0, 10));
#if defined(__mips__) && _MIPS_SIM == _ABIO32
   // Work around inconsistent type definitions on MIPS
   // The st_rdev field in struct stat (which is 32 bits) is not type
   // dev_t (which is 64 bits)
   dev_t tmp_rdev = Stat.st_rdev;
   Stat.st_mode   = XrdPosixMap::Flags2Mode(&tmp_rdev, sInfo->GetFlags());
   Stat.st_rdev = tmp_rdev;
#else
   Stat.st_mode   = XrdPosixMap::Flags2Mode(&Stat.st_rdev, sInfo->GetFlags());
#endif
   Stat.st_mtime  = static_cast<time_t>(sInfo->GetModTime());

   if (sInfo->ExtendedFormat())
      {Stat.st_ctime = static_cast<time_t>(sInfo->GetChangeTime());
       Stat.st_atime = static_cast<time_t>(sInfo->GetAccessTime());
      } else {
       Stat.st_ctime = Stat.st_mtime;
       Stat.st_atime = time(0);
      }

// Delete our status information and return final result
//
   delete sInfo;
   return true;
}
