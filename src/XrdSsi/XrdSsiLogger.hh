#ifndef __XRDSSILOGGER_HH__
#define __XRDSSILOGGER_HH__
/******************************************************************************/
/*                                                                            */
/*                       X r d S s i L o g g e r . h h                        */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
/* Produced by Andrew Hanushevsky for Stanford University under contract      */
/*            DE-AC02-76-SFO0515 with the Deprtment of Energy                 */
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

#include <cstdarg>
 
//-----------------------------------------------------------------------------
//! The XrdSsiLogger object is used to route messages to the default log file.
//-----------------------------------------------------------------------------

struct iovec;

class XrdSsiLogger
{
public:

//-----------------------------------------------------------------------------
//! Insert a space delimited error message into the log file.
//!
//! @param  pfx  !0 -> the text to prefix the message; the message is formed as
//!                    <timestamp> pfx: txt1 [txt2] [txt3]\n
//!         pfx  =0 -> add message to the log without a time stamp or prefix.
//! @param  txt1,txt2,txt3  the message to be added to the log.
//-----------------------------------------------------------------------------

static void Msg(const char *pfx,    const char *txt1,
                const char *txt2=0, const char *txt3=0);

//-----------------------------------------------------------------------------
//! Insert a formated error message into the log file using variable args.
//!
//! @param  pfx  !0 -> the text to prefix the message; the message is formed as
//!                    <timestamp> <pfx>: <formated_text>\n
//!         pfx  =0 -> add message to the log without a time stamp or prefix.
//! @param  fmt  the message formatting template (i.e. sprintf format). Note
//!              that a newline character is always appended to the message.
//! @param  ...  the arguments that should be used with the template. The
//!              formatted message is truncated at 2048 bytes.
//-----------------------------------------------------------------------------

static void Msgf(const char *pfx, const char *fmt, ...);

//-----------------------------------------------------------------------------
//! Insert a formated error message into the log file using a va_list.
//!
//! @param  pfx  !0 -> the text to prefix the message; the message is formed as
//!                    <timestamp> <pfx>: <formated_text>\n
//!         pfx  =0 -> add message to the log without a time stamp or prefix.
//! @param  fmt  the message formatting template (i.e. sprintf format). Note
//!              that a newline character is always appended to the message.
//! @param  aP   the arguments that should be used with the template. The
//!              formatted message is truncated at 2048 bytes.
//-----------------------------------------------------------------------------

static void Msgv(const char *pfx, const char *fmt, va_list aP);

//-----------------------------------------------------------------------------
//! Insert a formated error message into the log file using a iovec.
//!
//! @param  iovP pointer to an iovec that contains the message.
//!              that a newline character is always appended to the message.
//! @param  iovN the number of elements in the iovec.
//-----------------------------------------------------------------------------

static void Msgv(struct iovec *iovP, int iovN);

//-----------------------------------------------------------------------------
//! Set a message callback function for messages issued via this object. This
//! method should be called during static initialization (this means the call
//! needs to occur at global scope).
//!
//! @param  mCB   Reference to the message callback function as defined by
//!               the typedef MCB_t.
//! @param  mcbt  Specifies the type of callback being set, as follows:
//!               mcbAll    - callback for client-side and server-side logging.
//!               mcbClient - Callback for client-side logging.
//!               mcbServer - Callback for server-side logging.
//!
//! @return bool  A value of true indicates success, otherwise false returned.
//!               The return value can generally be ignored and is provided as
//!               a means to call this method via dynamic global initialization.
//-----------------------------------------------------------------------------

typedef void (MCB_t)(struct timeval const &mtime, //!< TOD of message
                     unsigned long         tID,   //!< Thread issuing msg
                     const char           *msg,   //!< Message text
                     int                   mlen); //!< Length of message text

enum mcbType {mcbAll=0, mcbClient, mcbServer};

static bool SetMCB(MCB_t &mcbP, mcbType mcbt=mcbAll);

//-----------------------------------------------------------------------------
//! Define helper functions to allow std::ostream std::cerr output to appear in the log.
//! The following two functions are used with the macros below.
//! The SSI_LOG macro preceedes the message with a time stamp; SSI_SAY does not.
//! The std::endl std::ostream output item is automatically added to all output!
//-----------------------------------------------------------------------------

#define SSI_LOG(x) {std::cerr <<XrdSSiLogger::TBeg()      <<x; XrdSsiLogger::TEnd();}
#define SSI_SAY(x)        {XrdSSiLogger::TBeg();std::cerr <<x; XrdSsiLogger::TEnd();}

static const char *TBeg();
static void        TEnd();

//-----------------------------------------------------------------------------
//! Constructor and destructor
//-----------------------------------------------------------------------------

         XrdSsiLogger() {}
        ~XrdSsiLogger() {}
};

/******************************************************************************/
/*          S e r v e r - S i d e   L o g g i n g   C a l l b a c k           */
/******************************************************************************/

//-----------------------------------------------------------------------------
//! To establish a log message callback to route messages to you own logging
//! framework, you normally use the SetMCB() method. This is called at part of
//! your shared library initialization at load time (i.e. SetMCB() is called at
//! global scope). If this is not convenient on the server-side you can use
//! the following alternative; include the following definition at file level:
//!
//! XrdSsiLogger::MCB_t *XrdSsiLoggerMCB = &<your_log_function>
//!
//! For instance:
//!
//! void LogMsg(struct timeval const &mtime, unsigned long tID,
//!             const char *msg, int mlen) {...}
//!
//! XrdSsiLogger::MCB_t *XrdSsiLoggerMCB = &LogMsg;
//-----------------------------------------------------------------------------
#endif
