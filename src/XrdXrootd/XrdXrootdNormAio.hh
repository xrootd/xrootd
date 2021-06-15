#ifndef __XRDXROOTDNORMAIO_H__
#define __XRDXROOTDNORMAIO_H__
/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d N o r m A i o . h h                    */
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

#include "XrdXrootd/XrdXrootdAioTask.hh"

class XrdXrootdAioBuff;
class XrdXrootdFile;
  
class XrdXrootdNormAio : public XrdXrootdAioTask
{
public:

static XrdXrootdNormAio  *Alloc(XrdXrootdProtocol *protP,
                                XrdXrootdResponse &resp,
                                XrdXrootdFile     *fP);

       void               DoIt() override;

       void               Read(long long offs, int dlen) override;

       void               Recycle(bool release) override;

       int                Write(long long offs, int dlen) override;

private:

         XrdXrootdNormAio() : XrdXrootdAioTask("aio request"),
                              sendQ(0), reorders(0) {}
virtual ~XrdXrootdNormAio() {}

       bool               CopyF2L_Add2Q(XrdXrootdAioBuff *aioP=0);
       void               CopyF2L() override;
       int                CopyL2F() override;
       bool               CopyL2F(XrdXrootdAioBuff *aioP) override;
       bool               Send(XrdXrootdAioBuff *aioP, bool final=false);

static const char        *TraceID;

       XrdXrootdNormAio  *next;
       XrdXrootdAioBuff  *sendQ;
       off_t              sendOffset; // Required offset of next chunk to send
       int                reorders;   // Number of buffers that were reordered
};
#endif
