#ifndef __XRDOFSTPCPROG_HH__
#define __XRDOFSTPCPROG_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d O f s T P C P r o g . h h                       */
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

#include "XrdOuc/XrdOucProg.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdSys/XrdSysPthread.hh"
  
class XrdOfsTPCJob;
class XrdOucProg;
  
class XrdOfsTPCProg
{
public:

       void      Cancel() {JobStream.Drain();}

static int       Init();

       void      Run();

static
XrdOfsTPCProg   *Start(XrdOfsTPCJob *jP, int &rc);

       int       Xeq();

                 XrdOfsTPCProg(XrdOfsTPCProg *Prev, int num, int errMon);

                ~XrdOfsTPCProg() {}
private:

static XrdSysMutex    pgmMutex;
static XrdOfsTPCProg *pgmIdle;

       XrdOucProg     Prog;
       XrdOucStream   JobStream;
       XrdOfsTPCProg *Next;
       XrdOfsTPCJob  *Job;
       char           Pname[16];
       char           eRec[1024];
};
#endif
