/******************************************************************************/
/*                                                                            */
/*                        X r d O f s P o s c q . c c                         */
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

#include <cstring>
#include <strings.h>
#include <stddef.h>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "XrdOfs/XrdOfsPoscq.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdSfs/XrdSfsFlags.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysPlatform.hh"

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdOfsPoscq::XrdOfsPoscq(XrdSysError *erp, XrdOss *oss, const char *fn, int sv)
{
   eDest = erp;
   ossFS = oss;
   pocFN = strdup(fn);
   pocFD = -1;
   pocSZ = 0;
   pocIQ = 0;
   SlotList = SlotLust = 0;

   if (sv > 32767) sv = 32767;
      else if (sv < 0) sv = 0;
   pocWS = pocSV = sv-1;
}
  
/******************************************************************************/
/*                                   A d d                                    */
/******************************************************************************/
  
int XrdOfsPoscq::Add(const char *Tident, const char *Lfn, bool isNew)
{
   XrdSysMutexHelper myHelp(myMutex);
   std::map<std::string,int>::iterator it = pqMap.end();
   XrdOfsPoscq::Request tmpReq;
   struct stat Stat;
   FileSlot *freeSlot;
   int fP;

// Add is only called when file is to be created. Therefore, it must not exist
// unless it is being replaced typically due to a retry. If not being replaced
// then We need to check this to avoid deleting already created files.
// Otherwise, we need to see if the file is already in the queue to avoid it
// being deleted after the fact because it would be in the queue twice.
//
   if (!ossFS->Stat(Lfn, &Stat))
      {if (isNew) return -EEXIST;
       it = pqMap.find(std::string(Lfn));
       if (it != pqMap.end() && VerOffset(Lfn, it->second)) return it->second;
      }

// Construct the request
//
   tmpReq.addT = 0;
   strlcpy(tmpReq.LFN,  Lfn,    sizeof(tmpReq.LFN));
   strlcpy(tmpReq.User, Tident, sizeof(tmpReq.User));
   memset(tmpReq.Reserved, 0, sizeof(tmpReq.Reserved));

// Obtain a free slot
//
   if ((freeSlot = SlotList))
      {fP = freeSlot->Offset;
       SlotList = freeSlot->Next;
       freeSlot->Next = SlotLust;
       SlotLust = freeSlot;
      } else {fP = pocSZ; pocSZ += ReqSize;}
   pocIQ++;

// Write out the record
//
   if (!reqWrite((void *)&tmpReq, sizeof(tmpReq), fP))
      {eDest->Emsg("Add", Lfn, "not added to the persist queue.");
       myMutex.Lock(); pocIQ--; myMutex.UnLock();
       return -EIO;
      }

// Check if we update the map or simply add it to the map
//
   if (it != pqMap.end()) it->second = fP;
      else pqMap[std::string(Lfn)] = fP;

// Return the record offset
//
   return fP;
}
  
/******************************************************************************/
/*                                C o m m i t                                 */
/******************************************************************************/

int XrdOfsPoscq::Commit(const char *Lfn, int Offset)
{
   long long addT = static_cast<long long>(time(0));

// Verify the offset it must be correct
//
   if (!VerOffset(Lfn, Offset)) return -EINVAL;

// Indicate the record is free
//
   if (!reqWrite((void *)&addT, sizeof(addT), Offset))
      {eDest->Emsg("Commit", Lfn, "not committed to the persist queue.");
       return -EIO;
      }

// Remove entry from the map and return
//
   myMutex.Lock();
   pqMap.erase(std::string(Lfn));
   myMutex.UnLock();
   return 0;
}

/******************************************************************************/
/*                                   D e l                                    */
/******************************************************************************/

int XrdOfsPoscq::Del(const char *Lfn, int Offset, int Unlink)
{
   static int Zero = 0;
   FileSlot *freeSlot;
   int retc;

// Verify the offset it must be correct
//
   if (!VerOffset(Lfn, Offset)) return -EINVAL;

// Unlink the file if need be
//
   if (Unlink && (retc = ossFS->Unlink(Lfn)) && retc != -ENOENT)
      {eDest->Emsg("Del", retc, "remove", Lfn);
       return (retc < 0 ? retc : -retc);
      }

// Indicate the record is free
//
   if (!reqWrite((void *)&Zero, sizeof(Zero), Offset+offsetof(Request,LFN)))
      {eDest->Emsg("Del", Lfn, "not removed from the persist queue.");
       return -EIO;
      }

// Serialize and place this on the free queue
//
   myMutex.Lock();
   if ((freeSlot = SlotLust)) SlotLust = freeSlot->Next;
      else freeSlot = new FileSlot;
   freeSlot->Offset = Offset;
   freeSlot->Next   = SlotList;
   SlotList         = freeSlot;
   if (pocIQ > 0) pocIQ--;

// Remove item from the map
//
   pqMap.erase(std::string(Lfn));
   myMutex.UnLock();

// All done
//
   return 0;
}
  
/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/

XrdOfsPoscq::recEnt *XrdOfsPoscq::Init(int &Ok)
{
   static const int Mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
   Request     tmpReq;
   struct stat buf, Stat;
   recEnt     *First = 0;
   char        Buff[80];
   int         rc, Offs, numreq = 0;

// Assume we will fail
//
   Ok = 0;

// Open the file first in r/w mode
//
   if ((pocFD = XrdSysFD_Open(pocFN, O_RDWR|O_CREAT, Mode)) < 0)
      {eDest->Emsg("Init",errno,"open",pocFN);
       return 0;
      }

// Get file status
//
   if (fstat(pocFD, &buf)) {FailIni("stat"); return 0;}

// Check for a new file here
//
   if (buf.st_size < ReqSize)
      {pocSZ = ReqOffs;
       if (ftruncate(pocFD, ReqOffs)) FailIni("trunc");
          else Ok = 1;
       return 0;
      }

// Read the full file
//
   for (Offs = ReqOffs; Offs < buf.st_size; Offs += ReqSize)
       {do {rc = pread(pocFD, (void *)&tmpReq, ReqSize, Offs);}
           while(rc < 0 && errno == EINTR);
        if (rc < 0) {eDest->Emsg("Init",errno,"read",pocFN); return First;}
        if (*tmpReq.LFN == '\0'
        ||  ossFS->Stat(tmpReq.LFN, &Stat)
        ||  !(S_ISREG(Stat.st_mode) || !(Stat.st_mode & XRDSFS_POSCPEND))) continue;
        First = new recEnt(tmpReq, Stat.st_mode & S_IAMB, First); numreq++;
       }

// Now write out the file and return
//
   sprintf(Buff, " %d pending create%s", numreq, (numreq != 1 ? "s" : ""));
   eDest->Say("Init", Buff, " recovered from ", pocFN);
   if (ReWrite(First)) Ok = 1;
   return First;
}
  
/******************************************************************************/
/*                                  L i s t                                   */
/******************************************************************************/
  
XrdOfsPoscq::recEnt *XrdOfsPoscq::List(XrdSysError *Say, const char *theFN)
{
   XrdOfsPoscq::Request tmpReq;
   struct stat buf;
   recEnt *First = 0;
   int    rc, theFD, Offs;

// Open the file first in r/o mode
//
   if ((theFD = XrdSysFD_Open(theFN, O_RDONLY)) < 0)
      {Say->Emsg("Init",errno,"open",theFN);
       return 0;
      }

// Get file status
//
   if (fstat(theFD, &buf))
      {Say->Emsg("Init",errno,"stat",theFN);
       close(theFD);
       return 0;
      }
   if (buf.st_size < ReqSize) buf.st_size = 0;

// Read the full file
//
   for (Offs = ReqOffs; Offs < buf.st_size; Offs += ReqSize)
       {do {rc = pread(theFD, (void *)&tmpReq, ReqSize, Offs);}
           while(rc < 0 && errno == EINTR);
        if (rc < 0) {Say->Emsg("List",errno,"read",theFN);
                     close(theFD); return First;
                    }
        if (*tmpReq.LFN != '\0') First = new recEnt(tmpReq, 0, First);
       }

// All done
//
   close(theFD);
   return First;
}

/******************************************************************************/
/*                               F a i l I n i                                */
/******************************************************************************/

void XrdOfsPoscq::FailIni(const char *txt)
{
   eDest->Emsg("Init", errno, txt, pocFN);
}

/******************************************************************************/
/*                              r e q W r i t e                               */
/******************************************************************************/
  
bool XrdOfsPoscq::reqWrite(void *Buff, int Bsz, int Offs)
{
   int rc = 0;

   do {rc = pwrite(pocFD, Buff, Bsz, Offs);} while(rc < 0 && errno == EINTR);

   if (rc >= 0 && Bsz > 8)
      {if (!pocWS) {pocWS = pocSV; rc = fsync(pocFD);}
          else pocWS--;
      }

   if (rc < 0) {eDest->Emsg("reqWrite",errno,"write", pocFN); return false;}
   return true;
}

/******************************************************************************/
/*                               R e W r i t e                                */
/******************************************************************************/
  
bool XrdOfsPoscq::ReWrite(XrdOfsPoscq::recEnt *rP)
{
   static const int Mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
   char newFN[MAXPATHLEN], *oldFN;
   int  newFD, oldFD, Offs = ReqOffs;
   bool aOK = true;

// Construct new file and open it
//
   strcpy(newFN, pocFN); strcat(newFN, ".new");
   if ((newFD = XrdSysFD_Open(newFN, O_RDWR|O_CREAT|O_TRUNC, Mode)) < 0)
      {eDest->Emsg("ReWrite",errno,"open",newFN); return false;}

// Setup to write/swap the file
//
   oldFD = pocFD; pocFD = newFD;
   oldFN = pocFN; pocFN = newFN;

// Rewrite all records if we have any
//
   while(rP)
        {rP->Offset = Offs;
         if (!reqWrite((void *)&rP->reqData, ReqSize, Offs))
            {aOK = false; break;}
         pqMap[std::string(rP->reqData.LFN)] = Offs;
         Offs += ReqSize;
         rP = rP->Next;
        }

// If all went well, rename the file
//
   if (aOK && rename(newFN, oldFN) < 0)
      {eDest->Emsg("ReWrite",errno,"rename",newFN); aOK = false;}

// Perform post processing
//
   if (aOK)  close(oldFD);
      else  {close(newFD); pocFD = oldFD;}
   pocFN = oldFN;
   pocSZ = Offs;
   return aOK;
}

/******************************************************************************/
/*                             V e r O f f s e t                              */
/******************************************************************************/
  
bool XrdOfsPoscq::VerOffset(const char *Lfn, int Offset)
{

// Verify the offset
//
   if (Offset < ReqOffs || (Offset-ReqOffs)%ReqSize)
      {char buff[128];
       sprintf(buff, "Invalid slot %d for", Offset);
       eDest->Emsg("VerOffset", buff, Lfn);
       return false;
      }
   return true;
}
