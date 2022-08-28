#ifndef __XRDTLS_H__
#define __XRDTLS_H__
/******************************************************************************/
/*                                                                            */
/*                             X r d T l s . h h                              */
/*                                                                            */
/* (c) 2019 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <string>

class XrdSysLogger;

class XrdTls
{
public:

enum RC {TLS_AOK = 0,          //!< All went well, will always be zero.
         TLS_CON_Closed,       //!< TLS connection has been closed
         TLS_CRT_Missing,      //!< The x509 certificate missing
         TLS_CTX_Missing,      //!< The TLS context is missing.
         TLS_HNV_Error,        //!< A hostname validation error occuured
         TLS_SSL_Error,        //!< An SSL error occurred
         TLS_SYS_Error,        //!< A system call error occurred
         TLS_UNK_Error,        //!< An unknown error occurred
         TLS_VER_Error,        //!< Certificate verification failed
         TLS_WantAccept,       //!< Reissue call when Accept()  completes
         TLS_WantConnect,      //!< Reissue call when Connect() completes
         TLS_WantRead,         //!< Reissue call when reads  do not block
         TLS_WantWrite         //!< Reissue call when writes do not block
        };

//------------------------------------------------------------------------
//! Route an optional error message and flush outstanding messages.
//!
//! @param  tid    - Optional trace identifier.
//! @param  msg    - An optional message.
//! @param  flush  - If true prints all outstanding ssl messages.
//!                  Otherwise, it clears all outstanding sll messages.
//------------------------------------------------------------------------

static void Emsg(const char *tid, const char *msg=0, bool flush=true);

//------------------------------------------------------------------------
//! Convert TLS RC code to a reason string.
//!
//! @param  rc     - The TLS return code.
//! @param  dbg    - True to include additional identifying text. Otherwise,
//!                  a concise message decribing the error is returned.
//!
//! @return A string describing the error.
//------------------------------------------------------------------------

static std::string RC2Text(XrdTls::RC rc, bool dbg=false);

//------------------------------------------------------------------------
//! Set the message callback.
//!
//! @param cbP       Pointer to the message callback function. If nil, messages
//!                  are sent to stderr. This is a global setting.
//!
//! @note            You should establish a callback once in the main thread.
//------------------------------------------------------------------------

typedef void (*msgCB_t)(const char *tid, const char *msg, bool sslmsg);

static void     SetMsgCB(msgCB_t cbP);

//------------------------------------------------------------------------
//! Set debugging on or off.
//!
//! @param opts      One of or more of the options below.
//! @param logP      Pointer to XrdSysLogger or the message callback (see above)
//!                  to route messages. If nil messages are routed to stderr.
//------------------------------------------------------------------------

static const int dbgOFF = 0; //!< Turn debugging off (initial deault)
static const int dbgCTX = 1; //!< Turn debugging in for context operations.
static const int dbgSOK = 2; //!< Turn debugging in for socket  operations.
static const int dbgSIO = 4; //!< Turn debugging in for socket  I/O
static const int dbgALL = 7; //!< Turn debugging for everything
static const int dbgOUT = 8; //!< Force msgs to stderr for easier client debug

static void SetDebug(int opts, XrdSysLogger *logP=0);

static void SetDebug(int opts, msgCB_t logP);

//------------------------------------------------------------------------
//! Convert SSL error to TLS::RC code.
//!
//! @param  sslrc  - the SSL error return code.
//!
//! @return The corresponding TLS::RC code.
//------------------------------------------------------------------------

static RC ssl2RC(int sslrc);

//------------------------------------------------------------------------
//! Convert SSL error to text.
//!
//! @param  sslrc  - the SSL error return code.
//! @param  dflt   - the default to be return when mapping does no exist.
//!
//! @return The corresponding text or the dflt string is returned.
//!
//! @note This is provided because some versions of OpenSSL do not
//!       provide a reasonable textual reason no matter what you use.
//------------------------------------------------------------------------

static const char *ssl2Text(int sslrc, const char *dflt="unknown_error");

//------------------------------------------------------------------------
//! Clear the SSL error queue for the calling thread
//------------------------------------------------------------------------

static void ClearErrorQueue();
};
#endif
