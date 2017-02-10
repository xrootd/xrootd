/******************************************************************************/
/*                                                                            */
/*                      X r d F r c R e q F i l e . c c                       */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "XrdFrc/XrdFrcCID.hh"
#include "XrdFrc/XrdFrcReqFile.hh"
#include "XrdFrc/XrdFrcTrace.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysPlatform.hh"

using namespace XrdFrc;

/******************************************************************************/
/*                      S t a t i c   V a r i a b l e s                       */
/******************************************************************************/
  
XrdSysMutex XrdFrcReqFile::rqMonitor::rqMutex;

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdFrcReqFile::XrdFrcReqFile(const char *fn, int aVal)
{
   char buff[1200];

   memset((void *)&HdrData, 0, sizeof(HdrData));
   reqFN = strdup(fn);
   strcpy(buff, fn); strcat(buff, ".lock");
   lokFN = strdup(buff);
   lokFD = reqFD = -1;
   isAgent = aVal;
}
  
/******************************************************************************/
/*                                   A d d                                    */
/******************************************************************************/
  
void XrdFrcReqFile::Add(XrdFrcRequest *rP)
{
   rqMonitor rqMon(isAgent);
   XrdFrcRequest tmpReq;
   int fP;

// Lock the file
//
   if (!FileLock()) {FailAdd(rP->LFN, 0); return;}

// Obtain a free slot
//
   if ((fP = HdrData.Free))
      {if (!reqRead((void *)&tmpReq, fP)) {FailAdd(rP->LFN, 1); return;}
       HdrData.Free = tmpReq.Next;
      } else {
       struct stat buf;
       if (fstat(reqFD, &buf))
          {Say.Emsg("Add",errno,"stat",reqFN); FailAdd(rP->LFN, 1); return;}
       fP = buf.st_size;
      }

// Chain in the request (registration requests go fifo)
//
   if (rP->Options & XrdFrcRequest::Register)
      {if (!(rP->Next = HdrData.First)) HdrData.Last = fP;
       HdrData.First = fP;
      } else {
       if (HdrData.First && HdrData.Last)
          {if (!reqRead((void *)&tmpReq, HdrData.Last))
              {FailAdd(rP->LFN, 1); return;}
           tmpReq.Next = fP;
           if (!reqWrite((void *)&tmpReq, HdrData.Last, 0))
              {FailAdd(rP->LFN, 1); return;}
          } else HdrData.First = fP;
       HdrData.Last = fP; rP->Next = 0;
      }

// Write out the file
//
   rP->This = fP;
   if (!reqWrite(rP, fP)) FailAdd(rP->LFN, 0);
   FileLock(lkNone);
}
  
/******************************************************************************/
/*                                   C a n                                    */
/******************************************************************************/

void XrdFrcReqFile::Can(XrdFrcRequest *rP)
{
   rqMonitor rqMon(isAgent);
   XrdFrcRequest tmpReq;
   int Offs, numCan = 0, numBad = 0;
   struct stat buf;
   char txt[128];

// Lock the file and get its size
//
   if (!FileLock() || fstat(reqFD, &buf)) {FailCan(rP->ID, 0); return;}

// Run through all of the file entries removing matching requests
//
   for (Offs = ReqSize; Offs < buf.st_size; Offs += ReqSize)
       {if (!reqRead((void *)&tmpReq, Offs)) return FailCan(rP->ID);
        if (!strcmp(tmpReq.ID, rP->ID))
           {tmpReq.LFN[0] = '\0';
            if (!reqWrite((void *)&tmpReq, Offs, 0)) numBad++;
               else numCan++;
           }
       }

// Make sure this is written to disk
//
   if (numCan) fsync(reqFD);

// Document the action
//
   if (numCan || numBad)
      {sprintf(txt, "has %d entries; %d removed (%d failures).",
                    numCan+numBad, numCan, numBad);
       Say.Emsg("Can", rP->ID, txt);
      }
   FileLock(lkNone);
}
  
/******************************************************************************/
/*                                   D e l                                    */
/******************************************************************************/

void XrdFrcReqFile::Del(XrdFrcRequest *rP)
{
   rqMonitor rqMon(isAgent);
   XrdFrcRequest tmpReq;

// Lock the file
//
   if (!FileLock()) {FailDel(rP->LFN, 0); return;}

// Put entry on the free chain
//
   memset(&tmpReq, 0, sizeof(tmpReq));
   tmpReq.Next  = HdrData.Free;
   HdrData.Free = rP->This;
   if (!reqWrite((void *)&tmpReq, rP->This)) FailDel(rP->LFN, 0);
   FileLock(lkNone);
}

/******************************************************************************/
/*                                   G e t                                    */
/******************************************************************************/
  
int XrdFrcReqFile::Get(XrdFrcRequest *rP)
{
   int fP, rc;

// Lock the file
//
   if (!FileLock()) return 0;

// Get the next request
//
   while((fP = HdrData.First))
        {if (!reqRead((void *)rP, fP)) {FileLock(lkNone); return 0;}
         HdrData.First= rP->Next;
         if (*(rP->LFN)) {reqWrite(0,0,1); break;}
         rP->Next     = HdrData.Free;
         HdrData.Free = fP;
         if (!reqWrite(rP, fP)) {fP = 0; break;}
      }
   if (fP) rc = (HdrData.First ? 1 : -1);
      else rc = 0;
   FileLock(lkNone);
   return rc;
}
  
/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/

int XrdFrcReqFile::Init()
{
   EPNAME("Init");
   static const int Mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
   XrdFrcRequest tmpReq;
   struct stat buf;
   recEnt *RegList = 0, *First = 0, *rP, *pP, *tP;
   int    Offs, rc, numreq = 0;

// Open the lock file first in r/w mode
//
   if ((lokFD = XrdSysFD_Open(lokFN, O_RDWR|O_CREAT, Mode)) < 0)
      {Say.Emsg("Init",errno,"open",lokFN); return 0;}

// Obtain a lock
//
   if (!FileLock(lkInit)) return 0;

// Open the file first in r/w mode
//
   if ((reqFD = XrdSysFD_Open(reqFN, O_RDWR|O_CREAT, Mode)) < 0)
      {FileLock(lkNone);
       Say.Emsg("Init",errno,"open",reqFN); 
       return 0;
      }

// Check for a new file here
//
   if (fstat(reqFD, &buf)) return FailIni("stat");
   if (buf.st_size < ReqSize)
      {memset(&tmpReq, 0, sizeof(tmpReq));
       HdrData.Free = ReqSize;
       if (!reqWrite((void *)&tmpReq, ReqSize)) return FailIni("init file");
       FileLock(lkNone);
       return 1;
      }

// We are done if this is a agent
//
   if (isAgent)
      {FileLock(lkNone);
       return 1;
      }

// Read the full file
//
   for (Offs = ReqSize; Offs < buf.st_size; Offs += ReqSize)
       {if (!reqRead((void *)&tmpReq, Offs)) return FailIni("read file");
        if (*tmpReq.LFN == '\0' || !tmpReq.addTOD
        ||  tmpReq.Opaque >= int(sizeof(tmpReq.LFN))) continue;
        pP = 0; rP = First; tP = new recEnt(tmpReq); numreq++;
        if (tmpReq.Options & XrdFrcRequest::Register)
           {tP->Next = RegList; RegList = tP;
           } else {
            while(rP && rP->reqData.addTOD < tmpReq.addTOD) {pP=rP;rP=rP->Next;}
            if (pP) pP->Next = tP;
               else First    = tP;
            tP->Next = rP;
           }
       }

// Plase registration requests in the front
//
   while((rP = RegList))
        {RegList = rP->Next;
         rP->Next = First;
         First = rP;
        }

// Now write out the file
//
   DEBUG(numreq <<" request(s) recovered from " <<reqFN);
   rc = ReWrite(First);

// Delete all the entries in memory while referencing known instance names
//
   while((tP = First)) 
        {First = tP->Next;
         CID.Ref(tP->reqData.iName);
         delete tP;
        }

// All done
//
   FileLock(lkNone);
   return rc;
}
  
/******************************************************************************/
/*                                  L i s t                                   */
/******************************************************************************/
  
char  *XrdFrcReqFile::List(char *Buff, int bsz, int &Offs,
                           XrdFrcRequest::Item *ITList, int ITNum)
{
   rqMonitor rqMon(isAgent);
   XrdFrcRequest tmpReq;
   int rc;

// Set Offs argument
//
   if (Offs < ReqSize) Offs = ReqSize;

// Lock the file
//
   if (!FileLock(lkShare)) return 0;

// Return next valid filename
//
   do{do {rc = pread(reqFD, (void *)&tmpReq, ReqSize, Offs);}
          while(rc < 0 && errno == EINTR);
      if (rc == ReqSize)
         {Offs += ReqSize;
          if (*tmpReq.LFN == '\0' || !tmpReq.addTOD
          ||  tmpReq.Opaque >= int(sizeof(tmpReq.LFN))
          ||  tmpReq.Options & XrdFrcRequest::Register) continue;
          FileLock(lkNone);
          if (!ITNum || !ITList) strlcpy(Buff, tmpReq.LFN, bsz);
             else ListL(tmpReq, Buff, bsz, ITList, ITNum);
          return Buff;
         }
     } while(rc == ReqSize);

// Diagnose ending condition
//
   if (rc < 0) Say.Emsg("List",errno,"read",reqFN);

// Return end of list
//
   FileLock(lkNone);
   return 0;
}

/******************************************************************************/
/*                                 L i s t L                                  */
/******************************************************************************/
  
void XrdFrcReqFile::ListL(XrdFrcRequest &tmpReq, char *Buff, int bsz,
                          XrdFrcRequest::Item *ITList, int ITNum)
{
   char What, tbuf[32];
   long long tval;
   int i, k, n, bln = bsz-2, Lfo;

   for (i = 0; i < ITNum && bln > 0; i++)
       {Lfo = tmpReq.LFO;
        switch(ITList[i])
              {case XrdFrcRequest::getOBJ:
                    Lfo = 0;
                    n = strlen(tmpReq.LFN);
                    strlcpy(Buff, tmpReq.LFN, bln);
                    break;

               case XrdFrcRequest::getLFN:     
                    n = strlen(tmpReq.LFN+Lfo);
                    strlcpy(Buff, tmpReq.LFN+Lfo, bln);
                    break;

               case XrdFrcRequest::getOBJCGI:
                    Lfo = 0;
                    n = strlen(tmpReq.LFN); tmpReq.LFN[n] = '?';
                    if (!tmpReq.Opaque) tmpReq.LFN[n+1] = '\0';
                    strlcpy(Buff, tmpReq.LFN, bln);
                    k = strlen(tmpReq.LFN);
                    tmpReq.LFN[n] = '\0'; n = k;
                    break;

               case XrdFrcRequest::getLFNCGI:
                    n = strlen(tmpReq.LFN); tmpReq.LFN[n] = '?';
                    if (!tmpReq.Opaque) tmpReq.LFN[n+1] = '\0';
                    strlcpy(Buff, tmpReq.LFN+Lfo, bln);
                    k = strlen(tmpReq.LFN+Lfo);
                    tmpReq.LFN[n] = '\0'; n = k;
                    break;

               case XrdFrcRequest::getMODE:
                    n = 0;
                    What = (tmpReq.Options & XrdFrcRequest::makeRW
                         ? 'w' : 'r');
                    if (bln) {Buff[n] = What; n++;}
                    if (tmpReq.Options & XrdFrcRequest::msgFail)
                    if (bln-n > 0) {Buff[n] = 'f'; n++;}
                    if (tmpReq.Options & XrdFrcRequest::msgSucc)
                    if (bln-n > 0) {Buff[n] = 'n'; n++;}
                    break;

               case XrdFrcRequest::getNOTE:
                    n = strlen(tmpReq.Notify);
                    strlcpy(Buff, tmpReq.Notify, bln);
                    break;

               case XrdFrcRequest::getOP:
                    *Buff     = tmpReq.OPc;
                    n = 1;
                    break;

               case XrdFrcRequest::getPRTY:
                    if (tmpReq.Prty == 2) What = '2';
                       else if (tmpReq.Prty == 1) What = '1';
                               else What = '0';
                    n = 1;
                    if (bln) *Buff = What;
                    break;

               case XrdFrcRequest::getQWT:
               case XrdFrcRequest::getTOD:
                    tval = tmpReq.addTOD;
                    if (ITList[i] == XrdFrcRequest::getQWT) tval = time(0)-tval;
                    if ((n = sprintf(tbuf, "%lld", tval)) >= 0)
                       strlcpy(Buff, tbuf, bln);
                    break;

               case XrdFrcRequest::getRID:
                    n = strlen(tmpReq.ID);
                    strlcpy(Buff, tmpReq.ID, bln);
                    break;

               case XrdFrcRequest::getUSER:
                    n = strlen(tmpReq.User);
                    strlcpy(Buff, tmpReq.User, bln);
                    break;

               default: n = 0; break;
              }
        if (bln > 0) {bln -= n; Buff += n;}
        if (bln > 0) {*Buff++ = ' '; bln--;}
       }
   *Buff = '\0';
}

/******************************************************************************/
/*                               F a i l A d d                                */
/******************************************************************************/
  
void XrdFrcReqFile::FailAdd(char *lfn, int unlk)
{
   Say.Emsg("Add", lfn, "not added to prestage queue.");
   if (unlk) FileLock(lkNone);
}
  
/******************************************************************************/
/*                               F a i l C a n                                */
/******************************************************************************/
  
void XrdFrcReqFile::FailCan(char *rid, int unlk)
{
   Say.Emsg("Can", rid, "request not removed from prestage queue.");
   if (unlk) FileLock(lkNone);
}
  
/******************************************************************************/
/*                               F a i l D e l                                */
/******************************************************************************/
  
void XrdFrcReqFile::FailDel(char *lfn, int unlk)
{
   Say.Emsg("Del", lfn, "not removed from prestage queue.");
   if (unlk) FileLock(lkNone);
}

/******************************************************************************/
/*                               F a i l I n i                                */
/******************************************************************************/

int XrdFrcReqFile::FailIni(const char *txt)
{
   Say.Emsg("Init", errno, txt, reqFN);
   FileLock(lkNone);
   return 0;
}
  
/******************************************************************************/
/*                              F i l e L o c k                               */
/******************************************************************************/
  
int XrdFrcReqFile::FileLock(LockType lktype)
{
   FLOCK_t lock_args;
   const char *What;
   int rc;

// Establish locking options
//
   memset(&lock_args, 0, sizeof(lock_args));
   lock_args.l_whence = SEEK_SET;
   if (lktype == lkNone)
      {lock_args.l_type = F_UNLCK; What = "unlock";
       if (isAgent && reqFD >= 0) {close(reqFD); reqFD = -1;}
      }
      else {lock_args.l_type = (lktype == lkShare ? F_RDLCK : F_WRLCK);
            What = "lock";
            flMutex.Lock();
           }

// Perform action.
//
   do {rc = fcntl(lokFD,F_SETLKW,&lock_args);}
       while(rc < 0 && errno == EINTR);
   if (rc < 0) {Say.Emsg("FileLock", errno, What , lokFN); return 0;}

// Refresh the header
//
   if (lktype == lkExcl || lktype == lkShare)
      {if (reqFD < 0 && (reqFD = XrdSysFD_Open(reqFN, O_RDWR)) < 0)
          {Say.Emsg("FileLock",errno,"open",reqFN);
           FileLock(lkNone);
           return 0;
          }
       do {rc = pread(reqFD, (void *)&HdrData, sizeof(HdrData), 0);}
           while(rc < 0 && errno == EINTR);
       if (rc < 0) {Say.Emsg("reqRead",errno,"refresh hdr from", reqFN);
                    FileLock(lkNone); return 0;
                   }
      } else if (lktype == lkNone) flMutex.UnLock();

// All done
//
   return 1;
}

/******************************************************************************/
/*                               r e q R e a d                                */
/******************************************************************************/
  
int XrdFrcReqFile::reqRead(void *Buff, int Offs)
{
   int rc;

   do {rc = pread(reqFD, Buff, ReqSize, Offs);} while(rc < 0 && errno == EINTR);
   if (rc < 0) {Say.Emsg("reqRead",errno,"read",reqFN); return 0;}
   return 1;
}

/******************************************************************************/
/*                              r e q W r i t e                               */
/******************************************************************************/
  
int XrdFrcReqFile::reqWrite(void *Buff, int Offs, int updthdr)
{
   int rc = 0;

   if (Buff && Offs)       do {rc = pwrite(reqFD, Buff, ReqSize, Offs);}
                              while(rc < 0 && errno == EINTR);
   if (rc >= 0 && updthdr){do {rc = pwrite(reqFD,&HdrData, sizeof(HdrData), 0);}
                              while(rc < 0 && errno == EINTR);
                           if (rc >= 0) rc = fsync(reqFD);
                          }
   if (rc < 0) {Say.Emsg("reqWrite",errno,"write", reqFN); return 0;}
   return 1;
}

/******************************************************************************/
/*                               R e W r i t e                                */
/******************************************************************************/
  
int XrdFrcReqFile::ReWrite(XrdFrcReqFile::recEnt *rP)
{
   static const int Mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
   char newFN[MAXPATHLEN], *oldFN;
   int  newFD, oldFD, Offs = ReqSize, aOK = 1;

// Construct new file and open it
//
   strcpy(newFN, reqFN); strcat(newFN, ".new");
   if ((newFD = XrdSysFD_Open(newFN, O_RDWR|O_CREAT|O_TRUNC, Mode)) < 0)
      {Say.Emsg("ReWrite",errno,"open",newFN); FileLock(lkNone); return 0;}

// Setup to write/swap the file
//
   oldFD = reqFD; reqFD = newFD;
   oldFN = reqFN; reqFN = newFN;

// Rewrite all records if we have any
//
   if (rP)
      {HdrData.First = Offs;
       while(rP)
            {rP->reqData.This = Offs;
             rP->reqData.Next = (rP->Next ? Offs+ReqSize : 0);
             if (!reqWrite((void *)&rP->reqData, Offs, 0)) {aOK = 0; break;}
             Offs += ReqSize;
             rP = rP->Next;
            }
       HdrData.Last = Offs - ReqSize;
      } else {
       HdrData.First = HdrData.Last = 0;
       if (ftruncate(newFD, ReqSize) < 0)
          {Say.Emsg("ReWrite",errno,"trunc",newFN); aOK = 0;}
      }

// Update the header
//
   HdrData.Free = 0;
   if (aOK && !(aOK = reqWrite(0, 0)))
      Say.Emsg("ReWrite",errno,"write header",newFN);

// If all went well, rename the file
//
   if (aOK && rename(newFN, oldFN) < 0)
      {Say.Emsg("ReWrite",errno,"rename",newFN); aOK = 0;}

// Perform post processing
//
   if (aOK)  close(oldFD);
      else  {close(newFD); reqFD = oldFD;}
   reqFN = oldFN;
   return aOK;
}
