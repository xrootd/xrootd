#ifndef __XRDSECLOADSECURITY_HH__
#define __XRDSECLOADSECURITY_HH__
/******************************************************************************/
/*                                                                            */
/*                 X r d S e c L o a d S e c u r i t y . h h                  */
/*                                                                            */
/* (c) 2014 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdSec/XrdSecInterface.hh"

//------------------------------------------------------------------------------
//! This include file defines utility functions that load the security
//! framework plugin specialized for server-side or client-side use.
//! These functions are public and remain ABI stable!
//------------------------------------------------------------------------------

/******************************************************************************/
/*                  X r d S e c L o a d S e c F a c t o r y                   */
/******************************************************************************/
  
//------------------------------------------------------------------------------
//! Load the Security Protocol Factory (used client-side)
//!
//! @param eBuff  Pointer to a buffer tat is to receive any messages. Upon
//!               failure it will contain an eror message. Upon success it
//!               will contain an informational message that describes the
//!               version that was loaded.
//! @param eBlen  The length of the eBuff, it should be at least 1K to
//!               avoid message truncation as the message may have a path.
//! @param seclib Pointer to the shared library path that contains the
//!               framework implementation. If a nill pointer is passed, then
//!               the default library is used.
//!
//! @return !0    Pointer to the to XrdSegGetProtocol() function is returned.
//!               returned in getP if it is not nil.
//! @return =0    The security frmaework could not be loaded. The error
//!               message describing the problem is in eBuff.
//------------------------------------------------------------------------------

XrdSecGetProt_t XrdSecLoadSecFactory(      char       *eBuff,
                                           int         eBlen,
                                     const char       *seclib=0);

/******************************************************************************/
/*                   X r d S e c G e t P r o t e c t i o n                    */
/******************************************************************************/

class  XrdSecProtect;
class  XrdSecProtector;
struct ServerResponseBody_Protocol;

//------------------------------------------------------------------------------
//! Obtain an instance of a security protection object based on the kXR_protocol
//! response. This is only used client-side.
//!
//! @param  protP   Place where the protection object point is placed.
//! @param  aprot   Uses the authentication protocol to protect requests. It
//!                 must be supplied and must be he protocol the client used
//!                 for authentication. Hence, authentication must occur first.
//! @param  resp    Reference to the response body returned by kXR_protocol.
//! @param  resplen Length of the response body.
//!
//! @return >0      pointer to the protect object placed in protP.
//! @return =0      No protection is needed, protP set to zero.
//! @return <0      An error occured getting the protection object the
//!                 return value is -errno and protP has been set to zero.
//------------------------------------------------------------------------------

int XrdSecGetProtection(XrdSecProtect              *&protP,
                        XrdSecProtocol              &aprot,
                        ServerResponseBody_Protocol &resp,
                        unsigned int                 resplen);
  
/******************************************************************************/
/*                  X r d S e c L o a d S e c S e r v i c e                   */
/******************************************************************************/
  
//------------------------------------------------------------------------------
//! Load the Security Protocol Service and obtain an instance (server-side).
//!
//! @param eDest  Pointer to the error object that routes error messages.
//! @param cfn    Pointer to the configuration file path.
//! @param seclib Pointer to the shared library path that contains the
//!               framework implementation. If the filename is XrdSec.xx
//!               then this library name is dynamically versioned. If a nil
//!               pointer is passed, then the defalt library is used.
//! @param getP   Upon success and if supplied, the pointer to the function
//!               XrdSecGetProtocol() used to get protocol objects.
//! @param proP   Upon success and ifsupplied, the pointer to the class
//!               that provides protection services (nill means non wanted).
//!
//! @return !0    Pointer to the XrdSecService object suitable for server use.
//!               This object is persisted and will not be deleted until exit.
//!               Additionally, the pointer to XrdSegGetProtocol() function is
//!               returned in getP if it is not nil.
//! @return =0    The security frmaework could not be loaded. Error messages
//!               describing the problem have been issued.
//------------------------------------------------------------------------------

class XrdSysError;

XrdSecService *XrdSecLoadSecService(XrdSysError      *eDest,
                                    const char       *cfn,
                                    const char       *seclib=0,
                                    XrdSecGetProt_t  *getP=0,
                                    XrdSecProtector **proP=0);
#endif
