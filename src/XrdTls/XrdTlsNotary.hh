#ifndef __XRDTLSNOTARY_H__
#define __XRDTLSNOTARY_H__
/******************************************************************************/
/*                                                                            */
/*                       X r d T l s N o t a r y . h h                        */
/*                                                                            */
/* (c) 2019 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

/* This class encapsulates the method to be used for hostname validation.
   A hostname is valid, as follows:
   1) When DNS is not allowed to be used:
      a) If a SAN extension is present and the hostname matches an entry
         in the extension it is considered valid.
      b) If there is no SAN extension and use of the common name is
         allowed and the names match it is considered valid.
      c) At this point hostname validation has failed.
   2) When DNS is allowed to be used:
      a) If a SAN extension is present and the hostname matches an entry
         in the extension it is considered valid.
      b) If the common name matches the hostname it is considered valid.
      c) If reverse lookup of the host IP address matches the name, it
         is considered valid.
      d) At this point hostname validation has failed.

   Notice the diference between the two is how we handle SAN matching. When
   DNS cannot be used the SAN, if present, must match. The fallback is
   to use the common name. This is selctable as the current recommendation
   is to require all certificates to have a SAN extension.
*/

class XrdNetAddrInfo;

class XrdTlsNotary
{
public:

//-----------------------------------------------------------------------------
//! Validate hostname using peer certificate (usually server's).
//!
//! @param  ssl     - pointer to peer's SSL object holding the cert.
//! @param  hName   - pointer to the hostname.
//! @param  netInfo - Pointer to the XrdNetAddrInfo object for the peer host.
//!                   This object will be used in a reverse lookup of the
//!                   IP address to see if the names match as a final fallback.
//!                   If nil, DNS fallback will not be tried.
//!
//! @return =0     - Hostname has been validated.
//! @return !0     - Hostname not validated, return value is pointer to reason.
//!                  The error message should be formed as follows:
//!                  Unable to validate host <name>; <returned reason>
//-----------------------------------------------------------------------------

static const char *Validate(const SSL      *ssl,
                            const char     *hName,
                            XrdNetAddrInfo *netInfo=0);

//-----------------------------------------------------------------------------
//! Indicate whether or not common name may be used in validation.
//!
//! @param  yesno  - True if common name may be used, false otherwise. The
//!                  common name is used only if the cert has no SAN extension
//!                  or if we are allowed to use the DNS for validation.
//!                  The default is true but is now deprecated!
//-----------------------------------------------------------------------------

static void        UseCN(bool yesno) {cnOK = yesno;}

private:

static bool cnOK;
};
#endif
