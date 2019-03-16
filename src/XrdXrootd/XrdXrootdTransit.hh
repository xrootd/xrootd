#ifndef __XRDXROOTDTRANSIT_HH_
#define __XRDXROOTDTRANSIT_HH_
/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d T r a n s i t . h h                    */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <sys/types.h>

#include "XrdSys/XrdSysPthread.hh"
#include "XrdXrootd/XrdXrootdBridge.hh"
#include "XrdXrootd/XrdXrootdProtocol.hh"

#include "Xrd/XrdObject.hh"

//-----------------------------------------------------------------------------
//! Transit
//!
//! The Bridge object implementation.
//-----------------------------------------------------------------------------

class  XrdOucSFVec;
class  XrdScheduler;
class  XrdXrootdTransPend;
struct iovec;

class XrdXrootdTransit : public XrdXrootd::Bridge, public XrdXrootdProtocol
{
public:

//-----------------------------------------------------------------------------
//! Get a new transit object.
//-----------------------------------------------------------------------------

static
XrdXrootdTransit *Alloc(XrdXrootd::Bridge::Result *respP,
                        XrdLink                   *linkP,
                        XrdSecEntity              *seceP,
                        const char                *nameP,
                        const char                *protP
                       );

//-----------------------------------------------------------------------------
//! Handle attention response (i.e. async response)
//-----------------------------------------------------------------------------

static int    Attn(XrdLink *lP, short *theSID, int rcode,
                   const struct iovec  *ioVec,  int ioNum, int ioLen);

//-----------------------------------------------------------------------------
//! Handle dismantlement
//-----------------------------------------------------------------------------

bool          Disc();

//-----------------------------------------------------------------------------
//! Perform one-time initialization
//-----------------------------------------------------------------------------

static void Init(XrdScheduler *schedP, int qMax, int qTTL);

//-----------------------------------------------------------------------------
//! Resume processing after a waitresp completion.
//-----------------------------------------------------------------------------

void          Proceed();

//-----------------------------------------------------------------------------
//! Handle link activation (replaces parent activation).
//-----------------------------------------------------------------------------

int           Process(XrdLink *lp); // XrdProtocol override

//-----------------------------------------------------------------------------
//! Handle link shutdown.
//-----------------------------------------------------------------------------

void          Recycle(XrdLink *lp, int consec, const char *reason);

//-----------------------------------------------------------------------------
//! Redrive a request after a wait
//-----------------------------------------------------------------------------

void          Redrive();

//-----------------------------------------------------------------------------
//! Initialize the valid request table.
//-----------------------------------------------------------------------------

static
const char   *ReqTable();

//-----------------------------------------------------------------------------
//! Inject an xrootd request into the protocol stack.
//-----------------------------------------------------------------------------

bool          Run(const char *xreqP,       //!< xrootd request header
                        char *xdataP=0,    //!< xrootd request data (optional)
                        int   xdataL=0     //!< xrootd request data length
                 );

//-----------------------------------------------------------------------------
//! Handle request data response.
//-----------------------------------------------------------------------------

int           Send(int rcode, const struct iovec *ioVec, int ioNum, int ioLen);

//-----------------------------------------------------------------------------
//! Handle request sendfile response.
//-----------------------------------------------------------------------------

int           Send(long long offset, int dlen, int fdnum);

int           Send(XrdOucSFVec *sfvec, int sfvnum, int dlen);

//-----------------------------------------------------------------------------
//! Set sendfile() enablement.
//-----------------------------------------------------------------------------

int           setSF(kXR_char *fhandle, bool seton=false)
                   {return SetSF(fhandle, seton);}

//-----------------------------------------------------------------------------
//! Set maximum wait time.
//-----------------------------------------------------------------------------

void          SetWait(int wtime, bool notify=false)
                     {runWMax = wtime; runWCall = notify;}

//-----------------------------------------------------------------------------
//! Constructor & Destructor
//-----------------------------------------------------------------------------

              XrdXrootdTransit() : TranLink(this),
                                   respJob(this, &XrdXrootdTransit::Proceed,
                                           "Transit proceed"),
                                   waitJob(this, &XrdXrootdTransit::Redrive,
                                           "Transit redrive")
                                 {}
virtual      ~XrdXrootdTransit() {}

private:
int   AttnCont(XrdXrootdTransPend *tP,  int rcode,
               const struct iovec *ioV, int ioN, int ioL);
bool  Fail(int ecode, const char *etext);
int   Fatal(int rc);
void  Init(Result     *rsltP, XrdLink    *linkP, XrdSecEntity *seceP,
           const char *nameP, const char *protP
          );
bool  ReqWrite(char *xdataP, int xdataL);
bool  RunCopy(char *buffP, int buffL);
int   Wait(XrdXrootd::Bridge::Context &rInfo,
           const struct iovec *ioV, int ioN, int ioL);
int   WaitResp(XrdXrootd::Bridge::Context &rInfo,
               const struct iovec *ioV, int ioN, int ioL);

class SchedReq : public XrdJob
     {public:
      typedef void (XrdXrootdTransit::*callbackFP)();
      void DoIt() {(spanP->*cbFunc)();}

           SchedReq(XrdXrootdTransit *tP, callbackFP cbP, const char *why)
                   : XrdJob(why), spanP(tP), cbFunc(cbP) {}
          ~SchedReq() {}
      private:
      XrdXrootdTransit *spanP;
      callbackFP        cbFunc;
     };

static XrdObjectQ<XrdXrootdTransit> TranStack;
XrdObject<XrdXrootdTransit>         TranLink;

SchedReq                     respJob;
SchedReq                     waitJob;
XrdSysMutex                  runMutex;
static const char           *reqTab;
XrdProtocol                 *realProt;
XrdXrootd::Bridge::Result   *respObj;
const char                  *runEText;
char                        *runArgs;
int                          runALen;
int                          runABsz;
int                          runError;
int                          runStatus;
int                          runWait;
int                          runWTot;
int                          runWMax;
bool                         runDone;
bool                         reInvoke;
bool                         runWCall;
int                          wBLen;
char                        *wBuff;
const char                  *pName;
time_t                       cTime;
};
#endif
