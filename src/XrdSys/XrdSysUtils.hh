#ifndef __XRDSYSUTILS_HH__
#define __XRDSYSUTILS_HH__
/******************************************************************************/
/*                                                                            */
/*                        X r d S y s U t i l s . h h                         */
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

#include <sys/types.h>
#include <sys/stat.h>

class XrdSysUtils
{
public:

//-----------------------------------------------------------------------------
//! Get the name of the current executable.
//!
//! @return the full path of the executable invoked.
//-----------------------------------------------------------------------------

static const char *ExecName();

//-----------------------------------------------------------------------------
//! Format the uname information
//!
//! @param  buff   - pointer to the buffer to hold the uname as:
//!                  <sysname> <release> [<version>] [<machine>]
//! @param  blen   - length of the buffer.
//!
//! @return the output of snprintf(buff, blen, ...);
//-----------------------------------------------------------------------------

static int         FmtUname(char *buff, int blen);

//-----------------------------------------------------------------------------
//! Get common signal number.
//!
//! @param  sname  - the signal name as in sigxxx or just xxx (see kill).
//!
//! @return =0     - unknown or unsupported signal.
//! @return !0     - the corresponding signal number.
//-----------------------------------------------------------------------------

static int         GetSigNum(const char *sname);

//-----------------------------------------------------------------------------
//! Block common signals. This must be called at program start.
//!
//! @return true   - common signals are blocked.
//! @return false  - common signals not blocked, errno has teh reason.
//-----------------------------------------------------------------------------

static bool        SigBlock();

//-----------------------------------------------------------------------------
//! Block a particular signal. This should be called at program start so that
//! the block applies to all threads.
//!
//! @aparam  numsig - The signal value to be blocked.
//!
//! @return true   - signal is  blocked.
//! @return false  - signal not blocked, errno has teh reason.
//-----------------------------------------------------------------------------

static bool        SigBlock(int numsig);

//-----------------------------------------------------------------------------
//! Constructor and destructor
//-----------------------------------------------------------------------------

       XrdSysUtils() {}
      ~XrdSysUtils() {}
};
#endif
