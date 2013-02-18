#ifndef __XRDSECLOADER_HH__
#define __XRDSECLOADER_HH__
/******************************************************************************/
/*                                                                            */
/*                       X r d S e c L o a d e r . h h                        */
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

#include "XrdSec/XrdSecInterface.hh"

class  XrdOucErrInfo;
class  XrdSysPlugin;
struct XrdVersionInfo;

/*! This class defines the security loader interface. It is intended to be used
    by xrootd clients to obtain an instance of a security protocol object when
    the server requires authentication. There should only be one instance of
    this object and should be created at initialization time or defined as a
    static member since it does not load any libraries until the GetProtocol()
    method is called.
*/
  
class XrdSecLoader
{
public:

//------------------------------------------------------------------------------
//! Get a supported XrdSecProtocol object for one of the protocols suggested by
//! the server and possibly based on the server's hostname or host address.
//!
//! @param  hostname The client's host name or the IP address as text.
//! @param  endPoint the XrdNetAddrInfo object describing the server end-point.
//! @param  sectoken The security token supplied by the server.
//! @param  einfo    The structure to record any error messages. These are
//!                  normally sent to the client. If einfo is a null pointer,
//!                  any messages are sent to standard error.
//!
//! @return Success: Address of protocol object to be used for authentication.
//!                  The object's delete method should be called to release the
//!                  storage. See the notes on when you can do this.
//!         Failure: Null, no protocol can be returned. The einfo parameter,
//!                  if supplied, has the reason.
//!
//! Notes:   1) There should be one protocol object per physical connection.
//!          2) When the connection is closed, the protocol's Delete() method
//!             should be called to properly delete the object.
//!          3) The object may also be deleted after successful authentication
//!             if you don't need to use any it's methods afterwards.
//------------------------------------------------------------------------------

XrdSecProtocol *GetProtocol(const char       *hostname,
                            XrdNetAddrInfo   &endPoint,
                            XrdSecParameters &sectoken,
                            XrdOucErrInfo    *einfo=0);

//------------------------------------------------------------------------------
//! Constructor
//!
//! @param  vInfo    Is the reference to the version information corresponding
//!                  to the xrootd version you compiled with. You define this
//!                  information using the XrdVERSIONINFODEF macro defined in
//!                  XrdVersion.hh. You must supply your version information
//!                  and it must be compatible with the loader and any shared
//!                  libraries that it might load on your behalf.
//------------------------------------------------------------------------------

           XrdSecLoader(XrdVersionInfo &vInfo) : urVersion(&vInfo),
                                                 secLib(0), secGet(0) {}

//------------------------------------------------------------------------------
//! Destructor
//!
//! The destructor deletes the SysPlugin object that loaded XrdLibSec which
//! means that the library will be closed upon last use (which should be so).
//------------------------------------------------------------------------------

          ~XrdSecLoader();

private:

bool             Init(XrdOucErrInfo *einfo);

XrdVersionInfo  *urVersion;
XrdSysPlugin    *secLib;
XrdSecGetProt_t  secGet;
};
#endif
