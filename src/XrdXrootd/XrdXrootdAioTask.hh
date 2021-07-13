#ifndef __XRDXROOTDAIOTASK_H__
#define __XRDXROOTDAIOTASK_H__
/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d A i o T a s k . h h                    */
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

#include "Xrd/XrdJob.hh"
#include "XrdXrootd/XrdXrootdProtocol.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysRAtomic.hh"

using namespace XrdSys;

class XrdLink;
class XrdXrootdAioBuff;
class XrdXrootdNormAio;
class XrdXrootdPgrwAio;
class XrdXrootdFile;
  
class XrdXrootdAioTask : public XrdJob, public XrdXrootdProtocol::gdCallBack
{
public:
friend class XrdXrootdAioFob;

        void               Completed(XrdXrootdAioBuff *aioP);

        const char        *ID();

        void               Init(XrdXrootdProtocol *protP,
                                XrdXrootdResponse &resp,
                                XrdXrootdFile     *fP);

virtual void               Read(long long offs, int dlen) = 0;

virtual void               Recycle(bool release) = 0;

        XrdXrootdProtocol *urProtocol() {return Protocol;}

virtual int                Write(long long offs, int dlen) = 0;

protected:

         XrdXrootdAioTask(const char *what="aio request")
                         : XrdJob(what), aioReady(aioMutex) {}
virtual ~XrdXrootdAioTask() {}

virtual void               CopyF2L() = 0;
virtual int                CopyL2F() = 0;
virtual bool               CopyL2F(XrdXrootdAioBuff *aioP) = 0;
        bool               Drain();
        int                gdDone() override;
        void               gdFail() override;
        XrdXrootdAioBuff*  getBuff(bool wait);
        void               SendError(int rc, const char *eText);
        void               SendFSError(int rc);
        bool               Validate(XrdXrootdAioBuff* aioP);

static  const char*        TraceID;

        XrdSysCondVar2     aioReady;
        XrdSysMutex        aioMutex;   // Locks private data
        XrdXrootdAioBuff*  pendQ;
        XrdXrootdAioBuff*  pendQEnd;   // -> Last element in pendQ

union  {XrdXrootdNormAio*  nextNorm;   // Never used in conflicting context!
        XrdXrootdPgrwAio*  nextPgrw;
        XrdXrootdAioTask*  nextTask;
       };

        XrdXrootdProtocol* Protocol;   // -> Protocol associated with dataLink
        XrdLink*           dataLink;   // -> Network link
        XrdXrootdFile*     dataFile;   // -> Associated file
union  {XrdXrootdAioBuff  *finalRead;  // -> A short read indicating EOF
        XrdXrootdAioBuff  *pendWrite;  // -> Pending write operation
       };
        off_t              highOffset; // F2L: EOF offset L2F: initial offset
        off_t              dataOffset; // Next offset
        int                dataLen;    // Size remaining

        char               aioState;  // See aioXXX below
        RAtomic_uchar      inFlight;
        RAtomic_bool       isDone;    // Request finished
        char               Status;    // Offline | Running | Waiting

        XrdXrootdResponse  Response;

// These values may be present in aioState
//
static const int aioDead = 0x01;      // This aio encountered a fatal link error
static const int aioHeld = 0x02;      // This aio is recycled but held
static const int aioPage = 0x04;      // This read is a pgread
static const int aioRead = 0x08;      // This is a read (i.e. File to Link copy)
static const int aioSchd = 0x10;      // Next read has been scheduled

// These must be inspected or set with aioMutex held
//
static const int Offline = 0;         // Needs to be rescheduled (read only)
static const int Running = 1;         // Executing
static const int Waiting = 2;         // Waiting for buffer needs to be signaled

private:

        bool               Wait4Buff(int maxWait=0);
};
#endif
