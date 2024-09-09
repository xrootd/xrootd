#ifndef __XRDXROOTDAIOBUFF__
#define __XRDXROOTDAIOBUFF__
/******************************************************************************/
/*                                                                            */
/*                         X r d A i o B u f f . h h                          */
/*                                                                            */
/* (c) 2021 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdSfs/XrdSfsAio.hh"

class XrdBuffer;
class XrdXrootdAioNorm;
class XrdXrootdAioPgrw;
class XrdXrootdAioTask;

class XrdXrootdAioBuff : public XrdSfsAio
{
public:

static
XrdXrootdAioBuff*       Alloc(XrdXrootdAioTask *arp);

        void            doneRead() override;

        void            doneWrite() override;

virtual void            Recycle() override;

XrdXrootdAioBuff*       next;

XrdXrootdAioPgrw* const pgrwP;  // -> Derived type is of this type or 0

                  XrdXrootdAioBuff(XrdXrootdAioTask* tP, XrdBuffer* bP)
                                  : pgrwP(0),     reqP(tP), buffP(bP) {}

                  XrdXrootdAioBuff(XrdXrootdAioPgrw* pgrwP,
                                   XrdXrootdAioTask* tP, XrdBuffer* bP)
                                  : pgrwP(pgrwP), reqP(tP), buffP(bP) {}
protected:

static const char* TraceID;
XrdXrootdAioTask*  reqP;         // -> Associated task
XrdBuffer*         buffP;        // -> Buffer object
};
#endif
