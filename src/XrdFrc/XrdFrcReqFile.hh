#ifndef __FRCREQFILE_H__
#define __FRCREQFILE_H__
/******************************************************************************/
/*                                                                            */
/*                      X r d F r c R e q F i l e . h h                       */
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

#include "XrdFrc/XrdFrcRequest.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdFrcReqFile
{
public:

       void   Add(XrdFrcRequest *rP);

       void   Can(XrdFrcRequest *rP);

       void   Del(XrdFrcRequest *rP);

       int    Get(XrdFrcRequest *rP);

       int    Init();

       char  *List(char *Buff, int bsz, int &Offs,
                    XrdFrcRequest::Item *ITList=0, int ITNum=0);

       void   ListL(XrdFrcRequest &tmpReq, char *Buff, int bsz,
                    XrdFrcRequest::Item *ITList, int ITNum);

              XrdFrcReqFile(const char *fn, int aVal);
             ~XrdFrcReqFile() {}

private:
enum LockType {lkNone, lkShare, lkExcl, lkInit};

static const int ReqSize  = sizeof(XrdFrcRequest);

void   FailAdd(char *lfn, int unlk=1);
void   FailCan(char *rid, int unlk=1);
void   FailDel(char *lfn, int unlk=1);
int    FailIni(const char *lfn);
int    FileLock(LockType ltype=lkExcl);
int    reqRead(void *Buff, int Offs);
int    reqWrite(void *Buff, int Offs, int updthdr=1);

XrdSysMutex flMutex;

struct FileHdr
{
int    First;
int    Last;
int    Free;
}      HdrData;

char  *lokFN;
int    lokFD;
int    reqFD;
char  *reqFN;

int    isAgent;

struct recEnt {recEnt       *Next;
               XrdFrcRequest reqData;
               recEnt(XrdFrcRequest &reqref) {Next = 0; reqData = reqref;}
              };
int    ReWrite(recEnt *rP);

class rqMonitor
{
public:
      rqMonitor(int isAgent) : doUL(isAgent)
                  {if (isAgent) rqMutex.Lock();}
     ~rqMonitor() {if (doUL)    rqMutex.UnLock();}
private:
static XrdSysMutex rqMutex;
int                doUL;
};
};
#endif
