#ifndef __XRDSECPROTECTOR_H__
#define __XRDSECPROTECTOR_H__
/******************************************************************************/
/*                                                                            */
/*                    X r d S e c P r o t e c t o r . h h                     */
/*                                                                            */
/* (c) 2016 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XProtocol/XPtypes.hh"
  
/******************************************************************************/
/*                    X r d S e c P r o t e c t P a r m s                     */
/******************************************************************************/

class XrdSecProtectParms
{
public:

enum    secLevel {secNone = 0,
                  secCompatible, secStandard, secIntense, secPedantic,
                  secFence
                 };

secLevel   level;   //!< In:  The desired level.
int        opts;    //!< In:  Options:

static const int   doData = 0x0000001; //!< Secure data
static const int   relax  = 0x0000002; //!< relax old clients
static const int   force  = 0x0000004; //!< Allow unencryted hash

            XrdSecProtectParms() : level(secNone), opts(0) {}
           ~XrdSecProtectParms() {}
};
  
/******************************************************************************/
/*                       X r d S e c P r o t e c t o r                        */
/******************************************************************************/
  
//------------------------------------------------------------------------------
//! The XrdSecProtector manages the XrdSecProtect objects.
//------------------------------------------------------------------------------

struct ServerResponseReqs_Protocol;
class  XrdNetAddrInfo;
class  XrdSecProtect;
class  XrdSecProtocol;
class  XrdSysLogger;

class XrdSecProtector
{
public:

//------------------------------------------------------------------------------
//! Configure protect for server-side use (not need for client)
//!
//! @param  lclParms Reference to local  client parameters.
//! @param  rmtParms Reference to remote client parameters.
//! @param  logr     Reference to the message logging object.
//!
//! @return true upon success and false upon failure.
//------------------------------------------------------------------------------

virtual bool         Config(const XrdSecProtectParms &lclParms,
                            const XrdSecProtectParms &rmtParms,
                            XrdSysLogger &logr);

//------------------------------------------------------------------------------
//! Convert protection level to its corresponding name.
//!
//! @param  level   The level value.
//!
//! @return Pointer to the name of the level.
//------------------------------------------------------------------------------

virtual const char  *LName(XrdSecProtectParms::secLevel level);

//------------------------------------------------------------------------------
//! Obtain a new instance of a protection object based on protocol response.
//! This is meant to be used client-side.
//!
//! @param  aprot   Sets the authentication protocol used and is the protocol
//!                 used to secure requests. It must be supplied. Security is
//!                 meaningless unless successful authentication has occured.
//! @param  inReqs  Reference to the security information returned in the
//!                 kXR_protocol request.
//! @param  reqLen  The actual length of inReqs (is validated).
//!
//! @return Pointer to a security object upon success and nil if security is
//!                 not needed.
//------------------------------------------------------------------------------

virtual XrdSecProtect *New4Client(      XrdSecProtocol              &aprot,
                                  const ServerResponseReqs_Protocol &inReqs,
                                        unsigned int                 reqLen);

//------------------------------------------------------------------------------
//! Obtain a new instance of a security object based on security setting for
//! this object. This is meant to be used severt-side.
//!
//! @param  aprot   Sets the authentication protocol used and is the protocol
//!                 used to secure requests. It must be supplied.
//! @param  plvl    The client's protocol level.
//!
//! @return Pointer to a security object upon success and nil if security is
//!                 not needed.
//------------------------------------------------------------------------------

virtual XrdSecProtect *New4Server(XrdSecProtocol &aprot, int plvl);

//------------------------------------------------------------------------------
//! Obtain the proper kXR_protocol response (server-side only)
//!
//! @param  resp    Reference to the place where the response is to be placed.
//! @param  nai     Reference to the client's network address.
//! @param  pver    Client's protocol version in host byte order.
//!
//! @return The length of the protocol response security information.
//------------------------------------------------------------------------------

virtual int            ProtResp(ServerResponseReqs_Protocol &resp,
                                XrdNetAddrInfo &nai, int pver);

//------------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------------

virtual ~XrdSecProtector() {}

enum lrType     {isLcl=0, isRmt=1, isLR=2};

protected:

                XrdSecProtector() {}

private:
void  Config(const XrdSecProtectParms    &parms,
             ServerResponseReqs_Protocol &reqs);
};
#endif
