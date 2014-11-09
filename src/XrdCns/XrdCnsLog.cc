/******************************************************************************/
/*                                                                            */
/*                          X r d C n s L o g . c c                           */
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
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "XrdCns/XrdCnsLog.hh"
#include "XrdCns/XrdCnsLogRec.hh"
#include "XrdNet/XrdNetAddr.hh"
#include "XrdOuc/XrdOucTList.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/

       const char  *XrdCnsLog::invFNa = "inv.log";
       const char  *XrdCnsLog::invFNt = "inventory";
       const char  *XrdCnsLog::invFNz = "Inventory";
  
namespace XrdCns
{
extern XrdSysError  MLog;
}

using namespace XrdCns;
  
/******************************************************************************/
/*                                  D i r s                                   */
/******************************************************************************/
  
XrdOucTList *XrdCnsLog::Dirs(const char *Path, int &rc)
{
   XrdOucNSWalk lDir(&MLog, Path, 0, XrdOucNSWalk::retDir
                                    |XrdOucNSWalk::Recurse);
   XrdOucNSWalk::NSEnt *nsP, *ntP;
   XrdOucTList *dList = 0;
   const char *fnP;

// Convert this to a file
//
   if ((fnP = rindex(Path, '/'))) fnP++;
      else fnP = Path;

// If the incomming path is an endpoint then just return it
//
   if (isEP(fnP)) return new XrdOucTList(Path, fnP-Path);

// Now get all of the directories
//
   while((nsP = lDir.Index(rc)))
        do {if (isEP(nsP->File))
               dList = new XrdOucTList(nsP->Path,nsP->File-nsP->Path,dList);
            ntP = nsP; nsP = nsP->Next; delete ntP;
           } while(nsP);

// All done
//
   return dList;
}

/******************************************************************************/
/*                                  L i s t                                   */
/******************************************************************************/
  
XrdOucNSWalk::NSEnt *XrdCnsLog::List(const char *logDir,
                                     XrdOucNSWalk::NSEnt **Base,
                                     int isEP)
{
   XrdOucNSWalk lDir(&MLog, logDir, 0, XrdOucNSWalk::retFile
                                      |XrdOucNSWalk::retStat);
   XrdOucNSWalk::NSEnt *nInv = 0, *nFirst = 0, *sP, *psP, *nsP, *ntP;
   const char *msg, *iFN = (isEP ? invFNz : invFNt);
   int rc;

// Now get all of the files in the directory
//
   nsP = lDir.Index(rc);
   if (rc) return 0;

// Construct list of pending log files
//
   while((ntP = nsP))
        {nsP = nsP->Next;
         if (isEP && *(ntP->File) == '.') {delete ntP; continue;}
         if (!strcmp(iFN, ntP->File) && !nInv && ntP->Stat.st_size)
            {nInv = ntP; continue;}
         rc = atoi(ntP->File+8);
         if (ntP->Stat.st_size == 0
         ||  strncmp("cns.log.", ntP->File, 8)
         ||  rc < 0 || rc >= XrdCnsLogRec::maxClients || *(ntP->File+9) != '.')
            {if (!isEP)
                {msg = (ntP->Stat.st_size ? "Removing improper log file"
                                          : "Removing empty log file");
                 MLog.Emsg("List", msg, ntP->Path);
                 unlink(ntP->Path);
                }
             delete ntP; continue;
            }
         sP = nFirst; psP = 0; ntP->Next = 0; ntP->Stat.st_nlink = rc;
         while(sP && sP->Stat.st_ctime < ntP->Stat.st_ctime)
              {psP = sP; sP = sP->Next;}
         ntP->Next = sP;
         if (psP) psP->Next = ntP;
            else  nFirst    = ntP;
        }

// Return whatever information we have
//
   *Base = nInv;
   return nFirst;
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                                  i s E P                                   */
/******************************************************************************/
  
int XrdCnsLog::isEP(const char *File)
{
   XrdNetAddr tAddr;
   const char *dotP;

// An endpoint must be a valid host name
//
   if (!(dotP = index(File,'.')) || dotP == rindex(File,'.')) return 0;
   return (tAddr.Set(File,0) == 0);
}
