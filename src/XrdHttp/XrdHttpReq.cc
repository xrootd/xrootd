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
#include <cstring>
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
#include "XrdOuc/XrdOucTUtils.hh"
#include "XrdOuc/XrdOucUtils.hh"

#include "XrdHttpUtils.hh"

#include "XrdHttpStatic.hh"

#define MAX_TK_LEN      256
#define MAX_RESOURCE_LEN 16384

// This is to fix the trace macros
#define TRACELINK prot->Link

namespace
{
const char *TraceID = "Req";
}

void trim(std::string &str)
{
    XrdOucUtils::trim(str);
}


std::string ISOdatetime(time_t t) {
  char datebuf[128];
  struct tm t1;

  memset(&t1, 0, sizeof (t1));
  gmtime_r(&t, &t1);

  strftime(datebuf, 127, "%a, %d %b %Y %H:%M:%S GMT", &t1);
  return (std::string) datebuf;

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

    // We memorize the headers also as a string                                                                                                                                              
    // because external plugins may need to process it differently                                                                                                                          
    std::string ss = val;
    trim(ss);
    allheaders[key] = ss;
	  
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
      // (rfc2616 14.35.1) says if Range header contains any range
      // which is syntactically invalid the Range header should be ignored.
      // Therefore no need for the range handler to report an error.
      readRangeHandler.ParseContentRange(val);
    } else if (!strcmp(key, "Content-Length")) {
      length = atoll(val);

    } else if (!strcmp(key, "Destination")) {
      destination.assign(val, line+len-val);
      trim(destination);
    } else if (!strcmp(key, "Want-Digest")) {
      m_req_digest.assign(val, line + len - val);
      trim(m_req_digest);
      //Transform the user requests' want-digest to lowercase
      std::transform(m_req_digest.begin(),m_req_digest.end(),m_req_digest.begin(),::tolower);
    } else if (!strcmp(key, "Depth")) {
      depth = -1;
      if (strcmp(val, "infinity"))
        depth = atoll(val);

    } else if (!strcmp(key, "Expect") && strstr(val, "100-continue")) {
      sendcontinue = true;
    } else if (!strcasecmp(key, "TE") && strstr(val, "trailers")) {
      m_trailer_headers = true;
    } else if (!strcasecmp(key, "Transfer-Encoding") && strstr(val, "chunked")) {
      m_transfer_encoding_chunked = true; 
    } else if (!strcasecmp(key, "X-Transfer-Status") && strstr(val, "true")) {
      m_transfer_encoding_chunked = true;
      m_status_trailer = true;
    } else if (!strcasecmp(key, "SciTag")) {
      if(prot->pmarkHandle != nullptr) {
        parseScitag(val);
      }
    } else if (!strcasecmp(key, "User-Agent")) {
      m_user_agent = val;
      trim(m_user_agent);
    } else {
      // Some headers need to be translated into "local" cgi info.
      auto it = std::find_if(prot->hdr2cgimap.begin(), prot->hdr2cgimap.end(),[key](const auto & item) {
        return !strcasecmp(key,item.first.c_str());
      });
      if (it != prot->hdr2cgimap.end() && (opaque ? (0 == opaque->Get(it->second.c_str())) : true)) {
        std::string s;
        s.assign(val, line+len-val);
        trim(s);
        addCgi(it->second,s);
      }
    }


    line[pos] = ':';
  }

  return 0;
}

int XrdHttpReq::parseHost(char *line) {
  host = line;
  trim(host);
  return 0;
}

void XrdHttpReq::parseScitag(const std::string & val) {
  // The scitag header has been populated and the packet marking was configured, the scitag will either be equal to 0
  // or to the value passed by the client
  mScitag = 0;
  std::string scitagS = val;
  trim(scitagS);
  if(scitagS.size()) {
    if(scitagS[0] != '-') {
      try {
        mScitag = std::stoi(scitagS.c_str(), nullptr, 10);
        if (mScitag > XrdNetPMark::maxTotID || mScitag < XrdNetPMark::minTotID) {
          mScitag = 0;
        }
      } catch (...) {
        //Nothing to do, scitag = 0 by default
      }
    }
  }
  addCgi("scitag.flow", std::to_string(mScitag));
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


  pos = p - line;
  // The first token cannot be too long
  if (pos > MAX_TK_LEN - 1) {
    request = rtMalformed;
    return -2;
  }

  // The first space-delimited char cannot be the first one
  // this allows to deal with the case when a client sends a first line that starts with a space " GET / HTTP/1.1"
  if(pos == 0) {
      request = rtMalformed;
      return -4;
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

int XrdHttpReq::ReqReadV(const XrdHttpIOList &cl) {


  // Now we build the protocol-ready read ahead list
  //  and also put the correct placeholders inside the cache
  int n = cl.size();
  ralist.clear();
  ralist.reserve(n);

  int j = 0;
  for (const auto &c: cl) {
    ralist.emplace_back();
    auto &ra = ralist.back();
    memcpy(&ra.fhandle, this->fhandle, 4);

    ra.offset = c.offset;
    ra.rlen = c.size;
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
  std::ostringstream s;

  s << "\r\n--" << token << "\r\n";
  s << "Content-type: text/plain; charset=UTF-8\r\n";
  s << "Content-range: bytes " << bytestart << "-" << byteend << "/" << fsz << "\r\n\r\n";

  return s.str();
}

std::string XrdHttpReq::buildPartialHdrEnd(char *token) {
  std::ostringstream s;

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

  // sendfile about to be sent by bridge for fetching data for GET:
  // no https, no chunked+trailer, no multirange

  //prot->SendSimpleResp(200, NULL, NULL, NULL, dlen);
  int rc = info.Send(0, 0, 0, 0);
  TRACE(REQ, " XrdHttpReq::File dlen:" << dlen << " send rc:" << rc);
  bool start, finish;
  // short read will be classed as error
  if (rc) {
    readRangeHandler.NotifyError();
    return false;
  }

  if (readRangeHandler.NotifyReadResult(dlen, nullptr, start, finish) < 0)
    return false;
  
    
  return true;
};

bool XrdHttpReq::Done(XrdXrootd::Bridge::Context & info) {

  TRACE(REQ, " XrdHttpReq::Done");

  xrdresp = kXR_ok;

  this->iovN = 0;
  
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

  if (etext_) {
    char *s = escapeXML(etext_);
    this->etext = s;
    free(s);
  }

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
  
  // port < 0 signals switch to full URL
  if (port < 0)
  {
    if (strncmp(hname, "file://", 7) == 0)
    {
      TRACE(REQ, " XrdHttpReq::Redir Switching to file:// ");
      redirdest = "Location: "; // "file://" already contained in hname
    }
  }
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

  if (port > 0) {
    sprintf(buf, ":%d", port);
    redirdest += buf;
  }

  redirdest += resource.c_str();
  
  // Here we put back the opaque info, if any
  if (vardata) {
    char *newvardata = quote(vardata);
    redirdest += "?&";
    redirdest += newvardata;
    free(newvardata);
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

  
  TRACE(REQ, " XrdHttpReq::Redir Redirecting to " << redirdest.c_str());

  if (request != rtGET)
    prot->SendSimpleResp(307, NULL, (char *) redirdest.c_str(), 0, 0, keepalive);
  else
    prot->SendSimpleResp(302, NULL, (char *) redirdest.c_str(), 0, 0, keepalive);
  
  reset();
  return false;
};


void XrdHttpReq::appendOpaque(XrdOucString &s, XrdSecEntity *secent, char *hash, time_t tnow) {

  int l = 0;
  char * p = 0;
  if (opaque)
    p = opaque->Env(l);

  if (hdr2cgistr.empty() && (l < 2) && !hash) return;

  // this works in most cases, except if the url already contains the xrdhttp tokens
  s = s + "?";
  if (!hdr2cgistr.empty()) {
    char *s1 = quote(hdr2cgistr.c_str());
    if (s1) {
        s += s1;
        free(s1);
    }
  }
  if (p && (l > 1)) {
    char *s1 = quote(p+1);
    if (s1) {
      if (!hdr2cgistr.empty()) {
        s = s + "&";
      }
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
        char *s1 = quote(secent->vorg);
        if (s1) {
          s += s1;
          free(s1);
        }
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


// Sanitize the resource from the http[s]://[host]/ questionable prefix
// https://github.com/xrootd/xrootd/issues/1675
void XrdHttpReq::sanitizeResourcePfx() {
  
  if (resource.beginswith("https://")) {
    // Find the slash that follows the hostname, and keep it
    int p = resource.find('/', 8);
    resource.erasefromstart(p);
    return;
  }
  
  if (resource.beginswith("http://")) {
    // Find the slash that follows the hostname, and keep it
    int p = resource.find('/', 7);
    resource.erasefromstart(p);
    return;
  }
}

void XrdHttpReq::addCgi(const std::string &key, const std::string &value) {
  if (hdr2cgistr.length() > 0) {
    hdr2cgistr.append("&");
  }
  hdr2cgistr.append(key);
  hdr2cgistr.append("=");
  hdr2cgistr.append(value);
}


// Parse a resource line:
// - sanitize
// - extracts the opaque info from the given url
// - sanitize the resource from http[s]://[host]/ questionable prefix
void XrdHttpReq::parseResource(char *res) {




  // Look for the first '?'
  char *p = strchr(res, '?');
  
  // Not found, then it's just a filename
  if (!p) {
    resource.assign(res, 0);
    
    // Some poor client implementations may inject a http[s]://[host]/ prefix
    // to the resource string. Here we choose to ignore it as a protection measure
    sanitizeResourcePfx();  
    
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
  
  // Some poor client implementations may inject a http[s]://[host]/ prefix
  // to the resource string. Here we choose to ignore it as a protection measure
  sanitizeResourcePfx();  
  
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
    resourceplusopaque.append(p + 1);
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
      case kXR_AuthFailed:
        httpStatusCode = 401; httpStatusText = "Unauthorized";
        break;
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
      case kXR_ItExists:
        if(request != ReqType::rtDELETE) {
          httpStatusCode = 409; httpStatusText = "File already exists";
        } else {
          // In the case the XRootD layer returns a kXR_ItExists after a deletion
          // was submitted, we return a 405 status code with the error message set by
          // the XRootD layer
          httpStatusCode = 405;
        }
        break;
      case kXR_InvalidRequest:
        httpStatusCode = 405; httpStatusText = "Method is not allowed";
        break;
      default:
        break;
    }

    if (!etext.empty()) httpStatusText = etext;

    TRACEI(REQ, "PostProcessHTTPReq mapping Xrd error [" << xrderrcode
                 << "] to status code [" << httpStatusCode << "]");

    httpStatusText += "\n";
  } else {
      httpStatusCode = 200;
      httpStatusText = "OK";
  }
}

int XrdHttpReq::ProcessHTTPReq() {

  kXR_int32 l;

  /// If we have to add extra header information, add it here.
  if (!m_appended_hdr2cgistr && !hdr2cgistr.empty()) {
    const char *p = strchr(resourceplusopaque.c_str(), '?');
    if (p) {
      resourceplusopaque.append("&");
    } else {
      resourceplusopaque.append("?");
    }

    char *q = quote(hdr2cgistr.c_str());
    resourceplusopaque.append(q);
    TRACEI(DEBUG, "Appended header fields to opaque info: '" 
                 << hdr2cgistr.c_str() << "'");

    // We assume that anything appended to the CGI str should also
    // apply to the destination in case of a MOVE.
    if (strchr(destination.c_str(), '?')) destination.append("&");
    else destination.append("?");
    destination.append(q);

    free(q);
    m_appended_hdr2cgistr = true;
    }

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

        m_req_cksum = prot->cksumHandler.getChecksumToRun(m_req_digest);
        if(!m_req_cksum) {
            // No HTTP IANA checksums have been configured by the server admin, return a "METHOD_NOT_ALLOWED" error
            prot->SendSimpleResp(403, NULL, NULL, (char *) "No HTTP-IANA compatible checksums have been configured.", 0, false);
            return -1;
        }
        if (!opaque) {
          m_resource_with_digest += "?cks.type=";
          m_resource_with_digest += m_req_cksum->getXRootDConfigDigestName().c_str();
        } else {
          m_resource_with_digest += "&cks.type=";
          m_resource_with_digest += m_req_cksum->getXRootDConfigDigestName().c_str();
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
      
      // The reqstate parameter basically moves us through a simple state machine.
      // - 0: Perform a stat on the resource
      // - 1: Perform a checksum request on the resource (only if requested in header; otherwise skipped)
      // - 2: Perform an open request (dirlist as appropriate).
      // - 3+: Reads from file; if at end, perform a close.
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
        case 1: // Checksum request
          if (!(fileflags & kXR_isDir) && !m_req_digest.empty()) {
            // In this case, the Want-Digest header was set.
            bool has_opaque = strchr(resourceplusopaque.c_str(), '?');
            // Note that doChksum requires that the memory stays alive until the callback is invoked.
            m_req_cksum = prot->cksumHandler.getChecksumToRun(m_req_digest);
            if(!m_req_cksum) {
                // No HTTP IANA checksums have been configured by the server admin, return a "METHOD_NOT_ALLOWED" error
                prot->SendSimpleResp(403, NULL, NULL, (char *) "No HTTP-IANA compatible checksums have been configured.", 0, false);
                return -1;
            }
            m_resource_with_digest = resourceplusopaque;
            if (has_opaque) {
              m_resource_with_digest += "&cks.type=";
              m_resource_with_digest += m_req_cksum->getXRootDConfigDigestName().c_str();
            } else {
              m_resource_with_digest += "?cks.type=";
              m_resource_with_digest += m_req_cksum->getXRootDConfigDigestName().c_str();
            }
            if (prot->doChksum(m_resource_with_digest) < 0) {
              prot->SendSimpleResp(500, NULL, NULL, (char *) "Failed to start internal checksum request to satisfy Want-Digest header.", 0, false);
              return -1;
            }
            return 0;
          } else {
            TRACEI(DEBUG, "No checksum requested; skipping to request state 2");
            reqstate += 1;
          }
        // fallthrough
        case 2: // Open() or dirlist
        {

          if (!prot->Bridge) {
              prot->SendSimpleResp(500, NULL, NULL, (char *) "prot->Bridge is NULL.", 0, false);
              return -1;
            }

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


            std::string res;
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
        // fallthrough
        default: // Read() or Close(); reqstate is 3+
        {

          const XrdHttpIOList &readChunkList = readRangeHandler.NextReadList();

          // Close() if we have finished, otherwise read the next chunk

          // --------- CLOSE
          if ( readChunkList.empty() )
          {

            memset(&xrdreq, 0, sizeof (ClientRequest));
            xrdreq.close.requestid = htons(kXR_close);
            memcpy(xrdreq.close.fhandle, fhandle, 4);

            if (!prot->Bridge->Run((char *) &xrdreq, 0, 0)) {
              prot->SendSimpleResp(404, NULL, NULL, (char *) "Could not run close request.", 0, false);
              return -1;
            }

            // We have finished
            readClosing = true;
            return 1;

          }
          // --------- READ or READV

          if ( readChunkList.size() == 1 ) {
            // Use a read request for single range

            long l;
            long long offs;
            
            // --------- READ
            memset(&xrdreq, 0, sizeof (xrdreq));
            xrdreq.read.requestid = htons(kXR_read);
            memcpy(xrdreq.read.fhandle, fhandle, 4);
            xrdreq.read.dlen = 0;
            
            offs = readChunkList[0].offset;
            l = readChunkList[0].size;

            xrdreq.read.offset = htonll(offs);
            xrdreq.read.rlen = htonl(l);

            // If we are using HTTPS or if the client requested trailers, or if the
            // read concerns a multirange reponse, disable sendfile
            // (in the latter two cases, the extra framing is only done in PostProcessHTTPReq)
            if (prot->ishttps || (m_transfer_encoding_chunked && m_trailer_headers) ||
                !readRangeHandler.isSingleRange()) {
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
            // --------- READV

            length = ReqReadV(readChunkList);

            if (!prot->Bridge->Run((char *) &xrdreq, (char *) &ralist[0], length)) {
              prot->SendSimpleResp(404, NULL, NULL, (char *) "Could not run read request.", 0, false);
              return -1;
            }

          }

          // We want to be invoked again after this request is finished
          return 0;
        } // case 3+
        
      } // switch (reqstate)


    } // case XrdHttpReq::rtGET

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
        if (! XrdHttpProtocol::usingEC) 
          xrdreq.open.options = htons(kXR_mkpath | kXR_open_wrto | kXR_delete);
        else
          xrdreq.open.options = htons(kXR_mkpath | kXR_open_wrto | kXR_new);

        if (!prot->Bridge->Run((char *) &xrdreq, (char *) resourceplusopaque.c_str(), l)) {
          prot->SendSimpleResp(404, NULL, NULL, (char *) "Could not run request.", 0, keepalive);
          return -1;
        }


        // We want to be invoked again after this request is finished
        // Only if there is data to fetch from the socket or there will
        // never be more data
        if (prot->BuffUsed() > 0 || (length == 0 && !sendcontinue))
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
            // Set a maximum size of chunk we will allow
            // Nginx sets this to "NGX_MAX_OFF_T_VALUE", which is 9223372036854775807 (a some crazy number)
            // We set it to 1TB, which is 1099511627776
            // This is to prevent a malicious client from sending a very large chunk size
            // or a malformed chunk request.
            // 1TB in base-16 is 0x40000000000, so only allow 11 characters, plus the CRLF
            long long max_chunk_size_chars = std::min(static_cast<long long>(prot->BuffUsed()), static_cast<long long>(13));
            for (; idx < max_chunk_size_chars; idx++) {
              if (prot->myBuffStart[idx] == '\n') {
                found_newline = true;
                break;
              }
            }
            // If we found a new line, but it is the first character in the buffer (no chunk length)
            // or if the previous character is not a CR.
            if (found_newline && ((idx == 0) || prot->myBuffStart[idx-1] != '\r')) {
              prot->SendSimpleResp(400, NULL, NULL, (char *)"Invalid chunked encoding", 0, false);
              TRACE(REQ, "XrdHTTP PUT: Sending invalid chunk encoding.  Start of chunk should have had a length, followed by a CRLF.");
              return -1;
            }
            if (found_newline) {
              char *endptr = NULL;
              std::string line_contents(prot->myBuffStart, idx);
              long long chunk_contents = strtol(line_contents.c_str(), &endptr, 16);
                // Chunk sizes can be followed by trailer information or CRLF
              if (*endptr != ';' && *endptr != '\r') {
                prot->SendSimpleResp(400, NULL, NULL, (char *)"Invalid chunked encoding", 0, false);
                TRACE(REQ, "XrdHTTP PUT: Sending invalid chunk encoding. Chunk size was not followed by a ';' or CR." << __LINE__);
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
            long long bytes_to_write = std::min(static_cast<long long>(prot->BuffUsed()),
                                           chunk_bytes_remaining);

            xrdreq.write.offset = htonll(writtenbytes);
            xrdreq.write.dlen = htonl(bytes_to_write);

            TRACEI(REQ, "XrdHTTP PUT: Writing chunk of size " << bytes_to_write << " starting with '" << *(prot->myBuffStart) << "'" << " with " << chunk_bytes_remaining << " bytes remaining in the chunk");
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

          long long bytes_to_read = std::min(static_cast<long long>(prot->BuffUsed()),
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
          std::string s = resourceplusopaque.c_str();


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

            std::string s = resourceplusopaque.c_str();

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

            std::string s = resourceplusopaque.c_str();

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
          std::string s = resourceplusopaque.c_str();


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

          std::string s = resourceplusopaque.c_str();
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

      std::string s = resourceplusopaque.c_str();
      xrdreq.mkdir.options[0] = (kXR_char) kXR_mkdirpath;

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

      std::string s = resourceplusopaque.c_str();
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

    TRACEI(REQ, "Checksum for HEAD " << resource.c_str() << " "
               << reinterpret_cast<char *>(iovP[0].iov_base) << "=" 
               << reinterpret_cast<char *>(iovP[iovN-1].iov_base));

    bool convert_to_base64 = m_req_cksum->needsBase64Padding();
    char *digest_value = reinterpret_cast<char *>(iovP[iovN-1].iov_base);
    if (convert_to_base64) {
      size_t digest_length = strlen(digest_value);
      unsigned char *digest_binary_value = (unsigned char *)malloc(digest_length);
      if (!Fromhexdigest(reinterpret_cast<unsigned char *>(digest_value), digest_length, digest_binary_value)) {
        prot->SendSimpleResp(500, NULL, NULL, (char *) "Failed to convert checksum hexdigest to base64.", 0, false);
        free(digest_binary_value);
        return -1;
      }
      char *digest_base64_value = (char *)malloc(digest_length + 1);
      // Binary length is precisely half the size of the hex-encoded digest_value; hence, divide length by 2.
      Tobase64(digest_binary_value, digest_length/2, digest_base64_value);
      free(digest_binary_value);
      digest_value = digest_base64_value;
    }

    digest_header = "Digest: ";
    digest_header += m_req_cksum->getHttpName();
    digest_header += "=";
    digest_header += digest_value;
    if (convert_to_base64) {free(digest_value);}
    return 0;
  } else {
    prot->SendSimpleResp(httpStatusCode, NULL, NULL, httpStatusText.c_str(), httpStatusText.length(), false);
    return -1;
  }
}


// This is invoked by the callbacks, after something has happened in the bridge

int XrdHttpReq::PostProcessHTTPReq(bool final_) {

  TRACEI(REQ, "PostProcessHTTPReq req: " << request << " reqstate: " << reqstate << " final_:" << final_);
  mapXrdErrorToHttpStatus();

  if(xrdreq.set.requestid == htons(kXR_set)) {
    // We have set the user agent, if it fails we return a 500 error, otherwise the callback is successful --> we continue
    if(xrdresp != kXR_ok) {
      prot->SendSimpleResp(500, nullptr, nullptr, "Could not set user agent.", 0, false);
      return -1;
    }
    return 0;
  }

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
          TRACEI(REQ, "Stat for HEAD " << resource.c_str()
                      << " stat=" << (char *) iovP[0].iov_base);

          long dummyl;
          sscanf((const char *) iovP[0].iov_base, "%ld %lld %ld %ld",
                  &dummyl,
                  &filesize,
                  &fileflags,
                  &filemodtime);

          if (m_req_digest.size()) {
            return 0;
          } else {
            prot->SendSimpleResp(200, NULL, "Accept-Ranges: bytes", NULL, filesize, keepalive);
            return keepalive ? 1 : -1;
          }
        }

        prot->SendSimpleResp(httpStatusCode, NULL, NULL, NULL, 0, keepalive);
        reset();
        return keepalive ? 1 : -1;
      } else { // We requested a checksum and now have its response.
        if (iovN > 0) {
          std::string response_headers;
          int response = PostProcessChecksum(response_headers);
          if (-1 == response) {
                return -1;
          }
          if (!response_headers.empty()) {response_headers += "\r\n";}
          response_headers += "Accept-Ranges: bytes";
          prot->SendSimpleResp(200, NULL, response_headers.c_str(), NULL, filesize, keepalive);
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

          char *estr = escapeXML(resource.c_str());
          
          stringresp += "<h1>Listing of: ";
          stringresp += estr;
          stringresp += "</h1>\n";

          free(estr);
          
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
              TRACEI(REQ, "Dirlist " << resource.c_str() << " entry=" << entry 
                       << " stat=" << endp);

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
              std::string p = "<tr>"
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
                
                char *estr = escapeXML(resource.c_str());
                
                  p += estr;
                  p += "/";
                  
                free(estr);
              }
              
              char *estr = escapeXML(e.path.c_str());
              
              p += e.path + "\">";
              p += e.path;
              
              free(estr);
              
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


      } // end handling of dirlist
      else
      { // begin handling of open-read-close

        // To duplicate the state diagram from the rtGET request state
        // - 0: Perform a stat on the resource
        // - 1: Perform a checksum request on the resource (only if requested in header; otherwise skipped)
        // - 2: Perform an open request (dirlist as appropriate).
        // - 3+: Reads from file; if at end, perform a close.
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
                TRACEI(REQ, "Stat for GET " << resource.c_str()
                         << " stat=" << (char *) iovP[0].iov_base);
                
                long dummyl;
                sscanf((const char *) iovP[0].iov_base, "%ld %lld %ld %ld",
                       &dummyl,
                       &filesize,
                       &fileflags,
                       &filemodtime);

                readRangeHandler.SetFilesize(filesize);

                // We will default the response size specified by the headers; if that
                // wasn't given, use the file size.
                if (!length) {
                    length = filesize;
                }
              }
              else {
                TRACEI(REQ, "Can't find the stat information for '" 
                         << resource.c_str() << "' Internal error?");
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
          } // end stat
          case 1:  // checksum was requested and now we have its response.
          {
            return PostProcessChecksum(m_digest_header);
          }
          case 2:  // open
          {
            if (xrdresp == kXR_ok) {

              getfhandle();
              
              // Always try to parse response.  In the case of a caching proxy, the open
              // will have created the file in cache
              if (iovP[1].iov_len > 1) {
                TRACEI(REQ, "Stat for GET " << resource.c_str()
                         << " stat=" << (char *) iovP[1].iov_base);

                long dummyl;
                sscanf((const char *) iovP[1].iov_base, "%ld %lld %ld %ld",
                      &dummyl,
                      &filesize,
                      &fileflags,
                      &filemodtime);

                readRangeHandler.SetFilesize(filesize);

                // As above: if the client specified a response size, we use that.
                // Otherwise, utilize the filesize
                if (!length) {
                  length = filesize;
                }
              }
              else {
                TRACEI(ALL, "GET returned no STAT information. Internal error?");
              }

              std::string responseHeader;
              if (!m_digest_header.empty()) {
                responseHeader = m_digest_header;
              }
              long one;
              if (filemodtime && XrdOucEnv::Import("XRDPFC", one)) {
                  if (!responseHeader.empty()) {
                    responseHeader += "\r\n";
                  }
                  long object_age = time(NULL) - filemodtime;
                  responseHeader += std::string("Age: ") + std::to_string(object_age < 0 ? 0 : object_age);
              }

              const XrdHttpReadRangeHandler::UserRangeList &uranges = readRangeHandler.ListResolvedRanges();
              if (uranges.empty() && readRangeHandler.getError()) {
                prot->SendSimpleResp(readRangeHandler.getError().httpRetCode, NULL, NULL, readRangeHandler.getError().errMsg.c_str(),0,false);
                return -1;
              }

              if (readRangeHandler.isFullFile()) {
                // Full file.

                if (m_transfer_encoding_chunked && m_trailer_headers) {
                  prot->StartChunkedResp(200, NULL, responseHeader.empty() ? NULL : responseHeader.c_str(), -1, keepalive);
                } else {
                  prot->SendSimpleResp(200, NULL, responseHeader.empty() ? NULL : responseHeader.c_str(), NULL, filesize, keepalive);
                }
                return 0;
              }

              if (readRangeHandler.isSingleRange()) {
                // Possibly with zero sized file but should have been included
                // in the FullFile case above
                if (uranges.size() != 1)
                  return -1;

                // Only one range to return to the user
                char buf[64];
                const off_t cnt = uranges[0].end - uranges[0].start + 1;

                XrdOucString s = "Content-Range: bytes ";
                sprintf(buf, "%lld-%lld/%lld", (long long int)uranges[0].start, (long long int)uranges[0].end, filesize);
                s += buf;
                if (!responseHeader.empty()) {
                  s += "\r\n";
                  s += responseHeader.c_str();
                }

                if (m_transfer_encoding_chunked && m_trailer_headers) {
                  prot->StartChunkedResp(206, NULL, (char *)s.c_str(), -1, keepalive);
                } else {
                  prot->SendSimpleResp(206, NULL, (char *)s.c_str(), NULL, cnt, keepalive);
                }
                return 0;
              }

              // Multiple reads to perform, compose and send the header
              off_t cnt = 0;
              for (auto &ur : uranges) {
                cnt += ur.end - ur.start + 1;

                cnt += buildPartialHdr(ur.start,
                        ur.end,
                        filesize,
                        (char *) "123456").size();

              }
              cnt += buildPartialHdrEnd((char *) "123456").size();
              std::string header = "Content-Type: multipart/byteranges; boundary=123456";
              if (!m_digest_header.empty()) {
                header += "\n";
                header += m_digest_header;
              }

              if (m_transfer_encoding_chunked && m_trailer_headers) {
                prot->StartChunkedResp(206, NULL, header.c_str(), -1, keepalive);
              } else {
                prot->SendSimpleResp(206, NULL, header.c_str(), NULL, cnt, keepalive);
              }
              return 0;


            } else { // xrdresp indicates an error occurred
              
              prot->SendSimpleResp(httpStatusCode, NULL, NULL,
                                   httpStatusText.c_str(), httpStatusText.length(), false);
              return -1;
            }

            // Case should not be reachable
            return -1;
          }
          default: //read or readv
          {
            // If we are postprocessing a close, potentially send out informational trailers
            if ((ntohs(xrdreq.header.requestid) == kXR_close) || readClosing)
            {
              const XrdHttpReadRangeHandler::Error &rrerror = readRangeHandler.getError();
              if (rrerror) {
                httpStatusCode = rrerror.httpRetCode;
                httpStatusText = rrerror.errMsg;
              }
                
              if (m_transfer_encoding_chunked && m_trailer_headers) {
                if (prot->ChunkRespHeader(0))
                  return -1;

                const std::string crlf = "\r\n";
                std::stringstream ss;
                ss << "X-Transfer-Status: " << httpStatusCode << ": " << httpStatusText << crlf;

                const auto header = ss.str();
                if (prot->SendData(header.c_str(), header.size()))
                  return -1;

                if (prot->ChunkRespFooter())
                  return -1;
              }

                if (rrerror) return -1;
                return keepalive ? 1 : -1;
            }

            // On error, we can only send out a message if trailers are enabled and the
            // status response in trailer behavior is requested.
            if (xrdresp == kXR_error) {
              if (m_transfer_encoding_chunked && m_trailer_headers && m_status_trailer) {
                // A trailer header is appropriate in this case; this is signified by
                // a chunk with size zero, then the trailer, then a crlf.
                //
                // We only send the status trailer when explicitly requested; otherwise a
                // "normal" HTTP client might simply see a short response and think it's a
                // success
                if (prot->ChunkRespHeader(0))
                  return -1;

                const std::string crlf = "\r\n";
                std::stringstream ss;
                ss << "X-Transfer-Status: " << httpStatusCode << ": " << httpStatusText << crlf;

		const auto header = ss.str();
                if (prot->SendData(header.c_str(), header.size()))
                  return -1;

                if (prot->ChunkRespFooter())
                  return -1;

                return -1;
              } else {
                return -1;
              }
            }


            TRACEI(REQ, "Got data vectors to send:" << iovN);

            XrdHttpIOList received;
            getReadResponse(received);

            int rc;
            if (readRangeHandler.isSingleRange()) {
               rc = sendReadResponseSingleRange(received);
            } else {
               rc = sendReadResponsesMultiRanges(received);
            }
            if (rc) {
              // make sure readRangeHandler will trigger close
              // of file after next NextReadList().
              readRangeHandler.NotifyError();
            }

            return 0;
          } // end read or readv

        } // switch reqstate

      } // End handling of the open-read-close case


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
        prot->ResumeBytes = std::min(length - writtenbytes, (long long) prot->BuffAvailable());

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
          prot->ResumeBytes = std::min(length - writtenbytes, (long long) prot->BuffAvailable());

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
            TRACEI(REQ, "Stat for removal " << resource.c_str() 
                     << " stat=" << (char *) iovP[0].iov_base);

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
            TRACEI(REQ, "Collection " << resource.c_str()
                     << " stat=" << (char *) iovP[0].iov_base);

            long dummyl;
            sscanf((const char *) iovP[0].iov_base, "%ld %lld %ld %ld",
                    &dummyl,
                    &e.size,
                    &e.flags,
                    &e.modtime);

            if (e.path.length() && (e.path != ".") && (e.path != "..")) {
              /* The entry is filled. */


              std::string p;
              stringresp += "<D:response xmlns:lp1=\"DAV:\" xmlns:lp2=\"http://apache.org/dav/props/\" xmlns:lp3=\"LCGDM:\">\n";
              
              char *estr = escapeXML(e.path.c_str());
              
              stringresp += "<D:href>";
              stringresp += estr;
              stringresp += "</D:href>\n";
              
              free(estr);
              
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
            std::string s = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<D:multistatus xmlns:D=\"DAV:\" xmlns:ns1=\"http://apache.org/dav/props/\" xmlns:ns0=\"DAV:\">\n";
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
                TRACEI(REQ, "Dirlist " <<resource.c_str() <<" entry=" <<entry
                            << " stat=" << endp);

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


                std::string p = resource.c_str();
                if (*p.rbegin() != '/') p += "/";
                
                p += e.path;
                
                stringresp += "<D:response xmlns:lp1=\"DAV:\" xmlns:lp2=\"http://apache.org/dav/props/\" xmlns:lp3=\"LCGDM:\">\n";
                
                char *estr = escapeXML(p.c_str());
                stringresp += "<D:href>";
                stringresp += estr;
                stringresp += "</D:href>\n";
                free(estr);
                
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
            std::string s = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<D:multistatus xmlns:D=\"DAV:\" xmlns:ns1=\"http://apache.org/dav/props/\" xmlns:ns0=\"DAV:\">\n";
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
        if (xrderrcode == kXR_ItExists) {
          prot->SendSimpleResp(405, NULL, NULL, (char *) "Method is not allowed; resource already exists.", 0, false);
        } else {
          prot->SendSimpleResp(httpStatusCode, NULL, NULL,
                               httpStatusText.c_str(), httpStatusText.length(), false);
        }
        return -1;
      }

      prot->SendSimpleResp(201, NULL, NULL, (char *) ":-)", 0, keepalive);
      return keepalive ? 1 : -1;

    }
    case XrdHttpReq::rtMOVE:
    {

      if (xrdresp != kXR_ok) {
        prot->SendSimpleResp(httpStatusCode, NULL, NULL, (char *) etext.c_str(), 0, false);
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
  readRangeHandler.reset();
  readClosing = false;
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
  ralist.clear();
  ralist.shrink_to_fit();

  request = rtUnset;
  resource = "";
  allheaders.clear();

  // Reset the state of the request's digest request.
  m_req_digest.clear();
  m_digest_header.clear();
  m_req_cksum = nullptr;

  m_resource_with_digest = "";
  m_user_agent = "";

  headerok = false;
  keepalive = true;
  length = 0;
  filesize = 0;
  depth = 0;
  sendcontinue = false;

  m_transfer_encoding_chunked = false;
  m_current_chunk_size = -1;
  m_current_chunk_offset = 0;

  m_trailer_headers = false;
  m_status_trailer = false;

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
  m_appended_hdr2cgistr = false;

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

void XrdHttpReq::getReadResponse(XrdHttpIOList &received) {
  received.clear();

  if (ntohs(xrdreq.header.requestid) == kXR_readv) {
    readahead_list *l;
    char *p;
    kXR_int32 len;

    // Cycle on all the data that is coming from the server
    for (int i = 0; i < iovN; i++) {

      for (p = (char *) iovP[i].iov_base; p < (char *) iovP[i].iov_base + iovP[i].iov_len;) {
        l = (readahead_list *) p;
        len = ntohl(l->rlen);

        received.emplace_back(p+sizeof(readahead_list), -1, len);

        p += sizeof (readahead_list);
        p += len;

      }
    }
    return;
  }

  // kXR_read result
  for (int i = 0; i < iovN; i++) {
    received.emplace_back((char*)iovP[i].iov_base, -1, iovP[i].iov_len);
  }

}

int XrdHttpReq::sendReadResponsesMultiRanges(const XrdHttpIOList &received) {

  if (received.size() == 0) {
    bool start, finish;
    if (readRangeHandler.NotifyReadResult(0, nullptr, start, finish) < 0) {
      return -1;
    }
    return 0;
  }

  // user is expecting multiple ranges, we must be prepared to send an
  // individual header for each and format it according to the http rules

  struct rinfo {
    bool start;
    bool finish;
    const XrdOucIOVec2 *ci;
    const XrdHttpReadRangeHandler::UserRange *ur;
    std::string st_header;
    std::string fin_header;
  };

  // report each received byte chunk to the range handler and record the details
  // of original user range it related to and if starts a range or finishes all.
  // also sum the total of the headers and data which need to be sent to the user,
  // in case we need it for chunked transfer encoding
  std::vector<rinfo> rvec;
  off_t sum_len = 0;

  rvec.reserve(received.size());

  for(const auto &rcv: received) {
    rinfo rentry;
    bool start, finish;
    const XrdHttpReadRangeHandler::UserRange *ur;

    if (readRangeHandler.NotifyReadResult(rcv.size, &ur, start, finish) < 0) {
      return -1;
    }
    rentry.ur = ur;
    rentry.start = start;
    rentry.finish = finish;
    rentry.ci = &rcv;

    if (start) {
      std::string s = buildPartialHdr(ur->start,
                         ur->end,
                         filesize,
                         (char *) "123456");

      rentry.st_header = s;
      sum_len += s.size();
    }

    sum_len += rcv.size;

    if (finish) {
      std::string s = buildPartialHdrEnd((char *) "123456");
      rentry.fin_header = s;
      sum_len += s.size();
    }

    rvec.push_back(rentry);
  }


  // Send chunked encoding header
  if (m_transfer_encoding_chunked && m_trailer_headers) {
    prot->ChunkRespHeader(sum_len);
  }

  // send the user the headers / data
  for(const auto &rentry: rvec) {

     if (rentry.start) {
       TRACEI(REQ, "Sending multipart: " << rentry.ur->start << "-" << rentry.ur->end);
       if (prot->SendData((char *) rentry.st_header.c_str(), rentry.st_header.size())) {
         return -1;
       }
    }

    // Send all the data we have
    if (prot->SendData((char *) rentry.ci->data, rentry.ci->size)) {
      return -1;
    }

    if (rentry.finish) {
      if (prot->SendData((char *) rentry.fin_header.c_str(), rentry.fin_header.size())) {
        return -1;
      }
    }
  }

  // Send chunked encoding footer
  if (m_transfer_encoding_chunked && m_trailer_headers) {
    prot->ChunkRespFooter();
  }

  return 0;
}

int XrdHttpReq::sendReadResponseSingleRange(const XrdHttpIOList &received) {
  // single range http transfer

  if (received.size() == 0) {
    bool start, finish;
    if (readRangeHandler.NotifyReadResult(0, nullptr, start, finish) < 0) {
      return -1;
    }
    return 0;
  }

  off_t sum = 0;
  // notify the range handler and return if error
  for(const auto &rcv: received) {
    bool start, finish;
    if (readRangeHandler.NotifyReadResult(rcv.size, nullptr, start, finish) < 0) {
      return -1;
    }
    sum += rcv.size;
  }

  // Send chunked encoding header
  if (m_transfer_encoding_chunked && m_trailer_headers) {
    prot->ChunkRespHeader(sum);
  }
  for(const auto &rcv: received) {
    if (prot->SendData((char *) rcv.data, rcv.size)) return -1;
  }
  if (m_transfer_encoding_chunked && m_trailer_headers) {
    prot->ChunkRespFooter();
  }
  return 0;
}
