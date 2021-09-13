/******************************************************************************/
/*                                                                            */
/*                       X r d O f s C h k P n t . c c                        */
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

#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "XrdOfs/XrdOfsChkPnt.hh"
#include "XrdOfs/XrdOfsConfigCP.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucIOVec.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdSys/XrdSysPthread.hh"

#ifndef ENODATA
#define ENODATA ENOATTR
#endif

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/

namespace
{
struct cUp
      {XrdOssDF *ossP;
       char     *buff;
       int       fd;

             cUp() : ossP(0), buff(0), fd(-1) {}
            ~cUp() {if (ossP) ossP->Close();
                    if (buff) free(buff);
                    if (fd >= 0) close(fd);
                   }
};
}

/******************************************************************************/
/*                         L o c a l   S t a t i c s                          */
/******************************************************************************/

extern XrdSysError OfsEroute;
extern XrdOss     *XrdOfsOss;
  
/******************************************************************************/
/*                                C r e a t e                                 */
/******************************************************************************/

int XrdOfsChkPnt::Create()
{
   struct stat Stat;
   int  rc;

// Make sure we don't have a checkpoint outstanding
//
   if (cpFile.isActive()) return -EEXIST;

// Get the file size
//
   if ((rc = ossFile.Fstat(&Stat))) return rc;
   fSize = Stat.st_size;

// Create the actual checkpoint
//
   if ((rc = cpFile.Create(lFN, Stat)))
      OfsEroute.Emsg("ChkPnt", rc, "create checkpoint for", lFN);

// Return result
//
   OfsEroute.Emsg("ChkPnt", cpFile.FName(true), "checkpoint created for", lFN);
   return rc;
}

/******************************************************************************/
/*                                D e l e t e                                 */
/******************************************************************************/

int XrdOfsChkPnt::Delete()
{
   int rc = 0;

// Delete the checkpoint file if we have one
//
   if (cpFile.isActive() && (rc = cpFile.Destroy()))
      OfsEroute.Emsg("ChkPnt", rc, "delete checkpoint", cpFile.FName());

// All done
//
   return rc;
}

/******************************************************************************/
/* Private:                       F a i l e d                                 */
/******************************************************************************/

int XrdOfsChkPnt::Failed(const char *opn, int eRC, bool *readok)
{
   static const mode_t mRO = S_IRUSR | S_IRGRP;
   const char *eWhat = "still accessible!";
   int  rc;

// Take action
//
   if (lFN)
      {if (XrdOfsConfigCP::cprErrNA)
          {rc = XrdOfsOss->Chmod(lFN, 0);
           if (rc) OfsEroute.Emsg("ChkPnt", rc, "chmod 000", lFN);
              else eWhat = "made inaccessible";
           if (readok) *readok = false;
          } else {
            rc = XrdOfsOss->Chmod(lFN, mRO);
            if (rc) OfsEroute.Emsg("ChkPnt", rc, "chmod r/o", lFN);
               else eWhat = "made read/only";
           if (readok) *readok = true;
          }
      }

// Handle checkpoint file
//
   if ((rc = cpFile.ErrState()))
      OfsEroute.Emsg("ChkPnt", rc, "suspend chkpnt", cpFile.FName());

// Print final messages
//
   if (opn) OfsEroute.Emsg("ChkPnt", eRC, opn, (lFN ? lFN : "\'???\'"));
   if (lFN) OfsEroute.Emsg("ChkPnt", lFN, "restore failed;", eWhat);

// All done
//
   return eRC;
}
  
/******************************************************************************/
/*                                 Q u e r y                                  */
/******************************************************************************/

int XrdOfsChkPnt::Query(struct iov &range)
{
   range.offset = cpUsed;
   range.size   = XrdOfsConfigCP::MaxSZ;
   return 0;
}
  
/******************************************************************************/
/*                               R e s t o r e                                */
/******************************************************************************/

int XrdOfsChkPnt::Restore(bool *readok)
{
   cUp  cup;
   XrdOfsCPFile::rInfo rinfo;
   const char *eWhy = 0;
   int  rc;

// Make sure we have a checkpoint to restore
//
   if (!cpFile.isActive()) return -ENOENT;

// Get the checkpoint information
//
   if ((rc = cpFile.RestoreInfo(rinfo, eWhy)))
      {if (rc == -ENODATA) {Delete(); return 0;}
       XrdOucString eMsg(256);
       eMsg = "process chkpnt (";
       if (eWhy) eMsg.append(eWhy);
       eMsg.append(')');
       OfsEroute.Emsg("ChkPnt", rc, eMsg.c_str(), cpFile.FName());
       lFN = rinfo.srcLFN;
       return Failed(0, rc, readok);
      }


// If we don't have a filename then we neeed to open it
//
   if (!lFN)
      {XrdOucEnv ckpEnv;
       lFN = rinfo.srcLFN;
       rc = ossFile.Open(lFN, O_RDWR, 0, ckpEnv);
       if (rc) return Failed("open", rc, readok);
       cup.ossP = &ossFile;
      }

// Truncate the file to its original size
//
   rc = ossFile.Ftruncate(rinfo.fSize);
   if (rc) return Failed("truncate", rc, readok);

// Write back the original contents of the file. It might not have any.
//
   if (rinfo.DataVec)
      {rc = ossFile.WriteV(rinfo.DataVec, rinfo.DataNum);
       if (rc != rinfo.DataLen)
          return Failed("write", (rc < 0 ? rc : -EIO), readok);
      }

// Sync the data to disk
//
   ossFile.Fsync();

// Set file modification time to the original value.
//
   struct timeval utArg[2];
   utArg[0].tv_sec  = utArg[1].tv_sec  = rinfo.mTime;
   utArg[0].tv_usec = utArg[1].tv_usec = 0;
   rc = ossFile.Fctl(XrdOssDF::Fctl_utimes, sizeof(utArg), (const char *)&utArg);
   if (rc && rc != -ENOTSUP) OfsEroute.Emsg("ChkPnt", rc, "set mtime for", lFN);

// Now we can delete the checkpoint record
//
   if ((rc = Delete()))
      {OfsEroute.Emsg("ChkPnt", rc, "delete chkpnt", cpFile.FName());
       return Failed(0, rc, readok);
      }

// All done
//
   OfsEroute.Emsg("ChkPnt", lFN, "successfully restored.");
   return 0;
}

/******************************************************************************/
/*                              T r u n c a t e                               */
/******************************************************************************/

int XrdOfsChkPnt::Truncate(struct iov *&range)
{
   cUp cup;
   int rc, dlen;

// Make sure we have a checkpoint active
//
   if (!cpFile.isActive()) return -ENOENT;

// Make sure offset is not negative
//
  if (range[0].offset < 0) return -EINVAL;

// Check if we really need to do something here
//
   if (range[0].offset >= fSize) return 0;

// Compute size to save and whether we will exceed our quota
//
   dlen = fSize - range[0].offset;
   if (dlen + cpUsed > XrdOfsConfigCP::MaxSZ) return -EDQUOT;

// Reserve space for all this data
//
   if (!cpFile.Reserve(dlen, 1)) return -ENOSPC;
   cpUsed += dlen;

// Allocate a buffer to read in the data
//
   if (!(cup.buff = (char *)malloc(dlen))) return -ENOMEM;

// Perform checkpoint
//
   rc = ossFile.Read(cup.buff, range[0].offset, dlen);
   if (rc < 0 || (rc && (rc = cpFile.Append(cup.buff, range[0].offset, rc))))
      return rc;

// Set new file size as it s now smaller
//

// Make sure all of it gets on media
//
   if (!(rc = cpFile.Sync())) fSize = range[0].offset;
   return rc;
}
  
/******************************************************************************/
/*                                 W r i t e                                  */
/******************************************************************************/

int XrdOfsChkPnt::Write(struct iov *&range, int rnum)
{
   cUp cup;
   int rc, dlen = 0, buffSZ = 0, numVS = 0, totSZ = 0;

// Make sure we have a checkpoint active
//
   if (!cpFile.isActive()) return -ENOENT;

// Run through the write vector computing what to checkpoint
//
   for (int i = 0; i < rnum; i++)
       {if (range[i].offset < 0) return -EINVAL;
        if (range[i].offset < fSize && range[i].size)
           {if (range[i].size + range[i].offset < fSize) dlen = range[i].size;
               else dlen = fSize - range[i].offset;
            if (dlen > XrdOfsConfigCP::MaxSZ) return -EDQUOT;
            if (dlen > buffSZ) buffSZ = dlen;
            range[i].info = dlen; totSZ += dlen; numVS++;
           } else range[i].info = 0;
       }

// If nothing to checkpoint, simply return
//
   if (!buffSZ) return 0;

// Check if we will exceed our quota with this checkpoint
//
   if (dlen + cpUsed > XrdOfsConfigCP::MaxSZ) return -EDQUOT;

// Allocate a buffer to read in the data
//
   if (!(cup.buff = (char *)malloc(buffSZ))) return -ENOMEM;

// Reserve space for all this data
//
   if (!cpFile.Reserve(dlen, numVS)) return -ENOSPC;
   cpUsed += dlen;

// Perform checkpoint
//
   for (int i = 0; i < rnum; i++)
       {if (range[i].info)
           {rc = ossFile.Read(cup.buff, range[i].offset, range[i].info);
            if (rc < 0
            || (rc && (rc = cpFile.Append(cup.buff, range[i].offset, rc))))
               return rc;
           }
       }

// Make sure all of it gets on media
//
   return cpFile.Sync();
}
