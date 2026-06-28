#ifndef __XRDHTTPMETRICSEXPORTERPUSH_HH__
#define __XRDHTTPMETRICSEXPORTERPUSH_HH__
/******************************************************************************/
/*                                                                            */
/*      X r d H t t p M e t r i c s E x p o r t e r P u s h . h h              */
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
/******************************************************************************/

//-----------------------------------------------------------------------------
//! The HTTP transport for the metrics pushers, factored out of the plugin so
//! the curl wiring (method, content type, body) can be exercised end-to-end by
//! a test against a loopback receiver without standing up the whole handler.
//! Header-only and compiled only where libcurl is available.
//-----------------------------------------------------------------------------

#ifdef XRDMETRICS_HAVE_CURL

#include <string>

#include <curl/curl.h>

namespace XrdHttpMetricsExporterPush
{
//! Discard any response body the receiver returns.
inline size_t Discard(char*, size_t sz, size_t nm, void*) { return sz * nm; }

//! Send one request body over an already-initialised easy handle. The handle is
//! reset first, so the same handle can be reused across pushes. @p ctHeader is a
//! full header line (e.g. "Content-Type: application/json"); when @p put is true
//! the verb is PUT (Pushgateway), otherwise POST (OTLP/HTTP).
//!
//! @return CURLE_OK on a completed transfer (regardless of HTTP status).
inline CURLcode Send(CURL *curl, const std::string &url, const char *ctHeader,
                     bool put, const std::string &body)
{
   struct curl_slist *hdrs = curl_slist_append(nullptr, ctHeader);

   curl_easy_reset(curl);
   curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
   if (put) curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
   else     curl_easy_setopt(curl, CURLOPT_POST, 1L);
   curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
   curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
   curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
   curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Discard);
   curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

   CURLcode rc = curl_easy_perform(curl);

   curl_slist_free_all(hdrs);
   return rc;
}
}
#endif
#endif
