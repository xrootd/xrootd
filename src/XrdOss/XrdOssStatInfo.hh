#ifndef _XRDOSSSTATINFO_H
#define _XRDOSSSTATINFO_H
/******************************************************************************/
/*                                                                            */
/*                     X r d O s s S t a t I n f o . h h                      */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

class  XrdOss;
class  XrdOucEnv;
class  XrdSysLogger;
struct stat;

namespace XrdOssStatEvent
{
static const int FileAdded   = 1; //!< Path has been added
static const int PendAdded   = 2; //!< Path has been added in pending mode
static const int FileRemoved = 0; //!< Path has been removed
}

//------------------------------------------------------------------------------
//! This file defines the alternate stat() function that can be used as a
//! replacement for the normal system stat() call that is used to determine the
//! file attributes, including whether the file exists or not. It is loaded as
//! a plug-in via the XrdOssStatInfoInit() function residing in the shared
//! library pecified by the 'oss.statlib' directive. The returned function is
//! preferentially used by the XrdOssSys::Stat() method.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! Get file information.
//!
//! @param  path       -> the file path whose stat information is wanted.
//! @param  buff       -> to the stat structure that is to be filled in with
//!                       stat information the same way that stat() would have,
//! @param  opts          A combination of XRDOSS_xxxx options. See XrdOss.hh.
//! @param  envP       -> environment pointer which includes CGI information.
//!                       This pointer is nil if no special environment exists.
//! @param  lfn        -> the corresponding logical file name. This is only
//!                       passed for version 2 calls (see XrdOssStatInfoInit2).
//!
//! @return Success:      zero with the stat structure filled in.
//! @return Failure:      a -1 with errno set to the correct err number value.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! Set file information.
//!
//! When the arevents option is specified in the oss.statlib directive and the
//! executable is the cmsd running in server mode, then the StatInfo function is
//! also used to relay add/remove file requests send by the companion xrootd to
//! the cmsd. The parameters then are as follows:
//!
//! @param  path       -> the file path whose whose stat information changed.
//! @param  buff       -> Nil; this indicates that stat information is being set.
//! @param  opts          One of the following options:
//!                       XrdOssStatEvent::FileAdded,
//!                       XrdOssStatEvent::PendAdded,
//!                       XrdOssStatEvent::FileRemoved.
//! @param  envP       -> Nil
//! @param  lfn        -> the logical file name whose stat information changed.
//!
//! @return The return value should be zero but is not currently inspected.
//------------------------------------------------------------------------------

typedef int (*XrdOssStatInfo_t) (const char *path, struct stat *buff,
                                 int         opts, XrdOucEnv   *envP);

typedef int (*XrdOssStatInfo2_t)(const char *path, struct stat *buff,
                                 int         opts, XrdOucEnv   *envP,
                                 const char *lfn);

/******************************************************************************/
/*           X r d O s s S t a t I n f o   I n s t a n t i a t o r            */
/******************************************************************************/
  
//------------------------------------------------------------------------------
//! Get the address of the appropriate XrdOssStatInfo function.
//!
//! @param  native_oss -> object that implements the storage system.
//! @param  Logger     -> The message routing object to be used in conjunction
//!                       with an XrdSysError object for error messages.
//! @param  config_fn  -> The name of the config file.
//! @param  parms      -> Any parameters specified after the path on the
//!                       oss.statlib directive. If there are no parameters, the
//!                       pointer may be zero.
//!
//! @return Success:      address of the XrdOssStatInfo function to be used
//!                       for stat() calls by the underlying storage system.
//!         Failure:      Null pointer which causes initialization to fail.
//!
//! Additionally, two special envars may be queried to determine the context:
//!
//! getenv("XRDPROG")     Indicates which program is loading the library:
//!                       "cmsd", "frm_purged", "frm_xfrd", or "xrootd"
//!                       Any other value, inclduing a nil pointer, indicates
//!                       a non-standard program is doing the load.
//!
//! getenv("XRDROLE")     Is the role of the program. The envar is set only when
//!                       XRDPROG is set to "cmsd" or "xrootd". Valid roles are:
//!                       "manager", "supervisor", "server", "proxy", or "peer".
//!
//! The function creator must be declared as an extern "C" function in the
//! plug-in shared library as follows:
//------------------------------------------------------------------------------
/*! @code {.cpp}
    extern "C" XrdOssStatInfo_t XrdOssStatInfoInit(XrdOss        *native_oss,
                                                   XrdSysLogger  *Logger,
                                                   const char    *config_fn,
                                                   const char    *parms);
    @endcode

    An alternate entry point may be defined in lieu of the previous entry point.
    This normally identified by a version option in the configuration file (e.g.
    oss.statlib -2 \<path\>). It differs in that an extra parameter is passed and
    if returns a function that accepts an extra parameter.

    @param  envP     - Pointer to the environment containing implementation
                       specific information.

    @code {.cpp}
    extern "C" XrdOssStatInfo2_t XrdOssStatInfoInit2(XrdOss        *native_oss,
                                                     XrdSysLogger  *Logger,
                                                     const char    *config_fn,
                                                     const char    *parms,
                                                     XrdOucEnv     *envP);
    @endcode
*/

//------------------------------------------------------------------------------
//! Declare compilation version.
//!
//! Additionally, you *must* declare the xrootd version you used to compile
//! your plug-in. Include the code shown below at file level in your source.
//------------------------------------------------------------------------------

/*!
        #include "XrdVersion.hh"
        XrdVERSIONINFO(XrdOssStatInfoInit,<name>);

    where \<name\> is a 1- to 15-character unquoted name identifying your plugin.
*/

//------------------------------------------------------------------------------
//! The typedef that describes the XRdOssStatInfoInit external.
//------------------------------------------------------------------------------

typedef XrdOssStatInfo_t (*XrdOssStatInfoInit_t)(XrdOss        *native_oss,
                                                 XrdSysLogger  *Logger,
                                                 const char    *config_fn,
                                                 const char    *parms);

typedef XrdOssStatInfo2_t (*XrdOssStatInfoInit2_t)(XrdOss       *native_oss,
                                                   XrdSysLogger *Logger,
                                                   const char   *config_fn,
                                                   const char   *parms,
                                                   XrdOucEnv    *envP);
#endif
