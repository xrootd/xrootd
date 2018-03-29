/******************************************************************************/
/*                                                                            */
/*                       X r d C k s M a n O s s . c c                        */
/*                                                                            */
/* (c) 2014 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
  
#include "XrdCks/XrdCksCalc.hh"
#include "XrdCks/XrdCksManOss.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSys/XrdSysError.hh"

/******************************************************************************/
/*                         L o c a l   S t a t i c s                          */
/******************************************************************************/
  
namespace
{
XrdOss      *ossP = 0;
int          rdSz = 67108864;
}

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
class LfnPfn
     {public:
      const char *Lfn;
            char  Pfn[MAXPATHLEN+8];

                  LfnPfn(const char *lfn, int &rc) : Lfn(lfn)
                        {rc = ossP->Lfn2Pfn(lfn, Pfn, MAXPATHLEN);
                         if (rc > 0) rc = -rc;
                        }
                 ~LfnPfn() {}
     };

namespace
{
const char *Pfn2Lfn(const char *Lfn)
            {LfnPfn *Xfn = (LfnPfn *)(Lfn-sizeof(const char *));
             return Xfn->Lfn;
            }
}

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdCksManOss::XrdCksManOss(XrdOss *ossX, XrdSysError *erP, int iosz,
                           XrdVersionInfo &vInfo, bool autoload)
             : XrdCksManager(erP, rdSz, vInfo, autoload)
             {if (rdSz <= 65536) rdSz = 67108864;
                 else rdSz = ((rdSz/65536) + (rdSz%65536 != 0)) * 65536;
              eDest = erP;
              ossP  = ossX;
             }

/******************************************************************************/
/*                                  C a l c                                   */
/******************************************************************************/
  
int XrdCksManOss::Calc(const char *Lfn, XrdCksData &Cks, int doSet)
{
   int rc;
   LfnPfn Xfn(Lfn, rc);

// If lfn conversion failed, bail out
//
   if (rc) return rc;

// Return the result
//
   return XrdCksManager::Calc(Xfn.Pfn, Cks, doSet);
}

/******************************************************************************/
  
int XrdCksManOss::Calc(const char *Pfn, time_t &MTime, XrdCksCalc *csP)
{
   class inFile
        {public:
         XrdOssDF *fP;
             inFile() {fP = ossP->newFile("ckscalc");}
            ~inFile() {if (fP) delete fP;}
        } In;
   XrdOucEnv openEnv;
   const char *Lfn = Pfn2Lfn(Pfn);
   struct stat Stat;
   char  *buffP;
   off_t  Offset=0, fileSize;
   size_t ioSize, calcSize;
   int    rc;

// Open the input file
//
   if ((rc = In.fP->Open(Lfn,O_RDONLY,0,openEnv))) return (rc > 0 ? -rc : rc);

// Get the file characteristics
//
   if ((rc = In.fP->Fstat(&Stat))) return (rc > 0 ? -rc : rc);
   if (!(Stat.st_mode & S_IFREG)) return -EPERM;
   calcSize = fileSize = Stat.st_size;
   MTime = Stat.st_mtime;

// Compute read size and allocate a buffer
//
   ioSize = (fileSize < (off_t)rdSz ? fileSize : rdSz); rc = 0;
   buffP  = (char *)malloc(ioSize);
   if (!buffP) return -ENOMEM;

// We now compute checksum 64MB at a time using mmap I/O
//
   while(calcSize)
        {if ((rc= In.fP->Read(buffP, Offset, ioSize)) < 0) break;
         csP->Update(buffP, ioSize);
         calcSize -= ioSize; Offset += ioSize;
         if (calcSize < (size_t)ioSize) ioSize = calcSize;
        }

// Issue error message if we have an error
//
   if (rc < 0) eDest->Emsg("Cks", rc, "read", Pfn);

// Return
//
   return (rc < 0 ? rc : 0);
}

/******************************************************************************/
/*                                   D e l                                    */
/******************************************************************************/

int XrdCksManOss::Del(const char *Lfn, XrdCksData &Cks)
{
   int rc;
   LfnPfn Xfn(Lfn, rc);

// If lfn conversion failed, bail out
//
   if (rc) return rc;

// Delete the attribute and return the result
//
   return XrdCksManager::Del(Xfn.Pfn, Cks);
}

/******************************************************************************/
/*                                   G e t                                    */
/******************************************************************************/

int XrdCksManOss::Get(const char *Lfn, XrdCksData &Cks)
{
   int rc;
   LfnPfn Xfn(Lfn, rc);

// If lfn conversion failed, bail out
//
   if (rc) return rc;

// Return result
//
   return XrdCksManager::Get(Xfn.Pfn, Cks);
}

/******************************************************************************/
/*                                  L i s t                                   */
/******************************************************************************/
  
char *XrdCksManOss::List(const char *Lfn, char *Buff, int Blen, char Sep)
{
   int rc;
   LfnPfn Xfn(Lfn, rc);

// If lfn conversion failed, bail out
//
   if (rc) return 0;

// Simply invoke the base class list
//
   return XrdCksManager::List(Xfn.Pfn, Buff, Blen,Sep);
}

/******************************************************************************/
/*                               M o d T i m e                                */
/******************************************************************************/
  
int XrdCksManOss::ModTime(const char *Pfn, time_t &MTime)
{
   const char *Lfn = Pfn2Lfn(Pfn);
   struct stat Stat;
   int rc;

   if (!(rc = ossP->Stat(Lfn, &Stat))) MTime = Stat.st_mtime;

   return (rc > 0 ? -rc : 0);
}

/******************************************************************************/
/*                                   S e t                                    */
/******************************************************************************/
  
int XrdCksManOss::Set(const char *Lfn, XrdCksData &Cks, int myTime)
{
   int rc;
   LfnPfn Xfn(Lfn, rc);

// If lfn conversion failed, bail out
//
   if (rc) return rc;

// Now set the checksum information in the extended attribute object
//
   return XrdCksManager::Set(Xfn.Pfn, Cks, myTime);
}

/******************************************************************************/
/*                                   V e r                                    */
/******************************************************************************/

int XrdCksManOss::Ver(const char *Lfn, XrdCksData &Cks)
{
   int rc;
   LfnPfn Xfn(Lfn, rc);

// If lfn conversion failed, bail out
//
   if (rc) return rc;

// Return result invoking the base class
//
   return XrdCksManager::Ver(Lfn, Cks);
}
