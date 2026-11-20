#ifndef XRDCRYPTOSSLCOMPAT_HH
#define XRDCRYPTOSSLCOMPAT_HH
/******************************************************************************/
/*                                                                            */
/*               X r d C r y p t o s s l C o m p a t . h h                    */
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

// Helpers for the OpenSSL API, which hide the differences between the
// versions of OpenSSL. This header is private: do not install it, and do not
// include it from an installed header.

#include <string>

#include <openssl/asn1.h>
#include <openssl/opensslv.h>

// Copy the data of an ASN1_STRING into a std::string. Use this, and not the
// result of ASN1_STRING_get0_data() as a C string: OpenSSL does not guarantee
// a NUL after the data. OpenSSL 4.1 marks the byte after the data of a decoded
// string as not accessible for Valgrind and the sanitizers. The result can
// have an embedded NUL character.
inline std::string XrdCryptosslASN1String(const ASN1_STRING *x)
{
   const char *data = reinterpret_cast<const char *>(ASN1_STRING_get0_data(x));
#if OPENSSL_VERSION_NUMBER >= 0x40100000L
   // OpenSSL 4.1 deprecates ASN1_STRING_length(), which returns an int
   return std::string(data, ASN1_STRING_get_length(x));
#else
   return std::string(data, static_cast<size_t>(ASN1_STRING_length(x)));
#endif
}

#endif
