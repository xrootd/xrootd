#ifndef __SYS_EROUTE_H__
#define __SYS_EROUTE_H__
/******************************************************************************/
/*                                                                            */
/*                       X r d O u c E R o u t e . h h                        */
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

class XrdOucStream;
class XrdSysError;
  
class XrdOucERoute
{
public:

//-----------------------------------------------------------------------------
//! Format an error message into a buffer in the form of:
//! "Unable to <etxt1> <etxt2>; <syserror[enum]>"
//!
//! @param  buff    pointer to the buffer where the msg is to be placed.
//! @param  blen    the length of the buffer.
//! @param  ecode   the error number associated iwth the error.
//! @param  etxt1   associated text token #1.
//! @param  etxt2   associated text token #2 (optional).
//! @param  xtra    Optional additional text to include on the next line
//!
//! @return <int>   The number of characters placed in the buffer less null.
//-----------------------------------------------------------------------------

static int Format(char *buff, int blen, int ecode, const char *etxt1,
                                                   const char *etxt2=0,
                                                   const char *xtra =0);

//-----------------------------------------------------------------------------
//! Format an error message using Format() and route it as requested.
//!
//! @param  elog    pointer to the XrdSysError object to use to route the
//!                 message to the log, If null, the message isn't routed there.
//! @param  estrm   pointer to the XrdOucStrean object which is to receive the
//!                 error message text or null if none exists.
//! @param  esfx    The suffix identifier to use when routing to the log.
//! @param  ecode   the error number associated iwth the error.
//! @param  etxt1   associated text token #1.
//! @param  etxt2   associated text token #2 (optional).
//!
//! @return <int>   The -abs(enum) or -1 if enum is zero.
//-----------------------------------------------------------------------------

static int Route(XrdSysError *elog, XrdOucStream *estrm, const char *esfx,
                 int ecode, const char *etxt1, const char *etxt2=0);

         XrdOucERoute() {}

        ~XrdOucERoute() {}
};
#endif
