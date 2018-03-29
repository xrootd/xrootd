/******************************************************************************/
/*                                                                            */
/*                          X r d F r m C n s . c c                           */
/*                                                                            */
/* (c) 2011 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <poll.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

#include "XrdFrc/XrdFrcTrace.hh"
#include "XrdFrm/XrdFrmCns.hh"
#include "XrdFrm/XrdFrmConfig.hh"
#include "XrdOuc/XrdOucSxeq.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysTimer.hh"

using namespace XrdFrc;
using namespace XrdFrm;
  
/******************************************************************************/
/*                      S t a t i c   V a r i a b l e s                       */
/******************************************************************************/
  
char        *XrdFrmCns::cnsPath   =  0;
char        *XrdFrmCns::cnsHdr[2] = {0, 0};
int          XrdFrmCns::cnsHdrLen =  0;
int          XrdFrmCns::cnsFD     = -1;
int          XrdFrmCns::cnsMode   = XrdFrmCns::cnsIgnore;
int          XrdFrmCns::cnsInit   =  1;

/******************************************************************************/
/*                                   A d d                                    */
/******************************************************************************/
  
void XrdFrmCns::Add(const char *tID,  const char *Path,
                    long long   Size, mode_t      Mode)
{
   static const int mMask = S_IRWXU|S_IRWXG|S_IRWXO;
   static char NewLine = '\n';
   struct iovec iov[9];
   char mBuff[8], sBuff[24];

// Check if there is a cns here and we should initialize it
//
   if (!cnsMode) return;
   if (cnsInit && !Init())
      {Say.Emsg("FrmCns", "Auto-ignore cnsd create", Path); return;}

// Fill out the io vector
//
   iov[0].iov_base = (char *)tID;
   iov[0].iov_len  = strlen(tID);
   iov[1].iov_base = (char *)" create ";
   iov[1].iov_len  = 8;
   iov[2].iov_base = mBuff;
   iov[2].iov_len  = sprintf(mBuff, "%3o ", Mode&mMask);
   iov[3].iov_base = (char *)Path;
   iov[3].iov_len  = strlen(Path);
   iov[4].iov_base = &NewLine;
   iov[4].iov_len  = 1;
   iov[5]          = iov[0];
   iov[6].iov_base = (char *)" closew ";
   iov[6].iov_len  = 8;
   iov[7]          = iov[3];
   iov[8].iov_base = sBuff;
   iov[8].iov_len  = sprintf(sBuff, " %lld\n", Size);

// Send this off to the cnsd
//
   if (!Send2Cnsd(iov, 9)) Say.Emsg("FrmCns", "Auto-ignore cnsd create", Path);
}

/******************************************************************************/
/*                                   D e l                                    */
/******************************************************************************/
  
void XrdFrmCns::Del(const char *Path, int HdrType, int islfn)
{
   static char NewLine = '\n';
   struct iovec iov[] = {{cnsHdr[HdrType],(size_t)cnsHdrLen},{0,0},{&NewLine,1}};
   char buff[MAXPATHLEN];

// Check if we should initialize
//
   if (cnsInit && !Init())
      {Say.Emsg("FrmCns", "Auto-ignore cnsd remove", Path); return;}

// In most cases, del gets a pfn. We need to translate to an lfn.
//
   if (islfn)
      {iov[1].iov_base = (char *)Path;
       iov[1].iov_len  = strlen(Path);
      } else if (!Config.LogicalPath(Path, buff, sizeof(buff))) return;
                else {iov[1].iov_base = buff;
                      iov[1].iov_len  = strlen(buff);
                     }

// Send this off to the cnsd
//
   if (!Send2Cnsd(iov, 3)) Say.Emsg("FrmCns", "Auto-ignore cnsd remove", Path);
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
int XrdFrmCns::Init()
{
   static const int  oMode= O_WRONLY | O_NONBLOCK | O_NDELAY;
   static XrdSysMutex initMutex;
   int rc, pMsg = 0;

// Check if we really need to re-initialize
//
   initMutex.Lock();
   if (!cnsInit) {initMutex.UnLock(); return 1;}

// Open the events FIFO. It might not exists or we might not be able to write
//
   while((cnsFD = open(cnsPath, oMode)) < 0 && Retry(errno, pMsg)) {}

// Check how we ended
//
   cnsInit = (cnsFD < 0 ? 1 : 0);
   rc = !cnsInit;

// All done
//
   initMutex.UnLock();
   return rc;
}

/******************************************************************************/
  
int XrdFrmCns::Init(const char *aPath, int Opts)
{
   int rc;

   if (aPath && (rc = setPath(aPath, 0))) return rc;
   cnsMode = Opts;
   return 0;
}

/******************************************************************************/
  
int XrdFrmCns::Init(const char *myID, const char *aPath, const char *iName)
{
   char buff[2048];
   int rc;

// If we are ignoring the cns then don't bother with this
//
   if (!cnsMode) return 0;

// Construct the path to he cns events file (we know buff is large enough)
//
   if (!cnsPath && (rc = setPath(aPath, iName))) return rc;

// Create a static headers for deletes
//
   cnsHdrLen = sprintf(buff, "%s.%d.0@localhost rmdir ", myID, getpid());
   cnsHdr[HdrRmd] = strdup(buff);
               sprintf(buff, "%s.%d.0@localhost rm    ", myID, getpid());
   cnsHdr[HdrRmf] = strdup(buff);

// All done
//
   return 0;
}

/******************************************************************************/
/*                                 R e t r y                                  */
/******************************************************************************/

int XrdFrmCns::Retry(int eNum, int &pMsg)
{
   static const char *eAct = (cnsMode > 0 ? "Waiting for" : "Ignoring");
   static const int   Yawn = 10, Blurt = 6;

// Always retry interrupted calls (these rarely happen, if ever)
//
   if (eNum == EINTR) return 1;

// Issue message as needed
//
   if (eNum == ENOENT || eNum == EAGAIN || eNum == ENXIO || eNum == EPIPE)
      {if (!(pMsg++%Blurt)) Say.Emsg("FrmCns", eAct, "cnsd on path", cnsPath);}
      else Say.Emsg("FrmCns", errno, "notify cnsd via", cnsPath);

// Check if we should sleep and retry or simply ignore this
//
   if (cnsMode <= 0) return 0;
   XrdSysTimer::Snooze(Yawn);
   return 1;
}
  
/******************************************************************************/
/*                             S e n d 2 C n s d                              */
/******************************************************************************/
  
int XrdFrmCns::Send2Cnsd(struct iovec *iov, int iovn)
{
   int rc, pMsg = 0;
  
// Normally, writes will be atomic if we don't exceed PIPE_BUF, but this is
// not gauranteed when using vector writes. Plus, on some platforms, PIPE_BUF
// is way too small (i.e. 512 bytes). So, we just lock the file.
//
   XrdOucSxeq::Serialize(cnsFD, 0);

// Now write the data
//
   while((rc = writev(cnsFD, iov, iovn)) < 0 && Retry(errno, pMsg)) {}

// Unlock the file
//
   XrdOucSxeq::Release(cnsFD);

// All done
//
   return rc > 0;
}

/******************************************************************************/
/*                               s e t P a t h                                */
/******************************************************************************/

int XrdFrmCns::setPath(const char *aPath, const char *iName)
{
   static const char *sfx = "XrdCnsd.events";
   struct stat Stat;
   char buff[2048], *pP;

// Release any cnspath we have
//
   if (cnsPath) {free(cnsPath); cnsPath = 0;}

// Generate a new one and make sure it is usable
//
   pP = XrdOucUtils::genPath(aPath, iName, "cns");
   if (strlen(pP) + strlen(sfx) >= (int)sizeof(buff))
      {Say.Emsg("FrmCns", "Invalid cnsd apath", aPath); free(pP); return 1;}
   strcpy(buff, pP); free(pP);
   strcat(buff, "XrdCnsd.events");
   if (stat(buff, &Stat) && errno != ENOENT)
      {Say.Emsg("FrmCns", errno, "use cnsd file", buff); return 1;}
   cnsPath = strdup(buff);
   return 0;
}
