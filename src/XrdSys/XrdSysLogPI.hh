#ifndef __SYS_LOGPI_H__
#define __SYS_LOGPI_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d S y s L o g P I . h h                         */
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

#include <cstdlib>
#include <sys/time.h>

//-----------------------------------------------------------------------------
//! This header file defines the plugin interface used by the logging subsystem.
//! The following function is called for each message. A pointer to the
//! function is returned by XrdSysLogPInit(); see the definition below. The
//! log message function must be thread safe in synchronous mode as any number
//! of threads may call it at the same time. In async mode, only one thread
//! invokes the function for each message.
//!
//! @param  mtime     The time the message was generated. The time value is
//!                   zero when tID is zero (see below).
//! @param  tID       The thread ID that issued the message (Linux -> gettid()).
//!                   If tID is zero then the msg was captured from stderr.
//! @param  msg       Pointer to the null-terminaed message text.
//! @param  mlen      Length of the message text (exclusive of null byte).
//-----------------------------------------------------------------------------

typedef void (*XrdSysLogPI_t)(struct timeval const &mtime,
                              unsigned long         tID,
                              const char           *msg,
                              int                   mlen);

//-----------------------------------------------------------------------------
//! Initialize and return a pointer to the plugin. This function must reside in
//! the plugin shared library as an extern "C" function. The shared library is
//! library identified by the "-l @library" command line option. This function
//! is called only once during loging initialization.
//!
//! @param  cfgn      -> Configuration filename (nil if none).
//! @param  argv      -> command line arguments after "-+xrdlog".
//! @param  argc         number of command line arguments in argv.
//!
//! @return Upon success a pointer of type XrdSysLogPI_t which is the function
//!         that handles log messages (see above). Upon failure, a nil pointer
//!         should be returned. A sample deinition is shown below.
//-----------------------------------------------------------------------------

/*!
   extern "C" XrdSysLogPI_t XrdSysLogPInit(const char  *cfgn,
                                                 char **argv,
                                                 int    argc) { . . . }
*/

typedef XrdSysLogPI_t  (*XrdSysLogPInit_t)(const char  *cfgn,
                                                 char **argv,
                                                 int    argc);
  
//------------------------------------------------------------------------------
/*! Specify the compilation version.

    Additionally, you *should* declare the xrootd version you used to compile
    your plug-in. The plugin manager automatically checks for compatibility.
    Declare it as follows:

    #include "XrdVersion.hh"
    XrdVERSIONINFO(XrdSysLogPInit,<name>);

    where <name> is a 1- to 15-character unquoted name identifying your plugin.
*/
//------------------------------------------------------------------------------
#endif
