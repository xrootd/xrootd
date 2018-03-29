/******************************************************************************/
/*                                                                            */
/*                         X r d O s s C o p y . c c                          */
/*                                                                            */
/* (c) 2009 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <utime.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
  
#include "XrdOss/XrdOssCopy.hh"
#include "XrdOss/XrdOssTrace.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFAttr.hh"

/******************************************************************************/
/*                  E r r o r   R o u t i n g   O b j e c t                   */
/******************************************************************************/
  
extern XrdSysError OssEroute;

extern XrdOucTrace OssTrace;

/******************************************************************************/
/* Public:                          C o p y                                   */
/******************************************************************************/
  
off_t XrdOssCopy::Copy(const char *inFn, const char *outFn, int outFD)
{
   static const size_t segSize = 1024*1024;
   class ioFD
        {public:
         int FD;
             ioFD(int fd=-1) : FD(fd) {}
            ~ioFD() {if (FD >= 0) close(FD);}
        } In, Out(outFD);

   struct utimbuf tBuff;
   struct stat buf, bufO, bufSL;
   char *inBuff, *bP;
   off_t  Offset=0, fileSize;
   size_t ioSize, copySize;
   ssize_t rLen;
   int rc;

// Open the input file
//
   if ((In.FD = open(inFn, O_RDONLY)) < 0)
      return -OssEroute.Emsg("Copy", errno, "open", inFn);

// Get the input filesize
//
   if (fstat(In.FD, &buf)) return -OssEroute.Emsg("Copy", errno, "stat", inFn);
   copySize = fileSize = buf.st_size;

// We can dispense with the copy if both files are in the same filesystem.
// Note that the caller must have pre-allocate thed output file. We handle
// avoiding creating a hard link to a symlink instead of the underlying file.
//
   if (fstat(Out.FD, &bufO)) return -OssEroute.Emsg("Copy",errno,"stat",outFn);
   if (buf.st_dev == bufO.st_dev)
      {char lnkBuff[1024+8]; const char *srcFn = inFn; int n;
       if (lstat(inFn, &bufSL)) return -OssEroute.Emsg("Copy",errno,"lstat",inFn);
       if ((bufSL.st_mode & S_IFMT) == S_IFLNK)
          {if ((n = readlink(inFn, lnkBuff, sizeof(lnkBuff)-1)) < 0)
              return -OssEroute.Emsg("Copy", errno, "readlink", inFn);
           lnkBuff[n] = '\0'; srcFn = lnkBuff;
          }
       unlink(outFn);
       if (link(srcFn,outFn)) return -OssEroute.Emsg("Copy",errno,"link",outFn);
       return fileSize;
      }

// We now copy 1MB segments using direct I/O
//
   ioSize = (fileSize < (off_t)segSize ? fileSize : segSize);
   while(copySize)
        {if ((inBuff = (char *)mmap(0, ioSize, PROT_READ, 
                       MAP_NORESERVE|MAP_PRIVATE, In.FD, Offset)) == MAP_FAILED)
            {OssEroute.Emsg("Copy", errno, "memory map", inFn); break;}
         if (Write(outFn, Out.FD, inBuff, ioSize, Offset) < 0) break;
         copySize -= ioSize; Offset += ioSize;
         if (munmap(inBuff, ioSize) < 0)
            {OssEroute.Emsg("Copy", errno, "unmap memory for", inFn); break;}
         if (copySize < segSize) ioSize = copySize;
        }

// check if there was an error and if we can recover

   if (copySize)
   { if ((off_t)copySize != fileSize) return -EIO;
     // Do a traditional copy (note that we didn't copy anything yet)
     OssEroute.Emsg("Copy", "Trying traditional copy for", inFn, "...");
     char ioBuff[segSize];
     off_t rdSize, wrSize = segSize, inOff=0;
     while(copySize)
          {if (copySize < segSize) rdSize = wrSize = copySize;
              else rdSize = segSize;
           bP = ioBuff;
           while(rdSize)
                {do {rLen = pread(In.FD, bP, rdSize, inOff);}
                    while(rLen < 0 && errno == EINTR);
                 if (rLen <= 0) return -OssEroute.Emsg("Copy",
                                      rLen ? errno : ECANCELED, "read", inFn);
                 bP += rLen; rdSize -= rLen; inOff += rLen;
                }
           if ((rc = Write(outFn, Out.FD, ioBuff, wrSize, Offset)) < 0) return rc;
           copySize -= wrSize; Offset += wrSize;
          }
   }

// Copy over any extended attributes
//
   if (XrdSysFAttr::Xat->Copy(inFn, In.FD, outFn, Out.FD)) return -1;

// Now set the time on the file to the original time
//
   tBuff.actime = buf.st_atime;
   tBuff.modtime= buf.st_mtime;
   if (utime(outFn, &tBuff)) 
      OssEroute.Emsg("Copy", errno, "set mtime for", outFn);

// Success
//
   return fileSize;
}

/******************************************************************************/
/* private:                        W r i t e                                  */
/******************************************************************************/
  
int XrdOssCopy::Write(const char *outFn,
                      int oFD, char *Buff, size_t BLen, off_t BOff)
{
   ssize_t wLen;

// Copy out a segment
//
   while(BLen)
        {if ((wLen = pwrite(oFD, Buff, BLen, BOff)) < 0)
            {if (errno == EINTR) continue;
                else break;
            }
         Buff += wLen; BLen -= wLen; BOff += wLen;
        }

// Check for errors
//
   return (BLen ? -OssEroute.Emsg("Copy", errno, "write", outFn) : 0);
}
