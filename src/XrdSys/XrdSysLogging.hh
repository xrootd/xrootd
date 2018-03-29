#ifndef __SYS_LOGGING_H__
#define __SYS_LOGGING_H__
/******************************************************************************/
/*                                                                            */
/*                      X r d S y s L o g g i n g . h h                       */
/*                                                                            */
/*(c) 2016 by the Board of Trustees of the Leland Stanford, Jr., University   */
/*Produced by Andrew Hanushevsky for Stanford University under contract       */
/*           DE-AC02-76-SFO0515 with the Deprtment of Energy                  */
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

#include <limits.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/uio.h>

#include "XrdSys/XrdSysLogPI.hh"
#include "XrdSys/XrdSysPthread.hh"

//-----------------------------------------------------------------------------
//! XrdSysLogging is the object that is used to route messages to a plugin
//! and is used to configure the base logger. There is only one such object.
//-----------------------------------------------------------------------------

class XrdSysLogger;

class XrdSysLogging
{
public:

//-----------------------------------------------------------------------------
//! Constructor and destructor
//!
//-----------------------------------------------------------------------------

         XrdSysLogging() {}

        ~XrdSysLogging() {}

//-----------------------------------------------------------------------------
//! Parameters to be passed to configure.
//-----------------------------------------------------------------------------

struct Parms
      {const char    *logfn;    //!< -> log file name or nil if none.
       XrdSysLogPI_t  logpi;    //!< -> log plugin object or nil if none
       int            bufsz;    //!<    size of message buffer, -1 default, or 0
       int            keepV;    //!<    log keep argument
       bool           hiRes;    //!<    log using high resolution timestamp
       Parms() : logfn(0), logpi(0), bufsz(-1), keepV(0), hiRes(false) {}
      ~Parms() {}
     };

//-----------------------------------------------------------------------------
//! Configure the logger object using the parameters above.
//!
//! @param  logr      Reference to the logger object.
//! @param  parms     Reference to the parameters.
//!
//! @return true if successful and false if log could not be configured.
//-----------------------------------------------------------------------------

static bool  Configure(XrdSysLogger &logr, Parms &parms);

//-----------------------------------------------------------------------------
//! Forward a log message to a plugin.
//!
//! @param  mtime     The time the message was generated.
//! @param  tID       The thread ID that issued the message.
//! @param  iov       The vector describing what to forward.
//! @param  iovcnt    The number of elements in iov vector.
//!
//! @return false if the message needs to also be placed in a local log file.
//!         true  if all processing has completed.
//-----------------------------------------------------------------------------

static bool Forward(struct timeval mtime, unsigned long tID, 
                    struct iovec  *iov,   int iovcnt);

private:
struct MsgBuff
      {struct timeval msgtod; // time message was generated
       unsigned long  tID;    // Thread ID issuing message
       unsigned int   next;   // Offset to next message, 0 if none
       unsigned short buffsz; // In doublewords (max is 512K-8)
                short msglen; // Len of msg text (max 32K-1) if <0 ->lost msgs
//     char           msgtxt; // Text follows the message header
      };
static const int  msgOff    = sizeof(MsgBuff);
static const int  mbDwords  = (sizeof(MsgBuff)+7)/8*8;
static const int  maxMsgLen = SHRT_MAX;

static int        CopyTrunc(char *mbuff, struct iovec *iov, int iovcnt);
static bool       EMsg(XrdSysLogger &logr, const char *msg);
static MsgBuff   *getMsg(char **msgTxt, bool cont);
static void      *Send2PI(void *arg);

static pthread_t  lpiTID;
static bool       lclOut;
static bool       rmtOut;
};
#endif
