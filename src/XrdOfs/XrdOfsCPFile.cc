/******************************************************************************/
/*                                                                            */
/*                       X r d O f s C h k R e c . c c                        */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <fcntl.h>
#include <cstdio>
#include <cstring>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <vector>

#include "XrdOfs/XrdOfsConfigCP.hh"
#include "XrdOfs/XrdOfsCPFile.hh"
#include "XrdOuc/XrdOucCRC.hh"
#include "XrdOuc/XrdOucIOVec.hh"
#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysXAttr.hh"

#ifndef ENODATA
#define ENODATA ENOATTR
#endif

/******************************************************************************/
/*                     E x t e r n a l   L i n k a g e s                      */
/******************************************************************************/

extern XrdSysXAttr &XrdSysXAttrNative;

#define XATTR XrdSysXAttrNative
  
/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/

namespace
{
struct cUp
{  int         fd;

               cUp() : fd(-1) {}
              ~cUp() {if (fd >= 0) close(fd);}
};

struct cpHdr
{  uint32_t    crc32C;     // CRC32C of all following bytes in header
    int16_t    hdrLen;     // Length of the header
    int16_t    lfnLen;     // Length if lfn including null byte
   uint64_t    fSize;      // Original size of the file
   time_t      mTime;      // Original modification time
   uint64_t    rsvd[3];    // Reserved
   char        srcUrl[8];  // " file://" the lfn follows start at lfn
// char        srcLfn[];   // Appended to this struct of length lfnLen
};

struct cpSeg
{  uint32_t    crc32C;     // CRC32C of all following bytes in segment
    int32_t    dataLen;    // Length of data that follows
      off_t    dataOfs;    // Offset from where the data came and goes
};

static const unsigned int crcSZ   = sizeof(uint32_t);
static const unsigned int hdrSZ   = sizeof(cpHdr);
static const unsigned int segSZ   = sizeof(cpSeg);
static const char *attrName       = "xrdckp_srclfn";
}
  
/******************************************************************************/
/*             C h e c k p o i n t   F i l e   N a m e   D a t a              */
/******************************************************************************/
  
namespace
{

uint32_t InitSeq(char *buff, int n)
{
   uint32_t tod = static_cast<uint32_t>(time(0));
   snprintf(buff, n, "%08x", tod);
   return 1;
}

char        ckpHdr[12];
uint32_t    ckpSeq = InitSeq(ckpHdr, sizeof(ckpHdr));
}
  
/******************************************************************************/
/*     r p I n f o   C o n s t r u c t o r   a n d   D e s t r u c t o r      */
/******************************************************************************/

XrdOfsCPFile::rInfo::rInfo() : srcLFN(0), fSize(0), mTime(0),
                               DataVec(0), DataNum(0), DataLen(0), rBuff(0) {}

XrdOfsCPFile::rInfo::~rInfo()
{   if (DataVec) delete [] DataVec;
    if (rBuff) free(rBuff);
}
  
/******************************************************************************/
/*                  X r d O f s C P F i l e   M e t h o d s                   */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOfsCPFile::XrdOfsCPFile(const char *ckpfn)
             : ckpFN(ckpfn ? strdup(ckpfn) : 0), ckpFD(-1),
               ckpDLen(0), ckpSize(0) {}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdOfsCPFile::~XrdOfsCPFile()
{

// Close the file descriptor if need be
//
   if (ckpFD >= 0) close(ckpFD);
   if (ckpFN) free(ckpFN);
}
  
/******************************************************************************/
/*                                A p p e n d                                 */
/******************************************************************************/
  
int XrdOfsCPFile::Append(const char *data, off_t offset, int dlen)
{
   struct iovec ioV[2];
   cpSeg theSeg;
   int retval;

// Account for the data we will be writing
//
   ckpDLen += dlen;
   ckpSize += dlen + segSZ;

// Construct the next segment
//
   theSeg.dataOfs = offset;
   theSeg.dataLen = dlen;

// Compute checksum of the data and the segment information
//
   theSeg.crc32C = XrdOucCRC::Calc32C(((char *)&theSeg)+crcSZ, segSZ-crcSZ);
   theSeg.crc32C = XrdOucCRC::Calc32C(data, dlen, theSeg.crc32C);

// Construct iovec to write both pieces out
//
   ioV[0].iov_base = &theSeg;
   ioV[0].iov_len  = segSZ;
   ioV[1].iov_base = (void *)data;
   ioV[1].iov_len  = dlen;

// Write the data out
//
   retval = writev(ckpFD, ioV, 2);
   if (retval != (int)(dlen+segSZ)) return (retval < 0 ? -errno : -EIO);

// All done
//
   return 0;
}

/******************************************************************************/
/*                                C r e a t e                                 */
/******************************************************************************/

int XrdOfsCPFile::Create(const char *srcFN, struct stat &Stat)
{
   static const int oFlag = O_CREAT | O_EXCL  | O_WRONLY;
   static const int oMode = S_IRUSR | S_IWUSR | S_IRGRP;
   struct iovec ioV[2];
   cpHdr theHdr;
   int retval, rc = 0;

// Make sure we do not have an active checkpoint here
//
   if (ckpFD >= 0 || ckpFN) return -EEXIST;

// Generate the path to the checkpoint file
//
   ckpFN = genCkpPath();

// Create the checkpoint file and set its attribute
//
   if ((ckpFD = XrdSysFD_Open(ckpFN, oFlag, oMode)) < 0
   ||  XATTR.Set(attrName, srcFN, strlen(srcFN)+1, ckpFN, ckpFD) < 0)
      {rc = -errno;
       if (ckpFD >= 0) {close(ckpFD); ckpFD = -1;}
       unlink(ckpFN);
       free(ckpFN);
       ckpFN = 0;
      }

// Construct the header
//
   theHdr.lfnLen = strlen(srcFN) + 1;
   theHdr.hdrLen = hdrSZ + theHdr.lfnLen;
   theHdr.fSize  = Stat.st_size;
   theHdr.mTime  = Stat.st_mtime;
   memcpy(theHdr.srcUrl, " file://", sizeof(theHdr.srcUrl));
   memset(theHdr.rsvd, 0, sizeof(theHdr.rsvd));

// Generate CRC32C checksum for the header and source filename
//
   theHdr.crc32C = XrdOucCRC::Calc32C(((char *)&theHdr)+crcSZ, hdrSZ-crcSZ);
   theHdr.crc32C = XrdOucCRC::Calc32C(srcFN, theHdr.lfnLen, theHdr.crc32C);

// Construct I/O vector to write out the header
//
   ioV[0].iov_base = &theHdr;
   ioV[0].iov_len  = sizeof(theHdr);
   ioV[1].iov_base = (void *)srcFN;
   ioV[1].iov_len  = theHdr.lfnLen;
   ckpSize = sizeof(theHdr) + theHdr.lfnLen;

// Write out the header and make sure it gets stored
//
   retval = writev(ckpFD, ioV, 2);
   if (retval != ckpSize) rc = (retval < 0 ? -errno : -EIO);
      else if (fsync(ckpFD)) rc = -errno;

// Eliminate the checkpoint file if we encountered any error
//
   if (rc) {if (ftruncate(ckpFD, 0) && unlink(ckpFN)) {}}
   return rc;
}
  
/******************************************************************************/
/*                               D e s t r o y                                */
/******************************************************************************/

int XrdOfsCPFile::Destroy()
{
   int rc;

// Attempt to destroy the checkpoint file
//
   if (ckpFN && unlink(ckpFN))
      {rc = errno;
       if (!truncate(ckpFN, 0) || !ErrState()) rc = 0;
      } else rc = 0;

// All done
//
   return rc;
}
  
/******************************************************************************/
/*                              E r r S t a t e                               */
/******************************************************************************/

int XrdOfsCPFile::ErrState()
{
   char buff[MAXPATHLEN+8];

// Place checkpoint file in error state. If the rename fails, then the
// checkpoint will be applied again which should fail anyway. This just
// tries to avoid that issue and leave a trail.
//
   snprintf(buff, sizeof(buff), "%serr", ckpFN);
   return (rename(ckpFN, buff) ? -errno : 0);
}
  
/******************************************************************************/
/*                                 F N a m e                                  */
/******************************************************************************/

const char *XrdOfsCPFile::FName(bool trim)
{
   if (ckpFN)
      {if (trim)
          {char *slash = rindex(ckpFN, '/');
           if (slash) return slash+1;
          }
       return ckpFN;
      }
   return "???";
}
  
/******************************************************************************/
/* Static Private:            g e n C k p P a t h                             */
/******************************************************************************/
  
char *XrdOfsCPFile::genCkpPath()
{
   static XrdSysMutex mtx;
   char ckpPath[MAXPATHLEN];
   uint32_t seq;

   mtx.Lock(); seq = ckpSeq++; mtx.UnLock();

   snprintf(ckpPath, sizeof(ckpPath), "%s%s-%u.ckp",
                     XrdOfsConfigCP::Path, ckpHdr, seq);
   return strdup(ckpPath);
}

/******************************************************************************/
/* Static Private:             g e t S r c L f n                              */
/******************************************************************************/
  
int XrdOfsCPFile::getSrcLfn(const char *cFN, XrdOfsCPFile::rInfo &rinfo,
                            int fd, int rc)
{
   char srcfn[MAXPATHLEN+80];
   int n;


   if ((n = XATTR.Get(attrName, srcfn, sizeof(srcfn)-1, cFN, fd)) > 0)
      {srcfn[n] = 0;
       if (rinfo.rBuff) free(rinfo.rBuff);
       rinfo.rBuff  = strdup(srcfn);
       rinfo.srcLFN = (const char *)rinfo.rBuff;
      }
   return -rc;
}

/******************************************************************************/
/*                               R e s e r v e                                */
/******************************************************************************/
  
bool XrdOfsCPFile::Reserve(int dlen, int nseg)
{
// Make sure paramenters are valid
//
   if (dlen < 0 || nseg < 0 || ckpFD < 0) return false;

// Calculate the amount of space to reserve
//
   dlen += nseg*segSZ;

// Now allocate the space
//
#ifdef __APPLE__
   fstore_t Store = {F_ALLOCATEALL, F_PEOFPOSMODE, ckpSize, dlen};
   if (fcntl(ckpFD, F_PREALLOCATE, &Store) == -1
   &&  ftruncate(ckpFD, ckpSize + dlen)    == -1) return false;
#else
   if (posix_fallocate(ckpFD, ckpSize, dlen))
      {if (ftruncate(ckpFD, ckpSize)) {}
       return false;
      }
#endif

// All done
//
   return true;
}

/******************************************************************************/
/* Static:                   R e s t o r e I n f o                            */
/******************************************************************************/
  
int XrdOfsCPFile::RestoreInfo(XrdOfsCPFile::rInfo &rinfo, const char *&eWhy)
{
   std::vector<XrdOucIOVec> vecIO;
   struct stat Stat;
   XrdOucIOVec *ioV, ioItem;
   char *ckpRec, *ckpEnd;
   cpSeg theSeg;
   cUp cup;
   int retval;
   bool aOK;

// Open the file
//
   if ((cup.fd = XrdSysFD_Open(ckpFN, O_RDONLY)) < 0)
      {if (errno == ENOENT) return -ENOENT;
       eWhy = "open failed";
       return getSrcLfn(ckpFN, rinfo, cup.fd, errno);
      }

// Get the size of the file
//
   if (fstat(cup.fd, &Stat))
      {eWhy = "stat failed";
       return getSrcLfn(ckpFN, rinfo, cup.fd, errno);
      }

// If this is a zero length file, then it has not been comitted which is OK
//
   if (Stat.st_size == 0) return getSrcLfn(ckpFN, rinfo, cup.fd, ENODATA);

// The file must be at least the basic record size
//
   if (Stat.st_size < hdrSZ+1)
      {eWhy = "truncated header";
       return getSrcLfn(ckpFN, rinfo, cup.fd, EDOM);
      }

// Allocate memory to read the whole file
//
   if (!(ckpRec = (char *)malloc(Stat.st_size)))
      return getSrcLfn(ckpFN, rinfo, cup.fd, ENOMEM);
   rinfo.rBuff = ckpRec;

// Now read the whole file into the buffer
//
   if ((retval = read(cup.fd, ckpRec, Stat.st_size)) != Stat.st_size)
      {eWhy = "read failed";
       return getSrcLfn(ckpFN, rinfo, cup.fd, (retval < 0 ? errno : EIO));
      }

// We have a catch-22 as we need to use the record length to verify the checksum
// but it may have been corrupted. So, we first verify the value is reasonably
// correct relative to the value of the lfn length and the fixed header length.
//
   cpHdr &theHdr = *((cpHdr *)ckpRec);
   if (theHdr.hdrLen > Stat.st_size
   || (theHdr.hdrLen - theHdr.lfnLen) != (int)hdrSZ)
      {eWhy = "corrupted header";
       return getSrcLfn(ckpFN, rinfo, cup.fd, EDOM);
      }

// Verify the header checksum
//
   if (!XrdOucCRC::Ver32C(ckpRec+crcSZ, theHdr.hdrLen-crcSZ, theHdr.crc32C))
      {eWhy = "header checksum mismatch";
       return getSrcLfn(ckpFN, rinfo, cup.fd, EDOM);
      }

// Set the source file name and other information
//
   rinfo.srcLFN = ckpRec+hdrSZ;
   rinfo.fSize  = theHdr.fSize;
   rinfo.mTime  = theHdr.mTime;

// Prepare to verify and record the segments
//
   ckpEnd = ckpRec + Stat.st_size;
   ckpRec = ckpRec + theHdr.hdrLen;
   ioItem.info   = 0;
   vecIO.reserve(16);

// Verify all of the segments
//
   aOK = false; eWhy = 0;
   while(ckpRec+sizeof(cpSeg) < ckpEnd)
        {memcpy(&theSeg, ckpRec, segSZ);
         if (!theSeg.dataLen && !theSeg.dataOfs && !theSeg.crc32C)
            {aOK = true;
             break;
            }
         char *ckpData = ckpRec + segSZ;
         if (theSeg.dataLen <= 0 || ckpData + theSeg.dataLen > ckpEnd) break;
         int cLen = theSeg.dataLen+sizeof(cpSeg)-crcSZ;
         if (!XrdOucCRC::Ver32C(ckpRec+crcSZ, cLen, theSeg.crc32C))
            {eWhy = "data checksum mismatch";
             break;
            }
         ioItem.offset  = theSeg.dataOfs;
         ioItem.size    = theSeg.dataLen;
         ioItem.data    = ckpRec + segSZ;
         rinfo.DataLen += theSeg.dataLen;
         vecIO.push_back(ioItem);
         ckpRec += (segSZ + theSeg.dataLen);
        }

// Check that we ended perfectly (we accept a failed write as long as the
// space was already allocated).
//
   if (!aOK && ckpRec != ckpEnd)
      {if (!eWhy) eWhy = "truncated file";
       return -EDOM;
      }

// If the file had no data changed, return as only the size changed. Otherwise,
// allocate an iovec for all of the segments we need to restore.
//
   if (!vecIO.size()) return 0;
   ioV = new XrdOucIOVec[vecIO.size()];

// Fill in the vector in reverse order as this is the restore sequence
//
   int j = vecIO.size() - 1;
   for (int i = 0; i < (int)vecIO.size(); i++) ioV[j--] = vecIO[i];

// All done
//
   rinfo.DataVec = ioV;
   rinfo.DataNum = vecIO.size();
   return 0;
}
  
/******************************************************************************/
/*                                  S y n c                                   */
/******************************************************************************/

int XrdOfsCPFile::Sync()
{
   if (fsync(ckpFD)) return -errno;
   return 0;
}
  
/******************************************************************************/
/* Static:                        T a r g e t                                 */
/******************************************************************************/

char *XrdOfsCPFile::Target(const char *ckpfn)
{
   struct {cpHdr hdr; char srcfn[MAXPATHLEN+8];} ckpRec;
   cUp cup;
   const char *eMsg = "Target unknown; corrupt checkpoint file";
   int n;

// Try to get the name via the extended attributes first
//
   if ((n = XATTR.Get(attrName,ckpRec.srcfn,sizeof(ckpRec.srcfn)-1,ckpfn)) > 0)
      {ckpRec.srcfn[n] = 0;
       return strdup(ckpRec.srcfn);
      }

// Open the file
//
   if ((cup.fd = XrdSysFD_Open(ckpfn, O_RDONLY)) < 0)
      {char buff[256];
       snprintf(buff, sizeof(buff), "Target unknown; %s", XrdSysE2T(errno));
       return strdup(buff);
      }

// Now read the file header
//
   if ((n = read(cup.fd, &ckpRec, sizeof(ckpRec))) <= (int)sizeof(cpHdr))
      return strdup(eMsg);

// Make sure the length of the lfn is reasonable
//
   if (ckpRec.hdr.lfnLen <= 1 || ckpRec.hdr.lfnLen > (int)MAXPATHLEN)
      return strdup(eMsg);

// Return a copy of the filename
//
   ckpRec.srcfn[ckpRec.hdr.lfnLen-1] = 0;
   return strdup(ckpRec.srcfn);
}
  
/******************************************************************************/
/*                                  U s e d                                   */
/******************************************************************************/
  
int XrdOfsCPFile::Used(int nseg) {return ckpSize + (nseg*segSZ);}
