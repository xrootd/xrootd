/******************************************************************************/
/*                                                                            */
/*                      X r d P o s i x E x t r a . c c                       */
/*                                                                            */
/*                                                                            */
/* (c) 2021 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
/* Modified by Frank Winklmeier to add the full Posix file system definition. */
/******************************************************************************/

#include <cerrno>

#include "XrdOuc/XrdOucCache.hh"
#include "XrdOuc/XrdOucPgrwUtils.hh"
#include "XrdPosix/XrdPosixCallBack.hh"
#include "XrdPosix/XrdPosixExtra.hh"
#include "XrdPosix/XrdPosixFile.hh"

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
namespace XrdPosixGlobals
{
extern XrdOucCache *theCache;
}

using namespace XrdPosixGlobals;

/******************************************************************************/
/*                                  F c t l                                   */
/******************************************************************************/
  
int XrdPosixExtra::Fctl(int fildes, XrdOucCacheOp::Code opc,
                        const std::string& args, std::string& resp)
{
   XrdPosixFile *fp;

// Execute the request (this handles simple cases)
//
   switch(opc)
         {case XrdOucCacheOp::Code::QFinfo:
               if (!(fp = XrdPosixObject::File(fildes)))
                  {resp = "Invalid file descriptor.";
                   return -1;
                  }
               return fp->XCio->Fcntl(XrdOucCacheOp::Code::QFinfo, args, resp);
          case XrdOucCacheOp::Code::QFSinfo:
               if (fildes >= 0) {errno = EINVAL; return -1;}
               if (XrdPosixGlobals::theCache)
                  {auto cmd = XrdOucCacheOp::Code::QFSinfo;
                   return XrdPosixGlobals::theCache->Fcntl(cmd, args, resp);
                  }
                [[fallthrough]];
          default:
               errno = ENOTSUP;   
               break;
         }

// Determine the problem
//
   if (errno == ENOTSUP) resp = "Operation not supported.";
   return -1;
}

/******************************************************************************/
/*                                p g R e a d                                 */
/******************************************************************************/
  
ssize_t XrdPosixExtra::pgRead (int   fildes, void*    buffer,
                               off_t offset, size_t   rdlen,
                               std::vector<uint32_t>& csvec,
                               uint64_t               opts,
                               XrdPosixCallBackIO*    cbp)
{
   XrdPosixFile *fp;
   long long     offs, bytes;
   uint64_t      fOpts;
   int           iosz;

// Find the file object
//
   if (!(fp = XrdPosixObject::File(fildes)))
      {if (!cbp) return -1;
       cbp->Complete(-1);
       return 0;
      }

// Make sure the size is not too large
//
   if (rdlen > (size_t)0x7fffffff)
      {fp->UnLock();
       errno = EOVERFLOW;
       if (!cbp) return -1;
       cbp->Complete(-1);
       return 0;
      }

// Get the parameters
//
   iosz = static_cast<int>(rdlen);
   offs = static_cast<long long>(offset);
   csvec.clear();
   fOpts= (opts & forceCS ? XrdOucCacheIO::forceCS : 0);

// Issue the read in the sync case
//
   if (!cbp)
      {bytes = fp->XCio->pgRead((char *)buffer, offs, (int)iosz, csvec, fOpts);
       fp->UnLock();
       return (ssize_t)bytes;
      }

// Handle the read in the async case
//
   cbp->theFile = fp;
   fp->Ref(); fp->UnLock();

// Issue the read
//
   fp->XCio->pgRead(*cbp, (char *)buffer, offs, (int)iosz, csvec, fOpts);
   return 0;
}

/******************************************************************************/
/*                               p g W r i t e                                */
/******************************************************************************/
  
ssize_t XrdPosixExtra::pgWrite(int   fildes, void*    buffer,
                               off_t offset, size_t   wrlen,
                               std::vector<uint32_t>& csvec,
                               uint64_t               opts,
                               XrdPosixCallBackIO*    cbp)
{
   XrdPosixFile *fp;
   long long     offs;
   int           iosz, bytes;

// Find the file object
//
   if (!(fp = XrdPosixObject::File(fildes)))
      {if (!cbp) return -1;
       cbp->Complete(-1);
       return 0;
      }

// Make sure the size is not too large
//
   if (wrlen > (size_t)0x7fffffff)
      {fp->UnLock();
       errno = EOVERFLOW;
       if (!cbp) return -1;
       cbp->Complete(-1);
       return 0;
      }

// Check if we need to generate checksums or verify that we have the right num.
//
   if (csvec.size() == 0)
      XrdOucPgrwUtils::csCalc((const char *)buffer, offset, wrlen, csvec);
      else if (XrdOucPgrwUtils::csNum(offset, wrlen) != (int)csvec.size())
              {fp->UnLock();
               errno = EINVAL;
               if (!cbp) return -1;
               cbp->Complete(-1);
               return 0;
              }

// Get the parameters
//
   iosz = static_cast<int>(wrlen);
   offs = static_cast<long long>(offset);

// Sync: Issue the write
//
   if (!cbp)
      {bytes = fp->XCio->pgWrite((char *)buffer, offs, (int)iosz, csvec);
       fp->UpdtSize(offs + iosz);
       fp->UnLock();
       return (ssize_t)bytes;
      }

// Async: Prepare for writing
//
   cbp->theFile = fp;
   fp->Ref(); fp->UnLock();

// Issue the write
//
   fp->XCio->pgWrite(*cbp, (char *)buffer, offs, (int)iosz, csvec);
   return 0;
}
