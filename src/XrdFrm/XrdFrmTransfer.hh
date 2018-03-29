#ifndef __FRMTRANSFER_H__
#define __FRMTRANSFER_H__
/******************************************************************************/
/*                                                                            */
/*                     X r d F r m T r a n s f e r . h h                      */
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

#include "XrdOuc/XrdOucHash.hh"
#include "XrdSys/XrdSysPthread.hh"

struct XrdFrmTranArg;
struct XrdFrmTranChk;
class  XrdFrmXfrJob;
class  XrdOucProg;

class XrdFrmTransfer
{
public:

static
const  char *checkFF(const char *Path);

static int   Init();

       void  Start();

             XrdFrmTransfer();
            ~XrdFrmTransfer() {}

private:
const char *Fetch();
const char *FetchDone(char *lfnpath, struct stat &Stat, int &rc);
const char *ffCheck();
      void  ffMake(int nofile=0);
      int   SetupCmd(XrdFrmTranArg *aP);
      int   TrackDC(char *Lfn, char *Mdp, char *Rfn);
      int   TrackDC(char *Rfn);
const char *Throw();
      void  Throwaway();
      void  ThrowDone(XrdFrmTranChk *cP, time_t endTime);
const char *ThrowOK(XrdFrmTranChk *cP);

static XrdSysMutex               pMutex;
static XrdOucHash<char>          pTab;

XrdOucProg    *xfrCmd[4];
XrdFrmXfrJob  *xfrP;
char           cmdBuff[4096];
};
#endif
