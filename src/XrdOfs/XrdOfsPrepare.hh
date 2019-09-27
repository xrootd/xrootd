#ifndef __XRDOFSPREPARE_H__
#define __XRDOFSPREPARE_H__
/******************************************************************************/
/*                                                                            */
/*                      X r d O f s P r e p a r e . h h                       */
/*                                                                            */
/* (c) 2019 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <string>
#include <vector>

//! Class XrdOfsPrepare is used to customize the kXR_prepare request. It is an
//! OFS layer plugin and loaded via the ofs.preplib directive.

class XrdOss;
class XrdOucEnv;
class XrdOucErrInfo;
class XrdSecEntity;
class XrdSfsFileSystem;
class XrdSfsPrep;
  
class XrdOfsPrepare
{
public:

//------------------------------------------------------------------------------
//! Execute a prepare request.
//!
//! @param  pargs  - The prepare arguments (see XrdSfsInterface.hh).
//! @param  eInfo  - The object where error or data response is to be returned.
//! @param  client - Client's identify (may be null).
//!
//! @return One of SFS_OK, SFS_DATA, SFS_ERROR, SFS_REDIRECT, SFS_STALL, or
//!         SFS_STARTED.
//!
//! @note Special action taken with certain return codes:
//!       - SFS_DATA  The data is sent to the client as the "requestID".
//!       - SFS_OK    The data pointed to by pargs.reqid is sent to the
//!                   client as the "requestID".
//-----------------------------------------------------------------------------

virtual int            begin(      XrdSfsPrep      &pargs,
                                   XrdOucErrInfo   &eInfo,
                             const XrdSecEntity    *client = 0) = 0;

//------------------------------------------------------------------------------
//! Cancel a preveious prepare request.
//!
//! @param  pargs  - The prepare arguments (see XrdSfsInterface.hh). The
//!                  pargs.reqid points to the "requestID" associated with the
//!                  previously issued prepare request.
//! @param  eInfo  - The object where error or data response is to be returned.
//! @param  client - Client's identify (may be null).
//!
//! @return One of SFS_OK, SFS_ERROR, SFS_REDIRECT, SFS_STALL, or
//!         SFS_STARTED.
//-----------------------------------------------------------------------------

virtual int            cancel(      XrdSfsPrep      &pargs,
                                    XrdOucErrInfo   &eInfo,
                              const XrdSecEntity    *client = 0) = 0;

//------------------------------------------------------------------------------
//! Query a preveious prepare request.
//!
//! @param  pargs  - The prepare arguments (see XrdSfsInterface.hh). The
//!                  pargs.reqid points to the "requestID" associated with the
//!                  previously issued prepare request.
//! @param  eInfo  - The object where error or data response is to be returned.
//! @param  client - Client's identify (may be null).
//!
//! @return One of SFS_OK, SFS_ERROR, SFS_REDIRECT, or SFS_STALL.
//-----------------------------------------------------------------------------

virtual int            query(      XrdSfsPrep      &pargs,
                                   XrdOucErrInfo   &eInfo,
                             const XrdSecEntity    *client = 0) = 0;

//------------------------------------------------------------------------------
//! Constructor
//------------------------------------------------------------------------------

             XrdOfsPrepare() {}

//------------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------------

virtual     ~XrdOfsPrepare() {}
};

/******************************************************************************/
/*                      X r d O f s g e t P r e p a r e                       */
/******************************************************************************/
  
//------------------------------------------------------------------------------
//! Obtain an instance of the XrdOfsPrepare object.
//!
//! This extern "C" function is called when a shared library plug-in containing
//! implementation of this class is loaded. It must exist in the shared library
//! and must be thread-safe.
//!
//! @param  eDest -> The error object that must be used to print any errors or
//!                  other messages (see XrdSysError.hh).
//! @param  confg -> Name of the configuration file that was used. This pointer
//!                  may be null though that would be impossible.
//! @param  parms -> Argument string specified on the namelib directive. It may
//!                  be null or point to a null string if no parms exist.
//! @param  theSfs-> Pointer to the XrdSfsFileSystem plugin.
//! @param  theOSs-> Pointer to the OSS plugin.
//! @param  envP  -> Pointer to environmental information (may be nil).
//!
//! @return Success: A pointer to an instance of the XrdOfsPrepare object.
//!         Failure: A null pointer which causes initialization to fail.
//------------------------------------------------------------------------------

class XrdSysError;

typedef XrdOfsPrepare *(*XrdOfsgetPrepare_t)(XrdSysError *eDest,
                                             const char  *confg,
                                             const char  *parms,
                                             XrdSfsFileSystem
                                                         *theSfs,
                                             XrdOss      *theOss,
                                             XrdOucEnv   *envP
                                            );

#define XrdOfsgetPrepareArguments            XrdSysError *eDest,\
                                             const char  *confg,\
                                             const char  *parms,\
                                             XrdSfsFileSystem\
                                                         *theSfs,\
                                             XrdOss      *theOss,\
                                             XrdOucEnv   *envP
/*
extern "C" XrdOfsPrepare_t *XrdOfsgetPrepare;
*/

/******************************************************************************/
/*                      X r d O f s A d d P r e p a r e                       */
/******************************************************************************/
  
//------------------------------------------------------------------------------
//! Obtain an instance of the XrdOfsPrepare wrapper object.
//!
//! This extern "C" function is called when a shared library plug-in containing
//! implementation of this class is loaded. It must exist in the shared library
//! and must be thread-safe.
//!
//! @param  eDest -> The error object that must be used to print any errors or
//!                  other messages (see XrdSysError.hh).
//! @param  confg -> Name of the configuration file that was used. This pointer
//!                  may be null though that would be impossible.
//! @param  parms -> Argument string specified on the namelib directive. It may
//!                  be null or point to a null string if no parms exist.
//! @param  theSfs-> Pointer to the XrdSfsFileSystem plugin.
//! @param  theOSs-> Pointer to the OSS plugin.
//! @param  envP  -> Pointer to environmental information (may be nil).
//! @param  prepP -> Pointer to the existing XrdOfsPrepare object that should
//!                  be wrapped by the returned object.
//!
//! @return Success: A pointer to an instance of the XrdOfsPrepare object.
//!         Failure: A null pointer which causes initialization to fail.
//------------------------------------------------------------------------------

typedef XrdOfsPrepare *(*XrdOfsAddPrepare_t)(XrdSysError *eDest,
                                             const char  *confg,
                                             const char  *parms,
                                             XrdSfsFileSystem
                                                         *theSfs,
                                             XrdOss      *theOss,
                                             XrdOucEnv   *envP,
                                          XrdOfsPrepare  *prepP
                                            );

#define XrdOfsAddPrepareArguments            XrdSysError *eDest,\
                                             const char  *confg,\
                                             const char  *parms,\
                                             XrdSfsFileSystem\
                                                         *theSfs,\
                                             XrdOss      *theOss,\
                                             XrdOucEnv   *envP,\
                                          XrdOfsPrepare  *prepP
/*
extern "C" XrdOfsPrepare_t *XrdOfsAddPrepare;
*/
  
//------------------------------------------------------------------------------
//! Declare compilation version.
//!
//! Additionally, you *should* declare the xrootd version you used to compile
//! your plug-in. While not currently required, it is highly recommended to
//! avoid execution issues should the class definition change. Declare it as:
//------------------------------------------------------------------------------

/*! #include "XrdVersion.hh"
    XrdVERSIONINFO(XrdOfsgetPrepare,<name>);

    where <name> is a 1- to 15-character unquoted name identifying your plugin.
*/
#endif
