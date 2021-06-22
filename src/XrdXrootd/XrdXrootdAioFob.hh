#ifndef __XRDXROOTDAIOFOB_HH_
#define __XRDXROOTDAIOFOB_HH_
/******************************************************************************/
/*                                                                            */
/*                    X r d X r o o t d A i o F o b . h h                     */
/*                                                                            */
/* (c) 2021 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdXrootd/XrdXrootdProtocol.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdXrootdAioTask;

class XrdXrootdAioFob
{
public:

void         Reset();

void         Reset(XrdXrootdProtocol *protP);

void         Schedule(XrdXrootdAioTask *aioP);

void         Schedule(XrdXrootdProtocol *protP);

             XrdXrootdAioFob() : maxQ(0) {}

            ~XrdXrootdAioFob() {Reset();}

private:
void         Notify(XrdXrootdAioTask *aioP, const char *what);

XrdSysMutex         fobMutex;
bool                Running[XrdXrootdProtocol::maxStreams] = {false};
struct AioTasks
      {XrdXrootdAioTask *first;
       XrdXrootdAioTask *last;
                         AioTasks() : first(0), last(0) {}
                        ~AioTasks() {}
      } aioQ[XrdXrootdProtocol::maxStreams];
int     maxQ;
};
#endif
