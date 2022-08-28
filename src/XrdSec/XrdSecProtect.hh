#ifndef __XRDSECPROTECT_H__
#define __XRDSECPROTECT_H__
/******************************************************************************/
/*                                                                            */
/*                      X r d S e c P r o t e c t . h h                       */
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

#include "XProtocol/XProtocol.hh"

//------------------------------------------------------------------------------
//! This class implements the XRootD protocol security protection.
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//! Provide a replacement for the std::invoke() function available in C++17 to
//! invoke the Need2Secure member function without a vtable lookup. The
//! calling convention is: if (NEED2SECURE(<propP>))(<reqR>) and where:
//! <protP> is a pointer to the relevant XrdSecProtect object (may be nil).
//! <reqR>  is a reference to the ClientRequest object to be inspected.
//------------------------------------------------------------------------------

#define NEED2SECURE(protP) protP && ((*protP).*(protP->Need2Secure))

/******************************************************************************/
/*                         X r d S e c P r o t e c t                          */
/******************************************************************************/
  
struct iovec;
class  XrdSecProtectParms;
class  XrdSecProtocol;

class XrdSecProtect
{
public:
friend class XrdSecProtector;

//------------------------------------------------------------------------------
//! Delete this object. Use this method as opposed to operator delete.
//------------------------------------------------------------------------------

virtual void         Delete() {delete this;}

//------------------------------------------------------------------------------
//! Test whether or not a request needs to be secured. This method pointer
//! should only be invoked via the NEED2SECURE macro (see above).
//!
//! @param  thereq Reference to the request header/body in network byte order.
//!
//! @return false  - request need not be secured (equals false).
//! @return true   - request needs to be secured.
//------------------------------------------------------------------------------

        bool        (XrdSecProtect::*Need2Secure)(ClientRequest &thereq);

//------------------------------------------------------------------------------
//! Secure a request.
//!
//! Request securement is optional and this call should be gaurded by an if
//! statement to avoid securing requests that need not be secured as follows:
//!
//! if (NEED2SECURE(<protP>)(thereq)) result = <protP>->Secure(....);
//!    else result = 0;
//!
//! Modify the above to your particuar needs but gaurd the call!
//!
//! @param  newreq  A reference to a pointer where the new request, if needed,
//!                 will be placed. The new request will consist of a kXR_sigver
//!                 request followed by hash. The request buffer must be freed
//!                 using free() when it is no longer needed.
//! @param  thereq  Reference to the client request header/body that needs to
//!                 be secured. The request must be in network byte order.
//! @param  thedata The request data whose length resides in theReq.dlen. If
//!                 thedata is nil but thereq.dlen is not zero then the request
//!                 data must follow the request header in the thereq buffer.
//!
//! @return <0      An error occurred and the return value is -errno.
//! @return >0      The length of the new request whose pointer is in newreq.
//!                 This is the nuber of bytes that must be sent.
//------------------------------------------------------------------------------

virtual int          Secure(SecurityRequest *&newreq,
                            ClientRequest    &thereq,
                            const char       *thedata
                           );

//------------------------------------------------------------------------------
//! Verify that a request was properly secured.
//!
//! @param  secreq  A reference to the kXR_sigver request followed by whatever
//!                 data was sent (normally an encrypted verification hash).
//!                 All but the request code must be in network byte order.
//! @param  thereq  Reference to the client request header/body that needs to
//!                 be verified. The request must be in network byte order.
//! @param  thedata The request data whose length resides in theReq.dlen.
//!
//! @return Upon success zero is returned. Otherwise a pointer to a null
//!         delimited string describing the problem is returned.
//------------------------------------------------------------------------------

virtual const char  *Verify(SecurityRequest  &secreq,
                            ClientRequest    &thereq,
                            const char       *thedata
                           );

//------------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------------

virtual ~XrdSecProtect() {}

protected:

         XrdSecProtect(XrdSecProtocol *aprot=0, bool edok=true)     // Client!
                      : Need2Secure(&XrdSecProtect::Screen),
                        authProt(aprot), secVec(0), lastSeqno(1),
                        edOK(edok), secVerData(false)
                        {}

         XrdSecProtect(XrdSecProtocol *aprot, XrdSecProtect &pRef, // Server!
                       bool edok=true)
                      : Need2Secure(&XrdSecProtect::Screen),
                        authProt(aprot), secVec(pRef.secVec),
                        lastSeqno(0), edOK(edok),
                        secVerData(pRef.secVerData) {}

void     SetProtection(const ServerResponseReqs_Protocol &inReqs);

private:
bool            GetSHA2(unsigned char *hBuff, struct iovec *iovP, int iovN);
bool            Screen(ClientRequest &thereq);

XrdSecProtocol              *authProt;
const char                  *secVec;
ServerResponseReqs_Protocol  myReqs;
union {kXR_unt64             lastSeqno;  // Used by Secure()
       kXR_unt64             nextSeqno;  // Used by Verify()
      };
bool                         edOK;
bool                         secVerData;
static const unsigned int    maxRIX = kXR_REQFENCE-kXR_auth;
char                         myVec[maxRIX];
};
#endif
