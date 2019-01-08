/******************************************************************************/
/*                                                                            */
/*                      X r d X r o o t d F i l e . c c                       */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
  
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdXrootd/XrdXrootdFile.hh"
#include "XrdXrootd/XrdXrootdFileLock.hh"
#include "XrdXrootd/XrdXrootdMonFile.hh"
#include "XrdXrootd/XrdXrootdMonitor.hh"
#define  TRACELINK this
#include "XrdXrootd/XrdXrootdTrace.hh"
 
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

#ifndef NODEBUG  
extern XrdOucTrace      *XrdXrootdTrace;
#endif

       XrdXrootdFileLock *XrdXrootdFile::Locker;

       int              XrdXrootdFile::sfOK         = 1;
       const char      *XrdXrootdFile::TraceID      = "File";
       const char      *XrdXrootdFileTable::TraceID = "FileTable";
       const char      *XrdXrootdFileTable::ID      = "";

namespace
{
             XrdSysError   *eDest;

static const unsigned long  heldSpotV = 1UL;;

static       XrdXrootdFile *heldSpotP = (XrdXrootdFile *)1;

static const unsigned long  heldMask = ~1UL;
}

/******************************************************************************/
/*                        x r d _ F i l e   C l a s s                         */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdXrootdFile::XrdXrootdFile(const char *id, const char *path, XrdSfsFile *fp,
                             char mode, bool async, int sfok, struct stat *sP)
{
    static XrdSysMutex seqMutex;
    struct stat buf;
    off_t mmSize;

    XrdSfsp  = fp;
    FileKey  = strdup(path);
    mmAddr   = 0;
    FileMode = mode;
    AsyncMode= (async ? 1 : 0);
    fhProc   = 0;
    ID       = id;

    Stats.Init();

// Get the file descriptor number (none if not a regular file)
//
   if (fp->fctl(SFS_FCTL_GETFD, 0, fp->error) != SFS_OK) fdNum = -1;
      else fdNum = fp->error.getErrInfo();
   sfEnabled = (sfOK && sfok && (fdNum >= 0||fdNum==(int)SFS_SFIO_FDVAL) ? 1:0);

// Determine if file is memory mapped
//
   if (fp->getMmap((void **)&mmAddr, mmSize) != SFS_OK) isMMapped = 0;
      else {isMMapped = (mmSize ? 1 : 0);
            Stats.fSize = static_cast<long long>(mmSize);
           }

// Get file status information (we need it) and optionally return it to caller
//
   if (sP || !isMMapped)
      {if (!sP) sP = &buf;
       fp->stat(sP);
       if (!isMMapped) Stats.fSize = static_cast<long long>(sP->st_size);
      }
}
  
/******************************************************************************/
/*                                                                            */
/*                                  I n i t                                   */
/******************************************************************************/
  
void XrdXrootdFile::Init(XrdXrootdFileLock *lp, XrdSysError *erP, int sfok)
{
   Locker = lp;
   eDest  = erP;
   sfOK   = sfok;
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdXrootdFile::~XrdXrootdFile()
{

   if (XrdSfsp)
      {TRACEI(FS, "closing " <<FileMode <<' ' <<FileKey);
       delete XrdSfsp;
       XrdSfsp = 0;
       Locker->Unlock(FileKey, FileMode);
      }

   if (fhProc) fhProc->Avail(fHandle);

   if (FileKey) free(FileKey);
}

/******************************************************************************/
/*                   x r d _ F i l e T a b l e   C l a s s                    */
/******************************************************************************/
/******************************************************************************/
/*                                   A d d                                    */
/******************************************************************************/
  
int XrdXrootdFileTable::Add(XrdXrootdFile *fp)
{
   const int allocsz = XRD_FTABSIZE*sizeof(fp);
   XrdXrootdFile **newXTab, **oldXTab;
   int i;

// If we have a file handle processor, see if it can give us a file handle
// that's already in our table.
//
   if (fhProc && (i = fhProc->Get()) >= 0) 
      {XrdXrootdFile **fP;
       if (i < XRD_FTABSIZE)   fP = &FTab[i];
          else {i -= XRD_FTABSIZE;
                if (XTab && i < XTnum) fP = &XTab[i];
                   else fP = 0;
               }
       if (fP && *fP == heldSpotP)
          {*fP = fp;
           TRACEI(FS, "reusing fh " <<i <<" for " <<fp->FileKey);
           return i;
          }
       char fhn[32];
       snprintf(fhn, sizeof(fhn), "%d", i);
       eDest->Emsg("FTab_Add", "Invalid recycled fHandle",fhn,"ignored.");
      }

// Find a free spot in the internal table
//
   for (i = FTfree; i < XRD_FTABSIZE; i++) if (!FTab[i]) break;

   if (i < XRD_FTABSIZE)
      {FTab[i] = fp; FTfree = i+1; return i;}

// Allocate an external table if we do not have one
//
   if (!XTab)
      {if (!(XTab = (XrdXrootdFile **)malloc(allocsz))) return -1;
       memset((void *)XTab, 0, allocsz);
       XTnum   = XRD_FTABSIZE;
       XTfree  = 1;
       XTab[0] = fp;
       return XRD_FTABSIZE;
      }

// Find a free spot in the external table
//
   for (i = XTfree; i < XTnum; i++) if (!XTab[i]) break;
   if (i < XTnum)
      {XTab[i] = fp; XTfree = i+1; return i+XRD_FTABSIZE;}

// Extend the table
//
   if (!(newXTab = (XrdXrootdFile **)malloc(XTnum*sizeof(XrdXrootdFile *)+allocsz)))
      return -1;
   memcpy((void *)newXTab, (const void *)XTab, XTnum*sizeof(XrdXrootdFile *));
   memset((void *)(newXTab+XTnum), 0, allocsz);
   oldXTab = XTab;
   XTab = newXTab;
   XTab[XTnum] = fp;
   i = XTnum;
   XTfree = XTnum+1;
   XTnum += XRD_FTABSIZE;
   free(oldXTab);
   return i+XRD_FTABSIZE;
}
 
/******************************************************************************/
/*                                   D e l                                    */
/******************************************************************************/
  
XrdXrootdFile *XrdXrootdFileTable::Del(XrdXrootdMonitor *monP, int fnum,
                                       bool dodel)
{
   union {XrdXrootdFile *fp; unsigned long fv;};
   XrdXrootdFile *repVal = (dodel ? 0 : heldSpotP);
   int  fh = fnum;

   if (fnum < XRD_FTABSIZE) 
      {fp = FTab[fnum];
       FTab[fnum] = repVal;
       if (fnum < FTfree) FTfree = fnum;
      } else {
       fnum -= XRD_FTABSIZE;
       if (XTab && fnum < XTnum)
          {fp = XTab[fnum];
           XTab[fnum] = repVal;
           if (fnum < XTfree) XTfree = fnum;
          }
           else fp = 0;
      }

   fv &= heldMask;

   if (fp)
      {XrdXrootdFileStats &Stats = fp->Stats;

       if (monP) monP->Close(Stats.FileID,
                             Stats.xfr.read + Stats.xfr.readv,
                             Stats.xfr.write);
       if (Stats.MonEnt != -1) XrdXrootdMonFile::Close(&Stats, false);
       if (dodel) {delete fp; fp = 0;}  // Will do the close
          else {if (!fhProc) fhProc = new XrdXrootdFileHP;
                   else fhProc->Ref();
                fp->fHandle = fh;
                fp->fhProc  = fhProc;
                TRACEI(FS, "defer fh " <<fh <<" del for " <<fp->FileKey);
               }
      }
   return fp;
}

/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/
  
// WARNING! The object subject to this method must be serialized. There can
// be no active requests on link associated with this object at the time the
// destructor is called. The same restrictions apply to Add() and Del().
//
void XrdXrootdFileTable::Recycle(XrdXrootdMonitor *monP)
{
   int i;

// Delete all objects from the internal table (see warning)
//
   FTfree = 0;
   for (i = 0; i < XRD_FTABSIZE; i++)
       if (FTab[i] && FTab[i] != heldSpotP)
          {XrdXrootdFileStats &Stats = FTab[i]->Stats;
           if (monP) monP->Close(Stats.FileID,
                                 Stats.xfr.read+Stats.xfr.readv,
                                 Stats.xfr.write);
           if (Stats.MonEnt != -1) XrdXrootdMonFile::Close(&Stats, true);
           delete FTab[i]; FTab[i] = 0;
          }

// Delete all objects from the external table (see warning)
//
if (XTab)
  {for (i = 0; i < XTnum; i++)
      {if (XTab[i] && XTab[i] != heldSpotP)
          {XrdXrootdFileStats &Stats = XTab[i]->Stats;
           if (monP) monP->Close(Stats.FileID,
                                 Stats.xfr.read+Stats.xfr.readv,
                                 Stats.xfr.write);
           if (Stats.MonEnt != -1) XrdXrootdMonFile::Close(&Stats, true);
           delete XTab[i];
          }
       }
   free(XTab); XTab = 0; XTnum = 0; XTfree = 0;
  }

// If we have a filehandle processor, delete it. Note that it will stay alive
// until all requests for file handles against it are resolved.
//
   if (fhProc) fhProc->Delete();

// Delete this object
//
   delete this;
}
  
/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                               b i n 2 h e x                                */
/******************************************************************************/
  
int XrdXrootdFile::bin2hex(char *outbuff, char *inbuff, int inlen)
{
    static char hv[] = "0123456789abcdef";
    int i, j = 0;

// Skip leading zeroes
//
    for (i = 0; i < inlen; i++) if (inbuff[i]) break;
    if (i >= inlen)
       {outbuff[0] = '0'; outbuff[1] = '\0'; return 1;}

// Format the data
//
    for (     ; i < inlen; i++)
       {outbuff[j++] = hv[(inbuff[i] >> 4) & 0x0f];
        outbuff[j++] = hv[ inbuff[i]       & 0x0f];
       }
     outbuff[j] = '\0';
     return j;
}
