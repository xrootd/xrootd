#ifndef __XRDTLSPEERCERTS_H__
#define __XRDTLSPEERCERTS_H__
/******************************************************************************/
/*                                                                            */
/*                    X r d T l s P e e r C e r t s . h h                     */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <openssl/ssl.h>

class XrdTlsPeerCerts
{
public:

//------------------------------------------------------------------------
//! Obtain pointer to the cert.
//!
//! @param upref  When true the cert reference count is increased by one.
//!               Otherwise, the reference count stays the same (see note).
//!
//! @return Upon success, the pointer to the cert is returned.
//!         Upon failure, a nil pointer is returned.
//!
//! @note If the cert is being passed to a method that will call X509_free()
//!       on the cert (many do) the reference count must be increased as the
//!       destructor decreases the reference count. Incorrrect handling of
//!       the reference count will invariable SEGV when the session is freed.
//!       Do *not* pass the cert to an opaque method without verifying how it
//!       handles the cert upon return.
//------------------------------------------------------------------------

X509           *getCert(bool upref=true);

//------------------------------------------------------------------------
//! Obtain pointer to the chain.
//!
//! @return Upon success, the pointer to the cert is returned which may be
//!         nil if there is no chain.
//!
//! @note The chain is the actual chain associated with the SSL session.
//!       When he SSL session is freed, the chain becomes invalid and all
//!       references to it must cease.
//------------------------------------------------------------------------

STACK_OF(X509) *getChain() {return chain;}

//------------------------------------------------------------------------
//! Check if this object has a cert.
//!
//! @return True if a cert is present and false otherwise.
//------------------------------------------------------------------------

bool            hasCert() {return cert != 0;}

//------------------------------------------------------------------------
//! Check if this object has a chain.
//!
//! @return True if a chain is present and false otherwise.
//------------------------------------------------------------------------

bool            hasChain() {return chain != 0;}

//------------------------------------------------------------------------
//! Constructor
//!
//! @param  pCert    - pointer to the cert.
//! @param  pChain   - pointer to the chain.
//------------------------------------------------------------------------

                XrdTlsPeerCerts(X509 *pCert=0,  STACK_OF(X509) *pChain=0)
                               : cert(pCert), chain(pChain) {}

               ~XrdTlsPeerCerts();

private:

X509           *cert;
STACK_OF(X509) *chain;
};
#endif
