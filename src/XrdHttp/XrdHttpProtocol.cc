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


#include "XrdVersion.hh"

#include "Xrd/XrdBuffer.hh"
#include "Xrd/XrdLink.hh"
#include "XProtocol/XProtocol.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "XrdSys/XrdSysPlugin.hh"


#include "XrdHttpTrace.hh"
#include "XrdHttpProtocol.hh"
//#include "XrdXrootd/XrdXrootdStats.hh"

#include <sys/stat.h>
#include "XrdHttpUtils.hh"
#include "XrdHttpSecXtractor.hh"


#include <openssl/err.h>
#include <vector>
#include <arpa/inet.h>

#define XRHTTP_TK_GRACETIME     600



/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/



//
// Static stuff
//

int XrdHttpProtocol::hailWait = 0;
int XrdHttpProtocol::readWait = 0;
int XrdHttpProtocol::Port = 1094;
char *XrdHttpProtocol::Port_str = 0;
char *XrdHttpProtocol::Addr_str = 0;
int XrdHttpProtocol::Window = 0;
char *XrdHttpProtocol::SecLib = 0;

//XrdXrootdStats *XrdHttpProtocol::SI = 0;
char *XrdHttpProtocol::sslcert = 0;
char *XrdHttpProtocol::sslkey = 0;
char *XrdHttpProtocol::sslcadir = 0;
char *XrdHttpProtocol::listredir = 0;
bool XrdHttpProtocol::listdeny = false;
bool XrdHttpProtocol::embeddedstatic = true;
kXR_int32 XrdHttpProtocol::myRole = kXR_isManager;
bool XrdHttpProtocol::selfhttps2http = false;
bool XrdHttpProtocol::isdesthttps = false;
char *XrdHttpProtocol::sslcafile = 0;
char *XrdHttpProtocol::secretkey = 0;
int XrdHttpProtocol::sslverifydepth = 9;
SSL_CTX *XrdHttpProtocol::sslctx = 0;
BIO *XrdHttpProtocol::sslbio_err = 0;
XrdCryptoFactory *XrdHttpProtocol::myCryptoFactory = 0;
XrdHttpSecXtractor *XrdHttpProtocol::secxtractor = 0;

static const unsigned char *s_server_session_id_context = (const unsigned char *) "XrdHTTPSessionCtx";
static int s_server_session_id_context_len = 18;

XrdScheduler *XrdHttpProtocol::Sched = 0; // System scheduler
XrdBuffManager *XrdHttpProtocol::BPool = 0; // Buffer manager
XrdSysError XrdHttpProtocol::eDest = 0; // Error message handler
XrdSecService *XrdHttpProtocol::CIA = 0; // Authentication Server


/******************************************************************************/
/*            P r o t o c o l   M a n a g e m e n t   S t a c k s             */
/******************************************************************************/

XrdObjectQ<XrdHttpProtocol>
XrdHttpProtocol::ProtStack("ProtStack",
        "xrootd protocol anchor");

/******************************************************************************/
/*                       P r o t o c o l   L o a d e r                        */
/*                        X r d g e t P r o t o c o l                         */
/******************************************************************************/

// This protocol can live in a shared library. The interface below is used by
// the protocol driver to obtain a copy of the protocol object that can be used
// to decide whether or not a link is talking a particular protocol.
//
XrdVERSIONINFO(XrdgetProtocol, xrootd);

extern "C" {

  XrdProtocol *XrdgetProtocol(const char *pname, char *parms,
          XrdProtocol_Config *pi) {
    XrdProtocol *pp = 0;
    const char *txt = "completed.";

    // Put up the banner
    //
    pi->eDest->Say("Copr. 2012 CERN IT, an HTTP implementation for the XROOTD framework.");
    pi->eDest->Say("++++++ HTTP protocol initialization started.");

    // Return the protocol object to be used if static init succeeds
    //
    if (XrdHttpProtocol::Configure(parms, pi))
      pp = (XrdProtocol *)new XrdHttpProtocol(false);
    else txt = "failed.";
    pi->eDest->Say("------ HTTP protocol initialization ", txt);
    return pp;
  }
}

/******************************************************************************/
/*                                                                            */
/*           P r o t o c o l   P o r t   D e t e r m i n a t i o n            */
/*                    X r d g e t P r o t o c o l P o r t                     */
/******************************************************************************/

// This function is called early on to determine the port we need to use. The
// default is ostensibly 1094 but can be overidden; which we allow.
//
XrdVERSIONINFO(XrdgetProtocolPort, xrootd);

extern "C" {

  int XrdgetProtocolPort(const char *pname, char *parms, XrdProtocol_Config *pi) {

    // Figure out what port number we should return. In practice only one port
    // number is allowed. However, we could potentially have a clustered port
    // and several unclustered ports. So, we let this practicality slide.
    //
    if (pi->Port < 0) return 1094;
    return pi->Port;
  }
}

/******************************************************************************/
/*               X r d H T T P P r o t o c o l   C l a s s                    */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */

/******************************************************************************/

XrdHttpProtocol::XrdHttpProtocol(bool imhttps)
: XrdProtocol("HTTP protocol handler"), ProtLink(this),
SecEntity(""), CurrentReq(this) {
  myBuff = 0;
  Reset();
  ishttps = imhttps;

}

/******************************************************************************/
/*                   A s s i g n m e n t   O p e r a t o r                    */

/******************************************************************************/

XrdHttpProtocol XrdHttpProtocol::operator =(const XrdHttpProtocol &rhs) {

  return *this;
}

/******************************************************************************/
/*                                 M a t c h                                  */
/******************************************************************************/

#define TRACELINK lp

XrdProtocol *XrdHttpProtocol::Match(XrdLink *lp) {
  char mybuf[16], mybuf2[1024];
  XrdHttpProtocol *hp;
  int dlen;
  bool myishttps = false;

  // Peek at the first 20 bytes of data
  //
  if ((dlen = lp->Peek(mybuf, (int) sizeof (mybuf), hailWait)) < (int) sizeof (mybuf)) {
    if (dlen <= 0) lp->setEtext("handshake not received");
    return (XrdProtocol *) 0;
  }
  mybuf[dlen - 1] = '\0';

  // Trace the data
  //

  TRACEI(DEBUG, "received dlen: " << dlen);
  //TRACEI(REQ, "received buf: " << mybuf);
  mybuf2[0] = '\0';
  for (int i = 0; i < dlen; i++) {
    char mybuf3[16];
    sprintf(mybuf3, "%.02d ", mybuf[i]);
    strcat(mybuf2, mybuf3);

  }
  TRACEI(DEBUG, "received dump: " << mybuf2);

  // Decide if it looks http or not. For now we are happy if all the received characters are alphanumeric
  bool ismine = true;
  for (int i = 0; i < dlen - 1; i++)
    if (!isprint(mybuf[i]) && (mybuf[i] != '\r') && (mybuf[i] != '\n')) {
      ismine = false;
      TRACEI(DEBUG, "This does not look like http at pos " << i);
      break;
    }

  // If it does not look http then look if it looks like https
  if ((!ismine) && (dlen >= 4)) {
    char check[4] = {00, 00, 00, 00};
    if (memcmp(mybuf, check, 4)) {

      if (sslcert) {
        ismine = true;
        myishttps = true;
        TRACEI(DEBUG, "This may look like https");
      } else {
        TRACEI(ALL, "This may look like https, but https is not configured");
      }

    }
  }

  if (!ismine) {
    TRACEI(DEBUG, "This does not look like https. Protocol not matched.");
    return (XrdProtocol *) 0;
  }

  // It does look http or https...
  // Get a protocol object off the stack (if none, allocate a new one)
  //

  TRACEI(REQ, "Protocol matched. https: " << myishttps);
  if (!(hp = ProtStack.Pop())) hp = new XrdHttpProtocol(myishttps);
  else
    hp->ishttps = myishttps;

  // Bind the protocol to the link and return the protocol
  //
  hp->Link = lp;


  return (XrdProtocol *) hp;
}








/******************************************************************************/
/*                               G e t V O M S D a t a                        */

/******************************************************************************/



int XrdHttpProtocol::GetVOMSData(XrdLink *lp) {


  SecEntity.host = GetClientIPStr();



  // Invoke our instance of the Security exctractor plugin
  // This will fill the XrdSec thing with VOMS info, if VOMS is
  // installed. If we have no sec extractor then do nothing, just plain https
  // will work.
  if (secxtractor) secxtractor->GetSecData(lp, SecEntity, ssl);
  else {

      X509 *peer_cert;

      // No external plugin, hence we fill our XrdSec with what we can do here
      peer_cert = SSL_get_peer_certificate(ssl);
      TRACEI(DEBUG, " SSL_get_peer_certificate returned :" << peer_cert);
      if (peer_cert && peer_cert->name) {

          TRACEI(DEBUG, " Setting Username :" << peer_cert->name);
          lp->setID(peer_cert->name, 0);

          // Here we should fill the SecEntity instance with the DN and the voms stuff
          SecEntity.name = strdup((char *) peer_cert->name);

        }

      if (peer_cert) X509_free(peer_cert);
    }

  return 0;
}

char *XrdHttpProtocol::GetClientIPStr() {
  char buf[256];
  buf[0] = '\0';
  if (!Link) return strdup("unknown");
  XrdNetAddrInfo *ai = Link->AddrInfo();
  if (!ai) return strdup("unknown");

  if (!Link->AddrInfo()->Format(buf, 255, XrdNetAddrInfo::fmtAddr)) return strdup("unknown");

  return strdup(buf);
}






/******************************************************************************/
/*                               P r o c e s s                                */
/******************************************************************************/

#undef  TRACELINK
#define TRACELINK Link

int XrdHttpProtocol::Process(XrdLink *lp) // We ignore the argument here
{
  int rc = 0;

  TRACEI(DEBUG, " Process. lp:" << lp << " reqstate: " << CurrentReq.reqstate);

  if (!myBuff || !myBuff->buff || !myBuff->bsize) {
    TRACE(ALL, " Process. No buffer available. Internal error.");
    return -1;
  }


  if (!SecEntity.host) {
    char *nfo = GetClientIPStr();
    if (nfo) {
      TRACEI(REQ, " Setting host: " << nfo);
      SecEntity.host = nfo;
    }
  }



  // If https then check independently for the ssl handshake
  if (ishttps && !ssldone) {
    ssl = SSL_new(sslctx);
    if (!ssl) {
      TRACEI(DEBUG, " SSL_new returned NULL");
      ERR_print_errors(sslbio_err);
      return -1;
    }
    SSL_set_fd(ssl, Link->FDnum());
    struct timeval tv;
    tv.tv_sec = readWait;
    tv.tv_usec = 0;
    setsockopt(Link->FDnum(), SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval));
    setsockopt(Link->FDnum(), SOL_SOCKET, SO_SNDTIMEO, (struct timeval *)&tv, sizeof(struct timeval));
    
    int res = SSL_accept(ssl);
    TRACEI(DEBUG, " SSL_accept returned :" << res);
    ERR_print_errors(sslbio_err);
    if (res < 0) {
      SSL_free(ssl);
      ssl = 0;
      return -1;
    }

    // Get the voms string and auth information
    GetVOMSData(Link);

    ERR_print_errors(sslbio_err);
    res = SSL_get_verify_result(ssl);
    TRACEI(DEBUG, " SSL_get_verify_result returned :" << res);
    ERR_print_errors(sslbio_err);

    if (res != X509_V_OK) return -1;
    ssldone = true;
  }



  if (!DoingLogin) {
    // Re-invocations triggered by the bridge have lp==0
    // In this case we keep track of a different request state
    if (lp) {

      // This is an invocation that was triggered by a socket event
      // Read all the data that is available, throw it into the buffer
      if ((rc = getDataOneShot(BuffAvailable())) < 0) {
        // Error -> exit
        return -1;
      }

      // If we need more bytes, let's wait for another invokation
      if (BuffUsed() < ResumeBytes) return 1;


    } else
      CurrentReq.reqstate++;
  }
  DoingLogin = false;


  // Read the next request header, that is, read until a double CRLF is found


  if (!CurrentReq.headerok) {
    
    // Read as many lines as possible into the buffer. An empty line breaks
    while ((rc = BuffgetLine(tmpline)) > 0) {
      TRACE(DEBUG, " rc:" << rc << " got hdr line: " << tmpline);

      if ((rc == 2) && (tmpline[rc - 1] == '\n')) {
        CurrentReq.headerok = true;
        TRACE(DEBUG, " rc:" << rc << " detected header end.");
        break;
      }


      if (CurrentReq.request == CurrentReq.rtUnknown)
        CurrentReq.parseFirstLine((char *)tmpline.c_str(), rc);
      else
        CurrentReq.parseLine((char *)tmpline.c_str(), rc);


    }

    // Here we have CurrentReq loaded with the header, or its relevant fields

    if (!CurrentReq.headerok) {
      TRACEI(REQ, " rc:" << rc << "Header not yet complete.");
      // Waiting for more data
      return 1;
    }

  }

  // If we are in self-redirect mode, then let's do it
  if (ishttps && ssldone && selfhttps2http) {
    char hash[512];
    time_t timenow = time(0);


    calcHashes(hash, CurrentReq.resource.c_str(), (kXR_int16) CurrentReq.request,
            &SecEntity,
            timenow,
            secretkey);



    if (hash[0]) {

      // Workaround... delete the previous opaque information
      if (CurrentReq.opaque) {
        delete CurrentReq.opaque;
        CurrentReq.opaque = 0;
      }

      TRACEI(REQ, " rc:" << rc << " self-redirecting to http with security token.");

      XrdOucString dest = "Location: http://";
      // Here I should put the IP addr of the server
      Link->Host();

      dest += Addr_str;
      dest += ":";
      dest += Port_str;
      dest += CurrentReq.resource.c_str();
      CurrentReq.appendOpaque(dest, &SecEntity, hash, timenow);
      SendSimpleResp(302, NULL, (char *) dest.c_str(), 0, 0);
      CurrentReq.reset();
      return 1;
    }
  }

  // If this is not https, then extract the signed information from the url
  // and fill the SecEntity structure as if we were using https
  if (!ishttps && !ssldone) {


    if (CurrentReq.opaque) {
      char * tk = CurrentReq.opaque->Get("xrdhttptk");
      // If there is a hash then we use it as authn info
      if (tk) {

        time_t tim = 0;
        char * t = CurrentReq.opaque->Get("xrdhttptime");
        if (t) tim = atoi(t);
        if (!t) {
          TRACEI(REQ, " xrdhttptime not specified. Authentication failed.");
          return -1;
        }
        if (abs((time(0) - tim) > XRHTTP_TK_GRACETIME)) {
          TRACEI(REQ, " Token expired. Authentication failed.");
          return -1;
        }

        // Fill the Secentity from the fields in the URL:name, vo, host
        char *nfo;

        nfo = CurrentReq.opaque->Get("xrdhttpvorg");
        if (nfo) {
          TRACEI(REQ, " Setting vorg: " << nfo);
          SecEntity.vorg = strdup(nfo);
        }

        nfo = CurrentReq.opaque->Get("xrdhttpname");
        if (nfo) {
          TRACEI(REQ, " Setting name: " << nfo);
          SecEntity.name = unquote(nfo);
        }

        //nfo = CurrentReq.opaque->Get("xrdhttphost");


        // TODO: compare the xrdhttphost with the real client IP
        // If they are different then reject

        char hash[512];

        calcHashes(hash, CurrentReq.resource.c_str(), (kXR_int16) CurrentReq.request,
                &SecEntity,
                tim,
                secretkey);

        if (compareHash(hash, tk)) {
          TRACEI(REQ, " Invalid tk. Authentication failed.");
          return -1;
        }

      } else {
        // Client is plain http. If we have a secret key then we reject it
        if (secretkey) {
          TRACEI(ALL, " Rejecting plain http with no valid token as we have a secretkey.");
          return -1;
        }
      }

    } else {
      // Client is plain http. If we have a secret key then we reject it
      if (secretkey) {
        TRACEI(ALL, " Rejecting plain http with no valid token as we have a secretkey.");
        return -1;
      }
    }

    ssldone = true;
  }



  // Now we have everything that is needed to try the login
  if (!Bridge) {
    Bridge = XrdXrootd::Bridge::Login(&CurrentReq, Link, &SecEntity, "unnamed", "XrdHttp");
    if (!Bridge) {
      TRACEI(REQ, " Autorization failed.");
      return -1;
    }

    // Let the bridge process the login, and then reinvoke us
    DoingLogin = true;
    return 0;
  }

  // Compute and send the response. This may involve further reading from the socket
  rc = CurrentReq.ProcessHTTPReq();
  if (rc < 0)
    CurrentReq.reset();



  TRACEI(REQ, "Process is exiting rc:" << rc);
  return rc;
}
/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/

#undef  TRACELINK
#define TRACELINK Link

void XrdHttpProtocol::Recycle(XrdLink *lp, int csec, const char *reason) {

  // Release all appendages
  //

  Cleanup();


  // Set fields to starting point (debugging mostly)
  //
  Reset();

  // Push ourselves on the stack
  //
  ProtStack.Push(&ProtLink);
}

int XrdHttpProtocol::Stats(char *buff, int blen, int do_sync) {
  // Synchronize statistics if need be
  //
  //  if (do_sync) {
  //
  //    SI->statsMutex.Lock();
  //    SI->readCnt += numReads;
  //    cumReads += numReads;
  //    numReads = 0;
  //    SI->prerCnt += numReadP;
  //    cumReadP += numReadP;
  //    numReadP = 0;
  //    SI->rvecCnt += numReadV;
  //    cumReadV += numReadV;
  //    numReadV = 0;
  //    SI->rsegCnt += numSegsV;
  //    cumSegsV += numSegsV;
  //    numSegsV = 0;
  //    SI->writeCnt += numWrites;
  //    cumWrites += numWrites;
  //    numWrites = 0;
  //    SI->statsMutex.UnLock();
  //  }
  //
  //  // Now return the statistics
  //  //
  //  return SI->Stats(buff, blen, do_sync);

  return 0;
}






/******************************************************************************/
/*                                C o n f i g                                 */
/******************************************************************************/

#define TS_Xeq(x,m) (!strcmp(x,var)) GoNo = m(Config)

int XrdHttpProtocol::Config(const char *ConfigFN) {
  XrdOucEnv myEnv;
  XrdOucStream Config(&eDest, getenv("XRDINSTANCE"), &myEnv, "=====> ");
  char *var;
  int cfgFD, GoNo, NoGo = 0, ismine;

  // Open and attach the config file
  //
  if ((cfgFD = open(ConfigFN, O_RDONLY, 0)) < 0)
    return eDest.Emsg("Config", errno, "open config file", ConfigFN);
  Config.Attach(cfgFD);

  // Process items
  //
  while ((var = Config.GetMyFirstWord())) {
    if ((ismine = !strncmp("http.", var, 5)) && var[5]) var += 5;
    else if ((ismine = !strcmp("all.export", var))) var += 4;
    else if ((ismine = !strcmp("all.pidpath", var))) var += 4;
    else if ((ismine = !strcmp("all.seclib", var))) var += 4;

    if (ismine) {
      if TS_Xeq("seclib", xsecl);
      else if TS_Xeq("trace", xtrace);
      else if TS_Xeq("cert", xsslcert);
      else if TS_Xeq("key", xsslkey);
      else if TS_Xeq("cadir", xsslcadir);
      else if TS_Xeq("cafile", xsslcafile);
      else if TS_Xeq("secretkey", xsecretkey);
      else if TS_Xeq("desthttps", xdesthttps);
      else if TS_Xeq("secxtractor", xsecxtractor);
      else if TS_Xeq("selfhttps2http", xselfhttps2http);
      else if TS_Xeq("embeddedstatic", xembeddedstatic);
      else if TS_Xeq("listingredir", xlistredir);
      else if TS_Xeq("listingdeny", xlistdeny);
      else {
        eDest.Say("Config warning: ignoring unknown directive '", var, "'.");
        Config.Echo();
        continue;
      }
      if (GoNo) {

        Config.Echo();
        NoGo = 1;
      }
    }
  }


  if (sslcert)
    InitSecurity();

  return NoGo;
}







/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */

/******************************************************************************/

/// Copy a full line of text from the buffer into dest. Zero if no line can be found in the buffer

int XrdHttpProtocol::BuffgetLine(XrdOucString &dest) {

  dest = "";

  // Easy case
  if (myBuffEnd >= myBuffStart) {
    int l = 0;
    for (char *p = myBuffStart; p < myBuffEnd; p++) {
      l++;
      if (*p == '\n') {
        dest.assign(myBuffStart, 0, l-1);

        //strncpy(dest, myBuffStart, l);
        //dest[l] = '\0';
        BuffConsume(l);

        //if (dest[l-1] == '\n') dest[l - 1] = '\0';
        return l;
      }

    }

    return 0;
  } else {
    // More complex case... we have to do it in two segments

    // Segment 1: myBuffStart->myBuff->buff+myBuff->bsize
    int l = 0;
    for (char *p = myBuffStart; p < myBuff->buff + myBuff->bsize; p++) {
      l++;
      if ((*p == '\n') || (*p == '\0')) {

        dest.assign(myBuffStart, 0, l-1);
        //strncpy(dest, myBuffStart, l);

        BuffConsume(l);

        //if (dest[l-1] == '\n') dest[l - 1] = '\0';
        return l;
      }

    }

    // We did not find the \n, let's keep on searching in the 2nd segment
    // Segment 2: myBuff->buff --> myBuffEnd
    l = 0;
    for (char *p = myBuff->buff; p < myBuffEnd; p++) {
      l++;
      if ((*p == '\n') || (*p == '\0')) {

        // Remember the 1st segment
        int l1 = myBuff->buff + myBuff->bsize - myBuffStart;
        
        dest.assign(myBuffStart, 0, l1-1);
        //strncpy(dest, myBuffStart, l1);
        BuffConsume(l1);

        dest.insert(myBuffStart, l1, l-1);
        //strncpy(dest + l1, myBuffStart, l);
        //dest[l + l1] = '\0';
        BuffConsume(l);

        //if (dest[l + l1 - 1] == '\n') dest[l + l1 - 1] = '\0';
        return l + l1;
      }

    }



  }

  return 0;
}

int XrdHttpProtocol::getDataOneShot(int blen, bool wait) {
  int rlen, maxread;

  // Get up to blen bytes from the connection. Put them into mybuff.
  // This primitive, for the way it is used, is not supposed to block if wait=false

  // Returns:
  // 2: no space left in buffer
  // 1: timeout
  // -1: error
  // 0: everything read correctly



  // Check for buffer overflow first
  maxread = min(blen, BuffAvailable());
  TRACE(DEBUG, "getDataOneShot BuffAvailable: " << BuffAvailable() << " maxread: " << maxread);

  if (!maxread)
    return 2;

  if (ishttps) {
    int sslavail = maxread;

    if (!wait) {
      int l = SSL_pending(ssl);
      if (l > 0)
        sslavail = min(maxread, SSL_pending(ssl));
    }

    if (sslavail < 0) {
      Link->setEtext("link SSL_pending error");
      ERR_print_errors(sslbio_err);
      return -1;
    }

    TRACE(DEBUG, "getDataOneShot sslavail: " << sslavail);
    if (sslavail <= 0) return 0;

    if (myBuffEnd - myBuff->buff >= myBuff->bsize) {
      TRACE(DEBUG, "getDataOneShot Buffer panic");
      myBuffEnd = myBuff->buff;
    }

    rlen = SSL_read(ssl, myBuffEnd, sslavail);
    if (rlen <= 0) {
      Link->setEtext("link SSL read error");
      ERR_print_errors(sslbio_err);
      return -1;
    }


  } else {

    if (myBuffEnd - myBuff->buff >= myBuff->bsize) {
      TRACE(DEBUG, "getDataOneShot Buffer panic");
      myBuffEnd = myBuff->buff;
    }

    if (wait)
      rlen = Link->Recv(myBuffEnd, maxread, readWait);
    else
      rlen = Link->Recv(myBuffEnd, maxread);


    if (rlen == 0) {
      Link->setEtext("link read error or closed");
      return -1;
    }

    if (rlen < 0) {
      Link->setEtext("link timeout");
      return 1;
    }


  }



  myBuffEnd += rlen;


  TRACE(REQ, "read " << rlen << " of " << blen << " bytes");

  return 0;
}

/// How many bytes still fit into the buffer in a contiguous way

int XrdHttpProtocol::BuffAvailable() {
  int r;

  if (myBuffEnd >= myBuffStart)
    r = myBuff->buff + myBuff->bsize - myBuffEnd;
  else
    r = myBuffStart - myBuffEnd;

  if ((r < 0) || (r > myBuff->bsize)) {
    TRACE(REQ, "internal error, myBuffAvailable: " << r << " myBuff->bsize " << myBuff->bsize);
    abort();
  }

  return r;
}

/// How many bytes in the buffer

int XrdHttpProtocol::BuffUsed() {
  int r;

  if (myBuffEnd >= myBuffStart)
    r = myBuffEnd - myBuffStart;
  else

    r = myBuff->bsize - (myBuffStart - myBuffEnd);

  if ((r < 0) || (r > myBuff->bsize)) {
    TRACE(REQ, "internal error, myBuffUsed: " << r << " myBuff->bsize " << myBuff->bsize);
    abort();
  }

  return r;
}
/// How many bytes free in the buffer

int XrdHttpProtocol::BuffFree() {
  return (myBuff->bsize - BuffUsed());
}

void XrdHttpProtocol::BuffConsume(int blen) {

  if (blen > myBuff->bsize) {
    TRACE(REQ, "internal error, BuffConsume(" << blen << ") smaller than buffsize");
    abort();
  }

  if (blen > BuffUsed()) {
    TRACE(REQ, "internal error, BuffConsume(" << blen << ") larger than BuffUsed:" << BuffUsed());
    abort();
  }

  myBuffStart = myBuffStart + blen;

  if (myBuffStart >= myBuff->buff + myBuff->bsize)
    myBuffStart -= myBuff->bsize;

  if (myBuffEnd >= myBuff->buff + myBuff->bsize)
    myBuffEnd -= myBuff->bsize;

  if (BuffUsed() == 0)
    myBuffStart = myBuffEnd = myBuff->buff;
}

/// Get a pointer, valid for up to blen bytes from the buffer. Returns the n
/// of bytes that one is allowed to read

int XrdHttpProtocol::BuffgetData(int blen, char **data, bool wait) {
  int rlen;

  if (wait && (blen > BuffUsed())) {
    TRACE(REQ, "BuffgetData: need to read " << blen - BuffUsed() << " bytes");
    if (getDataOneShot(blen - BuffUsed(), true) < 0) return 0;
  }

  if (myBuffStart < myBuffEnd) {
    rlen = min( (long) blen, (long)(myBuffEnd - myBuffStart) );

  } else
    rlen = min( (long) blen, (long)(myBuff->buff + myBuff->bsize - myBuffStart) );

  *data = myBuffStart;
  BuffConsume(rlen);
  return rlen;
}

/// Send some data to the client

int XrdHttpProtocol::SendData(char *body, int bodylen) {
  int r;



  if (body && bodylen) {
    TRACE(REQ, "Sending " << bodylen << " bytes");
    if (ishttps) {
      r = SSL_write(ssl, body, bodylen);
      if (r <= 0) {
        ERR_print_errors(sslbio_err);
        return -1;
      }

    } else {
      r = Link->Send(body, bodylen);
      if (r <= 0) return -1;
    }
  }

  return 0;
}

/// Sends a basic response. If the length is < 0 then it is calculated internally
/// Header_to_add is a set of header lines each CRLF terminated to be added to the header
/// Returns 0 if OK

int XrdHttpProtocol::SendSimpleResp(int code, char *desc, char *header_to_add, char *body, int bodylen) {
  char outhdr[512];
  char b[16];
  int l;
  const char *crlf = "\r\n";
  outhdr[0] = '\0';


  //
  // Prepare the header
  //
  strcat(outhdr, "HTTP/1.1 ");
  sprintf(b, "%d ", code);
  strcat(outhdr, b);

  if (desc) strcat(outhdr, desc);
  else {
    if (code == 200) strcat(outhdr, "OK");
    else if (code == 206) strcat(outhdr, "Partial content");
    else if (code == 302) strcat(outhdr, "Redirect");
    else if (code == 404) strcat(outhdr, "Not found");
    else strcat(outhdr, "Unknown");
  }
  strncat(outhdr, crlf, 2);

  //strcat(outhdr, "Content-Type: text/html");
  //strncat(outhdr, crlf, 2);

  l = bodylen;
  if (l <= 0) {
    if (body) l = strlen(body);
    else l = 0;
  }

  sprintf(b, "%d", l);
  strcat(outhdr, "Content-Length: ");
  strcat(outhdr, b);
  strncat(outhdr, crlf, 2);

  if (header_to_add) {
    strcat(outhdr, header_to_add);
    strncat(outhdr, crlf, 2);
  }
  strncat(outhdr, crlf, 2);

  //
  // Send the header
  //
  TRACEI(RSP, "Sending resp: " << code << " len:" << l);

  if (SendData(outhdr, strlen(outhdr)))
    return -1;

  //
  // Send the data
  //
  if (body)
    return SendData(body, l);

  return 0;

}

int XrdHttpProtocol::Configure(char *parms, XrdProtocol_Config * pi) {
  /*
    Function: Establish configuration at load time.

    Input:    None.

    Output:   0 upon success or !0 otherwise.
   */

  extern XrdSecService * XrdXrootdloadSecurity(XrdSysError *, char *,
          char *, void **);

  extern int optind, opterr;


  void *secGetProt = 0;
  char *rdf, c;

  // Copy out the special info we want to use at top level
  //
  eDest.logger(pi->eDest->logger());
  XrdHttpTrace = new XrdOucTrace(&eDest);
  //  SI = new XrdXrootdStats(pi->Stats);
  Sched = pi->Sched;
  BPool = pi->BPool;
  hailWait = 10000;
  readWait = 30000;

  Port = pi->Port;

  {
    char buf[16];
    sprintf(buf, "%d", Port);
    Port_str = strdup(buf);


    // now get it back and print it
    inet_ntop(AF_INET, &((struct sockaddr_in *) pi->myAddr)->sin_addr, buf, INET_ADDRSTRLEN);
    Addr_str = strdup(buf);
  }



  Window = pi->WSize;



  // Prohibit this program from executing as superuser
  //
  if (geteuid() == 0) {
    eDest.Emsg("Config", "Security reasons prohibit xrootd running as "
            "superuser; xrootd is terminating.");
    _exit(8);
  }

  // Process any command line options
  //
  opterr = 0;
  optind = 1;
  if (pi->argc > 1 && '-' == *(pi->argv[1]))
    while ((c = getopt(pi->argc, pi->argv, "mrst")) && ((unsigned char) c != 0xff)) {
      switch (c) {
        case 'm': XrdOucEnv::Export("XRDREDIRECT", "R");
          break;
        case 's': XrdOucEnv::Export("XRDRETARGET", "1");
          break;
        default: eDest.Say("Config warning: ignoring invalid option '", pi->argv[optind - 1], "'.");
      }
    }


  // Now process and configuration parameters
  //
  rdf = (parms && *parms ? parms : pi->ConfigFN);
  if (rdf && Config(rdf)) return 0;
  if (pi->DebugON) XrdHttpTrace->What = TRACE_ALL;


  // Initialize the security system if this is wanted
  //
  if (!SecLib) eDest.Say("Config warning: 'xrootd.seclib' not specified;"
          " strong authentication disabled!");
  else {
    TRACE(DEBUG, "Loading security library " << SecLib);
    if (!(CIA = XrdXrootdloadSecurity(&eDest, SecLib, pi->ConfigFN,
            &secGetProt))) {
      eDest.Emsg("Config", "Unable to load security system.");
      return 0;
    }
  }



  // Set the redirect flag if we are a pure redirector
  //
  myRole = kXR_isServer;
  if ((rdf = getenv("XRDROLE"))) {
    eDest.Emsg("Config", "XRDROLE: ", rdf);

    if (!strcasecmp(rdf, "manager") || !strcasecmp(rdf, "supervisor")) {
      myRole = kXR_isManager;
      eDest.Emsg("Config", "Configured as HTTP(s) redirector.");
    } else {

      eDest.Emsg("Config", "Configured as HTTP(s) data server.");
    }

  } else {
    eDest.Emsg("Config", "No XRDROLE specified.");
  }









  // Schedule protocol object cleanup
  //
  ProtStack.Set(pi->Sched, XrdHttpTrace, TRACE_MEM);
  ProtStack.Set((pi->ConnMax / 3 ? pi->ConnMax / 3 : 30), 60 * 60);

  // Return success
  //

  return 1;
}







// --------------
// security stuff
// --------------

extern "C" int verify_callback(int ok, X509_STORE_CTX * store) {
  char data[256];


  if (!ok) {
    X509 *cert = X509_STORE_CTX_get_current_cert(store);
    int depth = X509_STORE_CTX_get_error_depth(store);
    int err = X509_STORE_CTX_get_error(store);

    fprintf(stderr, "-Error with certificate at depth: %i\n", depth);
    X509_NAME_oneline(X509_get_issuer_name(cert), data, 256);
    fprintf(stderr, "  issuer   = %s\n", data);
    X509_NAME_oneline(X509_get_subject_name(cert), data, 256);
    fprintf(stderr, "  subject  = %s\n", data);
    fprintf(stderr, "  err %i:%s\n", err, X509_verify_cert_error_string(err));
  }

  return ok;
}

//
///* Check that the common name matches the host name*/ 
//void check_cert_chain(SSL *ssl, char *host) {
//
//    X509 *peer; 
//    char peer_CN[256];
//    if(SSL_get_verify_result(ssl)!=X509_V_OK) 
//              berr_exit("Certificate doesn't verify");
//
//    /*Check the common name*/ 
//   peer=SSL_get_peer_certificate(ssl); 
//   X509_NAME_get_text_by_NID ( 
//            X509_get_subject_name (peer),  NID_commonName,  peer_CN, 256); 
//   if(strcasecmp(peer_CN, host)) 
//            err_exit("Common name doesn't match host name");
//
//  } 
//
//
//










/// Initialization of the ssl security

int XrdHttpProtocol::InitSecurity() {

#ifdef HAVE_XRDCRYPTO
#ifndef WIN32
  // Borrow the initialization of XrdCryptossl, in order to share the
  // OpenSSL threading bits
  if (!(myCryptoFactory = XrdCryptoFactory::GetCryptoFactory("ssl"))) {
          cerr << "Cannot instantiate crypto factory ssl" << endl;
          exit(1);
        }

#endif
#endif

  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_all_ciphers();
  OpenSSL_add_all_algorithms();
  OpenSSL_add_all_digests();

  const SSL_METHOD *meth;
  meth = SSLv23_method();
  sslctx = SSL_CTX_new((SSL_METHOD *)meth);
  SSL_CTX_set_options(sslctx, SSL_OP_NO_SSLv2);
  SSL_CTX_set_session_cache_mode(sslctx, SSL_SESS_CACHE_SERVER);
  SSL_CTX_set_session_id_context(sslctx, s_server_session_id_context,
          s_server_session_id_context_len);

  /* An error write context */
  sslbio_err = BIO_new_fp(stderr, BIO_NOCLOSE);




  // Enable proxy certificates
  X509_STORE *store;
  X509_VERIFY_PARAM *param;

  store = SSL_CTX_get_cert_store(sslctx);
  param = X509_VERIFY_PARAM_new();
  if (!param) {
    ERR_print_errors(sslbio_err);
    exit(1);
    /* ERROR */
  }
  X509_VERIFY_PARAM_set_flags(param, X509_V_FLAG_ALLOW_PROXY_CERTS);
  X509_STORE_set1_param(store, param);
  X509_VERIFY_PARAM_free(param);





  /* Load server certificate into the SSL context */
  if (SSL_CTX_use_certificate_file(sslctx, sslcert,
          SSL_FILETYPE_PEM) <= 0) {

    ERR_print_errors(sslbio_err); /* == ERR_print_errors_fp(stderr); */
    exit(1);
  }

  /* Load the server private-key into the SSL context */
  if (SSL_CTX_use_PrivateKey_file(sslctx, sslkey,
          SSL_FILETYPE_PEM) <= 0) {

    ERR_print_errors(sslbio_err); /* == ERR_print_errors_fp(stderr); */
    exit(1);
  }

  /* Load trusted CA. */
  //eDest.Say(" Setting cafile ", sslcafile, "'.");
  //eDest.Say(" Setting cadir ", sslcadir, "'.");
  if (!SSL_CTX_load_verify_locations(sslctx, sslcafile, sslcadir)) {
    ERR_print_errors(sslbio_err); /* == 
                  ERR_print_errors_fp(stderr); */
    exit(1);
  }

  //eDest.Say(" Setting verify depth to ", itoa(sslverifydepth), "'.");
  SSL_CTX_set_verify_depth(sslctx, sslverifydepth);
  ERR_print_errors(sslbio_err);
  //SSL_CTX_set_verify(sslctx,
  //        SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verify_callback);
  SSL_CTX_set_verify(sslctx,
          SSL_VERIFY_PEER, verify_callback);



  if (secxtractor) secxtractor->Init(sslctx, XrdHttpTrace->What);

  ERR_print_errors(sslbio_err);
  return 0;
}

void XrdHttpProtocol::Cleanup() {

  TRACE(ALL, " Cleanup");

  if (BPool && myBuff) {
    BuffConsume(BuffUsed());
    BPool->Release(myBuff);
    myBuff = 0;
  }


  if (ssl) {
    if (SSL_shutdown(ssl) != 1) {
      TRACE(ALL, " SSL_shutdown failed");
      ERR_print_errors(sslbio_err);
    }
    else
     SSL_free(ssl);
  }

  ssl = 0;


  if (SecEntity.vorg) free(SecEntity.vorg);
  if (SecEntity.name) free(SecEntity.name);
  if (SecEntity.host) free(SecEntity.host);

  memset(&SecEntity, 0, sizeof (SecEntity));


}

void XrdHttpProtocol::Reset() {

  TRACE(ALL, " Reset");
  Link = 0;
  CurrentReq.reset();
  CurrentReq.reqstate = 0;

  if (!myBuff) {
    myBuff = BPool->Obtain(1024 * 1024);
  }
  myBuffStart = myBuffEnd = myBuff->buff;

  DoingLogin = false;

  ResumeBytes = 0;
  Resume = 0;

  //
  //  numReads = 0;
  //  numReadP = 0;
  //  numReadV = 0;
  //  numSegsV = 0;
  //  numWrites = 0;
  //  numFiles = 0;
  //  cumReads = 0;
  //  cumReadV = 0;
  //  cumSegsV = 0;
  //  cumWrites = 0;
  //  totReadP = 0;

  memset(&SecEntity, 0, sizeof (SecEntity));

  ishttps = false;
  ssldone = false;

  Bridge = 0;
  ssl = 0;

}












/******************************************************************************/
/*                   x s s l v e r i f y d e p t h                            */
/******************************************************************************/

/* Function: xsslverifydepth

   Purpose:  To parse the directive: sslverifydepth <depth>

             <path>    the max depth of the ssl cert verification

  Output: 0 upon success or !0 upon failure.
 */

int XrdHttpProtocol::xsslverifydepth(XrdOucStream & Config) {
  char *val;

  // Get the val
  //
  val = Config.GetWord();
  if (!val || !val[0]) {
    eDest.Emsg("Config", "XRootd sslverifydepth not specified");
    return 1;
  }

  // Record the val
  //
  sslverifydepth = atoi(val);

  return 0;
}






/******************************************************************************/
/*                                 x s e c l                                  */
/******************************************************************************/

/* Function: xsecl

   Purpose:  To parse the directive: seclib <path>

             <path>    the path of the security library to be used.

  Output: 0 upon success or !0 upon failure.
 */

int XrdHttpProtocol::xsecl(XrdOucStream & Config) {
  char *val;

  // Get the path
  //
  val = Config.GetWord();
  if (!val || !val[0]) {
    eDest.Emsg("Config", "XRootd seclib not specified");
    return 1;
  }

  // Record the path
  //
  if (SecLib) free(SecLib);
  SecLib = strdup(val);

  return 0;
}




/******************************************************************************/
/*                                 x s s l c e r t                            */
/******************************************************************************/

/* Function: xsslcert

   Purpose:  To parse the directive: sslcert <path>

             <path>    the path of the server certificate to be used.

  Output: 0 upon success or !0 upon failure.
 */

int XrdHttpProtocol::xsslcert(XrdOucStream & Config) {
  char *val;

  // Get the path
  //
  val = Config.GetWord();
  if (!val || !val[0]) {
    eDest.Emsg("Config", "HTTP X509 certificate not specified");
    return 1;
  }

  // Record the path
  //
  if (sslcert) free(sslcert);
  sslcert = strdup(val);

  return 0;
}


/******************************************************************************/
/*                                 x s s l k e y                              */
/******************************************************************************/

/* Function: xsslkey

   Purpose:  To parse the directive: sslkey <path>

             <path>    the path of the server key to be used.

  Output: 0 upon success or !0 upon failure.
 */

int XrdHttpProtocol::xsslkey(XrdOucStream & Config) {
  char *val;

  // Get the path
  //
  val = Config.GetWord();
  if (!val || !val[0]) {
    eDest.Emsg("Config", "HTTP X509 key not specified");
    return 1;
  }

  // Record the path
  //
  if (sslkey) free(sslkey);
  sslkey = strdup(val);

  return 0;
}


/******************************************************************************/
/*                                 x s s l c a f i l e                        */
/******************************************************************************/

/* Function: xsslcafile

   Purpose:  To parse the directive: sslcafile <path>

             <path>    the path of the server key to be used.

  Output: 0 upon success or !0 upon failure.
 */

int XrdHttpProtocol::xsslcafile(XrdOucStream & Config) {
  char *val;

  // Get the path
  //
  val = Config.GetWord();
  if (!val || !val[0]) {
    eDest.Emsg("Config", "HTTP X509 CAfile not specified");
    return 1;
  }

  // Record the path
  //
  if (sslcafile) free(sslcafile);
  sslcafile = strdup(val);

  return 0;
}



/******************************************************************************/
/*                                 x s e c r e t k e y                        */
/******************************************************************************/

/* Function: xsecretkey

   Purpose:  To parse the directive: xsecretkey <key>

             <key>    the key to be used

  Output: 0 upon success or !0 upon failure.
 */

int XrdHttpProtocol::xsecretkey(XrdOucStream & Config) {
  char *val;

  // Get the path
  //
  val = Config.GetWord();
  if (!val || !val[0]) {
    eDest.Emsg("Config", "Shared secret key not specified");
    return 1;
  }

  // Record the path
  //
  if (secretkey) free(secretkey);
  secretkey = strdup(val);

  return 0;
}


/******************************************************************************/
/*                                 x l i s t d e n y                          */
/******************************************************************************/

/* Function: xlistdeny

   Purpose:  To parse the directive: listingdeny <yes|no|0|1>

             <val>    makes this redirector deny listings with an error

   Output: 0 upon success or !0 upon failure.
 */

int XrdHttpProtocol::xlistdeny(XrdOucStream & Config) {
  char *val;

  // Get the path
  //
  val = Config.GetWord();
  if (!val || !val[0]) {
    eDest.Emsg("Config", "listingdeny flag not specified");
    return 1;
  }

  // Record the value
  //
  listdeny = (!strcasecmp(val, "true") || !strcasecmp(val, "yes") || !strcmp(val, "1"));


  return 0;
}

/******************************************************************************/
/*                                 x l i s t r e d i r                        */
/******************************************************************************/

/* Function: xlistredir

   Purpose:  To parse the directive: listingredir <Url>

             <Url>    http/https server to redirect to in the case of listing

  Output: 0 upon success or !0 upon failure.
 */

int XrdHttpProtocol::xlistredir(XrdOucStream & Config) {
  char *val;

  // Get the path
  //
  val = Config.GetWord();
  if (!val || !val[0]) {
    eDest.Emsg("Config", "listingredir flag not specified");
    return 1;
  }

  // Record the value
  //
  if (listredir) free(listredir);
  listredir = strdup(val);


  return 0;
}


/******************************************************************************/
/*                                 x s s l d e s t h t t p s                  */
/******************************************************************************/

/* Function: xdesthttps

   Purpose:  To parse the directive: desthttps <yes|no|0|1>

             <val>    makes this redirector produce http or https redirection targets

  Output: 0 upon success or !0 upon failure.
 */

int XrdHttpProtocol::xdesthttps(XrdOucStream & Config) {
  char *val;

  // Get the path
  //
  val = Config.GetWord();
  if (!val || !val[0]) {
    eDest.Emsg("Config", "desthttps flag not specified");
    return 1;
  }

  // Record the value
  //
  isdesthttps = (!strcasecmp(val, "true") || !strcasecmp(val, "yes") || !strcmp(val, "1"));


  return 0;
}



/******************************************************************************/
/*                          x e m b e d d e d s t a t i c                     */
/******************************************************************************/

/* Function: xembeddedstatic

   Purpose:  To parse the directive: embeddedstatic <yes|no|0|1|true|false>

             <val>    this server will redirect HTTPS to itself using HTTP+token

  Output: 0 upon success or !0 upon failure.
 */

int XrdHttpProtocol::xembeddedstatic(XrdOucStream & Config) {
  char *val;

  // Get the path
  //
  val = Config.GetWord();
  if (!val || !val[0]) {
    eDest.Emsg("Config", "embeddedstatic flag not specified");
    return 1;
  }

  // Record the value
  //
  embeddedstatic = (!strcasecmp(val, "true") || !strcasecmp(val, "yes") || !strcmp(val, "1"));


  return 0;
}




/******************************************************************************/
/*                          x s e l f h t t p s 2 h t t p                        */
/******************************************************************************/

/* Function: selfhttps2http

   Purpose:  To parse the directive: selfhttps2http <yes|no|0|1>

             <val>    this server will redirect HTTPS to itself using HTTP+token

  Output: 0 upon success or !0 upon failure.
 */

int XrdHttpProtocol::xselfhttps2http(XrdOucStream & Config) {
  char *val;

  // Get the path
  //
  val = Config.GetWord();
  if (!val || !val[0]) {
    eDest.Emsg("Config", "selfhttps2http flag not specified");
    return 1;
  }

  // Record the value
  //
  selfhttps2http = (!strcasecmp(val, "true") || !strcasecmp(val, "yes") || !strcmp(val, "1"));


  return 0;
}



/******************************************************************************/
/*                            x s e c x t r a c t o r                         */
/******************************************************************************/

/* Function: xsecxtractor

   Purpose:  To parse the directive: secxtractor <path>

             <path>    the path of the plugin to be loaded

  Output: 0 upon success or !0 upon failure.
 */

int XrdHttpProtocol::xsecxtractor(XrdOucStream & Config) {
  char *val;

  // Get the path
  //
  val = Config.GetWord();
  if (!val || !val[0]) {
    eDest.Emsg("Config", "No security extractor plugin specified.");
    return 1;
  } else {

      // Try to load the plugin (if available) that extracts info from the user cert/proxy
      //
      if (LoadSecXtractor(&eDest, val, 0))
          return 1;
  }


  return 0;
}

/******************************************************************************/
/*                                 x s s l c a d i r                          */
/******************************************************************************/

/* Function: xsslcadir

   Purpose:  To parse the directive: sslcadir <path>

             <path>    the path of the server key to be used.

  Output: 0 upon success or !0 upon failure.
 */

int XrdHttpProtocol::xsslcadir(XrdOucStream & Config) {
  char *val;

  // Get the path
  //
  val = Config.GetWord();
  if (!val || !val[0]) {
    eDest.Emsg("Config", "HTTP X509 CAdir not specified");
    return 1;
  }

  // Record the path
  //
  if (sslcadir) free(sslcadir);
  sslcadir = strdup(val);

  return 0;
}

/******************************************************************************/
/*                                x t r a c e                                 */
/******************************************************************************/

/* Function: xtrace

   Purpose:  To parse the directive: trace <events>

             <events> the blank separated list of events to trace. Trace
                      directives are cummalative.

   Output: 0 upon success or 1 upon failure.
 */

int XrdHttpProtocol::xtrace(XrdOucStream & Config) {

  char *val;

  static struct traceopts {
    const char *opname;
    int opval;
  } tropts[] = {
    {"all", TRACE_ALL},
    {"emsg", TRACE_EMSG},
    {"debug", TRACE_DEBUG},
    {"fs", TRACE_FS},
    {"login", TRACE_LOGIN},
    {"mem", TRACE_MEM},
    {"stall", TRACE_STALL},
    {"redirect", TRACE_REDIR},
    {"request", TRACE_REQ},
    {"response", TRACE_RSP}
  };
  int i, neg, trval = 0, numopts = sizeof (tropts) / sizeof (struct traceopts);

  if (!(val = Config.GetWord())) {
    eDest.Emsg("config", "trace option not specified");
    return 1;
  }
  while (val) {
    if (!strcmp(val, "off")) trval = 0;
    else {
      if ((neg = (val[0] == '-' && val[1]))) val++;
      for (i = 0; i < numopts; i++) {
        if (!strcmp(val, tropts[i].opname)) {
          if (neg) trval &= ~tropts[i].opval;
          else trval |= tropts[i].opval;
          break;
        }
      }
      if (i >= numopts)
        eDest.Emsg("config", "invalid trace option", val);
    }
    val = Config.GetWord();
  }
  XrdHttpTrace->What = trval;
  return 0;
}

int XrdHttpProtocol::doStat(char *fname) {
  int l;
  bool b;
  CurrentReq.filesize = 0;
  CurrentReq.fileflags = 0;
  CurrentReq.filemodtime = 0;

  memset(&CurrentReq.xrdreq, 0, sizeof (ClientRequest));
  CurrentReq.xrdreq.stat.requestid = htons(kXR_stat);
  memset(CurrentReq.xrdreq.stat.reserved, 0,
          sizeof (CurrentReq.xrdreq.stat.reserved));
  l = strlen(fname) + 1;
  CurrentReq.xrdreq.stat.dlen = htonl(l);

  b = Bridge->Run((char *) &CurrentReq.xrdreq, fname, l);
  if (!b) {
    return -1;
  }


  return 0;
}




// Loads the SecXtractor plugin, if available
int XrdHttpProtocol::LoadSecXtractor(XrdSysError *myeDest, const char *libName,
                                     const char *libParms) {
    XrdSysPlugin     myLib(myeDest, libName, "secxtractorlib");
    XrdHttpSecXtractor *(*ep)(XrdHttpSecXtractorArgs);
    //static XrdVERSIONINFODEF (myVer, XrdHttpSecXtractor, XrdVNUMBER, XrdVERSION);


    // Get the entry point of the object creator
    //
    ep = (XrdHttpSecXtractor *(*)(XrdHttpSecXtractorArgs))(myLib.getPlugin("XrdHttpGetSecXtractor"));
    if (!ep) return 1;
    myLib.Persist();

    // Get the Object now
    //
    secxtractor = ep(myeDest, NULL, libParms);

    return 0;
}


