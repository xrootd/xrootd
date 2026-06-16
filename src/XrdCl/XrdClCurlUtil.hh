/******************************************************************************/
/*                                                                            */
/*                    X r d C l C u r l U t i l . h h                         */
/*                                                                            */
/* (c) 2026 by the XRootD Collaboration                                       */
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
/******************************************************************************/

#ifndef __XRD_CL_CURL_UTIL_HH__
#define __XRD_CL_CURL_UTIL_HH__

#include <cstdint>
#include <string>
#include <utility>

#include <curl/curl.h>

namespace XrdCl
{
  class Env;
  class Log;

  namespace CurlUtil
  {
    struct X509Credentials
    {
      std::string cert;
      std::string key;
    };

    bool HTTPStatusIsError( unsigned status );
    std::pair<uint16_t, uint32_t> HTTPStatusConvert( unsigned status );
    std::pair<uint16_t, uint32_t> CurlCodeConvert( CURLcode result );

    void ConfigureX509Env( Env *env, Log &log, uint64_t logTopic );
    bool UseClientX509( Env *env );
    X509Credentials GetClientX509Credentials( Env *env );

    void ApplyCurlCAConfig( CURL *curl, Env *env );
    void ApplyClientX509Credentials( CURL *curl,
                                     const X509Credentials &credentials,
                                     Log *log = nullptr,
                                     uint64_t logTopic = 0,
                                     bool requireKey = false );
    CURL *CreateCurlHandle( const char *userAgent, bool verbose, Env *env );
  }
}

#endif // __XRD_CL_CURL_UTIL_HH__
