/******************************************************************************/
/*                                                                            */
/*                    X r d C l C u r l U t i l . c c                         */
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

#include "XrdClCurlUtil.hh"

#include "XrdCl/XrdClEnv.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdVersion.hh"
#include "XProtocol/XProtocol.hh"

#include <cerrno>
#include <cstdlib>

#include <unistd.h>

namespace
{
  void SetIfEmpty( XrdCl::Env *env, XrdCl::Log &log,
                   const std::string &optName, const std::string &envName,
                   uint64_t logTopic )
  {
    if( !env ) return;

    std::string val;
    if( !env->GetString( optName, val ) || val.empty() )
    {
      env->PutString( optName, "" );
      env->ImportString( optName, envName );
    }
    if( env->GetString( optName, val ) && !val.empty() )
    {
      log.Info( logTopic, "Setting %s to value '%s'",
                optName.c_str(), val.c_str() );
    }
  }
}

namespace XrdCl
{
namespace CurlUtil
{
  bool HTTPStatusIsError( unsigned status )
  {
    return (status < 100) || (status >= 400);
  }

  std::pair<uint16_t, uint32_t> HTTPStatusConvert( unsigned status )
  {
    switch( status )
    {
      case 400: // Bad Request
        return std::make_pair( XrdCl::errErrorResponse, kXR_InvalidRequest );
      case 401: // Unauthorized (needs authentication)
        return std::make_pair( XrdCl::errErrorResponse, kXR_NotAuthorized );
      case 402: // Payment Required
      case 403: // Forbidden (failed authorization)
        return std::make_pair( XrdCl::errErrorResponse, kXR_NotAuthorized );
      case 404:
        return std::make_pair( XrdCl::errErrorResponse, kXR_NotFound );
      case 405: // Method not allowed
      case 406: // Not acceptable
        return std::make_pair( XrdCl::errErrorResponse, kXR_InvalidRequest );
      case 407: // Proxy Authentication Required
        return std::make_pair( XrdCl::errErrorResponse, kXR_NotAuthorized );
      case 408: // Request timeout
        return std::make_pair( XrdCl::errErrorResponse, kXR_ReqTimedOut );
      case 409: // Conflict
        return std::make_pair( XrdCl::errErrorResponse, kXR_Conflict );
      case 410: // Gone
        return std::make_pair( XrdCl::errErrorResponse, kXR_NotFound );
      case 411: // Length required
      case 412: // Precondition failed
      case 413: // Payload too large
      case 414: // URI too long
      case 415: // Unsupported Media Type
      case 416: // Range Not Satisfiable
      case 417: // Expectation Failed
      case 418: // I'm a teapot
        return std::make_pair( XrdCl::errErrorResponse, kXR_InvalidRequest );
      case 421: // Misdirected Request
      case 422: // Unprocessable Content
        return std::make_pair( XrdCl::errErrorResponse, kXR_InvalidRequest );
      case 423: // Locked
        return std::make_pair( XrdCl::errErrorResponse, kXR_FileLocked );
      case 424: // Failed Dependency
      case 425: // Too Early
      case 426: // Upgrade Required
      case 428: // Precondition Required
        return std::make_pair( XrdCl::errErrorResponse, kXR_InvalidRequest );
      case 429: // Too Many Requests
        return std::make_pair( XrdCl::errErrorResponse, kXR_Overloaded );
      case 431: // Request Header Fields Too Large
        return std::make_pair( XrdCl::errErrorResponse, kXR_InvalidRequest );
      case 451: // Unavailable For Legal Reasons
        return std::make_pair( XrdCl::errErrorResponse, kXR_Impossible );
      case 500: // Internal Server Error
      case 501: // Not Implemented
      case 502: // Bad Gateway
      case 503: // Service Unavailable
        return std::make_pair( XrdCl::errErrorResponse, kXR_ServerError );
      case 504: // Gateway Timeout
        return std::make_pair( XrdCl::errErrorResponse, kXR_ReqTimedOut );
      case 507: // Insufficient Storage
        return std::make_pair( XrdCl::errErrorResponse, kXR_overQuota );
      case 508: // Loop Detected
      case 510: // Not Extended
      case 511: // Network Authentication Required
        return std::make_pair( XrdCl::errErrorResponse, kXR_ServerError );
    }
    return std::make_pair( XrdCl::errUnknown, status );
  }

  std::pair<uint16_t, uint32_t> CurlCodeConvert( CURLcode result )
  {
    switch( result )
    {
      case CURLE_OK:
        return std::make_pair( XrdCl::errNone, 0 );
      case CURLE_COULDNT_RESOLVE_PROXY:
      case CURLE_COULDNT_RESOLVE_HOST:
        return std::make_pair( XrdCl::errInvalidAddr, 0 );
      case CURLE_LOGIN_DENIED:
      // Commented-out cases are for platforms (RHEL7) where the error
      // codes are undefined.
      //case CURLE_AUTH_ERROR:
      //case CURLE_SSL_CLIENTCERT:
      case CURLE_REMOTE_ACCESS_DENIED:
        return std::make_pair( XrdCl::errLoginFailed, EACCES );
      case CURLE_SSL_CONNECT_ERROR:
      case CURLE_SSL_ENGINE_NOTFOUND:
      case CURLE_SSL_ENGINE_SETFAILED:
      case CURLE_SSL_CERTPROBLEM:
      case CURLE_SSL_CIPHER:
      case 51: // In old curl versions, this is CURLE_PEER_FAILED_VERIFICATION
      case CURLE_SSL_SHUTDOWN_FAILED:
      case CURLE_SSL_CRL_BADFILE:
      case CURLE_SSL_ISSUER_ERROR:
      case CURLE_SSL_CACERT:
        return std::make_pair( XrdCl::errTlsError, 0 );
      case CURLE_SEND_ERROR:
      case CURLE_RECV_ERROR:
        return std::make_pair( XrdCl::errSocketError, EIO );
      case CURLE_COULDNT_CONNECT:
      case CURLE_GOT_NOTHING:
        return std::make_pair( XrdCl::errConnectionError, ECONNREFUSED );
      case CURLE_OPERATION_TIMEDOUT:
#ifdef HAVE_XPROTOCOL_TIMEREXPIRED
        return std::make_pair( XrdCl::errErrorResponse,
                               XErrorCode::kXR_TimerExpired );
#else
        return std::make_pair( XrdCl::errOperationExpired, ESTALE );
#endif
      case CURLE_UNSUPPORTED_PROTOCOL:
      case CURLE_NOT_BUILT_IN:
        return std::make_pair( XrdCl::errNotSupported, ENOSYS );
      case CURLE_FAILED_INIT:
        return std::make_pair( XrdCl::errInternal, 0 );
      case CURLE_URL_MALFORMAT:
        return std::make_pair( XrdCl::errInvalidArgs, result );
      //case CURLE_WEIRD_SERVER_REPLY:
      //case CURLE_HTTP2:
      //case CURLE_HTTP2_STREAM:
        return std::make_pair( XrdCl::errCorruptedHeader, result );
      case CURLE_PARTIAL_FILE:
        return std::make_pair( XrdCl::errDataError, result );
      // These two errors indicate a failure in the callback. That should
      // generate its own failure, meaning this should never get used.
      case CURLE_READ_ERROR:
      case CURLE_WRITE_ERROR:
        return std::make_pair( XrdCl::errInternal, result );
      case CURLE_RANGE_ERROR:
      case CURLE_BAD_CONTENT_ENCODING:
        return std::make_pair( XrdCl::errNotSupported, result );
      case CURLE_TOO_MANY_REDIRECTS:
        return std::make_pair( XrdCl::errRedirectLimit, result );
      default:
        return std::make_pair( XrdCl::errUnknown, result );
    }
  }

  void ConfigureX509Env( Env *env, Log &log, uint64_t logTopic )
  {
    if( !env ) return;

    SetIfEmpty( env, log, "HttpCertFile", "XRD_HTTPCERTFILE", logTopic );
    SetIfEmpty( env, log, "HttpCertDir", "XRD_HTTPCERTDIR", logTopic );
    SetIfEmpty( env, log, "HttpClientCertFile",
                "XRD_HTTPCLIENTCERTFILE", logTopic );
    SetIfEmpty( env, log, "HttpClientKeyFile",
                "XRD_HTTPCLIENTKEYFILE", logTopic );

    int disableProxy = 0;
    env->PutInt( "HttpDisableX509", 0 );
    env->ImportInt( "HttpDisableX509", "XRD_HTTPDISABLEX509" );

    std::string filename;
    char *filenameChar;
    if( !disableProxy &&
        (!env->GetString( "HttpClientCertFile", filename ) || filename.empty()) )
    {
      if( (filenameChar = getenv( "X509_USER_PROXY" )) )
      {
        filename = filenameChar;
      }
      if( filename.empty() )
      {
        filename = "/tmp/x509up_u" + std::to_string( geteuid() );
      }
      if( access( filename.c_str(), R_OK ) == 0 )
      {
        log.Debug( logTopic,
                   "Using X509 proxy file found at %s for TLS client credential",
                   filename.c_str() );
        env->PutString( "HttpClientCertFile", filename );
        env->PutString( "HttpClientKeyFile", filename );
      }
    }
    if( (!env->GetString( "HttpCertDir", filename ) || filename.empty()) &&
        (filenameChar = getenv( "X509_CERT_DIR" )) )
    {
      env->PutString( "HttpCertDir", filenameChar );
    }
  }

  bool UseClientX509( Env *env )
  {
    int disableX509;
    return env && env->GetInt( "HttpDisableX509", disableX509 ) && !disableX509;
  }

  X509Credentials GetClientX509Credentials( Env *env )
  {
    X509Credentials credentials;
    if( !env ) return credentials;
    env->GetString( "HttpClientCertFile", credentials.cert );
    env->GetString( "HttpClientKeyFile", credentials.key );
    return credentials;
  }

  void ApplyCurlCAConfig( CURL *curl, Env *env )
  {
    if( !curl || !env ) return;

    std::string caFile;
    if( !env->GetString( "HttpCertFile", caFile ) || caFile.empty() )
    {
      char *x509CAFile = getenv( "X509_CERT_FILE" );
      if( x509CAFile )
      {
        caFile = std::string( x509CAFile );
      }
    }
    if( !caFile.empty() )
    {
      curl_easy_setopt( curl, CURLOPT_CAINFO, caFile.c_str() );
    }

    std::string caDir;
    if( !env->GetString( "HttpCertDir", caDir ) || caDir.empty() )
    {
      char *x509CADir = getenv( "X509_CERT_DIR" );
      if( x509CADir )
      {
        caDir = std::string( x509CADir );
      }
    }
    if( !caDir.empty() )
    {
      curl_easy_setopt( curl, CURLOPT_CAPATH, caDir.c_str() );
    }
  }

  void ApplyClientX509Credentials( CURL *curl,
                                   const X509Credentials &credentials,
                                   Log *log,
                                   uint64_t logTopic,
                                   bool requireKey )
  {
    if( !curl || credentials.cert.empty() ) return;

    if( log )
    {
      log->Debug( logTopic, "Using client X.509 credential found at %s",
                  credentials.cert.c_str() );
    }
    curl_easy_setopt( curl, CURLOPT_SSLCERT, credentials.cert.c_str() );
    if( credentials.key.empty() )
    {
      if( requireKey && log )
      {
        log->Error( logTopic,
                    "X.509 client credential specified but not the client key" );
      }
    }
    else
    {
      curl_easy_setopt( curl, CURLOPT_SSLKEY, credentials.key.c_str() );
    }
  }

  CURL *CreateCurlHandle( const char *userAgent, bool verbose, Env *env )
  {
    CURL *curl = curl_easy_init();
    if( !curl ) return curl;

    if( userAgent )
    {
      curl_easy_setopt( curl, CURLOPT_USERAGENT, userAgent );
    }
    if( verbose )
    {
      curl_easy_setopt( curl, CURLOPT_VERBOSE, 1L );
    }
    ApplyCurlCAConfig( curl, env );
    return curl;
  }
}
}
