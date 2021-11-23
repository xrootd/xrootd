#ifndef __BWM_HANDLE__
#define __BWM_HANDLE__
/******************************************************************************/
/*                                                                            */
/*                       X r d B w m H a n d l e . h h                        */
/*                                                                            */
/* (c) 2008 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdlib>

#include "XrdBwm/XrdBwmPolicy.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdBwmLogger;
  
class XrdBwmHandle
{
public:

enum HandleState {Idle = 0, Scheduled, Dispatched};

       HandleState   Status;

       int           Activate(XrdOucErrInfo &einfo);

static XrdBwmHandle *Alloc(const char *theUsr,  const char *thePath,
                           const char *lclNode, const char *rmtNode,
                           int Incoming);

static void         *Dispatch();

inline const char   *Name() {return Parms.Lfn;}

       void          Retire();

static int           setPolicy(XrdBwmPolicy *pP, XrdBwmLogger *lP);

                     XrdBwmHandle() : Status(Idle), Next(0), qTime(0), rTime(0),
                                      xSize(0), xTime(0)
                                    {}

                    ~XrdBwmHandle() {}

private:
static XrdBwmHandle *Alloc(XrdBwmHandle *oldHandle=0);
static XrdBwmHandle *refHandle(int refID, XrdBwmHandle *hP=0);

static XrdBwmPolicy      *Policy;
static XrdBwmLogger      *Logger;
static XrdBwmHandle      *Free;       // List of free handles
static unsigned int       numQueued;

       XrdSysMutex        hMutex;
XrdBwmPolicy::SchedParms  Parms;
       XrdBwmHandle      *Next;
       XrdOucEICB        *ErrCB;
       unsigned long long ErrCBarg;
       time_t             qTime;
       time_t             rTime;
                long long xSize;
                     long xTime;
       int                rHandle;

class  theEICB : public XrdOucEICB
{
public:

         void Done(int &Result, XrdOucErrInfo *eInfo, const char *Path=0)
                  {mySem.Post();}

         int  Same(unsigned long long arg1, unsigned long long arg2) 
                  {return arg1 == arg2;}

         void Wait() {mySem.Wait();}

              theEICB() : mySem(0) {}

virtual      ~theEICB() {}

private:
XrdSysSemaphore mySem;
}                         myEICB;
};
#endif
