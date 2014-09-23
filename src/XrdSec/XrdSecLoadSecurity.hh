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
#include "XrdSys/XrdSysError.hh"

//------------------------------------------------------------------------------
//! This include file defines the utility function that loads the server-side
//! security framework plugin. It should be included a linkable utility library.
//! This function is public whose ABI may not be changed!
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! Load the security framework.
//!
//! @param eDest  Pointer to the error object that routes error messages.
//! @param seclib Pointer to the shared library path that contains the
//!               framework implementation. If the filename is XrdSec.xx
//!               then this library name is dynamically versioned.
//! @param cfn    Pointer to the configuration file path.
//! @param getP   Upon success the pointer to the XrdSecGetProtocol function
//!               that must be used to obtain protocol object.
//!
//! @return !0    Pointer to the XrdSecService object suitable for server use.
//!               This object is persisted and will not be deleted until exit.
//!               Additionally, the pointer to XrdSegGetProtocol() function is
//!               returned in getP.
//! @return =0    The security frmaework could not be loaded. Error messages
//!               describing the problem have been issued.
//------------------------------------------------------------------------------

extern XrdSecService *XrdSecLoadSecurity(XrdSysError *eDest, char *seclib,
                                         char *cfn, XrdSecGetProt_t **getP);
#endif
