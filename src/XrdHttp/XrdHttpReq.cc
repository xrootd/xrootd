//------------------------------------------------------------------------------
// This file is part of XrdHTTP: A pragmatic implementation of the
// HTTP/WebDAV protocol for the Xrootd framework
//
// Copyright (c) 2013 by European Organization for Nuclear Research (CERN)
// Author: Fabrizio Furano <furano@cern.ch>
// File Date: Nov 2012
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









/** @file  XrdHttpReq.cc
 * @brief  Main request/response class, handling the logical status of the communication
 * @author Fabrizio Furano
 * @date   Nov 2012
 * 
 * 
 * 
 */
#include "XrdVersion.hh"
#include "XrdHttpReq.hh"
#include "XrdHttpTrace.hh"
#include "XrdHttpExtHandler.hh"
#include <string.h>
#include <arpa/inet.h>
#include <sstream>
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdHttpProtocol.hh"
#include "Xrd/XrdLink.hh"
#include "XrdXrootd/XrdXrootdBridge.hh"
#include "Xrd/XrdBuffer.hh"

#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>
#include <string>

#include "XrdHttpUtils.hh"

#include "XrdHttpStatic.hh"

#define MAX_TK_LEN      256
#define MAX_RESOURCE_LEN 16384

// This is to fix the trace macros
#define TRACELINK prot->Link


static XrdOucString convert_digest_name(const std::string &rfc_name_multiple)
{
  std::stringstream rfc_name_multiple_ss;
  rfc_name_multiple_ss << rfc_name_multiple;
  for (std::string rfc_name; std::getline(rfc_name_multiple_ss, rfc_name, ','); ) {
    rfc_name.erase(rfc_name.find_last_not_of(" \n\r\t") + 1);
    rfc_name.erase(0, rfc_name.find_first_not_of(" \n\r\t"));
    rfc_name = rfc_name.substr(0, rfc_name.find(";"));
    if (!strcasecmp(rfc_name.c_str(), "md5")) {
      return "md5";
    } else if (!strcasecmp(rfc_name.c_str(), "adler32")) {
      return "adler32";
    } else if (!strcasecmp(rfc_name.c_str(), "SHA")) {
      return "sha1";
    } else if (!strcasecmp(rfc_name.c_str(), "SHA-256")) {
      return "sha256";
    } else if (!strcasecmp(rfc_name.c_str(), "SHA-512")) {
      return "sha512";
    } else if (!strcasecmp(rfc_name.c_str(), "UNIXcksum")) {
      return "cksum";
    }
  }
  return "unknown";
}


static std::string convert_xrootd_to_rfc_name(const std::string &xrootd_name)
{
  if (!strcasecmp(xrootd_name.c_str(), "md5")) {
    return "md5";
  } else if (!strcasecmp(xrootd_name.c_str(), "adler32")) {
    return "adler32";
  } else if (!strcasecmp(xrootd_name.c_str(), "sha1")) {
    return "SHA";
  } else if (!strcasecmp(xrootd_name.c_str(), "sha256")) {
    return "SHA-256";
  } else if (!strcasecmp(xrootd_name.c_str(), "sha512")) {
    return "SHA-512";
  } else if (!strcasecmp(xrootd_name.c_str(), "cksum")) {
    return "UNIXcksum";
  }
  return "unknown";
}


static bool needs_base64_padding(const std::string &rfc_name)
{
  if (!strcasecmp(rfc_name.c_str(), "md5")) {
    return true;
  } else if (!strcasecmp(rfc_name.c_str(), "adler32")) {
    return false;
  } else if (strcasecmp(rfc_name.c_str(), "SHA")) {
    return true;
  } else if (strcasecmp(rfc_name.c_str(), "SHA-256")) {
    return true;
  } else if (strcasecmp(rfc_name.c_str(), "SHA-512")) {
    return true;
  } else if (strcasecmp(rfc_name.c_str(), "UNIXcksum")) {
    return false;
  }
  return false;
}


void trim(std::string &str)
{
  // Trim leading non-letters
  while( str.size() && !isgraph(str[0]) ) str.erase(str.begin());

  // Trim trailing non-letters
  
  while( str.size() && !isgraph(str[str.size()-1]) )
    str.resize (str.size () - 1);

}


std::string ISOdatetime(time_t t) {
  char datebuf[128];
  struct tm t1;

  memset(&t1, 0, sizeof (t1));
  gmtime_r(&t, &t1);

  strftime(datebuf, 127, "%a, %d %b %Y %H:%M:%S GMT", &t1);
  return (string) datebuf;

}

int XrdHttpReq::parseBody(char *body, long long len) {
  /*
   * The document being in memory, it has no base per RFC 2396,
   * and the "noname.xml" argument will serve as its base.
   */
  //xmlbody = xmlReadMemory(body, len, "noname.xml", NULL, 0);
  //if (xmlbody == NULL) {
  //  fprintf(stderr, "Failed to parse document\n");
  //  return 1;
  //}



  return 1;
}

XrdHttpReq::~XrdHttpReq() {
  //if (xmlbody) xmlFreeDoc(xmlbody);

  reset();
}

int XrdHttpReq::parseLine(char *line, int len) {

  char *key = line;
  int pos;

  // Do the parsing
  if (!line) return -1;


  char *p = strchr((char *) line, (int) ':');
  if (!p) {

    request = rtMalformed;
    return 0;
  }

  pos = (p - line);
  if (pos > (MAX_TK_LEN - 1)) {

    request = rtMalformed;
    return -2;
  }

  if (pos > 0) {
    line[pos] = 0;
    char *val = line + pos + 1;

    // Trim left
    while ( (!isgraph(*val) || (!*val)) && (val < line+len)) val++;

    // Here we are supposed to initialize whatever flag or variable that is needed
    // by looking at the first token of the line
    // The token is key
    // The value is val
    
    // Screen out the needed header lines
    if (!strcmp(key, "Connection")) {

      if (!strcasecmp(val, "Keep-Alive\r\n")) {
        keepalive = true;
      } else if (!strcasecmp(val, "close\r\n")) {
        keepalive = false;
      }

    } else if (!strcmp(key, "Host")) {
      parseHost(val);
    } else if (!strcmp(key, "Range")) {
      parseContentRange(val);
    } else if (!strcmp(key, "Content-Length")) {
      length = atoll(val);

    } else if (!strcmp(key, "Destination")) {
      destination.assign(val, line+len-val);
      trim(destination);
    } else if (!strcmp(key, "Want-Digest")) {
      m_req_digest.assign(val, line + len - val);
      trim(m_req_digest);
    } else if (!strcmp(key, "Depth")) {
      depth = -1;
      if (strcmp(val, "infinity"))
        depth = atoll(val);

    } else if (!strcmp(key, "Expect") && strstr(val, "100-continue")) {
      sendcontinue = true;
    } else if (!strcasecmp(key, "Transfer-Encoding") && strstr(val, "chunked")) {
      m_transfer_encoding_chunked = true;
    } else {
      // Some headers need to be translated into "local" cgi info. In theory they should already be quoted
      std::map< std:: string, std:: string > ::iterator it = prot->hdr2cgimap.find(key);
      if (it != prot->hdr2cgimap.end()) {
        std:: string s;
        s.assign(val, line+len-val);
        trim(s);
        
        if (hdr2cgistr.length() > 0) {
          hdr2cgistr.append("&");
        }
        hdr2cgistr.append(it->second);
        hdr2cgistr.append("=");
        hdr2cgistr.append(s);
        
          
      }
    }

    // We memorize the heaers also as a string
    // because external plugins may need to process it differently
    std::string ss = val;
    trim(ss);
    allheaders[key] = ss;
    line[pos] = ':';
  }

  return 0;
}

int XrdHttpReq::parseHost(char *line) {
  host = line;
  trim(host);
  return 0;
}

int XrdHttpReq::parseContentRange(char *line) {
  int j;
  char *str1, *saveptr1, *token;



  for (j = 1, str1 = line;; j++, str1 = NULL) {
    token = strtok_r(str1, " ,\n=", &saveptr1);
    if (token == NULL)
      break;

    //printf("%d: %s\n", j, token);

    if (!strlen(token)) continue;


    parseRWOp(token);

  }

  return j;
}

int XrdHttpReq::parseRWOp(char *str) {
  ReadWriteOp o1;
  int j;
  char *saveptr2, *str2, *subtoken, *endptr;
  bool ok = false;

  for (str2 = str, j = 0;; str2 = NULL, j++) {
    subtoken = strtok_r(str2, "-", &saveptr2);
    if (subtoken == NULL)
      break;

    switch (j) {
      case 0:
        o1.bytestart = strtoll(subtoken, &endptr, 0);
        if (!o1.bytestart && (endptr == subtoken)) o1.bytestart = -1;
        break;
      case 1:
        o1.byteend = strtoll(subtoken, &endptr, 0);
        if (!o1.byteend && (endptr == subtoken)) o1.byteend = -1;
        ok = true;
        break;
      default:
        // Malformed!
        ok = false;
        break;
    }

  }


  // This can be largely optimized
  if (ok) {

    kXR_int32 len_ok = 0;
    long long sz = o1.byteend - o1.bytestart + 1;
    kXR_int32 newlen = sz;

    if (filesize > 0)
      newlen = (kXR_int32) min(filesize - o1.bytestart, sz);

    rwOps.push_back(o1);

    while (len_ok < newlen) {
      ReadWriteOp nfo;
      int len = min(newlen - len_ok, READV_MAXCHUNKSIZE);

      nfo.bytestart = o1.bytestart + len_ok;
      nfo.byteend = nfo.bytestart + len - 1;
      len_ok += len;
      rwOps_split.push_back(nfo);
    }
    length += len_ok;


  }


  return j;
}

int XrdHttpReq::parseFirstLine(char *line, int len) {

  char *key = line;

  int pos;

  // Do the naive parsing
  if (!line) return -1;

  // Look for the first space-delimited token
  char *p = strchr((char *) line, (int) ' ');
  if (!p) {
    request = rtMalformed;
    return -1;
  }

  // The first token cannot be too long
  pos = p - line;
  if (pos > MAX_TK_LEN - 1) {
    request = rtMalformed;
    return -2;
  }

  // the first token must be non empty
  if (pos > 0) {
    line[pos] = 0;
    char *val = line + pos + 1;

    // Here we are supposed to initialize whatever flag or variable that is needed
    // by looking at the first token of the line

    // The token is key
    // The remainder is val, look for the resource
    p = strchr((char *) val, (int) ' ');

    if (!p) {
      request = rtMalformed;
      line[pos] = ' ';
      return -3;
    }

    *p = '\0';
    parseResource(val);

    *p = ' ';

    // Xlate the known header lines
    if (!strcmp(key, "GET")) {
      request = rtGET;
    } else if (!strcmp(key, "HEAD")) {
      request = rtHEAD;
    } else if (!strcmp(key, "PUT")) {
      request = rtPUT;
    } else if (!strcmp(key, "POST")) {
      request = rtPOST;
    } else if (!strcmp(key, "PATCH")) {
      request = rtPATCH;
    } else if (!strcmp(key, "OPTIONS")) {
      request = rtOPTIONS;
    } else if (!strcmp(key, "DELETE")) {
      request = rtDELETE;
    } else if (!strcmp(key, "PROPFIND")) {
      request = rtPROPFIND;

    } else if (!strcmp(key, "MKCOL")) {
      request = rtMKCOL;

    } else if (!strcmp(key, "MOVE")) {
      request = rtMOVE;
    } else {
      request = rtUnknown;
    }
    
    requestverb = key;

    // The last token should be the protocol.  If it is HTTP/1.0, then
    // keepalive is disabled by default.
    if (!strcmp(p+1, "HTTP/1.0\r\n")) {
      keepalive = false;
    }
    line[pos] = ' ';
  }

  return 0;
}




//___________________________________________________________________________

void XrdHttpReq::clientMarshallReadAheadList(int nitems) {
  // This function applies the network byte order on the
  // vector of read-ahead information
  kXR_int64 tmpl;



  for (int i = 0; i < nitems; i++) {
    memcpy(&tmpl, &(ralist[i].offset), sizeof (kXR_int64));
    tmpl = htonll(tmpl);
    memcpy(&(ralist[i].offset), &tmpl, sizeof (kXR_int64));
    ralist[i].rlen = htonl(ralist[i].rlen);
  }
}


//___________________________________________________________________________

void XrdHttpReq::clientUnMarshallReadAheadList(int nitems) {
  // This function applies the network byte order on the
  // vector of read-ahead information
  kXR_int64 tmpl;



  for (int i = 0; i < nitems; i++) {
    memcpy(&tmpl, &(ralist[i].offset), sizeof (kXR_int64));
    tmpl = ntohll(tmpl);
    memcpy(&(ralist[i].offset), &tmpl, sizeof (kXR_int64));
    ralist[i].rlen = ntohl(ralist[i].rlen);
  }
}

int XrdHttpReq::ReqReadV() {


  kXR_int64 total_len = 0;
  rwOpPartialDone = 0;
  // Now we build the protocol-ready read ahead list
  //  and also put the correct placeholders inside the cache
  int n = rwOps_split.size();
  if (!ralist) ralist = (readahead_list *) malloc(n * sizeof (readahead_list));

  int j = 0;
  for (int i = 0; i < n; i++) {

    // We can suppose that we know the length of the file
    // Hence we can sort out requests that are out of boundary or trim them
    if (rwOps_split[i].bytestart > filesize) continue;
    if (rwOps_split[i].byteend > filesize - 1) rwOps_split[i].byteend = filesize - 1;

    memcpy(&(ralist[j].fhandle), this->fhandle, 4);

    ralist[j].offset = rwOps_split[i].bytestart;
    ralist[j].rlen = rwOps_split[i].byteend - rwOps_split[i].bytestart + 1;
    total_len += ralist[j].rlen;
    j++;
  }

  if (j > 0) {

    // Prepare a request header 

    memset(&xrdreq, 0, sizeof (xrdreq));

    xrdreq.header.requestid = htons(kXR_readv);
    xrdreq.readv.dlen = htonl(j * sizeof (struct readahead_list));

    clientMarshallReadAheadList(j);


  }

  return (j * sizeof (struct readahead_list));
}

std::string XrdHttpReq::buildPartialHdr(long long bytestart, long long byteend, long long fsz, char *token) {
  ostringstream s;

  s << "\r\n--" << token << "\r\n";
  s << "Content-type: text/plain; charset=UTF-8\r\n";
  s << "Content-range: bytes " << bytestart << "-" << byteend << "/" << fsz << "\r\n\r\n";

  return s.str();
}

std::string XrdHttpReq::buildPartialHdrEnd(char *token) {
  ostringstream s;

  s << "\r\n--" << token << "--\r\n";

  return s.str();
}

bool XrdHttpReq::Data(XrdXrootd::Bridge::Context &info, //!< the result context
        const
        struct iovec *iovP_, //!< pointer to data array
        int iovN_, //!< array count
        int iovL_, //!< byte  count
        bool final_ //!< true -> final result
        ) {

  TRACE(REQ, " XrdHttpReq::Data! final=" << final);

  this->xrdresp = kXR_ok;
  this->iovP = iovP_;
  this->iovN = iovN_;
  this->iovL = iovL_;
  this->final = final_;

  if (PostProcessHTTPReq(final_)) reset();

  return true;

};

int XrdHttpReq::File(XrdXrootd::Bridge::Context &info, //!< the result context
        int dlen //!< byte  count
        ) {

  //prot->SendSimpleResp(200, NULL, NULL, NULL, dlen);
  int rc = info.Send(0, 0, 0, 0);
  TRACE(REQ, " XrdHttpReq::File dlen:" << dlen << " send rc:" << rc);
  if (rc) return false;
  writtenbytes += dlen;
  
    
  return true;
};

bool XrdHttpReq::Done(XrdXrootd::Bridge::Context & info) {

  TRACE(REQ, " XrdHttpReq::Done");

  xrdresp = kXR_ok;
  
  
  int r = PostProcessHTTPReq(true);
  // Beware, we don't have to reset() if the result is 0
  if (r) reset();
  if (r < 0) return false; 
  
  
  return true;
};

bool XrdHttpReq::Error(XrdXrootd::Bridge::Context &info, //!< the result context
        int ecode, //!< the "kXR" error code
        const char *etext_ //!< associated error message
        ) {

  TRACE(REQ, " XrdHttpReq::Error");

  xrdresp = kXR_error;
  xrderrcode = (XErrorCode) ecode;
  this->etext = etext_;


  if (PostProcessHTTPReq()) reset();

  // Second part of the ugly hack on stat()
  if ((request == rtGET) && (xrdreq.header.requestid == ntohs(kXR_stat)))
    return true;
  
  return false;
};

bool XrdHttpReq::Redir(XrdXrootd::Bridge::Context &info, //!< the result context
        int port, //!< the port number
        const char *hname //!< the destination host
        ) {



  char buf[512];
  char hash[512];
  hash[0] = '\0';
  
  if (prot->isdesthttps)
    redirdest = "Location: https://";
  else
    redirdest = "Location: http://";
  
  // Beware, certain Ofs implementations (e.g. EOS) add opaque data directly to the host name
  // This must be correctly treated here and appended to the opaque info
  // that we may already have
  char *pp = strchr((char *)hname, '?');
  char *vardata = 0;
  if (pp) {
    *pp = '\0';
    redirdest += hname;
    vardata = pp+1;
    int varlen = strlen(vardata);
    
    //Now extract the remaining, vardata points to it
    while(*vardata == '&' && varlen) {vardata++; varlen--;}
    
    // Put the question mark back where it was
    *pp = '?';
  }
  else
    redirdest += hname;

  if (port) {
    sprintf(buf, ":%d", port);
    redirdest += buf;
  }

  redirdest += resource.c_str();
  
  // Here we put back the opaque info, if any
  if (vardata) {
    redirdest += "?&";
    redirdest += vardata;
  }
  
  // Shall we put also the opaque data of the request? Maybe not
  //int l;
  //if (opaque && opaque->Env(l))
  //  redirdest += opaque->Env(l);


  time_t timenow = 0;
  if (!prot->isdesthttps && prot->ishttps) {
    // If the destination is not https, then we suppose that it
    // will need this token to fill its authorization info
    timenow = time(0);
    calcHashes(hash, this->resource.c_str(), (kXR_int16) request,
            &prot->SecEntity,
            timenow,
            prot->secretkey);
  }

  if (hash[0]) {
    appendOpaque(redirdest, &prot->SecEntity, hash, timenow);
  } else
    appendOpaque(redirdest, 0, 0, 0);

  
  TRACE(REQ, " XrdHttpReq::Redir Redirecting to " << redirdest);

  prot->SendSimpleResp(302, NULL, (char *) redirdest.c_str(), 0, 0, keepalive);

  reset();
  return false;
};


void XrdHttpReq::appendOpaque(XrdOucString &s, XrdSecEntity *secent, char *hash, time_t tnow) {

  int l = 0;
  char * p = 0;
  if (opaque)
    p = opaque->Env(l);

  if ((l < 2) && !hash) return;

  // this works in most cases, except if the url already contains the xrdhttp tokens
  s = s + "?";
  if (p && (l > 1)) {
    char *s1 = quote(p+1);
    if (s1) {
      s = s + s1;
      free(s1);
    }
  }



  if (hash) {
    if (l > 1) s += "&";
    s += "xrdhttptk=";
    s += hash;

    s += "&xrdhttptime=";
    char buf[256];
    sprintf(buf, "%ld", tnow);
    s += buf;

    if (secent) {
      if (secent->name) {
        s += "&xrdhttpname=";
        char *s1 = quote(secent->name);
        if (s1) {
          s += s1;
          free(s1);
        }
      }

      if (secent->vorg) {
        s += "&xrdhttpvorg=";
        s += secent->vorg;
      }

      if (secent->host) {
        s += "&xrdhttphost=";
        char *s1 = quote(secent->host);
        if (s1) {
          s += s1;
          free(s1);
        }
      }
      
      if (secent->moninfo) {
        s += "&xrdhttpdn=";
        char *s1 = quote(secent->moninfo);
        if (s1) {
          s += s1;
          free(s1);
        }
      }

      if (secent->role) {
        s += "&xrdhttprole=";
        char *s1 = quote(secent->role);
        if (s1) {
          s += s1;
          free(s1);
        }
      }
      
      if (secent->grps) {
        s += "&xrdhttpgrps=";
        char *s1 = quote(secent->grps);
        if (s1) {
          s += s1;
          free(s1);
        }
      }
      
      if (secent->endorsements) {
        s += "&xrdhttpendorsements=";
        char *s1 = quote(secent->endorsements);
        if (s1) {
          s += s1;
          free(s1);
        }
      }
      
      if (secent->credslen) {
        s += "&xrdhttpcredslen=";
        char buf[16];
        sprintf(buf, "%d", secent->credslen);
        char *s1 = quote(buf);
        if (s1) {
          s += s1;
          free(s1);
        }
      }
      
      if (secent->credslen) {
        if (secent->creds) {
          s += "&xrdhttpcreds=";
          // Apparently this string might be not 0-terminated (!)
          char *zerocreds = strndup(secent->creds, secent->credslen);
          if (zerocreds) {
            char *s1 = quote(zerocreds);
            if (s1) {
              s += s1;
              free(s1);
            }
            free(zerocreds);
          }
        }
      }
      
    }
  }

}

// Extracts the opaque info from the given url

void XrdHttpReq::parseResource(char *res) {
  // Look for the first '?'
  char *p = strchr(res, '?');
  
  // Not found, then it's just a filename
  if (!p) {
    resource.assign(res, 0);
    char *buf = unquote((char *)resource.c_str());
    resource.assign(buf, 0);
    resourceplusopaque.assign(buf, 0);
    free(buf);
    
    // Sanitize the resource string, removing double slashes
    int pos = 0;
    do { 
      pos = resource.find("//", pos);
      if (pos != STR_NPOS)
        resource.erase(pos, 1);
    } while (pos != STR_NPOS);
    
    return;
  }

  // Whatever comes before '?' is a filename

  int cnt = p - res; // Number of chars to copy
  resource.assign(res, 0, cnt - 1);

  char *buf = unquote((char *)resource.c_str());
  resource.assign(buf, 0);
  free(buf);
      
  // Sanitize the resource string, removing double slashes
  int pos = 0;
  do { 
    pos = resource.find("//", pos);
    if (pos != STR_NPOS)
      resource.erase(pos, 1);
  } while (pos != STR_NPOS);
  
  resourceplusopaque = resource;
  // Whatever comes after is opaque data to be parsed
  if (strlen(p) > 1) {
    buf = unquote(p + 1);
    opaque = new XrdOucEnv(buf);
    resourceplusopaque.append('?');
    resourceplusopaque.append(buf);
    free(buf);
  }
  
  
  
}

// Map an XRootD error code to an appropriate HTTP status code and message
// The variables httpStatusCode and httpStatusText will be populated

void XrdHttpReq::mapXrdErrorToHttpStatus() {
  // Set default HTTP status values for an error case
  httpStatusCode = 500;
  httpStatusText = "Unrecognized error";

  // Do error mapping
  if (xrdresp == kXR_error) {
    switch (xrderrcode) {
      case kXR_NotAuthorized:
        httpStatusCode = 403; httpStatusText = "Operation not permitted";
        break;
      case kXR_NotFound:
        httpStatusCode = 404; httpStatusText = "File not found";
        break;
      case kXR_Unsupported:
        httpStatusCode = 405; httpStatusText = "Operation not supported";
        break;
      case kXR_FileLocked:
        httpStatusCode = 423; httpStatusText = "Resource is a locked";
        break;
      case kXR_isDirectory:
        httpStatusCode = 409; httpStatusText = "Resource is a directory";
        break;
      default:
        break;
    }

    if (!etext.empty()) httpStatusText = etext;

    TRACEI(REQ, "PostProcessHTTPReq mapping Xrd error [" << xrderrcode
                 << "] to status code [" << httpStatusCode << "]");

    httpStatusText += "\n";
  }
}

int XrdHttpReq::ProcessHTTPReq() {

  kXR_int32 l;

  
  
  // Verify if we have an external handler for this request
  if (reqstate == 0) {
    XrdHttpExtHandler *exthandler = prot->FindMatchingExtHandler(*this);
    if (exthandler) {
      XrdHttpExtReq xreq(this, prot);
      int r = exthandler->ProcessReq(xreq);
      reset();
      if (!r) return 1; // All went fine, response sent
      if (r < 0) return -1; // There was a hard error... close the connection

      return 1; // There was an error and a response was sent
    }
  }
  
  /// If we have to add extra header information, add it here.
  if (!hdr2cgistr.empty()) {
    const char *p = strchr(resourceplusopaque.c_str(), '?');
    if (p) {
      resourceplusopaque.append("&");
    } else {
      resourceplusopaque.append("?");
    }
    
    char *q = quote(hdr2cgistr.c_str());
    resourceplusopaque.append(q);
    TRACEI(DEBUG, "Appended header fields to opaque info: '" << hdr2cgistr << "'");
    free(q);
    
    // Once we've appended the authorization to the full resource+opaque string,
    // reset the authz to empty: this way, any operation that triggers repeated ProcessHTTPReq
    // calls won't also trigger multiple copies of the authz.
    hdr2cgistr = "";
    }
  
  //
  // Here we process the request locally
  //

  switch (request) {
    case XrdHttpReq::rtUnset:
    case XrdHttpReq::rtUnknown:
    {
      prot->SendSimpleResp(400, NULL, NULL, (char *) "Request unknown", 0, false);
      reset();
      return -1;
    }
    case XrdHttpReq::rtMalformed:
    {
      prot->SendSimpleResp(400, NULL, NULL, (char *) "Request malformed", 0, false);
      reset();
      return -1;
    }
    case XrdHttpReq::rtHEAD:
    {
      if (reqstate == 0) {
        // Always start with Stat; in the case of a checksum request, we'll have a follow-up query
        if (prot->doStat((char *) resourceplusopaque.c_str())) {
          prot->SendSimpleResp(404, NULL, NULL, (char *) "Could not run request.", 0, false);
          return -1;
        }
        return 0;
      } else {
        const char *opaque = strchr(resourceplusopaque.c_str(), '?');
        // Note that doChksum requires that the memory stays alive until the callback is invoked.
        m_resource_with_digest = resourceplusopaque;
        if (!opaque) {
          m_resource_with_digest += "?cks.type=";
          m_resource_with_digest += convert_digest_name(m_req_digest);
        } else {
          m_resource_with_digest += "&cks.type=";
          m_resource_with_digest += convert_digest_name(m_req_digest);
        }
        if (prot->doChksum(m_resource_with_digest) < 0) {
          // In this case, the Want-Digest header was set and PostProcess gave the go-ahead to do a checksum.
          prot->SendSimpleResp(500, NULL, NULL, (char *) "Failed to create initial checksum request.", 0, false);
          return -1;
        }
        return 1;
      }
    }
    case XrdHttpReq::rtGET:
    {

        if (resource.beginswith("/static/")) {

            // This is a request for a /static resource
            // If we have to use the embedded ones then we return the ones in memory as constants

            // The sysadmin can always redirect the request to another host that
            // contains his static resources

            // We also allow xrootd to preread from the local disk all the files
            // that have to be served as static resources.

            if (prot->embeddedstatic) {

                // Default case: the icon and the css of the HTML rendering of XrdHttp
                if (resource == "/static/css/xrdhttp.css") {
                    prot->SendSimpleResp(200, NULL, NULL, (char *) static_css_xrdhttp_css, static_css_xrdhttp_css_len, keepalive);
                    reset();
                    return keepalive ? 1 : -1;
                  }
                if (resource == "/static/icons/xrdhttp.ico") {
                    prot->SendSimpleResp(200, NULL, NULL, (char *) favicon_ico, favicon_ico_len, keepalive);
                    reset();
                    return keepalive ? 1 : -1;
                  }

              }

              // If we are here then none of the embedded resources match (or they are disabled)
              // We may have to redirect to a host that is supposed to serve the static resources
              if (prot->staticredir) {

                  XrdOucString s = "Location: ";
                  s.append(prot->staticredir);

                  if (s.endswith('/'))
                    s.erasefromend(1);

                  s.append(resource);
                  appendOpaque(s, 0, 0, 0);

                  prot->SendSimpleResp(302, NULL, (char *) s.c_str(), 0, 0, false);
                  return -1;


                } else {

                  // We lookup the requested path in a hash containing the preread files
                  if (prot->staticpreload) {
                    XrdHttpProtocol::StaticPreloadInfo *mydata = prot->staticpreload->Find(resource.c_str());
                    if (mydata) {
                      prot->SendSimpleResp(200, NULL, NULL, (char *) mydata->data, mydata->len, keepalive);
                      reset();
                      return keepalive ? 1 : -1;
                    }
                  }
                  
                }


          }
      
      switch (reqstate) {
        case 0: // Stat()
          
          // Do a Stat
          if (prot->doStat((char *) resourceplusopaque.c_str())) {
            XrdOucString errmsg = "Error stating";
            errmsg += resource.c_str();
            prot->SendSimpleResp(404, NULL, NULL, (char *) errmsg.c_str(), 0, false);
            return -1;
          }

          return 0;
        case 1: // Open() or dirlist
        {

          if (fileflags & kXR_isDir) {

            if (prot->listdeny) {
              prot->SendSimpleResp(503, NULL, NULL, (char *) "Listings are disabled.", 0, false);
              return -1;
            }

            if (prot->listredir) {
              XrdOucString s = "Location: ";
              s.append(prot->listredir);

              if (s.endswith('/'))
                s.erasefromend(1);

              s.append(resource);
              appendOpaque(s, 0, 0, 0);

              prot->SendSimpleResp(302, NULL, (char *) s.c_str(), 0, 0, false);
              return -1;
            }


            string res;
            res = resourceplusopaque.c_str();
            //res += "?xrd.dirstat=1";

            // --------- DIRLIST
            memset(&xrdreq, 0, sizeof (ClientRequest));
            xrdreq.dirlist.requestid = htons(kXR_dirlist);
            xrdreq.dirlist.options[0] = kXR_dstat;
            l = res.length() + 1;
            xrdreq.dirlist.dlen = htonl(l);

            if (!prot->Bridge->Run((char *) &xrdreq, (char *) res.c_str(), l)) {
              prot->SendSimpleResp(404, NULL, NULL, (char *) "Could not run request.", 0, false);
              return -1;
            }

            // We don't want to be invoked again after this request is finished
            return 1;

          } else if (!m_req_digest.empty()) {
            // In this case, the Want-Digest header was set.
            bool has_opaque = strchr(resourceplusopaque.c_str(), '?');
            // Note that doChksum requires that the memory stays alive until the callback is invoked.
            m_resource_with_digest = resourceplusopaque;
            if (has_opaque) {
              m_resource_with_digest += "&cks.type=";
              m_resource_with_digest += convert_digest_name(m_req_digest);
            } else {
              m_resource_with_digest += "?cks.type=";
              m_resource_with_digest += convert_digest_name(m_req_digest);
            }
            if (prot->doChksum(m_resource_with_digest) < 0) {
              prot->SendSimpleResp(500, NULL, NULL, (char *) "Failed to start internal checksum request to satisfy Want-Digest header.", 0, false);
              return -1;
            }
            return 0;
          }
          else {


            // --------- OPEN
            memset(&xrdreq, 0, sizeof (ClientRequest));
            xrdreq.open.requestid = htons(kXR_open);
            l = resourceplusopaque.length() + 1;
            xrdreq.open.dlen = htonl(l);
            xrdreq.open.mode = 0;
            xrdreq.open.options = htons(kXR_retstat | kXR_open_read);

            if (!prot->Bridge->Run((char *) &xrdreq, (char *) resourceplusopaque.c_str(), l)) {
              prot->SendSimpleResp(404, NULL, NULL, (char *) "Could not run request.", 0, false);
              return -1;
            }

            // Prepare to chunk up the request
            writtenbytes = 0;
            
            // We want to be invoked again after this request is finished
            return 0;
          }


        }
        case 2:  // Open() in the case the user also requested a checksum.
        {
          if (!m_req_digest.empty()) {
            // --------- OPEN
            memset(&xrdreq, 0, sizeof (ClientRequest));
            xrdreq.open.requestid = htons(kXR_open);
            l = resourceplusopaque.length() + 1;
            xrdreq.open.dlen = htonl(l);
            xrdreq.open.mode = 0;
            xrdreq.open.options = htons(kXR_retstat | kXR_open_read);

            if (!prot->Bridge->Run((char *) &xrdreq, (char *) resourceplusopaque.c_str(), l)) {
              prot->SendSimpleResp(404, NULL, NULL, (char *) "Could not run request.", 0, false);
              return -1;
            }

            // Prepare to chunk up the request
            writtenbytes = 0;

            // We want to be invoked again after this request is finished
            return 0;
          }
        }
        // fallthrough
        default: // Read() or Close()
        {

          if ( ((reqstate == 3 || (!m_req_digest.empty() && (reqstate == 4))) && (rwOps.size() > 1)) ||
            (writtenbytes >= length) ) {

            // Close() if this was a readv or we have finished, otherwise read the next chunk

            // --------- CLOSE

            memset(&xrdreq, 0, sizeof (ClientRequest));
            xrdreq.close.requestid = htons(kXR_close);
            memcpy(xrdreq.close.fhandle, fhandle, 4);

            if (!prot->Bridge->Run((char *) &xrdreq, 0, 0)) {
              prot->SendSimpleResp(404, NULL, NULL, (char *) "Could not run close request.", 0, false);
              return -1;
            }

            // We have finished
            return 1;

          }
	  
          if (rwOps.size() <= 1) {
            // No chunks or one chunk... Request the whole file or single read

            long l;
            long long offs;
            
            // --------- READ
            memset(&xrdreq, 0, sizeof (xrdreq));
            xrdreq.read.requestid = htons(kXR_read);
            memcpy(xrdreq.read.fhandle, fhandle, 4);
            xrdreq.read.dlen = 0;
            
            if (rwOps.size() == 0) {
              l = (long)min(filesize-writtenbytes, (long long)1024*1024);
              offs = writtenbytes;
              xrdreq.read.offset = htonll(writtenbytes);
              xrdreq.read.rlen = htonl(l);
            } else {
              l = min(rwOps[0].byteend - rwOps[0].bytestart + 1 - writtenbytes, (long long)1024*1024);
              offs = rwOps[0].bytestart + writtenbytes;
              xrdreq.read.offset = htonll(offs);
              xrdreq.read.rlen = htonl(l);
            }

            if (prot->ishttps) {
              if (!prot->Bridge->setSF((kXR_char *) fhandle, false)) {
                TRACE(REQ, " XrdBridge::SetSF(false) failed.");

              }
            }


            
            if (l <= 0) {
              if (l < 0) {
                TRACE(ALL, " Data sizes mismatch.");
                return -1;
              }
              else {
                TRACE(ALL, " No more bytes to send.");
                reset();
                return 1;
              }
            }

            if ((offs >= filesize) || (offs+l > filesize)) {
              TRACE(ALL, " Requested range " << l << "@" << offs <<
              " is past the end of file (" << filesize << ")");
              //prot->SendSimpleResp(522, NULL, NULL, (char *) "Invalid range request", 0);
              return -1;
            }
            
            if (!prot->Bridge->Run((char *) &xrdreq, 0, 0)) {
              prot->SendSimpleResp(404, NULL, NULL, (char *) "Could not run read request.", 0, false);
              return -1;
            }
          } else {
            // More than one chunk to read... use readv

            length = ReqReadV();

            if (!prot->Bridge->Run((char *) &xrdreq, (char *) ralist, length)) {
              prot->SendSimpleResp(404, NULL, NULL, (char *) "Could not run read request.", 0, false);
              return -1;
            }

          }

          // We want to be invoked again after this request is finished
          return 0;
        }
        
      }


    }

    case XrdHttpReq::rtPUT:
    {
      //if (prot->ishttps) {
      //prot->SendSimpleResp(501, NULL, NULL, (char *) "HTTPS not supported yet for direct writing. Sorry.", 0);
      //return -1;
      //}

      if (!fopened) {

        // --------- OPEN for write!
        memset(&xrdreq, 0, sizeof (ClientRequest));
        xrdreq.open.requestid = htons(kXR_open);
        l = resourceplusopaque.length() + 1;
        xrdreq.open.dlen = htonl(l);
        xrdreq.open.mode = htons(kXR_ur | kXR_uw | kXR_gw | kXR_gr | kXR_or);
        xrdreq.open.options = htons(kXR_mkpath | kXR_open_wrto | kXR_delete);

        if (!prot->Bridge->Run((char *) &xrdreq, (char *) resourceplusopaque.c_str(), l)) {
          prot->SendSimpleResp(404, NULL, NULL, (char *) "Could not run request.", 0, keepalive);
          return -1;
        }


        // We want to be invoked again after this request is finished
        // Only if there is data to fetch from the socket
        if (prot->BuffUsed() > 0)
          return 0;

        return 1;

      } else {

        if (m_transfer_encoding_chunked) {
          if (m_current_chunk_size == m_current_chunk_offset) {
            // Chunk has been consumed; we now must process the CRLF.
            // Note that we don't support trailer headers.
            if (prot->BuffUsed() < 2) return 1;
            if (prot->myBuffStart[0] != '\r' || prot->myBuffStart[1] != '\n') {
              prot->SendSimpleResp(400, NULL, NULL, (char *) "Invalid trailing chunk encoding.", 0, keepalive);
              return -1;
            }
            prot->BuffConsume(2);
            if (m_current_chunk_size == 0) {
              // All data has been sent.  Turn off chunk processing and
              // set the bytes written and length appropriately; on next callback,
              // we will hit the close() block below.
              m_transfer_encoding_chunked = false;
              length = writtenbytes;
              return ProcessHTTPReq();
            }
            m_current_chunk_size = -1;
            m_current_chunk_offset = 0;
            // If there is more data, we try to process the next chunk; otherwise, return
            if (!prot->BuffUsed()) return 1;
          }
          if (-1 == m_current_chunk_size) {

              // Parse out the next chunk size.
            long long idx = 0;
            bool found_newline = false;
            for (; idx < prot->BuffAvailable(); idx++) {
              if (prot->myBuffStart[idx] == '\n') {
                found_newline = true;
                break;
              }
            }
            if ((idx == 0) || prot->myBuffStart[idx-1] != '\r') {
              prot->SendSimpleResp(400, NULL, NULL, (char *)"Invalid chunked encoding", 0, false);
              return -1;
            }
            if (found_newline) {
              char *endptr = NULL;
              std::string line_contents(prot->myBuffStart, idx);
              long long chunk_contents = strtol(line_contents.c_str(), &endptr, 16);
                // Chunk sizes can be followed by trailer information or CRLF
              if (*endptr != ';' && *endptr != '\r') {
                prot->SendSimpleResp(400, NULL, NULL, (char *)"Invalid chunked encoding", 0, false);
                return -1;
              }
              m_current_chunk_size = chunk_contents;
              m_current_chunk_offset = 0;
              prot->BuffConsume(idx + 1);
              TRACE(REQ, "XrdHTTP PUT: next chunk from client will be " << m_current_chunk_size << " bytes");
            } else {
                // Need more data!
              return 1;
            }
          }

          if (m_current_chunk_size == 0) {
            // All data has been sent.  Invoke this routine again immediately to process CRLF
            return ProcessHTTPReq();
          } else {
            // At this point, we have a chunk size defined and should consume payload data
            memset(&xrdreq, 0, sizeof (xrdreq));
            xrdreq.write.requestid = htons(kXR_write);
            memcpy(xrdreq.write.fhandle, fhandle, 4);

            long long chunk_bytes_remaining = m_current_chunk_size - m_current_chunk_offset;
            long long bytes_to_write = min(static_cast<long long>(prot->BuffUsed()),
                                           chunk_bytes_remaining);

            xrdreq.write.offset = htonll(writtenbytes);
            xrdreq.write.dlen = htonl(bytes_to_write);

            TRACEI(REQ, "Writing chunk of size " << bytes_to_write << " starting with '" << *(prot->myBuffStart) << "'");
            if (!prot->Bridge->Run((char *) &xrdreq, prot->myBuffStart, bytes_to_write)) {
              prot->SendSimpleResp(500, NULL, NULL, (char *) "Could not run write request.", 0, false);
              return -1;
            }
            // If there are more bytes in the buffer, then immediately call us after the
            // write is finished; otherwise, wait for data.
            return (prot->BuffUsed() > chunk_bytes_remaining) ? 0 : 1;
          }
        } else if (writtenbytes < length) {


          // --------- WRITE
          memset(&xrdreq, 0, sizeof (xrdreq));
          xrdreq.write.requestid = htons(kXR_write);
          memcpy(xrdreq.write.fhandle, fhandle, 4);

          long long bytes_to_read = min(static_cast<long long>(prot->BuffUsed()),
                                        length - writtenbytes);

          xrdreq.write.offset = htonll(writtenbytes);
          xrdreq.write.dlen = htonl(bytes_to_read);

          TRACEI(REQ, "Writing " << bytes_to_read);
          if (!prot->Bridge->Run((char *) &xrdreq, prot->myBuffStart, bytes_to_read)) {
            prot->SendSimpleResp(404, NULL, NULL, (char *) "Could not run write request.", 0, false);
            return -1;
          }

          if (writtenbytes + prot->BuffUsed() >= length)
            // Trigger an immediate recall after this request has finished
            return 0;
          else
            // We want to be invoked again after this request is finished
            // only if there is pending data
            return 1;



        } else {

          // --------- CLOSE
          memset(&xrdreq, 0, sizeof (ClientRequest));
          xrdreq.close.requestid = htons(kXR_close);
          memcpy(xrdreq.close.fhandle, fhandle, 4);


          if (!prot->Bridge->Run((char *) &xrdreq, 0, 0)) {
            prot->SendSimpleResp(404, NULL, NULL, (char *) "Could not run close request.", 0, false);
            return -1;
          }

          // We have finished
          return 1;

        }

      }

      break;

    }
    case XrdHttpReq::rtOPTIONS:
    {
      prot->SendSimpleResp(200, NULL, (char *) "DAV: 1\r\nDAV: <http://apache.org/dav/propset/fs/1>\r\nAllow: HEAD,GET,PUT,PROPFIND,DELETE,OPTIONS", NULL, 0, keepalive);
      reset();
      return  keepalive ? 1 : -1;
    }
    case XrdHttpReq::rtDELETE:
    {


      switch (reqstate) {

        case 0: // Stat()
        {


          // --------- STAT is always the first step
          memset(&xrdreq, 0, sizeof (ClientRequest));
          xrdreq.stat.requestid = htons(kXR_stat);
          string s = resourceplusopaque.c_str();


          l = resourceplusopaque.length() + 1;
          xrdreq.stat.dlen = htonl(l);

          if (!prot->Bridge->Run((char *) &xrdreq, (char *) resourceplusopaque.c_str(), l)) {
            prot->SendSimpleResp(501, NULL, NULL, (char *) "Could not run request.", 0, false);
            return -1;
          }

          // We need to be invoked again to complete the request
          return 0;
        }
        default:

          if (fileflags & kXR_isDir) {
            // --------- RMDIR
            memset(&xrdreq, 0, sizeof (ClientRequest));
            xrdreq.rmdir.requestid = htons(kXR_rmdir);

            string s = resourceplusopaque.c_str();

            l = s.length() + 1;
            xrdreq.rmdir.dlen = htonl(l);

            if (!prot->Bridge->Run((char *) &xrdreq, (char *) s.c_str(), l)) {
              prot->SendSimpleResp(501, NULL, NULL, (char *) "Could not run rmdir request.", 0, false);
              return -1;
            }
          } else {
            // --------- DELETE
            memset(&xrdreq, 0, sizeof (ClientRequest));
            xrdreq.rm.requestid = htons(kXR_rm);

            string s = resourceplusopaque.c_str();

            l = s.length() + 1;
            xrdreq.rm.dlen = htonl(l);

            if (!prot->Bridge->Run((char *) &xrdreq, (char *) s.c_str(), l)) {
              prot->SendSimpleResp(501, NULL, NULL, (char *) "Could not run rm request.", 0, false);
              return -1;
            }
          }


          // We don't want to be invoked again after this request is finished
          return 1;

      }



    }
    case XrdHttpReq::rtPATCH:
    {
      prot->SendSimpleResp(501, NULL, NULL, (char *) "Request not supported yet.", 0, false);

      return -1;
    }
    case XrdHttpReq::rtPROPFIND:
    {



      switch (reqstate) {

        case 0: // Stat() and add the current item to the list of the things to send
        {

          if (length > 0) {
            TRACE(REQ, "Reading request body " << length << " bytes.");
            char *p = 0;
            // We have to specifically read all the request body

            if (prot->BuffgetData(length, &p, true) < length) {
              prot->SendSimpleResp(501, NULL, NULL, (char *) "Error in getting the PROPFIND request body.", 0, false);
              return -1;
            }

            if ((depth > 1) || (depth < 0)) {
              prot->SendSimpleResp(501, NULL, NULL, (char *) "Invalid depth value.", 0, false);
              return -1;
            }


            parseBody(p, length);
          }


          // --------- STAT is always the first step
          memset(&xrdreq, 0, sizeof (ClientRequest));
          xrdreq.stat.requestid = htons(kXR_stat);
          string s = resourceplusopaque.c_str();


          l = resourceplusopaque.length() + 1;
          xrdreq.stat.dlen = htonl(l);

          if (!prot->Bridge->Run((char *) &xrdreq, (char *) resourceplusopaque.c_str(), l)) {
            prot->SendSimpleResp(501, NULL, NULL, (char *) "Could not run request.", 0, false);
            return -1;
          }


          if (depth == 0) {
            // We don't need to be invoked again
            return 1;
          } else
            // We need to be invoked again to complete the request
            return 0;



          break;
        }

        default: // Dirlist()
        {

          // --------- DIRLIST
          memset(&xrdreq, 0, sizeof (ClientRequest));
          xrdreq.dirlist.requestid = htons(kXR_dirlist);

          string s = resourceplusopaque.c_str();
          xrdreq.dirlist.options[0] = kXR_dstat;
          //s += "?xrd.dirstat=1";

          l = s.length() + 1;
          xrdreq.dirlist.dlen = htonl(l);

          if (!prot->Bridge->Run((char *) &xrdreq, (char *) s.c_str(), l)) {
            prot->SendSimpleResp(501, NULL, NULL, (char *) "Could not run request.", 0, false);
            return -1;
          }

          // We don't want to be invoked again after this request is finished
          return 1;
        }
      }


      break;
    }
    case XrdHttpReq::rtMKCOL:
    {

      // --------- MKDIR
      memset(&xrdreq, 0, sizeof (ClientRequest));
      xrdreq.mkdir.requestid = htons(kXR_mkdir);

      string s = resourceplusopaque.c_str();
      xrdreq.mkdir.options[0] = (kXR_char) kXR_mkpath;

      l = s.length() + 1;
      xrdreq.mkdir.dlen = htonl(l);

      if (!prot->Bridge->Run((char *) &xrdreq, (char *) s.c_str(), l)) {
        prot->SendSimpleResp(501, NULL, NULL, (char *) "Could not run request.", 0, false);
        return -1;
      }

      // We don't want to be invoked again after this request is finished
      return 1;
    }
    case XrdHttpReq::rtMOVE:
    {

      // --------- MOVE
      memset(&xrdreq, 0, sizeof (ClientRequest));
      xrdreq.mv.requestid = htons(kXR_mv);

      string s = resourceplusopaque.c_str();
      s += " ";

      char buf[256];
      char *ppath;
      int port = 0;
      if (parseURL((char *) destination.c_str(), buf, port, &ppath)) {
        prot->SendSimpleResp(501, NULL, NULL, (char *) "Cannot parse destination url.", 0, false);
        return -1;
      }

      char buf2[256];
      strcpy(buf2, host.c_str());
      char *pos = strchr(buf2, ':');
      if (pos) *pos = '\0';
     
      // If we are a redirector we enforce that the host field is equal to
      // whatever was written in the destination url
      //
      // If we are a data server instead we cannot enforce anything, we will
      // just ignore the host part of the destination
      if ((prot->myRole == kXR_isManager) && strcmp(buf, buf2)) {
        prot->SendSimpleResp(501, NULL, NULL, (char *) "Only in-place renaming is supported for MOVE.", 0, false);
        return -1;
      }




      s += ppath;

      l = s.length() + 1;
      xrdreq.mv.dlen = htonl(l);
      xrdreq.mv.arg1len = htons(resourceplusopaque.length());
      
      if (!prot->Bridge->Run((char *) &xrdreq, (char *) s.c_str(), l)) {
        prot->SendSimpleResp(501, NULL, NULL, (char *) "Could not run request.", 0, false);
        return -1;
      }

      // We don't want to be invoked again after this request is finished
      return 1;

    }
    default:
    {
      prot->SendSimpleResp(501, NULL, NULL, (char *) "Request not supported.", 0, false);
      return -1;
    }

  }

  return 1;
}


int
XrdHttpReq::PostProcessChecksum(std::string &digest_header) {
  if (iovN > 0) {
    if (xrdresp == kXR_error) {
      prot->SendSimpleResp(httpStatusCode, NULL, NULL, "Failed to determine checksum", 0, false);
      return -1;
    }

    TRACEI(REQ, "Checksum for HEAD " << resource << " " << reinterpret_cast<char *>(iovP[0].iov_base) << "=" << reinterpret_cast<char *>(iovP[iovN-1].iov_base));
    std::string response_name = convert_xrootd_to_rfc_name(reinterpret_cast<char *>(iovP[0].iov_base));

    bool convert_to_base64 = needs_base64_padding(m_req_digest);
    char *digest_value = reinterpret_cast<char *>(iovP[iovN-1].iov_base);
    if (convert_to_base64) {
      size_t digest_length = strlen(digest_value);
      unsigned char *digest_binary_value = (unsigned char *)malloc(digest_length);
      if (!Fromhexdigest(reinterpret_cast<unsigned char *>(digest_value), digest_length, digest_binary_value)) {
        prot->SendSimpleResp(500, NULL, NULL, (char *) "Failed to convert checksum hexdigest to base64.", 0, false);
        free(digest_binary_value);
        return -1;
      }
      char *digest_base64_value = (char *)malloc(digest_length);
      // Binary length is precisely half the size of the hex-encoded digest_value; hence, divide length by 2.
      Tobase64(digest_binary_value, digest_length/2, digest_base64_value);
      free(digest_binary_value);
      digest_value = digest_base64_value;
    }

    digest_header = "Digest: ";
    digest_header += m_req_digest;
    digest_header += "=";
    digest_header += digest_value;
    if (convert_to_base64) {free(digest_value);}
    return 0;
  } else {
    prot->SendSimpleResp(500, NULL, NULL, "Underlying filesystem failed to calculate checksum.", 0, false);
    return -1;
  }
}


// This is invoked by the callbacks, after something has happened in the bridge

int XrdHttpReq::PostProcessHTTPReq(bool final_) {

  TRACEI(REQ, "PostProcessHTTPReq req: " << request << " reqstate: " << reqstate);
  mapXrdErrorToHttpStatus();

  switch (request) {
    case XrdHttpReq::rtUnknown:
    {
      prot->SendSimpleResp(400, NULL, NULL, (char *) "Request malformed 1", 0, false);
      return -1;
    }
    case XrdHttpReq::rtMalformed:
    {
      prot->SendSimpleResp(400, NULL, NULL, (char *) "Request malformed 2", 0, false);
      return -1;
    }
    case XrdHttpReq::rtHEAD:
    {
      if (xrdresp != kXR_ok) {
        // NOTE that HEAD MUST NOT return a body, even in the case of failure.
        prot->SendSimpleResp(httpStatusCode, NULL, NULL, NULL, 0, false);
        return -1;
      } else if (reqstate == 0) {
        if (iovN > 0) {

          // Now parse the stat info
          TRACEI(REQ, "Stat for HEAD " << resource << " stat=" << (char *) iovP[0].iov_base);

          long dummyl;
          sscanf((const char *) iovP[0].iov_base, "%ld %lld %ld %ld",
                  &dummyl,
                  &filesize,
                  &fileflags,
                  &filemodtime);

          if (m_req_digest.size()) {
            return 0;
          } else {
            prot->SendSimpleResp(200, NULL, NULL, NULL, filesize, keepalive);
            return keepalive ? 1 : -1;
          }
        }

        prot->SendSimpleResp(httpStatusCode, NULL, NULL, NULL, 0, keepalive);
        reset();
        return keepalive ? 1 : -1;
      } else { // We requested a checksum and now have its response.
        if (iovN > 0) {
          std::string digest_response;
          int response = PostProcessChecksum(digest_response);
          if (-1 == response) {
                return -1;
          }
          prot->SendSimpleResp(200, NULL, digest_response.c_str(), NULL, filesize, keepalive);
          return keepalive ? 1 : -1;
        } else {
          prot->SendSimpleResp(500, NULL, NULL, "Underlying filesystem failed to calculate checksum.", 0, false);
          return -1;
        }
      }
    }
    case XrdHttpReq::rtGET:
    {

      if (xrdreq.header.requestid == ntohs(kXR_dirlist)) {


        if (xrdresp == kXR_error) {
          prot->SendSimpleResp(httpStatusCode, NULL, NULL,
                               httpStatusText.c_str(), httpStatusText.length(), false);
          return -1;
        }


        if (stringresp.empty()) {

          // Start building the HTML response
          stringresp = "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n"
                  "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"
                  "<head>\n"
                  "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>\n"
                  "<link rel=\"stylesheet\" type=\"text/css\" href=\"/static/css/xrdhttp.css\"/>\n"
                  "<link rel=\"icon\" type=\"image/png\" href=\"/static/icons/xrdhttp.ico\"/>\n";

          stringresp += "<title>";
          stringresp += resource.c_str();
          stringresp += "</title>\n";

          stringresp += "</head>\n"
                  "<body>\n";

          stringresp += "<h1>Listing of: ";
          stringresp += resource.c_str();
          stringresp += "</h1>\n";

          stringresp += "<div id=\"header\">";


          stringresp += "<table id=\"ft\">\n"
                  "<thead><tr>\n"
                  "<th class=\"mode\">Mode</th>"
                  "<th class=\"flags\">Flags</th>"
                  "<th class=\"size\">Size</th>"
                  "<th class=\"datetime\">Modified</th>"
                  "<th class=\"name\">Name</th>"
                  "</tr></thead>\n";

        }

        // Now parse the answer building the entries vector
        if (iovN > 0) {
          char *startp = (char *) iovP[0].iov_base, *endp = 0;
          char entry[1024];
          DirListInfo e;
          while ( (size_t)(startp - (char *) iovP[0].iov_base) < (size_t)( iovP[0].iov_len - 1) ) {
            // Find the filename, it comes before the \n
            if ((endp = (char *) strchr((const char*) startp, '\n'))) {
              strncpy(entry, (char *) startp, endp - startp);
              entry[endp - startp] = 0;
              e.path = entry;

              endp++;

              // Now parse the stat info
              TRACEI(REQ, "Dirlist " << resource << " entry=" << entry << " stat=" << endp);

              long dummyl;
              sscanf(endp, "%ld %lld %ld %ld",
                      &dummyl,
                      &e.size,
                      &e.flags,
                      &e.modtime);
            } else
              strcpy(entry, (char *) startp);


            if (e.path.length() && (e.path != ".") && (e.path != "..")) {
              // The entry is filled. <td class="ft-file"><a href="file1.txt">file1.txt</a></td>
              string p = "<tr>"
                      "<td class=\"mode\">";

              if (e.flags & kXR_isDir) p += "d";
              else p += "-";

              if (e.flags & kXR_other) p += "o";
              else p += "-";

              if (e.flags & kXR_offline) p += "O";
              else p += "-";

              if (e.flags & kXR_readable) p += "r";
              else p += "-";

              if (e.flags & kXR_writable) p += "w";
              else p += "-";

              if (e.flags & kXR_xset) p += "x";
              else p += "-";

              p += "</td>";
              p += "<td class=\"mode\">" + itos(e.flags) + "</td>"
                      "<td class=\"size\">" + itos(e.size) + "</td>"
                      "<td class=\"datetime\">" + ISOdatetime(e.modtime) + "</td>"
                      "<td class=\"name\">"
                      "<a href=\"";

              if (resource != "/") {
                  p += resource.c_str();
                  p += "/";
              }
              p += e.path + "\">";

              p += e.path;

              p += "</a></td></tr>";

              stringresp += p;


            }


            if (endp) {
                char *pp = (char *)strchr((const char *)endp, '\n');
                if (pp) startp = pp+1;
                else break;
            } else break;

          }
        }

        // If this was the last bunch of entries, send the buffer and empty it immediately
        if (final_) {
          stringresp += "</table></div><br><br><hr size=1>"
          "<p><span id=\"requestby\">Request by ";
          
          if (prot->SecEntity.name)
            stringresp += prot->SecEntity.name;
          else
            stringresp += prot->Link->ID;
          
          if (prot->SecEntity.vorg ||
            prot->SecEntity.name ||
            prot->SecEntity.moninfo ||
            prot->SecEntity.role)
            stringresp += " (";
          
          if (prot->SecEntity.vorg) {
            stringresp += " VO: ";
            stringresp += prot->SecEntity.vorg;
          }
          
          if (prot->SecEntity.moninfo) {
            stringresp += " DN: ";
            stringresp += prot->SecEntity.moninfo;
          } else
            if (prot->SecEntity.name) {
              stringresp += " DN: ";
              stringresp += prot->SecEntity.name;
            }
          
          
          if (prot->SecEntity.role) {
            stringresp += " Role: ";
            stringresp += prot->SecEntity.role;
            if (prot->SecEntity.endorsements) {
              stringresp += " (";
              stringresp += prot->SecEntity.endorsements;
              stringresp += ") ";
            }
          }
          
          
           
          if (prot->SecEntity.vorg ||
            prot->SecEntity.moninfo ||
            prot->SecEntity.role)
            stringresp += " )";
          
          if (prot->SecEntity.host) {
            stringresp += " ( ";
            stringresp += prot->SecEntity.host;
            stringresp += " )";
          }
          
          stringresp += "</span></p>\n";
          stringresp += "<p>Powered by XrdHTTP ";
          stringresp += XrdVSTRING;
          stringresp += " (CERN IT-SDC)</p>\n";
          
          prot->SendSimpleResp(200, NULL, NULL, (char *) stringresp.c_str(), 0, keepalive);
          stringresp.clear();
          return keepalive ? 1 : -1;
        }


      } else {



        switch (reqstate) {
          case 0: //stat
          {
            // Ugly hack. Be careful with EOS! Test with vanilla XrdHTTP and EOS, separately
            // A 404 on the preliminary stat() is fatal only
            // in a manager. A non-manager will ignore the result and try anyway to open the file
            // 
            if (xrdresp == kXR_ok) {
              
              if (iovN > 0) {
                
                // Now parse the stat info
                TRACEI(REQ, "Stat for GET " << resource << " stat=" << (char *) iovP[0].iov_base);
                
                long dummyl;
                sscanf((const char *) iovP[0].iov_base, "%ld %lld %ld %ld",
                       &dummyl,
                       &filesize,
                       &fileflags,
                       &filemodtime);

                // We will default the response size specified by the headers; if that
                // wasn't given, use the file size.
                if (!length) {
                    length = filesize;
                }
              }
              else {
                TRACEI(REQ, "Can't find the stat information for '" << resource << "' Internal error?");
              }
            }
            
            // We are here if the request failed
            
            if (prot->myRole == kXR_isManager) {
              prot->SendSimpleResp(httpStatusCode, NULL, NULL,
                                   httpStatusText.c_str(), httpStatusText.length(), false);
              return -1;
            }

            // We are here in the case of a negative response in a non-manager

            return 0;
          }
          case 1:  // open
          case 2:  // open when digest was requested
          {

            if (reqstate == 1 && !m_req_digest.empty()) { // We requested a checksum and now have its response.
              int response = PostProcessChecksum(m_digest_header);
              if (-1 == response) {
                return -1;
              }
              return 0;
            } else if (((reqstate == 2 && !m_req_digest.empty()) ||
                        (reqstate == 1 && m_req_digest.empty()))
              && (xrdresp == kXR_ok)) {


              getfhandle();
              
              // Now parse the stat info if we still don't have it
              if (filesize == 0) {
                if (iovP[1].iov_len > 1) {
                  TRACEI(REQ, "Stat for GET " << resource << " stat=" << (char *) iovP[1].iov_base);
              
                  long dummyl;
                  sscanf((const char *) iovP[1].iov_base, "%ld %lld %ld %ld",
                        &dummyl,
                        &filesize,
                        &fileflags,
                        &filemodtime);

                  // As above: if the client specified a response size, we use that.
                  // Otherwise, utilize the filesize
                  if (!length) {
                    length = filesize;
                  }
                }
                else
                  TRACEI(ALL, "GET returned no STAT information. Internal error?");
              }
              
              if (rwOps.size() == 0) {
                // Full file.
                
                prot->SendSimpleResp(200, NULL, m_digest_header.empty() ? NULL : m_digest_header.c_str(), NULL, filesize, keepalive);
                return 0;
              } else
                if (rwOps.size() == 1) {
                // Only one read to perform
                int cnt = (rwOps[0].byteend - rwOps[0].bytestart + 1);
                char buf[64];
                
                XrdOucString s = "Content-Range: bytes ";
                sprintf(buf, "%lld-%lld/%lld", rwOps[0].bytestart, rwOps[0].byteend, filesize);
                s += buf;
                if (!m_digest_header.empty()) {
                  s += "\n";
                  s += m_digest_header.c_str();
                }

                prot->SendSimpleResp(206, NULL, (char *)s.c_str(), NULL, cnt, keepalive);
                return 0;
              } else
                if (rwOps.size() > 1) {
                // Multiple reads to perform, compose and send the header
                int cnt = 0;
                for (size_t i = 0; i < rwOps.size(); i++) {

                  if (rwOps[i].bytestart > filesize) continue;
                  if (rwOps[i].byteend > filesize - 1)
                    rwOps[i].byteend = filesize - 1;

                  cnt += (rwOps[i].byteend - rwOps[i].bytestart + 1);

                  cnt += buildPartialHdr(rwOps[i].bytestart,
                          rwOps[i].byteend,
                          filesize,
                          (char *) "123456").size();
                }
                cnt += buildPartialHdrEnd((char *) "123456").size();
                std::string header = "Content-Type: multipart/byteranges; boundary=123456";
                if (!m_digest_header.empty()) {
                  header += "\n";
                  header += m_digest_header;
                }

                prot->SendSimpleResp(206, NULL, header.c_str(), NULL, cnt, keepalive);
                return 0;
              }



            } else if (xrdresp != kXR_ok) {
              
              // If it's a dir then we are in the wrong place and we did the wrong thing.
              //if (xrderrcode == 3016) {
              //  fileflags &= kXR_isDir;
              //  reqstate--;
              //  return 0;
              //}
              prot->SendSimpleResp(httpStatusCode, NULL, NULL,
                                   httpStatusText.c_str(), httpStatusText.length(), false);
              return -1;
            }

            // Remaining case: reqstate == 2 and we didn't ask for a digest (should be a read).
          }
          // fallthrough
          default: //read or readv
          {

            // Nothing to do if we are postprocessing a close
            if (ntohs(xrdreq.header.requestid) == kXR_close) return keepalive ? 1 : -1;
            
            // Close() if this was the third state of a readv, otherwise read the next chunk
            if ((reqstate == 3) && (ntohs(xrdreq.header.requestid) == kXR_readv)) return keepalive ? 1: -1;

            // Prevent scenario where data is expected but none is actually read
            // E.g. Accessing files which return the results of a script
            if ((ntohs(xrdreq.header.requestid) == kXR_read) &&
                (reqstate > 2) && (iovN == 0)) {
              TRACEI(REQ, "Stopping request because more data is expected "
                          "but no data has been read.");
              return -1;
            }

            // If we are here it's too late to send a proper error message...
            if (xrdresp == kXR_error) return -1;

            TRACEI(REQ, "Got data vectors to send:" << iovN);
            if (ntohs(xrdreq.header.requestid) == kXR_readv) {
              // Readv case, we must take out each individual header and format it according to the http rules
              readahead_list *l;
              char *p;
              int len;

              // Cycle on all the data that is coming from the server
              for (int i = 0; i < iovN; i++) {

                for (p = (char *) iovP[i].iov_base; p < (char *) iovP[i].iov_base + iovP[i].iov_len;) {
                  l = (readahead_list *) p;
                  len = ntohl(l->rlen);

                  // Now we have a chunk coming from the server. This may be a partial chunk

                  if (rwOpPartialDone == 0) {
                    string s = buildPartialHdr(rwOps[rwOpDone].bytestart,
                            rwOps[rwOpDone].byteend,
                            filesize,
                            (char *) "123456");

                    TRACEI(REQ, "Sending multipart: " << rwOps[rwOpDone].bytestart << "-" << rwOps[rwOpDone].byteend);
                    if (prot->SendData((char *) s.c_str(), s.size())) return -1;
                  }

                  // Send all the data we have
                  if (prot->SendData(p + sizeof (readahead_list), len)) return -1;

                  // If we sent all the data relative to the current original chunk request
                  // then pass to the next chunk, otherwise wait for more data
                  rwOpPartialDone += len;
                  if (rwOpPartialDone >= rwOps[rwOpDone].byteend - rwOps[rwOpDone].bytestart + 1) {
                    rwOpDone++;
                    rwOpPartialDone = 0;
                  }

                  p += sizeof (readahead_list);
                  p += len;

                }
              }

              if (rwOpDone == rwOps.size()) {
                string s = buildPartialHdrEnd((char *) "123456");
                if (prot->SendData((char *) s.c_str(), s.size())) return -1;
              }

            } else
              for (int i = 0; i < iovN; i++) {
                if (prot->SendData((char *) iovP[i].iov_base, iovP[i].iov_len)) return -1;
                writtenbytes += iovP[i].iov_len;
              }
              
            // Let's make sure that we avoid sending the same data twice,
            // in the case where PostProcessHTTPReq is invoked again
            this->iovN = 0;
            
            return 0;
          }

        } // switch reqstate


      }


      break;
    } // case GET


    case XrdHttpReq::rtPUT:
    {
      if (!fopened) {

        if (xrdresp != kXR_ok) {

          prot->SendSimpleResp(httpStatusCode, NULL, NULL,
                               httpStatusText.c_str(), httpStatusText.length(), keepalive);
          return -1;
        }

        getfhandle();
        fopened = true;

        // We try to completely fill up our buffer before flushing
        prot->ResumeBytes = min(length - writtenbytes, (long long) prot->BuffAvailable());

        if (sendcontinue) {
          prot->SendSimpleResp(100, NULL, NULL, 0, 0, keepalive);
          return 0;
        }

        break;
      } else {


        // If we are here it's too late to send a proper error message...
        if (xrdresp == kXR_error) return -1;

        if (ntohs(xrdreq.header.requestid) == kXR_write) {
          int l = ntohl(xrdreq.write.dlen);

          // Consume the written bytes
          prot->BuffConsume(ntohl(xrdreq.write.dlen));
          writtenbytes += l;

          // Update the chunk offset
          if (m_transfer_encoding_chunked) {
            m_current_chunk_offset += l;
          }

          // We try to completely fill up our buffer before flushing
          prot->ResumeBytes = min(length - writtenbytes, (long long) prot->BuffAvailable());

          return 0;
        }

        if (ntohs(xrdreq.header.requestid) == kXR_close) {
          if (xrdresp == kXR_ok) {
            prot->SendSimpleResp(200, NULL, NULL, (char *) ":-)", 0, keepalive);
            return keepalive ? 1 : -1;
          } else {
            prot->SendSimpleResp(httpStatusCode, NULL, NULL,
                                 httpStatusText.c_str(), httpStatusText.length(), keepalive);
            return -1;
          }
        }


      }





      break;
    }



    case XrdHttpReq::rtDELETE:
    {

      if (xrdresp != kXR_ok) {
        prot->SendSimpleResp(httpStatusCode, NULL, NULL,
                             httpStatusText.c_str(), httpStatusText.length(), keepalive);
        return -1;
      }




      switch (reqstate) {

        case 0: // response to stat()
        {
          if (iovN > 0) {

            // Now parse the stat info
            TRACEI(REQ, "Stat for removal " << resource << " stat=" << (char *) iovP[0].iov_base);

            long dummyl;
            sscanf((const char *) iovP[0].iov_base, "%ld %lld %ld %ld",
                    &dummyl,
                    &filesize,
                    &fileflags,
                    &filemodtime);
          }

          return 0;
        }
        default: // response to rm
        {
          if (xrdresp == kXR_ok) {
            prot->SendSimpleResp(200, NULL, NULL, (char *) ":-)", 0, keepalive);
            return keepalive ? 1 : -1;
          }
          prot->SendSimpleResp(httpStatusCode, NULL, NULL,
                               httpStatusText.c_str(), httpStatusText.length(), keepalive);
          return -1;
        }
      }


    }

    case XrdHttpReq::rtPROPFIND:
    {

      if (xrdresp == kXR_error) {
        prot->SendSimpleResp(httpStatusCode, NULL, NULL,
                             httpStatusText.c_str(), httpStatusText.length(), false);
        return -1;
      }

      switch (reqstate) {

        case 0: // response to stat()
        {
          DirListInfo e;
          e.size = 0;
          e.flags = 0;
          
          // Now parse the answer building the entries vector
          if (iovN > 0) {
            e.path = resource.c_str();

            // Now parse the stat info
            TRACEI(REQ, "Collection " << resource << " entry=" << resource << " stat=" << (char *) iovP[0].iov_base);

            long dummyl;
            sscanf((const char *) iovP[0].iov_base, "%ld %lld %ld %ld",
                    &dummyl,
                    &e.size,
                    &e.flags,
                    &e.modtime);

            if (e.path.length() && (e.path != ".") && (e.path != "..")) {
              /* The entry is filled. */


              string p;
              stringresp += "<D:response xmlns:lp1=\"DAV:\" xmlns:lp2=\"http://apache.org/dav/props/\" xmlns:lp3=\"LCGDM:\">\n";
              stringresp += "<D:href>" + e.path + "</D:href>\n";
              stringresp += "<D:propstat>\n<D:prop>\n";

              // Now add the properties that we have to add

              // File size
              stringresp += "<lp1:getcontentlength>";
              stringresp += itos(e.size);
              stringresp += "</lp1:getcontentlength>\n";



              stringresp += "<lp1:getlastmodified>";
              stringresp += ISOdatetime(e.modtime);
              stringresp += "</lp1:getlastmodified>\n";



              if (e.flags & kXR_isDir) {
                stringresp += "<lp1:resourcetype><D:collection/></lp1:resourcetype>\n";
                stringresp += "<lp1:iscollection>1</lp1:iscollection>\n";
              } else {
                stringresp += "<lp1:iscollection>0</lp1:iscollection>\n";
              }

              if (e.flags & kXR_xset) {
                stringresp += "<lp1:executable>T</lp1:executable>\n";
                stringresp += "<lp1:iscollection>1</lp1:iscollection>\n";
              } else {
                stringresp += "<lp1:executable>F</lp1:executable>\n";
              }



              stringresp += "</D:prop>\n<D:status>HTTP/1.1 200 OK</D:status>\n</D:propstat>\n</D:response>\n";


            }


          }

          // If this was the last bunch of entries, send the buffer and empty it immediately
          if ((depth == 0) || !(e.flags & kXR_isDir)) {
            string s = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<D:multistatus xmlns:D=\"DAV:\" xmlns:ns1=\"http://apache.org/dav/props/\" xmlns:ns0=\"DAV:\">\n";
            stringresp.insert(0, s);
            stringresp += "</D:multistatus>\n";
            prot->SendSimpleResp(207, (char *) "Multi-Status", (char *) "Content-Type: text/xml; charset=\"utf-8\"",
                    (char *) stringresp.c_str(), stringresp.length(), keepalive);
            stringresp.clear();
            return keepalive ? 1 : -1;
          }

          break;
        }
        default: // response to dirlist()
        {


          // Now parse the answer building the entries vector
          if (iovN > 0) {
            char *startp = (char *) iovP[0].iov_base, *endp = 0;
            char entry[1024];
            DirListInfo e;

            while ( (size_t)(startp - (char *) iovP[0].iov_base) < (size_t)(iovP[0].iov_len - 1) ) {
              // Find the filename, it comes before the \n
              if ((endp = (char *) mystrchrnul((const char*) startp, '\n'))) {
                strncpy(entry, (char *) startp, endp - startp);
                entry[endp - startp] = 0;
                e.path = entry;

                endp++;

                // Now parse the stat info
                TRACEI(REQ, "Dirlist " << resource << " entry=" << entry << " stat=" << endp);

                long dummyl;
                sscanf(endp, "%ld %lld %ld %ld",
                        &dummyl,
                        &e.size,
                        &e.flags,
                        &e.modtime);
              }


              if (e.path.length() && (e.path != ".") && (e.path != "..")) {
                /* The entry is filled.
          
                  <D:response xmlns:lp1="DAV:" xmlns:lp2="http://apache.org/dav/props/" xmlns:lp3="LCGDM:">
                      <D:href>/dpm/cern.ch/home/testers2.eu-emi.eu/</D:href>
                      <D:propstat>
                          <D:prop>
                              <lp1:getcontentlength>1</lp1:getcontentlength>
                              <lp1:getlastmodified>Tue, 01 May 2012 02:42:13 GMT</lp1:getlastmodified>
                              <lp1:resourcetype>
                                <D:collection/>
                              </lp1:resourcetype>
                          </D:prop>
                      <D:status>HTTP/1.1 200 OK</D:status>
                      </D:propstat>
                  </D:response>
                 */


                string p = resource.c_str();
                if (*p.rbegin() != '/') p += "/";
                p += e.path;
                stringresp += "<D:response xmlns:lp1=\"DAV:\" xmlns:lp2=\"http://apache.org/dav/props/\" xmlns:lp3=\"LCGDM:\">\n";
                stringresp += "<D:href>" + p + "</D:href>\n";
                stringresp += "<D:propstat>\n<D:prop>\n";



                // Now add the properties that we have to add

                // File size
                stringresp += "<lp1:getcontentlength>";
                stringresp += itos(e.size);
                stringresp += "</lp1:getcontentlength>\n";

                stringresp += "<lp1:getlastmodified>";
                stringresp += ISOdatetime(e.modtime);
                stringresp += "</lp1:getlastmodified>\n";

                if (e.flags & kXR_isDir) {
                  stringresp += "<lp1:resourcetype><D:collection/></lp1:resourcetype>\n";
                  stringresp += "<lp1:iscollection>1</lp1:iscollection>\n";
                } else {
                  stringresp += "<lp1:iscollection>0</lp1:iscollection>\n";
                }

                if (e.flags & kXR_xset) {
                  stringresp += "<lp1:executable>T</lp1:executable>\n";
                  stringresp += "<lp1:iscollection>1</lp1:iscollection>\n";
                } else {
                  stringresp += "<lp1:executable>F</lp1:executable>\n";
                }

                stringresp += "</D:prop>\n<D:status>HTTP/1.1 200 OK</D:status>\n</D:propstat>\n</D:response>\n";


              }



              if (endp) {
                  char *pp = (char *)strchr((const char *)endp, '\n');
                  if (pp) startp = pp+1;
                  else break;
              } else break;

            }
          }



          // If this was the last bunch of entries, send the buffer and empty it immediately
          if (final_) {
            string s = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<D:multistatus xmlns:D=\"DAV:\" xmlns:ns1=\"http://apache.org/dav/props/\" xmlns:ns0=\"DAV:\">\n";
            stringresp.insert(0, s);
            stringresp += "</D:multistatus>\n";
            prot->SendSimpleResp(207, (char *) "Multi-Status", (char *) "Content-Type: text/xml; charset=\"utf-8\"",
                    (char *) stringresp.c_str(), stringresp.length(), keepalive);
            stringresp.clear();
            return keepalive ? 1 : -1;
          }

          break;
        } // default reqstate
      } // switch reqstate


      break;

    } // case propfind

    case XrdHttpReq::rtMKCOL:
    {

      if (xrdresp != kXR_ok) {
        prot->SendSimpleResp(httpStatusCode, NULL, NULL,
                             httpStatusText.c_str(), httpStatusText.length(), false);
        return -1;
      }

      prot->SendSimpleResp(201, NULL, NULL, (char *) ":-)", 0, keepalive);
      return keepalive ? 1 : -1;

    }
    case XrdHttpReq::rtMOVE:
    {

      if (xrdresp != kXR_ok) {
        prot->SendSimpleResp(409, NULL, NULL, (char *) etext.c_str(), 0, false);
        return -1;
      }

      prot->SendSimpleResp(201, NULL, NULL, (char *) ":-)", 0, keepalive);
      return keepalive ? 1 : -1;

    }

    default:
      break;

  }


  switch (xrdresp) {
    case kXR_error:
      prot->SendSimpleResp(httpStatusCode, NULL, NULL,
                           httpStatusText.c_str(), httpStatusText.length(), false);
      return -1;
      break;

    default:

      break;
  }


  return 0;
}

void XrdHttpReq::reset() {

  TRACE(REQ, " XrdHttpReq request ended.");

  //if (xmlbody) xmlFreeDoc(xmlbody);
  rwOps.clear();
  rwOps_split.clear();
  rwOpDone = 0;
  rwOpPartialDone = 0;
  writtenbytes = 0;
  etext.clear();
  redirdest = "";

  //        // Here we should deallocate this
  //        const struct iovec *iovP //!< pointer to data array
  //                int iovN, //!< array count
  //                int iovL, //!< byte  count
  //                bool final //!< true -> final result


  //xmlbody = 0;
  depth = 0;
  xrdresp = kXR_noResponsesYet;
  xrderrcode = kXR_noErrorYet;
  if (ralist) free(ralist);
  ralist = 0;

  request = rtUnset;
  resource = "";
  allheaders.clear();

  // Reset the state of the request's digest request.
  m_req_digest.clear();
  m_resource_with_digest = "";

  headerok = false;
  keepalive = true;
  length = 0;
  filesize = 0;
  depth = 0;
  sendcontinue = false;

  m_transfer_encoding_chunked = false;
  m_current_chunk_size = -1;
  m_current_chunk_offset = 0;

  /// State machine to talk to the bridge
  reqstate = 0;

  memset(&xrdreq, 0, sizeof (xrdreq));
  memset(&xrdresp, 0, sizeof (xrdresp));
  xrderrcode = kXR_noErrorYet;

  etext.clear();
  redirdest = "";

  stringresp = "";

  host = "";
  destination = "";
  hdr2cgistr = "";

  iovP = 0;
  iovN = 0;
  iovL = 0;


  if (opaque) delete(opaque);
  opaque = 0;

  fopened = false;

  final = false;
}

void XrdHttpReq::getfhandle() {

  memcpy(fhandle, iovP[0].iov_base, 4);
  TRACEI(REQ, "fhandle:" <<
          (int) fhandle[0] << ":" << (int) fhandle[1] << ":" << (int) fhandle[2] << ":" << (int) fhandle[3]);

}
