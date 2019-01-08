#ifndef __XRDXROOTDCALLBACK_H__
#define __XRDXROOTDCALLBACK_H__
/******************************************************************************/
/*                                                                            */
/*                  X r d X r o o t d C a l l B a c k . h h                   */
/*                                                                            */
/* (c) 2006 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdScheduler;
class XrdOurError;
class XrdXrootdStats;

struct iovec;

class XrdXrootdCallBack : public XrdOucEICB
{
public:

        void        Done(int           &Result,   //I/O: Function result
                         XrdOucErrInfo *eInfo,    // In: Error information
                         const char    *Path=0);  // In: Path related to call

        const char *Func() {return Opname;}

        char        Oper() {return Opcode;}

        int         Same(unsigned long long arg1, unsigned long long arg2);

        void        sendError(int rc, XrdOucErrInfo *eInfo, const char *Path);

        void        sendResp(XrdOucErrInfo *eInfo,
                             XResponseType  xrt,       int  *Data=0,
                             const char    *Msg=0,     int   Mlen=0);

        void        sendVesp(XrdOucErrInfo *eInfo,
                             XResponseType  xrt,
                             struct iovec  *ioV,       int   ioN);

static  void        setVals(XrdSysError    *erp,
                            XrdXrootdStats *SIp,
                            XrdScheduler   *schp,
                            int             port);

                    XrdXrootdCallBack(const char *opn, const char opc)
                                     : Opname(opn), Opcode(opc) {}

                   ~XrdXrootdCallBack() {}
private:
       const char         *Opname;
       const char          Opcode;
};
#endif
