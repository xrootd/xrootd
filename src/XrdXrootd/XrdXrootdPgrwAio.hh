#ifndef __XRDXROOTDPGRWAIO_H__
#define __XRDXROOTDPGRWAIO_H__
/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d P g r w A i o . h h                    */
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

class XrdXrootdAioPgrw;
class XrdXrootdPgwBadCS;

class XrdXrootdPgrwAio : public XrdXrootdAioTask
{
public:

static XrdXrootdPgrwAio  *Alloc(XrdXrootdProtocol *protP,
                                XrdXrootdResponse &resp,
                                XrdXrootdFile     *fP,
                                XrdXrootdPgwBadCS *bcsP=0);

       void               DoIt() override;

       void               Read(long long offs, int dlen) override;

       void               Recycle(bool release) override;

       int                Write(long long offs, int dlen) override;

static const int    aioSZ = 64*1024; // 64K I/O size

private:

         XrdXrootdPgrwAio() : XrdXrootdAioTask("pgaio request") {}
virtual ~XrdXrootdPgrwAio() {}

       bool               CopyF2L_Add2Q(XrdXrootdAioPgrw *aioP=0);
       void               CopyF2L() override;
       int                CopyL2F() override;
       bool               CopyL2F(XrdXrootdAioBuff *bP) override;
       bool               SendData(XrdXrootdAioBuff *bP, bool final=false);
       int                SendDone();
       bool               VerCks(XrdXrootdAioPgrw *aioP);

static const char        *TraceID;

       XrdXrootdPgwBadCS *badCSP;     // -> Bad checksum recorder
};
#endif
