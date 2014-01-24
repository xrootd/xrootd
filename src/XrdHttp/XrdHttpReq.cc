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

#include "XrdHttpUtils.hh"

#include "XrdHttpStatic.hh"

#define MAX_TK_LEN      256
#define MAX_RESOURCE_LEN 16384

// This is to fix the trace macros
#define TRACELINK prot->Link






















void trim(std::string &str)
{
  // Trim leading non-letters
  while(str.size() && !isalnum(str[0])) str.erase(str.begin());

  // Trim trailing non-letters
  
  while(str.size() && !isalnum(str[str.size()-1]))
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
    while (!isalnum(*val) || (!*val)) val++;

    // Here we are supposed to initialize whatever flag or variable that is needed
    // by looking at the first token of the line
    // The token is key
    // The value is val

    // Screen out the needed header lines
    if (!strcmp(key, "Connection")) {


      if (!strcmp(val, "Keep-Alive"))
        keepalive = true;

    } else if (!strcmp(key, "Host")) {
      parseHost(val);
    } else if (!strcmp(key, "Range")) {
      parseContentRange(val);
    } else if (!strcmp(key, "Content-Length")) {
      length = atoll(val);

    } else if (!strcmp(key, "Destination")) {
      destination.assign(val, line+len-val);
      trim(destination);
    } else if (!strcmp(key, "Depth")) {
      depth = -1;
      if (strcmp(val, "infinity"))
        depth = atoll(val);

    } else if (!strcmp(key, "Expect") && strstr(val, "100-continue")) {
      sendcontinue = true;
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
  return true;
};

bool XrdHttpReq::Done(XrdXrootd::Bridge::Context & info) {

  TRACE(REQ, " XrdHttpReq::Done");

  xrdresp = kXR_ok;
  this->iovN = 0;

  if (PostProcessHTTPReq(true)) reset();

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

  redirdest += hname;

  if (port) {
    sprintf(buf, ":%d", port);
    redirdest += buf;
  }

  redirdest += resource.c_str();

  TRACE(REQ, " XrdHttpReq::Redir Redirecting to " << redirdest);

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



  prot->SendSimpleResp(302, NULL, (char *) redirdest.c_str(), 0, 0);

  reset();
  return false;
};



// Appends the opaque info that we have

void XrdHttpReq::appendOpaque(XrdOucString &s, XrdSecEntity *secent, char *hash, time_t tnow) {

  int l = 0;
  char * p = 0;
  if (opaque)
    p = opaque->Env(l);

  if ((l < 2) && !hash) return;

  s = s + "?";
  if (p && (l > 1)) s = s + (p + 1);



  if (hash) {
    if (l > 1) s += "&";
    s += "xrdhttptk=";
    s += hash;

    s += "&xrdhttptime=";
    char buf[32];
    sprintf(buf, "%ld", tnow);
    s += buf;

    if (secent) {
      if (secent->name) {
        s += "&xrdhttpname=";

        char *s1 = quote(secent->name);
        s += s1;
        free(s1);
      }

      if (secent->vorg) {
        s += "&xrdhttpvorg=";
        s += secent->vorg;
      }

//      if (secent->host) {
//        s += "&xrdhttphost=";
//        s += secent->host;
//      }

    }
  }

}

// Extracts the opaque info from the given url

void XrdHttpReq::parseResource(char *res) {
  // Look for the first '?'
  char *p = strchr(res, '?');

  // Not found, then it's just a filename
  if (!p) {
    resource.assign(res, 0, MAX_RESOURCE_LEN);
    return;
  }

  // Whatever comes before '?' is a filename

  int cnt = p - res; // Number of chars to copy
  resource.assign(res, 0, cnt - 1);

  // Whatever comes after is opaque data to be parsed
  if (strlen(p) > 1)
    opaque = new XrdOucEnv(p + 1);

}

int XrdHttpReq::ProcessHTTPReq() {

  kXR_int32 l;

  //
  // Prepare the data part
  //

  switch (request) {
    case XrdHttpReq::rtUnknown:
    case XrdHttpReq::rtMalformed:
    {
      prot->SendSimpleResp(400, NULL, NULL, (char *) "Request malformed", 0);
      reset();
      return -1;
    }
    case XrdHttpReq::rtHEAD:
    {

      // Do a Stat
      if (prot->doStat((char *) resource.c_str())) {
        prot->SendSimpleResp(404, NULL, NULL, (char *) "Could not run request.", 0);
        return -1;
      }

      return 1;
    }
    case XrdHttpReq::rtGET:
    {

      if (prot->embeddedstatic && resource.beginswith("/static/")) {
        // This is a request for an embedded static resource
        // The sysadmin can always use the file system if he wants to put his own.
        // In that case he has to respect the behavior of
        // the xrootd with the oss.localroot and all.export directives, which is
        // not very practical.

        if (resource == "/static/css/xrdhttp.css") {
          prot->SendSimpleResp(200, NULL, NULL, (char *) static_css_xrdhttp_css, static_css_xrdhttp_css_len);
          reset();
          return 1;
        }
        if (resource == "/static/icons/xrdhttp.ico") {
          prot->SendSimpleResp(200, NULL, NULL, (char *) favicon_ico, favicon_ico_len);
          reset();
          return 1;
        }


      }



      switch (reqstate) {
        case 0: // Stat()
          

          // Do a Stat
          if (prot->doStat((char *) resource.c_str())) {
            XrdOucString errmsg = "Error stating";
            errmsg += resource.c_str();
            prot->SendSimpleResp(404, NULL, NULL, (char *) errmsg.c_str(), 0);
            return -1;
          }

          return 0;
        case 1: // Open() or dirlist
        {

          if (fileflags & kXR_isDir) {

            if (prot->listdeny) {
              prot->SendSimpleResp(503, NULL, NULL, (char *) "Listings are disabled.", 0);
              return -1;
            }

            if (prot->listredir) {
              XrdOucString s = "Location: ";
              s.append(prot->listredir);

              if (s.endswith('/'))
                s.erasefromend(1);

              s.append(resource);
              appendOpaque(s, 0, 0, 0);

              prot->SendSimpleResp(302, NULL, (char *) s.c_str(), 0, 0);
              return -1;
            }


            string res;
            res = resource.c_str();
            //res += "?xrd.dirstat=1";

            // --------- DIRLIST
            memset(&xrdreq, 0, sizeof (ClientRequest));
            xrdreq.dirlist.requestid = htons(kXR_dirlist);
            xrdreq.dirlist.options[0] = kXR_dstat;
            l = res.length() + 1;
            xrdreq.dirlist.dlen = htonl(l);

            if (!prot->Bridge->Run((char *) &xrdreq, (char *) res.c_str(), l)) {
              prot->SendSimpleResp(404, NULL, NULL, (char *) "Could not run request.", 0);
              return -1;
            }

            // We don't want to be invoked again after this request is finished
            return 1;

          } else {


            // --------- OPEN
            memset(&xrdreq, 0, sizeof (ClientRequest));
            xrdreq.open.requestid = htons(kXR_open);
            l = resource.length() + 1;
            xrdreq.open.dlen = htonl(l);
            xrdreq.open.mode = 0;
            xrdreq.open.options = htons(kXR_retstat | kXR_open_read);

            if (!prot->Bridge->Run((char *) &xrdreq, (char *) resource.c_str(), l)) {
              prot->SendSimpleResp(404, NULL, NULL, (char *) "Could not run request.", 0);
              return -1;
            }

            // We want to be invoked again after this request is finished
            return 0;
          }


        }
        case 2: // Read()
        {

          if (rwOps.size() <= 1) {
            // No chunks or one chunk... Request the whole file or single read


            // --------- READ
            memset(&xrdreq, 0, sizeof (xrdreq));
            xrdreq.read.requestid = htons(kXR_read);
            memcpy(xrdreq.read.fhandle, fhandle, 4);
            xrdreq.read.dlen = 0;

            if (rwOps.size() == 0) {
              xrdreq.read.offset = 0;
              xrdreq.read.rlen = htonl(filesize);
            } else {
              xrdreq.read.offset = htonll(rwOps[0].bytestart);
              xrdreq.read.rlen = htonl(rwOps[0].byteend - rwOps[0].bytestart + 1);
            }

            if (prot->ishttps) {
              if (!prot->Bridge->setSF((kXR_char *) fhandle, false)) {
                TRACE(REQ, " XrdBridge::SetSF(false) failed.");

              }
            }

            if (!prot->Bridge->Run((char *) &xrdreq, 0, 0)) {
              prot->SendSimpleResp(404, NULL, NULL, (char *) "Could not run read request.", 0);
              return -1;
            }
          } else {
            // More than one chunk to read... use readv

            length = ReqReadV();

            if (!prot->Bridge->Run((char *) &xrdreq, (char *) ralist, length)) {
              prot->SendSimpleResp(404, NULL, NULL, (char *) "Could not run read request.", 0);
              return -1;
            }

          }

          // We want to be invoked again after this request is finished
          return 0;
        }
        case 3: // Close()
        {
          // --------- READ
          memset(&xrdreq, 0, sizeof (ClientRequest));
          xrdreq.close.requestid = htons(kXR_close);
          memcpy(xrdreq.close.fhandle, fhandle, 4);


          if (!prot->Bridge->Run((char *) &xrdreq, 0, 0)) {
            prot->SendSimpleResp(404, NULL, NULL, (char *) "Could not run close request.", 0);
            return -1;
          }

          // We have finished

          return 1;

        }
        default:
          return -1;
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
        l = resource.length() + 1;
        xrdreq.open.dlen = htonl(l);
        xrdreq.open.mode = htons(kXR_ur | kXR_uw | kXR_gw | kXR_gr | kXR_or);
        xrdreq.open.options = htons(kXR_mkpath | kXR_open_updt | kXR_new);

        if (!prot->Bridge->Run((char *) &xrdreq, (char *) resource.c_str(), l)) {
          prot->SendSimpleResp(404, NULL, NULL, (char *) "Could not run request.", 0);
          return -1;
        }


        // We want to be invoked again after this request is finished
        // Only if there is data to fetch from the socket
        if (prot->BuffUsed() > 0)
          return 0;

        return 1;

      } else {

        // Check if we have finished
        if (writtenbytes < length) {


          // --------- WRITE
          memset(&xrdreq, 0, sizeof (xrdreq));
          xrdreq.write.requestid = htons(kXR_write);
          memcpy(xrdreq.write.fhandle, fhandle, 4);


          xrdreq.write.offset = htonll(writtenbytes);
          xrdreq.write.dlen = htonl(prot->BuffUsed());

          TRACEI(REQ, "Writing " << prot->BuffUsed());
          if (!prot->Bridge->Run((char *) &xrdreq, prot->myBuffStart, prot->BuffUsed())) {
            prot->SendSimpleResp(404, NULL, NULL, (char *) "Could not run write request.", 0);
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
            prot->SendSimpleResp(404, NULL, NULL, (char *) "Could not run close request.", 0);
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
      prot->SendSimpleResp(200, NULL, (char *) "DAV: 1\r\nDAV: <http://apache.org/dav/propset/fs/1>\r\nAllow: HEAD,GET,PUT,PROPFIND,DELETE,OPTIONS", NULL, 0);
      reset();
      return 1;
    }
    case XrdHttpReq::rtDELETE:
    {


      switch (reqstate) {

        case 0: // Stat()
        {


          // --------- STAT is always the first step
          memset(&xrdreq, 0, sizeof (ClientRequest));
          xrdreq.stat.requestid = htons(kXR_stat);
          string s = resource.c_str();


          l = resource.length() + 1;
          xrdreq.stat.dlen = htonl(l);

          if (!prot->Bridge->Run((char *) &xrdreq, (char *) resource.c_str(), l)) {
            prot->SendSimpleResp(501, NULL, NULL, (char *) "Could not run request.", 0);
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

            string s = resource.c_str();

            l = s.length() + 1;
            xrdreq.rmdir.dlen = htonl(l);

            if (!prot->Bridge->Run((char *) &xrdreq, (char *) s.c_str(), l)) {
              prot->SendSimpleResp(501, NULL, NULL, (char *) "Could not run rmdir request.", 0);
              return -1;
            }
          } else {
            // --------- DELETE
            memset(&xrdreq, 0, sizeof (ClientRequest));
            xrdreq.rm.requestid = htons(kXR_rm);

            string s = resource.c_str();

            l = s.length() + 1;
            xrdreq.rm.dlen = htonl(l);

            if (!prot->Bridge->Run((char *) &xrdreq, (char *) s.c_str(), l)) {
              prot->SendSimpleResp(501, NULL, NULL, (char *) "Could not run rm request.", 0);
              return -1;
            }
          }


          // We don't want to be invoked again after this request is finished
          return 1;

      }



    }
    case XrdHttpReq::rtPATCH:
    {
      prot->SendSimpleResp(501, NULL, NULL, (char *) "Request not supported yet.", 0);

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
              prot->SendSimpleResp(501, NULL, NULL, (char *) "Error in getting the PROPFIND request body.", 0);
              return -1;
            }

            if ((depth > 1) || (depth < 0)) {
              prot->SendSimpleResp(501, NULL, NULL, (char *) "Invalid depth value.", 0);
              return -1;
            }


            parseBody(p, length);
          }


          // --------- STAT is always the first step
          memset(&xrdreq, 0, sizeof (ClientRequest));
          xrdreq.stat.requestid = htons(kXR_stat);
          string s = resource.c_str();


          l = resource.length() + 1;
          xrdreq.stat.dlen = htonl(l);

          if (!prot->Bridge->Run((char *) &xrdreq, (char *) resource.c_str(), l)) {
            prot->SendSimpleResp(501, NULL, NULL, (char *) "Could not run request.", 0);
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

          string s = resource.c_str();
          xrdreq.dirlist.options[0] = kXR_dstat;
          //s += "?xrd.dirstat=1";

          l = s.length() + 1;
          xrdreq.dirlist.dlen = htonl(l);

          if (!prot->Bridge->Run((char *) &xrdreq, (char *) s.c_str(), l)) {
            prot->SendSimpleResp(501, NULL, NULL, (char *) "Could not run request.", 0);
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

      string s = resource.c_str();
      xrdreq.mkdir.options[0] = (kXR_char) kXR_mkpath;

      l = s.length() + 1;
      xrdreq.mkdir.dlen = htonl(l);

      if (!prot->Bridge->Run((char *) &xrdreq, (char *) s.c_str(), l)) {
        prot->SendSimpleResp(501, NULL, NULL, (char *) "Could not run request.", 0);
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

      string s = resource.c_str();
      s += " ";

      char buf[256];
      char *ppath;
      int port = 0;
      if (parseURL((char *) destination.c_str(), buf, port, &ppath)) {
        prot->SendSimpleResp(501, NULL, NULL, (char *) "Cannot parse destination url.", 0);
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
        prot->SendSimpleResp(501, NULL, NULL, (char *) "Only in-place renaming is supported for MOVE.", 0);
        return -1;
      }




      s += ppath;

      l = s.length() + 1;
      xrdreq.mv.dlen = htonl(l);

      if (!prot->Bridge->Run((char *) &xrdreq, (char *) s.c_str(), l)) {
        prot->SendSimpleResp(501, NULL, NULL, (char *) "Could not run request.", 0);
        return -1;
      }

      // We don't want to be invoked again after this request is finished
      return 1;

    }
    default:
    {
      prot->SendSimpleResp(501, NULL, NULL, (char *) "Request not supported.", 0);
      return -1;
    }

  }

  return 1;
}



// This is invoked by the callbacks, after something has happened in the bridge

int XrdHttpReq::PostProcessHTTPReq(bool final_) {

  TRACEI(REQ, "PostProcessHTTPReq req: " << request << " reqstate: " << reqstate);

  switch (request) {
    case XrdHttpReq::rtUnknown:
    case XrdHttpReq::rtMalformed:
    {
      prot->SendSimpleResp(400, NULL, NULL, (char *) "Request malformed", 0);
      return -1;
    }
    case XrdHttpReq::rtHEAD:
    {

      if (xrdresp == kXR_ok) {

        if (iovN > 0) {

          // Now parse the stat info
          TRACEI(REQ, "Stat for HEAD " << resource << " stat=" << (char *) iovP[0].iov_base);

          long dummyl;
          sscanf((const char *) iovP[0].iov_base, "%ld %lld %ld %ld",
                  &dummyl,
                  &filesize,
                  &fileflags,
                  &filemodtime);

          prot->SendSimpleResp(200, NULL, NULL, NULL, filesize);
          return 1;
        }

        prot->SendSimpleResp(500, NULL, NULL, NULL, 0);
        reset();
        return 1;
      } else {
        prot->SendSimpleResp(404, NULL, NULL, (char *) "Error man!", 0);
        return -1;
      }
    }
    case XrdHttpReq::rtGET:
    {

      if (xrdreq.header.requestid == ntohs(kXR_dirlist)) {


        if (xrdresp == kXR_error) {
          prot->SendSimpleResp(404, NULL, NULL, (char *) etext.c_str(), 0);
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

            int l = 0;
            if (endp) {
              l = strlen(endp);
              startp = endp + l + 2;
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
         
          if (prot->SecEntity.vorg) {
            stringresp += " (VO: ";
            stringresp += prot->SecEntity.vorg;

          }

          if (prot->SecEntity.role) {
            stringresp += " Role: ";
            stringresp += prot->SecEntity.role;
            stringresp += " )";
          }
 
          if (prot->SecEntity.host) {
            stringresp += " ( ";
            stringresp += prot->SecEntity.host;
            stringresp += " )";
          }

          stringresp += "</span></p>\n";
          stringresp += "<p>Powered by XrdHTTP ";
          stringresp += XrdVSTRING;
          stringresp += " (CERN IT-SDC)</p>\n";

          prot->SendSimpleResp(200, NULL, NULL, (char *) stringresp.c_str(), 0);
          stringresp.clear();
          return 1;
        }


      } else {

        // If it's a dir then treat it as a dir by redirecting to ourself with one more slash
        if (xrderrcode == 3016) {

          string res = "Location: http://";
          if (prot->ishttps) res = "Location: https://";
          res += host;
          res += resource.c_str();
          res += "/";


          prot->SendSimpleResp(302, NULL, (char *) res.c_str(), NULL, 0);
          return 1;
        }

        switch (reqstate) {
          case 0: //stat
          {
            if (xrdresp != kXR_ok) {
              prot->SendSimpleResp(404, NULL, NULL, (char *) "File not found.", 0);
              return -1;
            }

            if (iovN > 0) {

              // Now parse the stat info
              TRACEI(REQ, "Stat for GET " << resource << " stat=" << (char *) iovP[0].iov_base);

              long dummyl;
              sscanf((const char *) iovP[0].iov_base, "%ld %lld %ld %ld",
                      &dummyl,
                      &filesize,
                      &fileflags,
                      &filemodtime);
            }

            return 0;
          }
          case 1: //open 
          {


            if (xrdresp == kXR_ok) {


              getfhandle();


              if (rwOps.size() == 0) {
                // Full file
                
                prot->SendSimpleResp(200, NULL, NULL, NULL, filesize);
                return 0;
              } else
                if (rwOps.size() == 1) {
                // Only one read to perform
                int cnt = (rwOps[0].byteend - rwOps[0].bytestart + 1);
                char buf[64];
                
                XrdOucString s = "Content-Range: bytes ";
                sprintf(buf, "%lld-%lld/%d", rwOps[0].bytestart, rwOps[0].byteend, cnt);
                s += buf;
                
                
                prot->SendSimpleResp(206, NULL, (char *)s.c_str(), NULL, cnt);
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

                prot->SendSimpleResp(206, NULL, (char *) "Content-Type: multipart/byteranges; boundary=123456", NULL, cnt);
                return 0;
              }



            } else {

              prot->SendSimpleResp(404, NULL, NULL, (char *) "Error man!", 0);
              return -1;
            }
          }
          case 2: //read
          {
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
                    prot->SendData((char *) s.c_str(), s.size());
                  }

                  // Send all the data we have
                  prot->SendData(p + sizeof (readahead_list), len);

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
                prot->SendData((char *) s.c_str(), s.size());
              }

            } else
              for (int i = 0; i < iovN; i++) {
                prot->SendData((char *) iovP[i].iov_base, iovP[i].iov_len);
              }

            return 0;
          }
          case 3: //close
          {
            return 1;
          }




          default:
            break;
        }


      }


      break;
    }


    case XrdHttpReq::rtPUT:
    {
      if (!fopened) {

        if (xrdresp != kXR_ok) {

          prot->SendSimpleResp(409, NULL, NULL, (char *) "Error man!", 0);
          return -1;
        }

        getfhandle();
        fopened = true;

        // We try to completely fill up our buffer before flushing
        prot->ResumeBytes = min(length - writtenbytes, (long long) prot->BuffAvailable());

        if (sendcontinue) {
          prot->SendSimpleResp(100, NULL, NULL, 0, 0);
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

          // We try to completely fill up our buffer before flushing
          prot->ResumeBytes = min(length - writtenbytes, (long long) prot->BuffAvailable());

          return 0;
        }

        if (ntohs(xrdreq.header.requestid) == kXR_close) {
          if (xrdresp == kXR_ok) {
            prot->SendSimpleResp(200, NULL, NULL, (char *) ":-)", 0);
            return 1;
          } else {
            prot->SendSimpleResp(500, NULL, NULL, (char *) etext.c_str(), 0);
            return -1;
          }
        }


      }





      break;
    }



    case XrdHttpReq::rtDELETE:
    {

      if (xrdresp != kXR_ok) {
        prot->SendSimpleResp(404, NULL, NULL, (char *) etext.c_str(), 0);
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
            prot->SendSimpleResp(200, NULL, NULL, (char *) ":-)", 0);
            return 1;
          }
          prot->SendSimpleResp(500, NULL, NULL, (char *) "Internal Error", 0);
          return -1;
        }
      }


    }

    case XrdHttpReq::rtPROPFIND:
    {

      if (xrdresp == kXR_error) {
        prot->SendSimpleResp(404, NULL, NULL, (char *) etext.c_str(), 0);
        return -1;
      }

      switch (reqstate) {

        case 0: // response to stat()
        {

          // Now parse the answer building the entries vector
          if (iovN > 0) {
            DirListInfo e;
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
          if (depth == 0) {
            string s = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<D:multistatus xmlns:D=\"DAV:\" xmlns:ns1=\"http://apache.org/dav/props/\" xmlns:ns0=\"DAV:\">\n";
            stringresp.insert(0, s);
            stringresp += "</D:multistatus>\n";
            prot->SendSimpleResp(207, (char *) "Multi-Status", (char *) "Content-Type: text/xml; charset=\"utf-8\"",
                    (char *) stringresp.c_str(), stringresp.length());
            stringresp.clear();
            return 1;
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

              int l = 0;
              if (endp) l = strlen(endp);
              startp = endp + l + 2;
            }
          }



          // If this was the last bunch of entries, send the buffer and empty it immediately
          if (final_) {
            string s = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<D:multistatus xmlns:D=\"DAV:\" xmlns:ns1=\"http://apache.org/dav/props/\" xmlns:ns0=\"DAV:\">\n";
            stringresp.insert(0, s);
            stringresp += "</D:multistatus>\n";
            prot->SendSimpleResp(207, (char *) "Multi-Status", (char *) "Content-Type: text/xml; charset=\"utf-8\"",
                    (char *) stringresp.c_str(), stringresp.length());
            stringresp.clear();
            return 1;
          }

          break;
        } // default reqstate
      } // switch reqstate


      break;

    } // case propfind

    case XrdHttpReq::rtMKCOL:
    {

      if (xrdresp != kXR_ok) {
        prot->SendSimpleResp(409, NULL, NULL, (char *) etext.c_str(), 0);
        return -1;
      }

      prot->SendSimpleResp(201, NULL, NULL, (char *) ":-)", 0);
      return 1;

    }
    case XrdHttpReq::rtMOVE:
    {

      if (xrdresp != kXR_ok) {
        prot->SendSimpleResp(409, NULL, NULL, (char *) etext.c_str(), 0);
        return -1;
      }

      prot->SendSimpleResp(201, NULL, NULL, (char *) ":-)", 0);
      return 1;

    }

    default:
      break;

  }





  switch (xrdresp) {
    case kXR_error:
      prot->SendSimpleResp(500, NULL, NULL, (char *) etext.c_str(), 0);
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


  keepalive = false;
  length = 0;
  //xmlbody = 0;
  depth = 0;
  xrdresp = kXR_noResponsesYet;
  xrderrcode = kXR_noErrorYet;
  if (ralist) free(ralist);
  ralist = 0;

  request = rtUnknown;
  resource[0] = 0;

  headerok = false;
  keepalive = true;
  length = 0;
  depth = 0;
  sendcontinue = false;


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
