#ifndef __FRMXFRQUEUE_H__
#define __FRMXFRQUEUE_H__
/******************************************************************************/
/*                                                                            */
/*                     X r d F r m X f r Q u e u e . h h                      */
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
#include "XrdOuc/XrdOucHash.hh"
#include "XrdSys/XrdSysPthread.hh"

class  XrdFrcReqFile;
class  XrdFrcRequest;
class  XrdFrmXfrJob;

class XrdFrmXfrQueue
{
public:

static int           Add(XrdFrcRequest *rP, XrdFrcReqFile *reqF, int theQ);

static void          Done(XrdFrmXfrJob *xP, const char *Msg);

static XrdFrmXfrJob *Get();

static int           Init();

static void          StopMon(void *parg);

                     XrdFrmXfrQueue() {}
                    ~XrdFrmXfrQueue() {}

private:

static XrdFrmXfrJob *Pull();
static int           Notify(XrdFrcRequest *rP,int qN,int rc,const char *msg=0);
static void          Send2File(char *Dest, char *Msg, int Mln);
static void          Send2UDP(char *Dest, char *Msg, int Mln);
static int           Stopped(int qNum);
static const char   *xfrName(XrdFrcRequest &reqData, int isOut);

static XrdSysMutex               hMutex;
static XrdOucHash<XrdFrmXfrJob>  hTab;

static XrdSysMutex               qMutex;
static XrdSysSemaphore           qReady;

struct theQueue
      {XrdSysSemaphore           Avail;
       XrdFrmXfrJob             *Free;
       XrdFrmXfrJob             *First;
       XrdFrmXfrJob             *Last;
              XrdSysSemaphore    Alert;
              const char        *File;
              const char        *Name;
              int                Stop;
              int                qNum;
              theQueue() : Avail(0),Free(0),First(0),Last(0),Alert(0),Stop(0) {}
             ~theQueue() {}
      };
static theQueue                  xfrQ[XrdFrcRequest::numQ];
};
#endif
