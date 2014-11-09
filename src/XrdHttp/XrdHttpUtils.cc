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



#include <string.h>
#include <openssl/hmac.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include <pthread.h>
#include <memory>
#include <vector>
#include <algorithm>

#include "XProtocol/XPtypes.hh"
#include "XrdSec/XrdSecEntity.hh"
# include "sys/param.h"
#include "XrdOuc/XrdOucString.hh"
static pthread_key_t cm_key;



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
  int l = min((int)(p2 - p), (int)sizeof (buf));
  strncpy(buf, p, l);
  buf[l] = '\0';

  // Now look for :
  p = strchr(buf, ':');
  if (p) {
    int l = min((int)(p - buf), (int)sizeof (buf));
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


  HMAC_CTX ctx;
  unsigned int len;
  unsigned char mdbuf[EVP_MAX_MD_SIZE];
  char buf[64];
  struct tm tms;

  // set so key destructor can trigger removal of
  // libcrypto error state when the thread finishes
  pthread_setspecific(cm_key, &cm_key);

  if (!hash) {
    return;
  }

  if (!key) {
    return;
  }

  hash[0] = '\0';

  if (!fn || !secent) {
    return;
  }

  HMAC_CTX_init(&ctx);




  HMAC_Init_ex(&ctx, (const void *) key, strlen(key), EVP_sha256(), 0);


  if (fn)
    HMAC_Update(&ctx, (const unsigned char *) fn,
          strlen(fn) + 1);

  HMAC_Update(&ctx, (const unsigned char *) &request,
          sizeof (request));

  if (secent->name)
    HMAC_Update(&ctx, (const unsigned char *) secent->name,
          strlen(secent->name) + 1);

  if (secent->vorg)
    HMAC_Update(&ctx, (const unsigned char *) secent->vorg,
          strlen(secent->vorg) + 1);

  if (secent->host)
    HMAC_Update(&ctx, (const unsigned char *) secent->host,
          strlen(secent->host) + 1);

  localtime_r(&tim, &tms);
  strftime(buf, sizeof (buf), "%s", &tms);
  HMAC_Update(&ctx, (const unsigned char *) buf,
          strlen(buf) + 1);

  HMAC_Final(&ctx, mdbuf, &len);

  Tobase64(mdbuf, len / 2, hash);

  HMAC_CTX_cleanup(&ctx);
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

char *quote(char *str) {
  int l = strlen(str);
  char *r = (char *) malloc(l + 1);
  r[0] = '\0';
  int i, j = 0;

  for (i = 0; i < l; i++) {
    char c = str[i];

    switch (c) {
      case ' ':
        strcpy(r + j, "%20");
        j += 3;
        break;
      default:
        r[j++] = c;
    }
  }

  r[j] = '\0';

  return r;
}






