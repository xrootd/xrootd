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
#include <string>
#include <cstring>
#include <vector>
#include <curl/curl.h>

#ifndef XRDHTTPUTILS_HH
#define	XRDHTTPUTILS_HH


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


/**
 * Encodes the URL passed in parameter (converts all letters consider illegal in URLs to their
 * %XX versions).
 * Returns a std::string containing the encoded URL
 */
template<typename T = std::string>
typename std::enable_if<std::is_same<T,std::string>::value,std::string>::type
 url_encode(const char * str) {
  if(!str) { return {}; }
  char* output = curl_easy_escape(NULL, str, ::strlen(str));
  if (!output) {
    return std::string(str);
  }

  std::string result(output);
  curl_free(output);
  return result;
}

/**
 * Encodes the URL passed in parameter (converts all letters consider illegal in URLs to their
 * %XX versions).
 * Returns a char * containing the encoded URL.
 * !!! IT MUST BE FREED AFTER USAGE USING free(...) !!!
 */
template<typename T = char *>
typename std::enable_if<std::is_same<T,char *>::value,char *>::type
url_encode(const char * str) {
  if(!str) {return nullptr;}
  char* output = curl_easy_escape(NULL, str, ::strlen(str));
  if (!output) {
    return nullptr;
  }
  char * result = strdup(output);
  curl_free(output);
  return result;
}

/**
 * Decodes the URL passed in parameter (converts all %XX codes to their 8bit
 * versions)
 * Returns the std::string containing the decoded URL.
 */
template<typename T = std::string>
typename std::enable_if<std::is_same<T,std::string>::value, std::string>::type
 url_decode(const char * str) {
  int out_length = 0;
  if(!str) {return {};}
  char* output = curl_easy_unescape(NULL, str, ::strlen(str), &out_length);
  if (!output) {
    return std::string(str);
  }

  std::string result(output, out_length);
  curl_free(output);
  return result;
}

/**
 * Decodes the URL passed in parameter (converts all %XX codes to their 8bit
 * versions)
 * Returns a char * containing the decoded URL.
 * !!! IT MUST BE FREED AFTER USAGE USING free(...) !!!
 */
template<typename T = char *>
typename std::enable_if<std::is_same<T,char *>::value, char *>::type
url_decode(const char * str) {
  int out_length = 0;
  if(!str) {return nullptr;}
  char* output = curl_easy_unescape(NULL, str, ::strlen(str), &out_length);
  if (!output) {
    return nullptr;
  }

  char* result = static_cast<char*>(malloc(out_length + 1));
  std::memcpy(result,output,out_length);
  result[out_length] = '\0';
  curl_free(output);
  return result;
}

// Create a new quoted string
char *quote(const char *str);
// unquote a string and return a new one
char *unquote(char *str);




// Escape a string and return a new one
char *escapeXML(const char *str);

typedef std::vector<XrdOucIOVec2> XrdHttpIOList;


 
#endif	/* XRDHTTPUTILS_HH */

