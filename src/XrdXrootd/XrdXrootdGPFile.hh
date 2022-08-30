#ifndef __XRDXROOTDGPFILE_H__
#define __XRDXROOTDGPFILE_H__
/******************************************************************************/
/*                                                                            */
/*                    X r d X r o o t d G P F i l e . h h                     */
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

//! Classes XrdXrootdGPFile and XrdXrootdGPFileInfo are used to implement
//! get/putFile requests. This is a plugin class with a default implementation.

class XrdOucEnv;
class XrdOucErrInfo;
class XrdSecEntity;
class XrdSfs;
class XrdXrootdGPFAgent;

/******************************************************************************/
/*             C l a s s   X r d X r o o t d G P F i l e I n f o              */
/******************************************************************************/
  
//------------------------------------------------------------------------------
//! The XrdXrootdGFileInfo class contains the get/putFile() parameters and
//! contains callback methods that indicate when the operation completes as
//! well as for progress status updates.
//------------------------------------------------------------------------------

class XrdXrootdGPFileInfo
{
public:

const char *cksType;      //!< Checksum    type  or nil if none wanted
const char *cksValue;     //!< Checksum    value or nil if none wanted
const char *src;          //!< Source      specification (path or URL)
const char *srcCgi;       //!< Source      cgi or nil if none.
const char *dst;          //!< Destination specification (path or URL)
const char *srcCgi;       //!< Destination cgi or nil if none.
uint16_t    pingsec;      //!< Seconds between ping call to Update()
uint16_t    streams;      //!< Number of parallel streams (0 -> default)

//------------------------------------------------------------------------------
//! Indicate that an accepted get/putFile requtest has completed. This must
//! be called at completion afterwhich this object must be deleted.
//!
//! @param  eMsg   - A text string describing the problem if in error. If no
//!                  error was encounteredm a nil pointer should be passed.
//! @param  eNum   - The errno value corresponding to the error type. A value
//!                  zero indicates that the copy successfully completed.
//!
//! @return true   - Completion sent to client.
//! @return false  - Client is no longer connected, completion not sent.
//------------------------------------------------------------------------------

bool Completed(const char *eMsg=0, int eNum=0);

//------------------------------------------------------------------------------
//! Supply status information to the client. This is normally done every
//! Info::pingsec seconds.
//!
//! @param  xfrsz  - The number of bytes transmitted.
//! @param  stat   - One of Status indicating execution stage.
//!
//! @return true   - Status sent to client.
//! @return false  - Client is no longer connected, status not sent.
//------------------------------------------------------------------------------

enum Status {isPending = 0, //!< Copy operation is pending
             isCopying = 1, //!< Copy operation in progress
             isProving = 2  //!< Copy operation verifiying checksum
            }

bool Update(uint64_t xfrsz, Status stat);

/******************************************************************************/
/*              C o n s t r u c t o r   &   D e s t r u c t o r               */
/******************************************************************************/
  
     XrdXrootdGPFileInfo(XrdXrootdGPFAgent &gpf)
                        : cksType(0), cksValue(0),
                          src(0), srcCgi(0), dst(0), dstCgi(0),
                          pingSec(0), streams(0),
                          gpfAgent(gpf) {}

    ~XrdXrootdGPFileInfo() {}

private:

XrdXrootdGPFAgent &gpfAgent;
};

/******************************************************************************/
/*                 c l a s s   X r d X r o o t d G P F i l e                  */
/******************************************************************************/
  
class XrdXrootdGPFile
{
public:

//------------------------------------------------------------------------------
//! Execute a getFile request.
//!
//! @param  gargs  - The getFile arguments.
//! @param  client - Client's identity (may be null).
//!
//! @note All status and information returns uses the methods in the gargs
//!       object. So, if even if the call rejected, then that occurrence is
//!       reflected by calling gargs.Completed() with an error code and msg.
//-----------------------------------------------------------------------------

virtual void getFile(const XrdXrootdGPFileInfo &gargs,
                     const XrdSecEntity        *client=0) = 0;

//------------------------------------------------------------------------------
//! Execute a putFile request.
//!
//! @param  pargs  - The putFile arguments.
//! @param  client - Client's identity (may be null).
//!
//! @note All status and information returns uses the methods in the pargs
//!       object. So, if even if the call rejected, then that occurrence is
//!       reflected by calling pargs.Completed() with an error code and msg.
//-----------------------------------------------------------------------------

virtual void putFile(const XrdXrootdGPFileInfo &pargs,
                     const XrdSecEntity        *client=0) = 0;

//------------------------------------------------------------------------------
//! Constructor
//------------------------------------------------------------------------------

             XrdXrootdGPFile() {}

//------------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------------

virtual     ~XrdXrootdGPFile() {}
};

/******************************************************************************/
/*                    X r d X r o o t d g e t G P F i l e                     */
/******************************************************************************/
  
//------------------------------------------------------------------------------
//! Obtain an instance of the XrdXrootdGPFile object.
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
//! @param  theOfs-> Pointer to the OFS plugin.
//! @param  theOSs-> Pointer to the OSS plugin.
//!
//! @return Success: A pointer to an instance of the XrdXrootdGPFile object.
//!         Failure: A null pointer which causes initialization to fail.
//------------------------------------------------------------------------------

class XrdSysError;

typedef XrdXrootdGPFile *(*XrdOfsgetPrepare_t)(XrdSysError *eDest,
                                             const char  *confg,
                                             const char  *parms,
                                             XrdSfs      *theSfs,
                                             XrdOucEnv   *envP
                                            );

#define XrdOfsgetPrepareArguments            XrdSysError *eDest,\
                                             const char  *confg,\
                                             const char  *parms,\
                                             XrdSfs      *theSfs,\
                                             XrdOucEnv   *envP
/*
extern "C" XrdXrootdGPFile_t *XrdXrootdgetGPFile;
*/

//------------------------------------------------------------------------------
//! Declare compilation version.
//!
//! Additionally, you *should* declare the xrootd version you used to compile
//! your plug-in to avoid execution issues should the class definition change.
//! Declare it as:
//------------------------------------------------------------------------------

/*! #include "XrdVersion.hh"
    XrdVERSIONINFO(XrdXrootdgetGPFile,<name>);

    where <name> is a 1- to 15-character unquoted name identifying your plugin.
*/
#endif
