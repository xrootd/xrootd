//------------------------------------------------------------------------------
// This file is part of XrdHTTP: A pragmatic implementation of the
// HTTP/WebDAV protocol for the Xrootd framework
//
// Copyright (c) 2013 by European Organization for Nuclear Research (CERN)
// Author: Fabrizio Furano <furano@cern.ch>
// File Date: Apr 2013
//------------------------------------------------------------------------------
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------







/** @file  XrdHttpUtils.hh
 * @brief  Utility functions for XrdHTTP
 * @author Fabrizio Furano
 * @date   April 2013
 * 
 * 
 * 
 */

#include "XProtocol/XPtypes.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdOuc/XrdOucIOVec.hh"
#include "XrdOuc/XrdOucTUtils.hh"
#include <string>
#include <cstring>
#include <vector>
#include <memory>
#include <sstream>

#ifndef XRDHTTPUTILS_HH
#define	XRDHTTPUTILS_HH

enum : int {
  HTTP_CONTINUE                        = 100,
  HTTP_SWITCHING_PROTOCOLS             = 101,
  HTTP_PROCESSING                      = 102,
  HTTP_EARLY_HINTS                     = 103,

  // 2xx Success
  HTTP_OK                              = 200,
  HTTP_CREATED                         = 201,
  HTTP_ACCEPTED                        = 202,
  HTTP_NON_AUTHORITATIVE_INFORMATION   = 203,
  HTTP_NO_CONTENT                      = 204,
  HTTP_RESET_CONTENT                   = 205,
  HTTP_PARTIAL_CONTENT                 = 206,
  HTTP_MULTI_STATUS                    = 207,
  HTTP_ALREADY_REPORTED                = 208,
  HTTP_IM_USED                         = 226,

  // 3xx Redirection
  HTTP_MULTIPLE_CHOICES                = 300,
  HTTP_MOVED_PERMANENTLY               = 301,
  HTTP_FOUND                           = 302,
  HTTP_SEE_OTHER                       = 303,
  HTTP_NOT_MODIFIED                    = 304,
  HTTP_USE_PROXY                       = 305,
  HTTP_TEMPORARY_REDIRECT              = 307,
  HTTP_PERMANENT_REDIRECT              = 308,

  // 4xx Client Errors
  HTTP_BAD_REQUEST                     = 400,
  HTTP_UNAUTHORIZED                    = 401,
  HTTP_PAYMENT_REQUIRED                = 402,
  HTTP_FORBIDDEN                       = 403,
  HTTP_NOT_FOUND                       = 404,
  HTTP_METHOD_NOT_ALLOWED              = 405,
  HTTP_NOT_ACCEPTABLE                  = 406,
  HTTP_PROXY_AUTHENTICATION_REQUIRED   = 407,
  HTTP_REQUEST_TIMEOUT                 = 408,
  HTTP_CONFLICT                        = 409,
  HTTP_GONE                            = 410,
  HTTP_LENGTH_REQUIRED                 = 411,
  HTTP_PRECONDITION_FAILED             = 412,
  HTTP_PAYLOAD_TOO_LARGE               = 413,
  HTTP_URI_TOO_LONG                    = 414,
  HTTP_UNSUPPORTED_MEDIA_TYPE          = 415,
  HTTP_RANGE_NOT_SATISFIABLE           = 416,
  HTTP_EXPECTATION_FAILED              = 417,
  HTTP_IM_A_TEAPOT                     = 418, // RFC 2324
  HTTP_MISDIRECTED_REQUEST             = 421,
  HTTP_UNPROCESSABLE_ENTITY            = 422,
  HTTP_LOCKED                          = 423,
  HTTP_FAILED_DEPENDENCY               = 424,
  HTTP_TOO_EARLY                       = 425,
  HTTP_UPGRADE_REQUIRED                = 426,
  HTTP_PRECONDITION_REQUIRED           = 428,
  HTTP_TOO_MANY_REQUESTS               = 429,
  HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
  HTTP_UNAVAILABLE_FOR_LEGAL_REASONS   = 451,

  // 5xx Server Errors
  HTTP_INTERNAL_SERVER_ERROR           = 500,
  HTTP_NOT_IMPLEMENTED                 = 501,
  HTTP_BAD_GATEWAY                     = 502,
  HTTP_SERVICE_UNAVAILABLE             = 503,
  HTTP_GATEWAY_TIMEOUT                 = 504,
  HTTP_HTTP_VERSION_NOT_SUPPORTED      = 505,
  HTTP_VARIANT_ALSO_NEGOTIATES         = 506,
  HTTP_INSUFFICIENT_STORAGE            = 507,
  HTTP_LOOP_DETECTED                   = 508,
  HTTP_NOT_EXTENDED                    = 510,
  HTTP_NETWORK_AUTHENTICATION_REQUIRED = 511,
};

// GetHost from URL
// Parse an URL and extract the host name and port
// Return 0 if OK
int parseURL(char *url, char *host, int &port, char **path);

// Simple itoa function
std::string itos(long i);

// Home made implementation of strchrnul
char *mystrchrnul(const char *s, int c);

void calcHashes(
        char *hash,

        const char *fn,

        kXR_int16 req,

        XrdSecEntity *secent,
        
        time_t tim,

        const char *key);


int compareHash(
        const char *h1,
        const char *h2);


bool Fromhexdigest(const unsigned char *input, int length, unsigned char *out);

void Tobase64(const unsigned char *input, int length, char *out);

// Create a new quoted string
char *quote(const char *str);
// unquote a string and return a new one
char *unquote(char *str);

/**
 * Creates a non-const copy of the string passed in parameter and
 * calls unquote() on it before returning the pointer
 * to the unquoted string
 * @param str the string to unquote
 * @return the malloc'd and unquoted string
 * !!! IT MUST BE FREED AFTER USAGE USING free(...) !!!
 */
inline char * decode_raw(const std::string & str) {
  size_t strLength = str.length();
  // uniquely own the temporary copy
  std::unique_ptr<char[]> buf(new char[strLength + 1]);
  std::memcpy(buf.get(), str.c_str(), strLength + 1);
  // unquote returns a fresh malloc()'d pointer
  return unquote(buf.get());
}

/**
 * Calls quote() on the string passed in parameter
 * @param str the string to quote
 * @return the pointer to the quoted string
 * !!! IT MUST BE FREED AFTER USAGE USING free(...) !!!
 */
inline char * encode_raw(const std::string & str) {
  return quote(str.c_str());
}

/**
 * Encodes the URL passed in parameter (converts all letters consider illegal in URLs to their
 * %XX versions).
 * Calls quote()
 * Returns a std::string containing the encoded string
 */
inline std::string encode_str(const std::string & str) {
  char * encodedRaw = encode_raw(str);
  std::string encoded { encodedRaw };
  free(encodedRaw);
  return encoded;
}

/**
 * Decodes the string passed in parameter (converts all %XX codes to their 8bit
 * versions)
 * Calls unquote()
 * Returns the std::string containing the decoded string.
 */
inline std::string decode_str(const std::string & str) {
  char * decodedRaw = decode_raw(str);
  std::string decoded { decodedRaw };
  free(decodedRaw);
  return decoded;
}

/**
 * Encodes opaque query string parameters
 * example: authz=Bearer token --> authz=Bearer%20token
 * @param opaque the opaque query string to encode
 * @return the opaque query string with encoded values
 */
inline std::string encode_opaque(const std::string & opaque) {
  std::ostringstream output;
  std::vector<std::string> allKeyValues;
  XrdOucTUtils::splitString(allKeyValues,opaque,"&");
  bool first = true;
  for(auto & kv: allKeyValues) {
    size_t equal = kv.find('=');
    if(equal != std::string::npos) {
      std::string key = kv.substr(0, equal);
      std::string value = kv.substr(equal + 1);
      if(!first) {
        output << "&";
      }
      output << key << "=" << encode_str(value);
      first = false;
    }
  }
  return output.str();
}

// Escape a string and return a new one
char *escapeXML(const char *str);

int mapErrNoToHttp(int err);

std::string httpStatusToString(int status);

typedef std::vector<XrdOucIOVec2> XrdHttpIOList;



#endif	/* XRDHTTPUTILS_HH */
