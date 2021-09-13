/******************************************************************************/
/*                                                                            */
/*                    X r d S f s I n t e r f a c e . h h                     */
/*                                                                            */
/* (c) 2019 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cassert>
#include <cstdio>
#include <arpa/inet.h>

#include "XrdOuc/XrdOucPgrwUtils.hh"
#include "XrdSfs/XrdSfsAio.hh"
#include "XrdSfs/XrdSfsFlags.hh"
#include "XrdSfs/XrdSfsInterface.hh"

/******************************************************************************/
/*       X r d S f s D i r e c t o r y   M e t h o d   D e f a u l t s        */
/******************************************************************************/
/******************************************************************************/
/*                              a u t o s t a t                               */
/******************************************************************************/

int XrdSfsDirectory::autoStat(struct stat *buf)
{
   (void)buf;
   error.setErrInfo(ENOTSUP, "Not supported.");
   return SFS_ERROR;
}
  
/******************************************************************************/
/*            X r d S f s F i l e   M e t h o d   D e f a u l t s             */
/******************************************************************************/
/******************************************************************************/
/*                            c h e c k p o i n t                             */
/******************************************************************************/
  
int XrdSfsFile::checkpoint(cpAct act, struct iov *range, int n)
{
// Provide reasonable answers
//
   switch(act)
         {case cpCreate:  error.setErrInfo(EDQUOT,"Checkpoint quota exceeded.");
                          break;
          case cpDelete:
          case cpRestore: error.setErrInfo(ENOENT,"Checkpoint does not exist.");
                          break;
          default:        error.setErrInfo(EINVAL,"Invalid checkpoint request.");
                          break;
         }
   return SFS_ERROR;
}

/******************************************************************************/
/*                                  f c t l                                   */
/******************************************************************************/
  
int XrdSfsFile::fctl(const int           cmd,
                           int           alen,
                     const char         *args,
                     const XrdSecEntity *client)
{
  (void)cmd; (void)alen; (void)args; (void)client;
  return SFS_OK;
}

/******************************************************************************/
/*                                p g R e a d                                 */
/******************************************************************************/
  
XrdSfsXferSize XrdSfsFile::pgRead(XrdSfsFileOffset   offset,
                                  char              *buffer,
                                  XrdSfsXferSize     rdlen,
                                  uint32_t          *csvec,
                                  uint64_t           opts)
{
   XrdSfsXferSize bytes;

// Read the data into the buffer
//
   if ((bytes = read(offset, buffer, rdlen)) <= 0) return bytes;

// Generate the crc's.
//
   XrdOucPgrwUtils::csCalc(buffer, offset, bytes, csvec);

// All done
//
   return bytes;
}

/******************************************************************************/
  
XrdSfsXferSize XrdSfsFile::pgRead(XrdSfsAio *aioparm, uint64_t opts)
{
   aioparm->Result = this->pgRead((XrdSfsFileOffset)aioparm->sfsAio.aio_offset,
                                            (char *)aioparm->sfsAio.aio_buf,
                                    (XrdSfsXferSize)aioparm->sfsAio.aio_nbytes,
                                                    aioparm->cksVec, opts);
   aioparm->doneRead();
   return SFS_OK;
}

/******************************************************************************/
/*                               p g W r i t e                                */
/******************************************************************************/
  
XrdSfsXferSize XrdSfsFile::pgWrite(XrdSfsFileOffset   offset,
                                   char              *buffer,
                                   XrdSfsXferSize     wrlen,
                                   uint32_t          *csvec,
                                   uint64_t           opts)
{

// If we have a checksum vector and verify is on, do verification.
//
   if (opts & Verify)
      {XrdOucPgrwUtils::dataInfo dInfo(buffer, csvec, offset, wrlen);
       off_t badoff;
       int   badlen;

       if (!XrdOucPgrwUtils::csVer(dInfo, badoff, badlen))
          {char eMsg[512];
           snprintf(eMsg, sizeof(eMsg), "Checksum error at offset %lld.", (long long) badoff);
           error.setErrInfo(EDOM, eMsg);
           return SFS_ERROR;
          }
      }

// Now just return the result of a plain write
//
   return write(offset, buffer, wrlen);
}

/******************************************************************************/
  
XrdSfsXferSize XrdSfsFile::pgWrite(XrdSfsAio *aioparm, uint64_t opts)
{
   aioparm->Result = this->pgWrite((XrdSfsFileOffset)aioparm->sfsAio.aio_offset,
                                             (char *)aioparm->sfsAio.aio_buf,
                                     (XrdSfsXferSize)aioparm->sfsAio.aio_nbytes,
                                                     aioparm->cksVec, opts);
   aioparm->doneWrite();
   return SFS_OK;
}

/******************************************************************************/
/*                                 r e a d v                                  */
/******************************************************************************/
  
XrdSfsXferSize XrdSfsFile::readv(XrdOucIOVec      *readV,
                                 int               rdvCnt)
{
   XrdSfsXferSize rdsz, totbytes = 0;

   for (int i = 0; i < rdvCnt; i++)
       {rdsz = read(readV[i].offset,
                    readV[i].data, readV[i].size);
        if (rdsz != readV[i].size)
           {if (rdsz < 0) return rdsz;
            error.setErrInfo(ESPIPE,"read past eof");
            return SFS_ERROR;
           }
        totbytes += rdsz;
       }
   return totbytes;
}

/******************************************************************************/
/*                              S e n d D a t a                               */
/******************************************************************************/
  
int XrdSfsFile::SendData(XrdSfsDio         *sfDio,
                         XrdSfsFileOffset   offset,
                         XrdSfsXferSize     size)
{
   (void)sfDio; (void)offset; (void)size;
   return SFS_OK;
}

/******************************************************************************/
/*                                w r i t e v                                 */
/******************************************************************************/
  
XrdSfsXferSize XrdSfsFile::writev(XrdOucIOVec      *writeV,
                                  int               wdvCnt)
{
    XrdSfsXferSize wrsz, totbytes = 0;

    for (int i = 0; i < wdvCnt; i++)
        {wrsz = write(writeV[i].offset,
                      writeV[i].data, writeV[i].size);
         if (wrsz != writeV[i].size)
            {if (wrsz < 0) return wrsz;
            error.setErrInfo(ESPIPE,"write past eof");
            return SFS_ERROR;
           }
        totbytes += wrsz;
       }
   return totbytes;
}

/******************************************************************************/
/*      X r d S f s F i l e S y s t e m   M e t h o d   D e f a u l t s       */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdSfsFileSystem::XrdSfsFileSystem()
{
   FeatureSet = XrdSfs::hasPGRW;
   if (getChkPSize() > 0) FeatureSet |= XrdSfs::hasCHKP;
}
  
/******************************************************************************/
/*                                c h k s u m                                 */
/******************************************************************************/
  
int XrdSfsFileSystem::chksum(      csFunc            Func,
                             const char             *csName,
                             const char             *path,
                                   XrdOucErrInfo    &eInfo,
                             const XrdSecEntity     *client,
                             const char             *opaque)
{
  (void)Func; (void)csName; (void)path; (void)eInfo; (void)client;
  (void)opaque;

  eInfo.setErrInfo(ENOTSUP, "Not supported.");
  return SFS_ERROR;
}
  
/******************************************************************************/
/*                                 F A t t r                                  */
/******************************************************************************/
  
int XrdSfsFileSystem::FAttr(      XrdSfsFACtl      *faReq,
                                  XrdOucErrInfo    &eInfo,
                            const XrdSecEntity     *client)
{
   (void)faReq; (void)client;

   eInfo.setErrInfo(ENOTSUP, "Not supported.");
   return SFS_ERROR;
}
  
/******************************************************************************/
/*                                 F S c t l                                  */
/******************************************************************************/
  
int XrdSfsFileSystem::FSctl(const int               cmd,
                                  XrdSfsFSctl      &args,
                                  XrdOucErrInfo    &eInfo,
                            const XrdSecEntity     *client)
{
  (void)cmd; (void)args; (void)eInfo; (void)client;

  return SFS_OK;
}

/******************************************************************************/
/*                                g p F i l e                                 */
/******************************************************************************/
  
int XrdSfsFileSystem::gpFile(      gpfFunc          &gpAct,
                                   XrdSfsGPFile     &gpReq,
                                   XrdOucErrInfo    &eInfo,
                             const XrdSecEntity     *client)
{
   (void)gpAct, (void)gpReq; (void)client;

   eInfo.setErrInfo(ENOTSUP, "Not supported.");
   return SFS_ERROR;
}
