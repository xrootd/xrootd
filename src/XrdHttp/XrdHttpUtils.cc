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








/** @file  XrdHttpUtils.cc
 * @brief  Utility functions for XrdHTTP
 * @author Fabrizio Furano
 * @date   April 2013
 * 
 * 
 * 
 */


#include "XrdHttpUtils.hh"

#include <cstring>
#include <openssl/hmac.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
# include "sys/param.h"

#include <pthread.h>
#include <memory>
#include <vector>
#include <algorithm>

#include "XrdSec/XrdSecEntity.hh"
#include "XrdOuc/XrdOucString.hh"

#if OPENSSL_VERSION_NUMBER < 0x10100000L
static HMAC_CTX* HMAC_CTX_new() {
  HMAC_CTX *ctx = (HMAC_CTX *)OPENSSL_malloc(sizeof(HMAC_CTX));
  if (ctx) HMAC_CTX_init(ctx);
  return ctx;
}

static void HMAC_CTX_free(HMAC_CTX *ctx) {
  if (ctx) {
    HMAC_CTX_cleanup(ctx);
    OPENSSL_free(ctx);
  }
}
#endif


// GetHost from URL
// Parse an URL and extract the host name and port
// Return 0 if OK
int parseURL(char *url, char *host, int &port, char **path) {
  // http://x.y.z.w:p/path

  *path = 0;

  // look for the second slash
  char *p = strstr(url, "//");
  if (!p) return -1;


  p += 2;

  // look for the end of the host:port
  char *p2 = strchr(p, '/');
  if (!p2) return -1;

  *path = p2;

  char buf[256];
  int l = std::min((int)(p2 - p), (int)sizeof (buf));
  strncpy(buf, p, l);
  buf[l] = '\0';

  // Now look for :
  p = strchr(buf, ':');
  if (p) {
    int l = std::min((int)(p - buf), (int)sizeof (buf));
    strncpy(host, buf, l);
    host[l] = '\0';

    port = atoi(p + 1);
  } else {
    port = 0;


    strcpy(host, buf);
  }

  return 0;
}


// Encode an array of bytes to base64

void Tobase64(const unsigned char *input, int length, char *out) {
  BIO *bmem, *b64;
  BUF_MEM *bptr;

  if (!out) return;

  out[0] = '\0';

  b64 = BIO_new(BIO_f_base64());
  BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
  bmem = BIO_new(BIO_s_mem());
  BIO_push(b64, bmem);
  BIO_write(b64, input, length);

  if (BIO_flush(b64) <= 0) {
    BIO_free_all(b64);
    return;
  }

  BIO_get_mem_ptr(b64, &bptr);


  memcpy(out, bptr->data, bptr->length);
  out[bptr->length] = '\0';

  BIO_free_all(b64);

  return;
}


static int
char_to_int(int c)
{
  if (isdigit(c)) {
    return c - '0';
  } else {
    c = tolower(c);
    if (c >= 'a' && c <= 'f') {
      return c - 'a' + 10;
    }
    return -1;
  }
}


// Decode a hex digest array to raw bytes.
//
bool Fromhexdigest(const unsigned char *input, int length, unsigned char *out) {
  for (int idx=0; idx < length; idx += 2) {
    int upper =  char_to_int(input[idx]);
    int lower =  char_to_int(input[idx+1]);
    if ((upper < 0) || (lower < 0)) {
      return false;
    }
    out[idx/2] = (upper << 4) + lower;
  }
  return true;
}


// Simple itoa function
std::string itos(long i) {
  char buf[128];
  sprintf(buf, "%ld", i);

  return buf;
}



// Home made implementation of strchrnul
char *mystrchrnul(const char *s, int c) {
  char *ptr = strchr((char *)s, c);

  if (!ptr)
    return strchr((char *)s, '\0');

  return ptr;
}



// Calculates the opaque arguments hash, needed for a secure redirection
//
// - hash is a string that will be filled with the hash
//
// - fn: the original filename that was requested
// - dhost: target redirection hostname
// - client: address:port of the client
// - tim: creation time of the url
// - tim_grace: validity time before and after creation time
//
// Input for the key (simple shared secret)
// - key
// - key length
//

void calcHashes(
        char *hash,

        const char *fn,

        kXR_int16 request,

        XrdSecEntity *secent,

        time_t tim,

        const char *key) {


#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  EVP_MAC *mac;
  EVP_MAC_CTX *ctx;
  size_t len;
#else
  HMAC_CTX *ctx;
  unsigned int len;
#endif
  unsigned char mdbuf[EVP_MAX_MD_SIZE];
  char buf[64];
  struct tm tms;


  if (!hash) {
    return;
  }
  hash[0] = '\0';
  
  if (!key) {
    return;
  }

  if (!fn || !secent) {
    return;
  }

#if OPENSSL_VERSION_NUMBER >= 0x30000000L

  mac = EVP_MAC_fetch(0, "sha256", 0);
  ctx = EVP_MAC_CTX_new(mac);

  if (!ctx) {
     return;
  }


  EVP_MAC_init(ctx, (const unsigned char *) key, strlen(key), 0);


  if (fn)
    EVP_MAC_update(ctx, (const unsigned char *) fn,
          strlen(fn) + 1);

  EVP_MAC_update(ctx, (const unsigned char *) &request,
          sizeof (request));

  if (secent->name)
    EVP_MAC_update(ctx, (const unsigned char *) secent->name,
          strlen(secent->name) + 1);

  if (secent->vorg)
    EVP_MAC_update(ctx, (const unsigned char *) secent->vorg,
          strlen(secent->vorg) + 1);

  if (secent->host)
    EVP_MAC_update(ctx, (const unsigned char *) secent->host,
          strlen(secent->host) + 1);

  if (secent->moninfo)
    EVP_MAC_update(ctx, (const unsigned char *) secent->moninfo,
          strlen(secent->moninfo) + 1);

  localtime_r(&tim, &tms);
  strftime(buf, sizeof (buf), "%s", &tms);
  EVP_MAC_update(ctx, (const unsigned char *) buf,
          strlen(buf) + 1);

  EVP_MAC_final(ctx, mdbuf, &len, EVP_MAX_MD_SIZE);

  EVP_MAC_CTX_free(ctx);
  EVP_MAC_free(mac);

#else

  ctx = HMAC_CTX_new();

  if (!ctx) {
    return;
  }



  HMAC_Init_ex(ctx, (const void *) key, strlen(key), EVP_sha256(), 0);


  if (fn)
    HMAC_Update(ctx, (const unsigned char *) fn,
          strlen(fn) + 1);

  HMAC_Update(ctx, (const unsigned char *) &request,
          sizeof (request));

  if (secent->name)
    HMAC_Update(ctx, (const unsigned char *) secent->name,
          strlen(secent->name) + 1);

  if (secent->vorg)
    HMAC_Update(ctx, (const unsigned char *) secent->vorg,
          strlen(secent->vorg) + 1);

  if (secent->host)
    HMAC_Update(ctx, (const unsigned char *) secent->host,
          strlen(secent->host) + 1);

  if (secent->moninfo)
    HMAC_Update(ctx, (const unsigned char *) secent->moninfo,
          strlen(secent->moninfo) + 1);

  localtime_r(&tim, &tms);
  strftime(buf, sizeof (buf), "%s", &tms);
  HMAC_Update(ctx, (const unsigned char *) buf,
          strlen(buf) + 1);

  HMAC_Final(ctx, mdbuf, &len);

  HMAC_CTX_free(ctx);

#endif

  Tobase64(mdbuf, len / 2, hash);
}

int compareHash(
        const char *h1,
        const char *h2) {

  if (h1 == h2) return 0;

  if (!h1 || !h2)
    return 1;

  return strcmp(h1, h2);

}

// unquote a string and return a new one

char *unquote(char *str) {
  int l = strlen(str);
  char *r = (char *) malloc(l + 1);
  r[0] = '\0';
  int i, j = 0;

  for (i = 0; i < l; i++) {

    if (str[i] == '%') {
      char savec = str[i + 3];
      str[i + 3] = '\0';

      r[j] = strtol(str + i + 1, 0, 16);
      str[i + 3] = savec;

      i += 2;
    } else r[j] = str[i];

    j++;
  }

  r[j] = '\0';

  return r;

}

// Quote a string and return a new one

char *quote(const char *str) {
  int l = strlen(str);
  char *r = (char *) malloc(l*3 + 1);
  r[0] = '\0';
  int i, j = 0;

  for (i = 0; i < l; i++) {
    char c = str[i];

    switch (c) {
      case ' ':
        strcpy(r + j, "%20");
        j += 3;
        break;
      case '[':
        strcpy(r + j, "%5B");
        j += 3;
        break;
      case ']':
        strcpy(r + j, "%5D");
        j += 3;
        break;
      case ':':
        strcpy(r + j, "%3A");
        j += 3;
        break;
      // case '/':
      //   strcpy(r + j, "%2F");
      //   j += 3;
      //   break;
      case '#':
         strcpy(r + j, "%23");
         j += 3;
         break;
      case '\n':
        strcpy(r + j, "%0A");
        j += 3;
        break;
      case '\r':
        strcpy(r + j, "%0D");
        j += 3;
        break;
      case '=':
        strcpy(r + j, "%3D");
        j += 3;
        break;
      default:
        r[j++] = c;
    }
  }

  r[j] = '\0';

  return r;
}


// Escape a string and return a new one

char *escapeXML(const char *str) {
  int l = strlen(str);
  char *r = (char *) malloc(l*6 + 1);
  r[0] = '\0';
  int i, j = 0;
  
  for (i = 0; i < l; i++) {
    char c = str[i];
    
    switch (c) {
      case '"':
        strcpy(r + j, "&quot;");
        j += 6;
        break;
      case '&':
        strcpy(r + j, "&amp;");
        j += 5;
        break;
      case '<':
        strcpy(r + j, "&lt;");
        j += 4;
        break;
      case '>':
        strcpy(r + j, "&gt;");
        j += 4;
        break;
      case '\'':
        strcpy(r + j, "&apos;");
        j += 6;
        break;
      
      default:
        r[j++] = c;
    }
  }
  
  r[j] = '\0';
  
  return r;
}

int mapXrdErrToHttp(XErrorCode xrdError) {

  int errNo = XProtocol::toErrno(xrdError);
  return mapErrNoToHttp(errNo);

}

int mapErrNoToHttp(int errNo) {

  switch (errNo) {

    case EACCES:
    case EROFS:
      return HTTP_FORBIDDEN;

    case EPERM:
      return HTTP_UNAUTHORIZED;

    case ENOENT:
      return HTTP_NOT_FOUND;

    case EEXIST:
    case ENOTEMPTY:
      return HTTP_CONFLICT;

    case ENOTDIR:
    case EISDIR:
    case EXDEV:
      return HTTP_UNPROCESSABLE_ENTITY;

    case ENAMETOOLONG:
      return HTTP_URI_TOO_LONG;

    case ELOOP:
      return HTTP_LOOP_DETECTED;

    case ENOSPC:
    case EDQUOT:
      return HTTP_INSUFFICIENT_STORAGE;

    case EFBIG:
      return HTTP_PAYLOAD_TOO_LARGE;

    case EINVAL:
    case EBADF:
    case EFAULT:
    case ENXIO:
    case ESPIPE:
    case ENODEV:
    case EOVERFLOW:
      return HTTP_BAD_REQUEST;

    case ENOTSUP: // EOPNOTSUPP
      return HTTP_NOT_IMPLEMENTED;

    case EBUSY:
    case EAGAIN:
    case EINTR:
    case ENOMEM:
    case EMFILE:
    case ENFILE:
    case ETXTBSY:
      return HTTP_SERVICE_UNAVAILABLE;

    case ETIMEDOUT:
      return HTTP_GATEWAY_TIMEOUT;

    case ECONNREFUSED:
    case ECONNRESET:
    case ENETDOWN:
    case ENETUNREACH:
    case EHOSTUNREACH:
    case EPIPE:
      return HTTP_BAD_GATEWAY;

    default:
      return HTTP_INTERNAL_SERVER_ERROR;
  }
}

std::string httpStatusToString(int status) {
  switch (status) {
    // 1xx Informational
    case 100: return "Continue";
    case 101: return "Switching Protocols";
    case 102: return "Processing";
    case 103: return "Early Hints";

    // 2xx Success
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 203: return "Non-Authoritative Information";
    case 204: return "No Content";
    case 205: return "Reset Content";
    case 206: return "Partial Content";
    case 207: return "Multi-Status";
    case 208: return "Already Reported";
    case 226: return "IM Used";

    // 3xx Redirection
    case 300: return "Multiple Choices";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 303: return "See Other";
    case 304: return "Not Modified";
    case 305: return "Use Proxy";
    case 307: return "Temporary Redirect";
    case 308: return "Permanent Redirect";

    // 4xx Client Errors
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 402: return "Payment Required";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 406: return "Not Acceptable";
    case 407: return "Proxy Authentication Required";
    case 408: return "Request Timeout";
    case 409: return "Conflict";
    case 410: return "Gone";
    case 411: return "Length Required";
    case 412: return "Precondition Failed";
    case 413: return "Payload Too Large";
    case 414: return "URI Too Long";
    case 415: return "Unsupported Media Type";
    case 416: return "Range Not Satisfiable";
    case 417: return "Expectation Failed";
    case 418: return "I'm a teapot";
    case 421: return "Misdirected Request";
    case 422: return "Unprocessable Entity";
    case 423: return "Locked";
    case 424: return "Failed Dependency";
    case 425: return "Too Early";
    case 426: return "Upgrade Required";
    case 428: return "Precondition Required";
    case 429: return "Too Many Requests";
    case 431: return "Request Header Fields Too Large";
    case 451: return "Unavailable For Legal Reasons";

    // 5xx Server Errors
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    case 504: return "Gateway Timeout";
    case 505: return "HTTP Version Not Supported";
    case 506: return "Variant Also Negotiates";
    case 507: return "Insufficient Storage";
    case 508: return "Loop Detected";
    case 510: return "Not Extended";
    case 511: return "Network Authentication Required";

    default:
      switch (status) {
        case 100 ... 199: return "Informational";
        case 200 ... 299: return "Success";
        case 300 ... 399: return "Redirection";
        case 400 ... 499: return "Client Error";
        case 500 ... 599: return "Server Error";
        default: return "Unknown";
      }
  }
}
