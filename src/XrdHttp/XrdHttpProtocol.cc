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
#include "XrdOuc/XrdOucGMap.hh"
#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "XrdOuc/XrdOucPinLoader.hh"
#include "XrdHttpTrace.hh"
#include "XrdHttpProtocol.hh"

#include <sys/stat.h>
#include "XrdHttpUtils.hh"
#include "XrdHttpSecXtractor.hh"
#include "XrdHttpExtHandler.hh"

#include "XrdTls/XrdTls.hh"
#include "XrdTls/XrdTlsContext.hh"

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <vector>
#include <arpa/inet.h>
#include <sstream>
#include <cctype>
#include <sys/stat.h>
#include <fcntl.h>

#define XRHTTP_TK_GRACETIME     600


/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

// It seems that eos needs this to be present
const char *XrdHttpSecEntityTident = "http";

//
// Static stuff
//

int XrdHttpProtocol::hailWait = 0;
int XrdHttpProtocol::readWait = 0;
int XrdHttpProtocol::Port = 1094;
char *XrdHttpProtocol::Port_str = 0;

//XrdXrootdStats *XrdHttpProtocol::SI = 0;
char *XrdHttpProtocol::sslcert = 0;
char *XrdHttpProtocol::sslkey = 0;
char *XrdHttpProtocol::sslcadir = 0;
int XrdHttpProtocol::crlRefIntervalSec = XrdTlsContext::DEFAULT_CRL_REF_INT_SEC;
char *XrdHttpProtocol::sslcipherfilter = 0;
char *XrdHttpProtocol::listredir = 0;
bool XrdHttpProtocol::listdeny = false;
bool XrdHttpProtocol::embeddedstatic = true;
char *XrdHttpProtocol::staticredir = 0;
XrdOucHash<XrdHttpProtocol::StaticPreloadInfo> *XrdHttpProtocol::staticpreload = 0;

kXR_int32 XrdHttpProtocol::myRole = kXR_isManager;
bool XrdHttpProtocol::selfhttps2http = false;
bool XrdHttpProtocol::isdesthttps = false;
char *XrdHttpProtocol::sslcafile = 0;
char *XrdHttpProtocol::secretkey = 0;

char *XrdHttpProtocol::gridmap = 0;
bool XrdHttpProtocol::isRequiredGridmap = false;
bool XrdHttpProtocol::compatNameGeneration = false;
int XrdHttpProtocol::sslverifydepth = 9;
BIO *XrdHttpProtocol::sslbio_err = 0;
XrdHttpSecXtractor *XrdHttpProtocol::secxtractor = 0;
bool XrdHttpProtocol::isRequiredXtractor = false;
struct XrdHttpProtocol::XrdHttpExtHandlerInfo XrdHttpProtocol::exthandler[MAX_XRDHTTPEXTHANDLERS];
int XrdHttpProtocol::exthandlercnt = 0;
std::map< std::string, std::string > XrdHttpProtocol::hdr2cgimap; 

bool XrdHttpProtocol::usingEC = false;

XrdScheduler *XrdHttpProtocol::Sched = 0; // System scheduler
XrdBuffManager *XrdHttpProtocol::BPool = 0; // Buffer manager
XrdSysError XrdHttpProtocol::eDest = 0; // Error message handler
XrdSecService *XrdHttpProtocol::CIA = 0; // Authentication Server
int XrdHttpProtocol::m_bio_type = 0; // BIO type identifier for our custom BIO.
BIO_METHOD *XrdHttpProtocol::m_bio_method = NULL; // BIO method constructor.
char *XrdHttpProtocol::xrd_cslist = nullptr;
XrdNetPMark * XrdHttpProtocol::pmarkHandle = nullptr;
XrdHttpChecksumHandler XrdHttpProtocol::cksumHandler = XrdHttpChecksumHandler();
XrdHttpReadRangeHandler::Configuration XrdHttpProtocol::ReadRangeConfig;

XrdSysTrace XrdHttpTrace("http");

namespace
{
const char *TraceID = "Protocol";
}

namespace XrdHttpProtoInfo
{
XrdTlsContext *xrdctx = 0;

static const int hsmAuto = -1;
static const int hsmOff  =  0;
static const int hsmMan  =  1;
static const int hsmOn   =  1; // Dual purpose but use a meaningful varname

int  httpsmode = hsmAuto;
int  tlsCache  = XrdTlsContext::scOff;
bool httpsspec = false;
bool xrdctxVer = false;
}

using namespace XrdHttpProtoInfo;

/******************************************************************************/
/*            P r o t o c o l   M a n a g e m e n t   S t a c k s             */
/******************************************************************************/

XrdObjectQ<XrdHttpProtocol>
XrdHttpProtocol::ProtStack("ProtStack",
        "xrootd protocol anchor");


/******************************************************************************/
/*               U g l y  O p e n S S L   w o r k a r o u n d s               */
/******************************************************************************/
#if OPENSSL_VERSION_NUMBER < 0x10100000L
void *BIO_get_data(BIO *bio) {
  return bio->ptr;
}
void BIO_set_data(BIO *bio, void *ptr) {
  bio->ptr = ptr;
}
#if OPENSSL_VERSION_NUMBER < 0x1000105fL
int BIO_get_flags(BIO *bio) {
  return bio->flags;
}
#endif
void BIO_set_flags(BIO *bio, int flags) {
  bio->flags = flags;
}
int BIO_get_init(BIO *bio) {
  return bio->init;
}
void BIO_set_init(BIO *bio, int init) {
  bio->init = init;
}
void BIO_set_shutdown(BIO *bio, int shut) {
  bio->shutdown = shut;
}
int BIO_get_shutdown(BIO *bio) {
  return bio->shutdown;
}
    
#endif
/******************************************************************************/
/*               X r d H T T P P r o t o c o l   C l a s s                    */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdHttpProtocol::XrdHttpProtocol(bool imhttps)
: XrdProtocol("HTTP protocol handler"), ProtLink(this),
SecEntity(""), CurrentReq(this, ReadRangeConfig) {
  myBuff = 0;
  Addr_str = 0;
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

      if (httpsmode) {
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

  // We now have to do some work arounds to tell the underlying framework
  // that is is https without invoking TLS on the actual link. Eventually,
  // we should just use the link's TLS native implementation.
  //
  hp->SecEntity.addrInfo = lp->AddrInfo();
  XrdNetAddr *netP = const_cast<XrdNetAddr*>(lp->NetAddr());
  netP->SetDialect("https");
  netP->SetTLS(true);

  // Allocate 1MB buffer from pool
  if (!hp->myBuff) {
    hp->myBuff = BPool->Obtain(1024 * 1024);
  }
  hp->myBuffStart = hp->myBuffEnd = hp->myBuff->buff;

  // Bind the protocol to the link and return the protocol
  //
  hp->Link = lp;
  return (XrdProtocol *) hp;
}

char *XrdHttpProtocol::GetClientIPStr() {
  char buf[256];
  buf[0] = '\0';
  if (!Link) return strdup("unknown");
  XrdNetAddrInfo *ai = Link->AddrInfo();
  if (!ai) return strdup("unknown");

  if (!Link->AddrInfo()->Format(buf, 255, XrdNetAddrInfo::fmtAddr, XrdNetAddrInfo::noPort)) return strdup("unknown");

  return strdup(buf);
}


// Various routines for handling XrdLink as BIO objects within OpenSSL.
#if OPENSSL_VERSION_NUMBER < 0x1000105fL
int BIO_XrdLink_write(BIO *bio, const char *data, size_t datal, size_t *written)
{
  if (!data || !bio) {
    *written = 0;
    return 0;
  }
  
  XrdLink *lp=static_cast<XrdLink *>(BIO_get_data(bio));
  
  errno = 0;
  int ret = lp->Send(data, datal);
  BIO_clear_retry_flags(bio);
  if (ret <= 0) {
    *written = 0;
    if ((errno == EINTR) || (errno == EINPROGRESS) || (errno == EAGAIN) || (errno == EWOULDBLOCK))
      BIO_set_retry_write(bio);
    return ret;
  }
  *written = ret;
  return 1;
}
#else
int BIO_XrdLink_write(BIO *bio, const char *data, int datal)
{
  if (!data || !bio) {
    errno = ENOMEM;
    return -1;
  }

  errno = 0;
  XrdLink *lp = static_cast<XrdLink *>(BIO_get_data(bio));
  int ret = lp->Send(data, datal);
  BIO_clear_retry_flags(bio);
  if (ret <= 0) {
    if ((errno == EINTR) || (errno == EINPROGRESS) || (errno == EAGAIN) || (errno == EWOULDBLOCK))
      BIO_set_retry_write(bio);
  }
  return ret;
}
#endif


#if OPENSSL_VERSION_NUMBER < 0x1000105fL
static int BIO_XrdLink_read(BIO *bio, char *data, size_t datal, size_t *read)
{
  if (!data || !bio) {
    *read = 0;
    return 0;
  }

  errno = 0;
  
  XrdLink *lp = static_cast<XrdLink *>(BIO_get_data(bio));  
  int ret = lp->Recv(data, datal);
  BIO_clear_retry_flags(bio);
  if (ret <= 0) {
    *read = 0;
    if ((errno == EINTR) || (errno == EINPROGRESS) || (errno == EAGAIN) || (errno == EWOULDBLOCK))
      BIO_set_retry_read(bio);
    return ret;
  }
  *read = ret;
}
#else
static int BIO_XrdLink_read(BIO *bio, char *data, int datal)
{
  if (!data || !bio) {
    errno = ENOMEM;
    return -1;
  }

  errno = 0;
  XrdLink *lp = static_cast<XrdLink *>(BIO_get_data(bio));
  int ret = lp->Recv(data, datal);
  BIO_clear_retry_flags(bio);
  if (ret <= 0) {
    if ((errno == EINTR) || (errno == EINPROGRESS) || (errno == EAGAIN) || (errno == EWOULDBLOCK))
      BIO_set_retry_read(bio);
  }
  return ret;
}
#endif


static int BIO_XrdLink_create(BIO *bio)
{
    
    
  BIO_set_init(bio, 0);
  //BIO_set_next(bio, 0);
  BIO_set_data(bio, NULL);
  BIO_set_flags(bio, 0);
  
#if OPENSSL_VERSION_NUMBER < 0x10100000L

  bio->num = 0;

#endif
  
  return 1;
}


static int BIO_XrdLink_destroy(BIO *bio)
{
  if (bio == NULL) return 0;
  if (BIO_get_shutdown(bio)) {
    if (BIO_get_data(bio)) {
      static_cast<XrdLink*>(BIO_get_data(bio))->Close();
    }
    BIO_set_init(bio, 0);
    BIO_set_flags(bio, 0);
  }
  return 1;
}


static long BIO_XrdLink_ctrl(BIO *bio, int cmd, long num, void * ptr)
{
  long ret = 1;
  switch (cmd) {
  case BIO_CTRL_GET_CLOSE:
    ret = BIO_get_shutdown(bio);
    break;
  case BIO_CTRL_SET_CLOSE:
    BIO_set_shutdown(bio, (int)num);
    break;  
  case BIO_CTRL_DUP:
  case BIO_CTRL_FLUSH:
    ret = 1;
    break;
  default:
    ret = 0;
    break;
  }
  return ret;
}


BIO *XrdHttpProtocol::CreateBIO(XrdLink *lp)
{
  if (m_bio_method == NULL)
    return NULL;

  BIO *ret = BIO_new(m_bio_method);

  BIO_set_shutdown(ret, 0);
  BIO_set_data(ret, lp);
  BIO_set_init(ret, 1);
  return ret;
}


/******************************************************************************/
/*                               P r o c e s s                                */
/******************************************************************************/

#undef  TRACELINK
#define TRACELINK Link

int XrdHttpProtocol::Process(XrdLink *lp) // We ignore the argument here
{
  int rc = 0;

  TRACEI(DEBUG, " Process. lp:"<<(void *)lp<<" reqstate: "<<CurrentReq.reqstate);

  if (!myBuff || !myBuff->buff || !myBuff->bsize) {
    TRACE(ALL, " Process. No buffer available. Internal error.");
    return -1;
  }


  if (!SecEntity.host) {
    char *nfo = GetClientIPStr();
    if (nfo) {
      TRACEI(REQ, " Setting host: " << nfo);
      SecEntity.host = nfo;
      strcpy(SecEntity.prot, "http");
    }
  }



  // If https then check independently for the ssl handshake
  if (ishttps && !ssldone) {

      if (!ssl) {
          sbio = CreateBIO(Link);
          BIO_set_nbio(sbio, 1);
          ssl = (SSL*)xrdctx->Session();
        }

      if (!ssl) {
          TRACEI(DEBUG, " SSL_new returned NULL");
          ERR_print_errors(sslbio_err);
          return -1;
        }

      // If a secxtractor has been loaded
      // maybe it wants to add its own initialization bits
      if (secxtractor)
        secxtractor->InitSSL(ssl, sslcadir);

      SSL_set_bio(ssl, sbio, sbio);
      //SSL_set_connect_state(ssl);

      //SSL_set_fd(ssl, Link->FDnum());
      struct timeval tv;
      tv.tv_sec = 10;
      tv.tv_usec = 0;
      setsockopt(Link->FDnum(), SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval));
      setsockopt(Link->FDnum(), SOL_SOCKET, SO_SNDTIMEO, (struct timeval *)&tv, sizeof(struct timeval));

      TRACEI(DEBUG, " Entering SSL_accept...");
      int res = SSL_accept(ssl);
      TRACEI(DEBUG, " SSL_accept returned :" << res);
      if ((res == -1) && (SSL_get_error(ssl, res) == SSL_ERROR_WANT_READ)) {
          TRACEI(DEBUG, " SSL_accept wants to read more bytes... err:" << SSL_get_error(ssl, res));
          return 1;
        }

      if(res <= 0) {
          ERR_print_errors(sslbio_err);
          if (res < 0) {

              SSL_free(ssl);
              ssl = 0;
              return -1;
          }
      }

      BIO_set_nbio(sbio, 0);

      strcpy(SecEntity.prot, "https");

      // Get the voms string and auth information
      if (HandleAuthentication(Link)) {
          SSL_free(ssl);
          ssl = 0;
          return -1;
      }

      ssldone = true;
      if (TRACING(TRACE_AUTH)) {
        SecEntity.Display(eDest);
      }
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
  } else if (!DoneSetInfo && !CurrentReq.userAgent().empty()) { // DoingLogin is true, meaning the login finished.
    std::string mon_info = "monitor info " + CurrentReq.userAgent();
    DoneSetInfo = true;
    if (mon_info.size() >= 1024) {
      TRACEI(ALL, "User agent string too long");
    } else if (!Bridge) {
      TRACEI(ALL, "Internal logic error: Bridge is null after login");
    } else {
      TRACEI(DEBUG, "Setting " << mon_info);
      memset(&CurrentReq.xrdreq, 0, sizeof (ClientRequest));
      CurrentReq.xrdreq.set.requestid = htons(kXR_set);
      CurrentReq.xrdreq.set.modifier = '\0';
      memset(CurrentReq.xrdreq.set.reserved, '\0', sizeof(CurrentReq.xrdreq.set.reserved));
      CurrentReq.xrdreq.set.dlen = htonl(mon_info.size());
      if (!Bridge->Run((char *) &CurrentReq.xrdreq, (char *) mon_info.c_str(), mon_info.size())) {
        SendSimpleResp(500, nullptr, nullptr, "Could not set user agent.", 0, false);
        return -1;
      }
      return 0;
    }
  } else {
    DoingLogin = false;
  }

  // Read the next request header, that is, read until a double CRLF is found


  if (!CurrentReq.headerok) {

    // Read as many lines as possible into the buffer. An empty line breaks
    while ((rc = BuffgetLine(tmpline)) > 0) {
      TRACE(DEBUG, " rc:" << rc << " got hdr line: " << tmpline.c_str());

      if ((rc == 2) && (tmpline.length() > 1) && (tmpline[rc - 1] == '\n')) {
        CurrentReq.headerok = true;
        TRACE(DEBUG, " rc:" << rc << " detected header end.");
        break;
      }


      if (CurrentReq.request == CurrentReq.rtUnset) {
        TRACE(DEBUG, " Parsing first line: " << tmpline.c_str());
        int result = CurrentReq.parseFirstLine((char *)tmpline.c_str(), rc);
        if (result < 0) {
          TRACE(DEBUG, " Parsing of first line failed with " << result);
          return -1;
        }
      }
      else
        CurrentReq.parseLine((char *)tmpline.c_str(), rc);


    }

    // Here we have CurrentReq loaded with the header, or its relevant fields

    if (!CurrentReq.headerok) {
      TRACEI(REQ, " rc:" << rc << "Header not yet complete.");
      
      // Here a subtle error condition. IF we failed reading a line AND the buffer
      // has a reasonable amount of data available THEN we consider the header
      // as corrupted and shutdown the client
      if ((rc <= 0) && (BuffUsed() >= 16384)) {
        TRACEI(ALL, "Corrupted header detected, or line too long. Disconnecting client.");
        return -1;
      }
        
        
      if (CurrentReq.reqstate > 0)
        CurrentReq.reqstate--;
      // Waiting for more data
      return 1;
    }

  }

  // If we are in self-redirect mode, then let's do it
  // Do selfredirect only with 'simple' requests, otherwise poor clients may misbehave
  if (ishttps && ssldone && selfhttps2http &&
    ( (CurrentReq.request == XrdHttpReq::rtGET) || (CurrentReq.request == XrdHttpReq::rtPUT) ||
    (CurrentReq.request == XrdHttpReq::rtPROPFIND)) ) {
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
      
      // We have to recompute it here because we don't know to which
      // interface the client had connected to
      struct sockaddr_storage sa;
      socklen_t sl = sizeof(sa);
      getsockname(this->Link->AddrInfo()->SockFD(), (struct sockaddr*)&sa, &sl); 
      
      // now get it back and print it
      char buf[256];
      bool ok = false;
      
      switch (sa.ss_family) {
        case AF_INET:
          if (inet_ntop(AF_INET, &(((sockaddr_in*)&sa)->sin_addr), buf, INET_ADDRSTRLEN)) {
            if (Addr_str) free(Addr_str);
            Addr_str = strdup(buf);
            ok = true;
          }
          break;
        case AF_INET6:
          if (inet_ntop(AF_INET6, &(((sockaddr_in6*)&sa)->sin6_addr), buf, INET6_ADDRSTRLEN)) {
            if (Addr_str) free(Addr_str);
            Addr_str = (char *)malloc(strlen(buf)+3);
            strcpy(Addr_str, "[");
            strcat(Addr_str, buf);
            strcat(Addr_str, "]");
            ok = true;
          }
          break;
        default:
          TRACEI(REQ, " Can't recognize the address family of the local host.");
      }
      
      if (ok) {
        dest += Addr_str;
        dest += ":";
        dest += Port_str;
        dest += CurrentReq.resource.c_str();
        TRACEI(REQ," rc:"<<rc<<" self-redirecting to http with security token: '"
                   << dest.c_str() << "'");

        
        CurrentReq.appendOpaque(dest, &SecEntity, hash, timenow);
        SendSimpleResp(302, NULL, (char *) dest.c_str(), 0, 0, true);
        CurrentReq.reset();
        return -1;
      }
      
      TRACEI(REQ, " rc:" << rc << " Can't perform self-redirection.");
      
    }
    else {
      TRACEI(ALL, " Could not calculate self-redirection hash");
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
        if (abs(time(0) - tim) > XRHTTP_TK_GRACETIME) {
          TRACEI(REQ, " Token expired. Authentication failed.");
          return -1;
        }

        // Fill the Secentity from the fields in the URL:name, vo, host
        char *nfo;

        nfo = CurrentReq.opaque->Get("xrdhttpvorg");
        if (nfo) {
          TRACEI(DEBUG, " Setting vorg: " << nfo);
          SecEntity.vorg = strdup(nfo);
          TRACEI(REQ, " Setting vorg: " << SecEntity.vorg);
        }

        nfo = CurrentReq.opaque->Get("xrdhttpname");
        if (nfo) {
          TRACEI(DEBUG, " Setting name: " << nfo);
          SecEntity.name = unquote(nfo);
          TRACEI(REQ, " Setting name: " << SecEntity.name);
        }
        
        nfo = CurrentReq.opaque->Get("xrdhttphost");
        if (nfo) {
          TRACEI(DEBUG, " Setting host: " << nfo);
          if (SecEntity.host) free(SecEntity.host);
          SecEntity.host = unquote(nfo);
          TRACEI(REQ, " Setting host: " << SecEntity.host);
        }
        
        nfo = CurrentReq.opaque->Get("xrdhttpdn");
        if (nfo) {
          TRACEI(DEBUG, " Setting dn: " << nfo);
          SecEntity.moninfo = unquote(nfo);
          TRACEI(REQ, " Setting dn: " << SecEntity.moninfo);
        }

        nfo = CurrentReq.opaque->Get("xrdhttprole");
        if (nfo) {
          TRACEI(DEBUG, " Setting role: " << nfo);
          SecEntity.role = unquote(nfo);
          TRACEI(REQ, " Setting role: " << SecEntity.role);
        }

        nfo = CurrentReq.opaque->Get("xrdhttpgrps");
        if (nfo) {
          TRACEI(DEBUG, " Setting grps: " << nfo);
          SecEntity.grps = unquote(nfo);
          TRACEI(REQ, " Setting grps: " << SecEntity.grps);
        }
        
        nfo = CurrentReq.opaque->Get("xrdhttpendorsements");
        if (nfo) {
          TRACEI(DEBUG, " Setting endorsements: " << nfo);
          SecEntity.endorsements = unquote(nfo);
          TRACEI(REQ, " Setting endorsements: " << SecEntity.endorsements);
        }
        
        nfo = CurrentReq.opaque->Get("xrdhttpcredslen");
        if (nfo) {
          TRACEI(DEBUG, " Setting credslen: " << nfo);
          char *s1 = unquote(nfo);
          if (s1 && s1[0]) {
            SecEntity.credslen = atoi(s1);
            TRACEI(REQ, " Setting credslen: " << SecEntity.credslen);
          }
          if (s1) free(s1);
        }
        
        if (SecEntity.credslen) {
          nfo = CurrentReq.opaque->Get("xrdhttpcreds");
          if (nfo) {
            TRACEI(DEBUG, " Setting creds: " << nfo);
            SecEntity.creds = unquote(nfo);
            TRACEI(REQ, " Setting creds: " << SecEntity.creds);
          }
        }
        
        char hash[512];

        calcHashes(hash, CurrentReq.resource.c_str(), (kXR_int16) CurrentReq.request,
                &SecEntity,
                tim,
                secretkey);

        if (compareHash(hash, tk)) {
          TRACEI(REQ, " Invalid tk '" << tk << "' != '" << hash << "'(calculated). Authentication failed.");
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
  // Remember that if there is an exthandler then it has the responsibility
  // for authorization in the paths that it manages
  if (!Bridge && !FindMatchingExtHandler(CurrentReq)) {
    if (SecEntity.name)
      Bridge = XrdXrootd::Bridge::Login(&CurrentReq, Link, &SecEntity, SecEntity.name, ishttps ? "https" : "http");
    else
      Bridge = XrdXrootd::Bridge::Login(&CurrentReq, Link, &SecEntity, "unknown", ishttps ? "https" : "http");
      
    if (!Bridge) {
      TRACEI(REQ, " Authorization failed.");
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
//#define TS_Xeq3(x,m) (!strcmp(x,var)) GoNo = m(Config, ConfigFN, myEnv)
#define TS_Xeq3(x,m) (!strcmp(x,var)) GoNo = m(Config, extHIVec)

#define HTTPS_ALERT(x,y,z) httpsspec = true;\
        if (xrdctx && httpsmode == hsmAuto && (z || xrdctx->x509Verify())) \
        eDest.Say("Config http." x " overrides the xrd." y " directive.")

int XrdHttpProtocol::Config(const char *ConfigFN, XrdOucEnv *myEnv) {
  XrdOucEnv cfgEnv;
  XrdOucStream Config(&eDest, getenv("XRDINSTANCE"), &cfgEnv, "=====> ");
  std::vector<extHInfo> extHIVec;
  char *var;
  int cfgFD, GoNo, NoGo = 0, ismine;

  var = nullptr;
  XrdOucEnv::Import("XRD_READV_LIMITS", var);
  XrdHttpReadRangeHandler::Configure(eDest, var, ReadRangeConfig);

  pmarkHandle = (XrdNetPMark* ) myEnv->GetPtr("XrdNetPMark*");

  cksumHandler.configure(xrd_cslist);
  auto nonIanaChecksums = cksumHandler.getNonIANAConfiguredCksums();
  if(nonIanaChecksums.size()) {
      std::stringstream warningMsgSS;
      warningMsgSS << "Config warning: the following checksum algorithms are not IANA compliant: [";
      std::string unknownCksumString;
      for(auto unknownCksum: nonIanaChecksums) {
          unknownCksumString += unknownCksum + ",";
      }
      unknownCksumString.erase(unknownCksumString.size() - 1);
      warningMsgSS << unknownCksumString << "]" << ". They therefore cannot be queried by a user via HTTP." ;
      eDest.Say(warningMsgSS.str().c_str());
  }

  // Initialize our custom BIO type.
  if (!m_bio_type) {

    #if OPENSSL_VERSION_NUMBER < 0x10100000L
      m_bio_type = (26|0x0400|0x0100);
      m_bio_method = static_cast<BIO_METHOD*>(OPENSSL_malloc(sizeof(BIO_METHOD)));
 
      if (m_bio_method) {
        memset(m_bio_method, '\0', sizeof(BIO_METHOD));
        m_bio_method->type = m_bio_type;
        m_bio_method->bwrite = BIO_XrdLink_write;
        m_bio_method->bread = BIO_XrdLink_read;
        m_bio_method->create = BIO_XrdLink_create;
        m_bio_method->destroy = BIO_XrdLink_destroy;
        m_bio_method->ctrl = BIO_XrdLink_ctrl;
      }
    #else
      // OpenSSL 1.1 has an internal counter for generating unique types.
      // We'll switch to that when widely available.
      m_bio_type = BIO_get_new_index();
      m_bio_method = BIO_meth_new(m_bio_type, "xrdhttp-bio-method");
      
      if (m_bio_method) {
        BIO_meth_set_write(m_bio_method, BIO_XrdLink_write);
        BIO_meth_set_read(m_bio_method, BIO_XrdLink_read);
        BIO_meth_set_create(m_bio_method, BIO_XrdLink_create);
        BIO_meth_set_destroy(m_bio_method, BIO_XrdLink_destroy);
        BIO_meth_set_ctrl(m_bio_method, BIO_XrdLink_ctrl);
      }
      
    #endif
  }

  // If we have a tls context record whether it configured for verification
  // so that we can provide meaningful error and warning messages.
  //
  xrdctxVer = xrdctx && xrdctx->x509Verify();

  // Open and attach the config file
  //
  if ((cfgFD = open(ConfigFN, O_RDONLY, 0)) < 0)
    return eDest.Emsg("Config", errno, "open config file", ConfigFN);
  Config.Attach(cfgFD);
  static const char *cvec[] = { "*** http protocol config:", 0 };
  Config.Capture(cvec);

  // Process items
  //
  while ((var = Config.GetMyFirstWord())) {
    if ((ismine = !strncmp("http.", var, 5)) && var[5]) var += 5;

    if (ismine) {
           if TS_Xeq("trace", xtrace);
      else if TS_Xeq("cert", xsslcert);
      else if TS_Xeq("key", xsslkey);
      else if TS_Xeq("cadir", xsslcadir);
      else if TS_Xeq("cipherfilter", xsslcipherfilter);
      else if TS_Xeq("gridmap", xgmap);
      else if TS_Xeq("cafile", xsslcafile);
      else if TS_Xeq("secretkey", xsecretkey);
      else if TS_Xeq("desthttps", xdesthttps);
      else if TS_Xeq("secxtractor", xsecxtractor);
      else if TS_Xeq3("exthandler", xexthandler);
      else if TS_Xeq("selfhttps2http", xselfhttps2http);
      else if TS_Xeq("embeddedstatic", xembeddedstatic);
      else if TS_Xeq("listingredir", xlistredir);
      else if TS_Xeq("staticredir", xstaticredir);
      else if TS_Xeq("staticpreload", xstaticpreload);
      else if TS_Xeq("listingdeny", xlistdeny);
      else if TS_Xeq("header2cgi", xheader2cgi);
      else if TS_Xeq("httpsmode", xhttpsmode);
      else if TS_Xeq("tlsreuse", xtlsreuse);
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

// To minimize message confusion down, if an error occurred during config
// parsing, just bail out now with a confirming message.
//
   if (NoGo)
      {eDest.Say("Config failure: one or more directives are flawed!");
       return 1;
      }

// Test if XrdEC is loaded
   if (getenv("XRDCL_EC")) usingEC = true;

// If https was disabled, then issue a warning message if xrdtls configured
// of it's disabled because httpsmode was auto and xrdtls was not configured.
// If we get past this point then we know https is a plausible option but we
// can still fail if we cannot supply any missing but required options.
//
   if (httpsmode == hsmOff || (httpsmode == hsmAuto && !xrdctx && !httpsspec))
      {const char *why = (httpsmode == hsmOff ? "has been disabled!"
                         : "was not configured.");
       const char *what = Configed();

       eDest.Say("Config warning: HTTPS functionality ", why);
       httpsmode = hsmOff;

       if (what)
          {eDest.Say("Config failure: ", what, " HTTPS but it ", why);
           NoGo = 1;
          }
       return NoGo;
      }

// Warn if a private key was specified without a cert as this has no meaning
// even as an auto overide as they must be paired.
//
   if (sslkey && !sslcert)
      {eDest.Say("Config warning: specifying http.key without http.cert "
                 "is meaningless; ignoring key!");
       free(sslkey); sslkey = 0;
      }

// If the mode is manual then we need to have at least a cert.
//
   if (httpsmode == hsmMan)
      {if (!sslcert)
          {eDest.Say("Config failure: 'httpsmode manual' requires atleast a "
                     "a cert specification!");
           return 1;
          }
      }

// If it's auto d through all possibilities. It's either auto with xrdtls
// configured or manual which needs at least a cert specification. For auto
// configuration we will only issue a warning if overrides were specified.
//
   if (httpsmode == hsmAuto && xrdctx)
      {const XrdTlsContext::CTX_Params *cP = xrdctx->GetParams();
       const char *what1 = 0, *what2 = 0, *what3 = 0;

       if (!sslcert && cP->cert.size())
          {sslcert = strdup(cP->cert.c_str());
           if (cP->pkey.size()) sslkey  = strdup(cP->pkey.c_str());
           what1 = "xrd.tls to supply 'cert' and 'key'.";
          }
       if (!sslcadir && cP->cadir.size())
          {sslcadir  = strdup(cP->cadir.c_str());
           what2 = "xrd.tlsca to supply 'cadir'.";
          }
       if (!sslcafile && cP->cafile.size())
          {sslcafile = strdup(cP->cafile.c_str());
           what2 = (what2 ? "xrd.tlsca to supply 'cadir' and 'cafile'."
                          : "xrd.tlsca to supply 'cafile'.");
          }
       if(cP->crlRT != XrdTlsContext::DEFAULT_CRL_REF_INT_SEC) {
           crlRefIntervalSec = cP->crlRT;
           what3 = "xrd.tlsca to supply 'refresh' interval.";
       }
       if (!httpsspec && what1) eDest.Say("Config Using ", what1);
       if (!httpsspec && what2) eDest.Say("Config Using ", what2);
       if (!httpsspec && what3) eDest.Say("Config Using ", what3);
      }

// If a gridmap or secxtractor is present then we must be able to verify certs
//
   if (!(sslcadir || sslcafile))
      {const char *what = Configed();
       const char *why  = (httpsspec ? "a cadir or cafile was not specified!"
                                     : "'xrd.tlsca noverify' was specified!");
       if (what)
          {eDest.Say("Config failure: ", what, " cert verification but ", why);
           return 1;
          }
      }
   httpsmode = hsmOn;

// Oddly we need to create an error bio at this point
//
   sslbio_err = BIO_new_fp(stderr, BIO_NOCLOSE);

// Now we can configure HTTPS. We will not reuse the passed context as we will
// be setting our own options specific to out implementation. One day we will.
//
   const char *how = "completed.";
   eDest.Say("++++++ HTTPS initialization started.");
   if (!InitTLS()) {NoGo = 1; how = "failed.";}
   eDest.Say("------ HTTPS initialization ", how);
   if (NoGo) return NoGo;

// We can now load all the external handlers
//
   if (LoadExtHandler(extHIVec, ConfigFN, *myEnv)) return 1;

// At this point, we can actually initialize security plugins
//
   return (InitSecurity() ? NoGo : 1);
}

/******************************************************************************/
/*                              C o n f i g e d                               */
/******************************************************************************/

const char *XrdHttpProtocol::Configed()
{
   if (secxtractor && gridmap) return "gridmap and secxtractor require";
   if (secxtractor) return "secxtractor requires";
   if (gridmap) return "gridmap requires";
   return 0;
}
  
/******************************************************************************/
/*                           B u f f g e t L i n e                            */
/******************************************************************************/
  
/// Copy a full line of text from the buffer into dest. Zero if no line can be found in the buffer

int XrdHttpProtocol::BuffgetLine(XrdOucString &dest) {

  dest = "";
  char save;

  // Easy case
  if (myBuffEnd >= myBuffStart) {
    int l = 0;
    for (char *p = myBuffStart; p < myBuffEnd; p++) {
      l++;
      if (*p == '\n') {
        save = *(p+1);
        *(p+1) = '\0';
        dest.assign(myBuffStart, 0, l-1);
        *(p+1) = save;

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
        save = *(p+1);
        *(p+1) = '\0';
        dest.assign(myBuffStart, 0, l-1);
        *(p+1) = save;

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
        save = *(p+1);
        *(p+1) = '\0';
        // Remember the 1st segment
        int l1 = myBuff->buff + myBuff->bsize - myBuffStart;

        dest.assign(myBuffStart, 0, l1-1);
        //strncpy(dest, myBuffStart, l1);
        BuffConsume(l1);

        dest.insert(myBuffStart, l1, l-1);
        //strncpy(dest + l1, myBuffStart, l);
        //dest[l + l1] = '\0';
        BuffConsume(l);

        *(p+1) = save;

        //if (dest[l + l1 - 1] == '\n') dest[l + l1 - 1] = '\0';
        return l + l1;
      }

    }



  }

  return 0;
}

/******************************************************************************/
/*                        g e t D a t a O n e S h o t                         */
/******************************************************************************/
  
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
  maxread = std::min(blen, BuffAvailable());
  TRACE(DEBUG, "getDataOneShot BuffAvailable: " << BuffAvailable() << " maxread: " << maxread);

  if (!maxread)
    return 2;

  if (ishttps) {
    int sslavail = maxread;

    if (!wait) {
      int l = SSL_pending(ssl);
      if (l > 0)
        sslavail = std::min(maxread, SSL_pending(ssl));
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
      Link->setEtext("link timeout or other error");
      return -1;
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

/******************************************************************************/
/*                              B u f f U s e d                               */
/******************************************************************************/
  
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

/******************************************************************************/
/*                              B u f f F r e e                               */
/******************************************************************************/
  
/// How many bytes free in the buffer

int XrdHttpProtocol::BuffFree() {
  return (myBuff->bsize - BuffUsed());
}

/******************************************************************************/
/*                           B u f f C o n s u m e                            */
/******************************************************************************/
  
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

/******************************************************************************/
/*                           B u f f g e t D a t a                            */
/******************************************************************************/
  
/// Get a pointer, valid for up to blen bytes from the buffer. Returns the n
/// of bytes that one is allowed to use in the *data block
/// If wait = true then the call may wait for the data to come from the socket
/// If wait = false then the call returns:
///   - what's in the buffer if there's anything in the buffer
///   - what can be read from the socket without waiting
///   In this case an error will return -1, instead the absence of data
///   will return 0
int XrdHttpProtocol::BuffgetData(int blen, char **data, bool wait) {
  int rlen;

  TRACE(DEBUG, "BuffgetData: requested " << blen << " bytes");
 

  if (wait) {
    // If there's not enough data in the buffer then wait on the socket until it comes
    if  (blen > BuffUsed()) {
      TRACE(REQ, "BuffgetData: need to read " << blen - BuffUsed() << " bytes");
      if ( getDataOneShot(blen - BuffUsed(), true) )
        // The wanted data could not be read. Either timeout of connection closed
        return 0;
    }
  } else {
    // Get a peek at the socket, without waiting, if we have no data in the buffer
    if ( !BuffUsed() ) {
      if ( getDataOneShot(blen, false) )
        // The wanted data could not be read. Either timeout of connection closed
        return -1;
    }
  }

  // And now make available the data taken from the buffer. Note that the buffer
  // may be empty...
  if (myBuffStart <= myBuffEnd) {
    rlen = std::min( (long) blen, (long)(myBuffEnd - myBuffStart) );

  } else
    rlen = std::min( (long) blen, (long)(myBuff->buff + myBuff->bsize - myBuffStart) );

  *data = myBuffStart;
  BuffConsume(rlen);
  return rlen;
}

/******************************************************************************/
/*                              S e n d D a t a                               */
/******************************************************************************/
  
/// Send some data to the client

int XrdHttpProtocol::SendData(const char *body, int bodylen) {

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

/******************************************************************************/
/*                       S t a r t S i m p l e R e s p                        */
/******************************************************************************/
  
int XrdHttpProtocol::StartSimpleResp(int code, const char *desc, const char *header_to_add, long long bodylen, bool keepalive) {
  std::stringstream ss;
  const std::string crlf = "\r\n";

  ss << "HTTP/1.1 " << code << " ";
  if (desc) {
    ss << desc;
  } else {
    if (code == 200) ss << "OK";
    else if (code == 100) ss << "Continue";
    else if (code == 206) ss << "Partial Content";
    else if (code == 302) ss << "Redirect";
    else if (code == 307) ss << "Temporary Redirect";
    else if (code == 400) ss << "Bad Request";
    else if (code == 403) ss << "Forbidden";
    else if (code == 404) ss << "Not Found";
    else if (code == 405) ss << "Method Not Allowed";
    else if (code == 416) ss << "Range Not Satisfiable";
    else if (code == 500) ss << "Internal Server Error";
    else ss << "Unknown";
  }
  ss << crlf;
  if (keepalive && (code != 100))
    ss << "Connection: Keep-Alive" << crlf;
  else
    ss << "Connection: Close" << crlf;

  ss << "Server: XrootD/" << XrdVSTRING << crlf;
  
  if ((bodylen >= 0) && (code != 100))
    ss << "Content-Length: " << bodylen << crlf;

  if (header_to_add && (header_to_add[0] != '\0'))
    ss << header_to_add << crlf;

  ss << crlf;

  const std::string &outhdr = ss.str();
  TRACEI(RSP, "Sending resp: " << code << " header len:" << outhdr.size());
  if (SendData(outhdr.c_str(), outhdr.size()))
    return -1;

  return 0;
}

/******************************************************************************/
/*                      S t a r t C h u n k e d R e s p                       */
/******************************************************************************/
  
int XrdHttpProtocol::StartChunkedResp(int code, const char *desc, const char *header_to_add, long long bodylen, bool keepalive) {
  const std::string crlf = "\r\n";
  std::stringstream ss;

  if (header_to_add && (header_to_add[0] != '\0')) {
    ss << header_to_add << crlf;
  }

  ss << "Transfer-Encoding: chunked";
  TRACEI(RSP, "Starting chunked response");
  return StartSimpleResp(code, desc, ss.str().c_str(), bodylen, keepalive);
}

/******************************************************************************/
/*                             C h u n k R e s p                              */
/******************************************************************************/
  
int XrdHttpProtocol::ChunkResp(const char *body, long long bodylen) {
  long long content_length = (bodylen <= 0) ? (body ? strlen(body) : 0) : bodylen;
  if (ChunkRespHeader(content_length))
    return -1;

  if (body && SendData(body, content_length))
    return -1;

  return ChunkRespFooter();
}

/******************************************************************************/
/*                       C h u n k R e s p H e a d e r                        */
/******************************************************************************/

int XrdHttpProtocol::ChunkRespHeader(long long bodylen) {
  const std::string crlf = "\r\n";
  std::stringstream ss;

  ss << std::hex << bodylen << std::dec << crlf;

  const std::string &chunkhdr = ss.str();
  TRACEI(RSP, "Sending encoded chunk of size " << bodylen);
  return (SendData(chunkhdr.c_str(), chunkhdr.size())) ? -1 : 0;
}

/******************************************************************************/
/*                       C h u n k R e s p F o o t e r                        */
/******************************************************************************/

int XrdHttpProtocol::ChunkRespFooter() {
  const std::string crlf = "\r\n";
  return (SendData(crlf.c_str(), crlf.size())) ? -1 : 0;
}

/******************************************************************************/
/*                        S e n d S i m p l e R e s p                         */
/******************************************************************************/
  
/// Sends a basic response. If the length is < 0 then it is calculated internally
/// Header_to_add is a set of header lines each CRLF terminated to be added to the header
/// Returns 0 if OK

int XrdHttpProtocol::SendSimpleResp(int code, const char *desc, const char *header_to_add, const char *body, long long bodylen, bool keepalive) {

  long long content_length = bodylen;
  if (bodylen <= 0) {
    content_length = body ? strlen(body) : 0;
  }

  if (StartSimpleResp(code, desc, header_to_add, content_length, keepalive) < 0)
    return -1;

  //
  // Send the data
  //
  if (body)
    return SendData(body, content_length);

  return 0;
}

/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
int XrdHttpProtocol::Configure(char *parms, XrdProtocol_Config * pi) {
  /*
    Function: Establish configuration at load time.

    Input:    None.

    Output:   0 upon success or !0 otherwise.
   */

  char *rdf;

  // Copy out the special info we want to use at top level
  //
  eDest.logger(pi->eDest->logger());
  XrdHttpTrace.SetLogger(pi->eDest->logger());
  //  SI = new XrdXrootdStats(pi->Stats);
  Sched = pi->Sched;
  BPool = pi->BPool;
  hailWait = 10000;
  readWait = 30000;
  xrd_cslist = getenv("XRD_CSLIST");

  Port = pi->Port;

  // Copy out the current TLS context
  //
  xrdctx = pi->tlsCtx;

  {
    char buf[16];
    sprintf(buf, "%d", Port);
    Port_str = strdup(buf);
  }

  // Now process and configuration parameters
  //
  rdf = (parms && *parms ? parms : pi->ConfigFN);
  if (rdf && Config(rdf, pi->theEnv)) return 0;
  if (pi->DebugON) XrdHttpTrace.What = TRACE_ALL;

  // Set the redirect flag if we are a pure redirector
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
  ProtStack.Set(pi->Sched, &XrdHttpTrace,
                (XrdHttpTrace.What & TRACE_MEM ? TRACE_MEM : 0));
  ProtStack.Set((pi->ConnMax / 3 ? pi->ConnMax / 3 : 30), 60 * 60);

  // Return success
  //

  return 1;
}



/******************************************************************************/
/*                               I n i t T L S                                */
/******************************************************************************/
  
bool XrdHttpProtocol::InitTLS() {

   std::string eMsg;
   uint64_t opts = XrdTlsContext::servr | XrdTlsContext::logVF |
                   XrdTlsContext::artON | XrdTlsContext::rfCRL;

// Create a new TLS context
//
   if (sslverifydepth > 255) sslverifydepth = 255;
   opts = TLS_SET_VDEPTH(opts, sslverifydepth);
   //TLS_SET_REFINT will set the refresh interval in minutes, hence the division by 60
   opts = TLS_SET_REFINT(opts, crlRefIntervalSec/60);
   xrdctx = new XrdTlsContext(sslcert,sslkey,sslcadir,sslcafile,opts,&eMsg);

// Make sure the context was created
//
   if (!xrdctx->isOK())
      {eDest.Say("Config failure: ", eMsg.c_str());
       return false;
      }

// Setup session cache (this is controversial). The default is off but many
// programs expect it being enabled and break when it is disabled. In such
// cases it should be enabled. This is, of course, a big OpenSSL mess.
//
   static const char *sess_ctx_id = "XrdHTTPSessionCtx";
   unsigned int n =(unsigned int)(strlen(sess_ctx_id)+1);
   xrdctx->SessionCache(tlsCache, sess_ctx_id, n);

// Set special ciphers if so specified.
//
   if (sslcipherfilter && !xrdctx->SetContextCiphers(sslcipherfilter))
      {eDest.Say("Config failure: ", "Unable to set allowable https ciphers!");
       return false;
      }

// All done
//
   return true;
}

/******************************************************************************/
/*                               C l e a n u p                                */
/******************************************************************************/
  
void XrdHttpProtocol::Cleanup() {

  TRACE(ALL, " Cleanup");

  if (BPool && myBuff) {
    BuffConsume(BuffUsed());
    BPool->Release(myBuff);
    myBuff = 0;
  }

  if (ssl) {
    // Shutdown the SSL/TLS connection
    // https://www.openssl.org/docs/man1.0.2/man3/SSL_shutdown.html
    // We don't need a bidirectional shutdown as
    // when we are here, the connection will not be re-used.
    // In the case SSL_shutdown returns 0,
    // "the output of SSL_get_error(3) may be misleading, as an erroneous SSL_ERROR_SYSCALL may be flagged even though no error occurred."
    // we will then just flush the thread's queue.
    // In the case an error really happened, we print the error that happened
    int ret = SSL_shutdown(ssl);
    if (ret != 1) {
        if(ret == 0) {
            // Clean this thread's error queue for the old openssl versions
            #if OPENSSL_VERSION_NUMBER < 0x10100000L
                ERR_remove_thread_state(nullptr);
            #endif
        } else {
            //ret < 0, an error really happened.
            TRACE(ALL, " SSL_shutdown failed");
            ERR_print_errors(sslbio_err);
        }
    }

    if (secxtractor)
        secxtractor->FreeSSL(ssl);

    SSL_free(ssl);

  }


  ssl = 0;
  sbio = 0;

  if (SecEntity.caps) free(SecEntity.caps);
  if (SecEntity.grps) free(SecEntity.grps);
  if (SecEntity.endorsements) free(SecEntity.endorsements);
  if (SecEntity.vorg) free(SecEntity.vorg);
  if (SecEntity.role) free(SecEntity.role);
  if (SecEntity.name) free(SecEntity.name);
  if (SecEntity.host) free(SecEntity.host);
  if (SecEntity.moninfo) free(SecEntity.moninfo);

  SecEntity.Reset();

  if (Addr_str) free(Addr_str);
  Addr_str = 0;
}

/******************************************************************************/
/*                                 R e s e t                                  */
/******************************************************************************/
  
void XrdHttpProtocol::Reset() {

  TRACE(ALL, " Reset");
  Link = 0;
  CurrentReq.reset();
  CurrentReq.reqstate = 0;

  if (myBuff) {
    BPool->Release(myBuff);
    myBuff = 0;
  }
  myBuffStart = myBuffEnd = 0;

  DoingLogin = false;
  DoneSetInfo = false;

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

  SecEntity.Reset();
  SecEntity.tident = XrdHttpSecEntityTident;
  ishttps = false;
  ssldone = false;

  Bridge = 0;
  ssl = 0;
  sbio = 0;

}

/******************************************************************************/
/*                            x h t t p s m o d e                             */
/******************************************************************************/

/* Function: xhttpsmode

   Purpose:  To parse the directive: httpsmode {auto | disable | manual}

             auto      configure https if configured in xrd framework.
             disable   do not configure https no matter what
             manual    configure https and ignore the xrd framework

  Output: 0 upon success or !0 upon failure.
 */

int XrdHttpProtocol::xhttpsmode(XrdOucStream & Config) {
  char *val;

  // Get the val
  //
  val = Config.GetWord();
  if (!val || !val[0]) {
    eDest.Emsg("Config", "httpsmode parameter not specified");
    return 1;
  }

  // Record the val
  //
       if (!strcmp(val, "auto"))    httpsmode = hsmAuto;
  else if (!strcmp(val, "disable")) httpsmode = hsmOff;
  else if (!strcmp(val, "manual"))  httpsmode = hsmMan;
  else {eDest.Emsg("Config", "invalid httpsmode parameter - ", val);
        return 1;
       }
  return 0;
}

/******************************************************************************/
/*                   x s s l v e r i f y d e p t h                            */
/******************************************************************************/

/* Function: xsslverifydepth

   Purpose:  To parse the directive: sslverifydepth <depth>

             <depth>   the max depth of the ssl cert verification

  Output: 0 upon success or !0 upon failure.
 */

int XrdHttpProtocol::xsslverifydepth(XrdOucStream & Config) {
  char *val;

  // Get the val
  //
  val = Config.GetWord();
  if (!val || !val[0]) {
    eDest.Emsg("Config", "sslverifydepth value not specified");
    return 1;
  }

  // Record the val
  //
  sslverifydepth = atoi(val);

  if (xrdctxVer){ HTTPS_ALERT("verifydepth","tlsca",false); }
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

  // If we have an xrd context issue reminder
  //
  HTTPS_ALERT("cert","tls",true);
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

  HTTPS_ALERT("key","tls",true);
  return 0;
}

/******************************************************************************/
/*                                     x g m a p                              */
/******************************************************************************/

/* Function: xgmap

   Purpose:  To parse the directive: gridmap [required] [compatNameGeneration] <path>

     required   optional parameter which if present treats any grimap errors
                as fatal.
     <path>     the path of the gridmap file to be used. Normally it's
                /etc/grid-security/gridmap. No mapfile means no translation
                required. Pointing to a non existing mapfile is an error.

  Output: 0 upon success or !0 upon failure.
 */

int XrdHttpProtocol::xgmap(XrdOucStream & Config) {
  char *val;

  // Get the path
  //
  val = Config.GetWord();
  if (!val || !val[0]) {
    eDest.Emsg("Config", "HTTP X509 gridmap file location not specified");
    return 1;
  }

  // Handle optional parameter "required"
  //
  if (!strncmp(val, "required", 8)) {
    isRequiredGridmap = true;
    val = Config.GetWord();

    if (!val || !val[0]) {
      eDest.Emsg("Config", "HTTP X509 gridmap file missing after [required] "
                 "parameter");
      return 1;
    }
  }

  // Handle optional parameter "compatNameGeneration"
  //
  if (!strcmp(val, "compatNameGeneration")) {
    compatNameGeneration = true;
    val = Config.GetWord();
    if (!val || !val[0]) {
      eDest.Emsg("Config", "HTTP X509 gridmap file missing after "
                 "[compatNameGeneration] parameter");
      return 1;
    }
  }


  // Record the path
  //
  if (gridmap) free(gridmap);
  gridmap = strdup(val);
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

  if (xrdctxVer){ HTTPS_ALERT("cafile","tlsca",false); }
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
  bool inFile = false;

  // Get the path
  //
  val = Config.GetWord();
  if (!val || !val[0]) {
    eDest.Emsg("Config", "Shared secret key not specified");
    return 1;
  }


  // If the token starts with a slash, then we interpret it as
  // the path to a file that contains the secretkey
  // otherwise, the token itself is the secretkey
  if (val[0] == '/') {
    struct stat st;
    inFile = true;
    if ( stat(val, &st) ) {
      eDest.Emsg("Config", errno, "stat shared secret key file", val);
      return 1;
    }

    if ( st.st_mode & S_IWOTH & S_IWGRP & S_IROTH) {
      eDest.Emsg("Config", "For your own security, the shared secret key file cannot be world readable or group writable'", val, "'");
      return 1;
    }

    FILE *fp = fopen(val,"r");

    if( fp == NULL ) {
      eDest.Emsg("Config", errno, "open shared secret key file", val);
      return 1;
    }

    char line[1024];
    while( fgets(line, 1024, fp) ) {
      char *pp;

      // Trim the end
      pp = line + strlen(line) - 1;
      while ( (pp >= line) && (!isalnum(*pp)) ) {
        *pp = '\0';
        pp--;
      }

      // Trim the beginning
      pp = line;
      while ( *pp && !isalnum(*pp) ) pp++;

      if ( strlen(pp) >= 32 ) {
        eDest.Say("Config", "Secret key loaded.");
        // Record the path
        if (secretkey) free(secretkey);
        secretkey = strdup(pp);

        fclose(fp);
        return 0;
      }

    }

    fclose(fp);
    eDest.Emsg("Config", "Cannot find useful secretkey in file '", val, "'");
    return 1;

  }

  if ( strlen(val) < 32 ) {
    eDest.Emsg("Config", "Secret key is too short");
    return 1;
  }

  // Record the path
  if (secretkey) free(secretkey);
  secretkey = strdup(val);
  if (!inFile) Config.noEcho();

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
/*                                 x r e d i r s t a t i c                    */
/******************************************************************************/

/* Function: xstaticredir

   Purpose:  To parse the directive: staticredir <Url>

             <Url>    http/https server to redirect to in the case of /static

  Output: 0 upon success or !0 upon failure.
 */

int XrdHttpProtocol::xstaticredir(XrdOucStream & Config) {
  char *val;

  // Get the path
  //
  val = Config.GetWord();
  if (!val || !val[0]) {
    eDest.Emsg("Config", "staticredir url not specified");
    return 1;
  }

  // Record the value
  //
  if (staticredir) free(staticredir);
  staticredir = strdup(val);

  return 0;
}

/******************************************************************************/
/*                             x p r e l o a d s t a t i c                    */
/******************************************************************************/

/* Function: xpreloadstatic

   Purpose:  To parse the directive: preloadstatic <http url path> <local file>

             <http url path>    http/http path whose response we are preloading
                                e.g. /static/mycss.css
                                NOTE: this must start with /static


  Output: 0 upon success or !0 upon failure.
 */

int XrdHttpProtocol::xstaticpreload(XrdOucStream & Config) {
  char *val, *k, key[1024];

  // Get the key
  //
  k = Config.GetWord();
  if (!k || !k[0]) {
    eDest.Emsg("Config", "preloadstatic urlpath not specified");
    return 1;
  }

  strcpy(key, k);

  // Get the val
  //
  val = Config.GetWord();
  if (!val || !val[0]) {
    eDest.Emsg("Config", "preloadstatic filename not specified");
    return 1;
  }

  // Try to load the file into memory
  int fp = open(val, O_RDONLY);
  if( fp < 0 ) {
    eDest.Emsg("Config", errno, "open preloadstatic filename", val);
    return 1;
  }

  StaticPreloadInfo *nfo = new StaticPreloadInfo;
  // Max 64Kb ok?
  nfo->data = (char *)malloc(65536);
  nfo->len = read(fp, (void *)nfo->data, 65536);
  close(fp);

  if (nfo->len <= 0) {
      eDest.Emsg("Config", errno, "read from preloadstatic filename", val);
      return 1;
  }

  if (nfo->len >= 65536) {
      eDest.Emsg("Config", "Truncated preloadstatic filename. Max is 64 KB '", val, "'");
      return 1;
  }

  // Record the value
  //
  if (!staticpreload)
    staticpreload = new XrdOucHash<StaticPreloadInfo>;

  staticpreload->Rep((const char *)key, nfo);
  return 0;
}

/******************************************************************************/
/*                          x s e l f h t t p s 2 h t t p                     */
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

   Purpose:  To parse the directive: secxtractor [required] <path> <params>

     required    optional parameter which if present treats any secxtractor
                 errors as fatal.
     <path>      the path of the plugin to be loaded
     <params>    parameters passed to the secxtractor library

  Output: 0 upon success or !0 upon failure.
 */

int XrdHttpProtocol::xsecxtractor(XrdOucStream& Config) {
  char *val;

  // Get the path
  //
  val = Config.GetWord();
  if (!val || !val[0]) {
    eDest.Emsg("Config", "No security extractor plugin specified.");
    return 1;
  } else {
    // Handle optional parameter [required]
    //
    if (!strncmp(val, "required", 8)) {
      isRequiredXtractor = true;
      val = Config.GetWord();

      if (!val || !val[0]) {
        eDest.Emsg("Config", "No security extractor plugin after [required] "
                   "parameter");
        return 1;
      }
    }

    char libName[4096];
    strlcpy(libName, val, sizeof(libName));
    libName[sizeof(libName) - 1] = '\0';
    char libParms[4096];

    if (!Config.GetRest(libParms, 4095)) {
      eDest.Emsg("Config", "secxtractor config params longer than 4k");
      return 1;
    }

    // Try to load the plugin (if available) that extracts info from the
    // user cert/proxy
    if (LoadSecXtractor(&eDest, libName, libParms)) {
      return 1;
    }
  }

  return 0;
}

/******************************************************************************/
/*                            x e x t h a n d l e r                           */
/******************************************************************************/

/* Function: xexthandler
 * 
 *   Purpose:  To parse the directive: exthandler <name> <path> <initparm>
 * 
 *             <name>      a unique name (max 16chars) to be given to this
 *                         instance, e.g 'myhandler1'
 *             <path>      the path of the plugin to be loaded
 *             <initparm>  a string parameter (e.g. a config file) that is
 *                         passed to the initialization of the plugin
 * 
 *  Output: 0 upon success or !0 upon failure.
 */

int XrdHttpProtocol::xexthandler(XrdOucStream &Config,
                                 std::vector<extHInfo> &hiVec) {
  char *val, path[1024], namebuf[1024];
  char *parm;
  
  // Get the name
  //
  val = Config.GetWord();
  if (!val || !val[0]) {
    eDest.Emsg("Config", "No instance name specified for an http external handler plugin.");
    return 1;
  }
  if (strlen(val) >= 16) {
    eDest.Emsg("Config", "Instance name too long for an http external handler plugin.");
    return 1;
  }
  strncpy(namebuf, val, sizeof(namebuf));
  namebuf[ sizeof(namebuf)-1 ] = '\0';
  
  // Get the path
  //
  val = Config.GetWord();
  if (!val || !val[0]) {
    eDest.Emsg("Config", "No http external handler plugin specified.");
    return 1;
  } 
  if (strlen(val) >= (int)sizeof(path)) {
    eDest.Emsg("Config", "Path too long for an http external handler plugin.");
    return 1;
  }

  strcpy(path, val);
  
  // Everything else is a free string
  //
  parm = Config.GetWord();

  // Verify whether this is a duplicate (we never supported replacements)
  //
  for (int i = 0; i < (int)hiVec.size(); i++)
      {if (hiVec[i].extHName == namebuf) {
         eDest.Emsg("Config", "Instance name already present for "
                              "http external handler plugin",
                               hiVec[i].extHPath.c_str());
         return 1;
      }
  }

  // Verify that we don't have more already than we are allowed to have
  //
  if (hiVec.size() >= MAX_XRDHTTPEXTHANDLERS) {
    eDest.Emsg("Config", "Cannot load one more exthandler. Max is 4");
    return 1;
  }

  // Create an info struct and push it on the list of ext handlers to load
  //
  hiVec.push_back(extHInfo(namebuf, path, (parm ? parm : "")));

  return 0;
}

/******************************************************************************/
/*                           x h e a d e r 2 c g i                            */
/******************************************************************************/
  
/* Function: xheader2cgi
 * 
 *   Purpose:  To parse the directive: header2cgi <headerkey> <cgikey>
 * 
 *             <headerkey>   the name of an incoming HTTP header
 *                           to be transformed
 *             <cgikey>      the name to be given when adding it to the cgi info
 *                           that is kept only internally
 * 
 *  Output: 0 upon success or !0 upon failure.
 */

int XrdHttpProtocol::xheader2cgi(XrdOucStream & Config) {
  char *val, keybuf[1024], parmbuf[1024];
  char *parm;
  
  // Get the path
  //
  val = Config.GetWord();
  if (!val || !val[0]) {
    eDest.Emsg("Config", "No headerkey specified.");
    return 1;
  } else {
    
    // Trim the beginning, in place
    while ( *val && !isalnum(*val) ) val++;
    strcpy(keybuf, val);
    
    // Trim the end, in place
    char *pp;
    pp = keybuf + strlen(keybuf) - 1;
    while ( (pp >= keybuf) && (!isalnum(*pp)) ) {
      *pp = '\0';
      pp--;
    }
    
    parm = Config.GetWord();
    
    // Trim the beginning, in place
    while ( *parm && !isalnum(*parm) ) parm++;
    strcpy(parmbuf, parm);
    
    // Trim the end, in place
    pp = parmbuf + strlen(parmbuf) - 1;
    while ( (pp >= parmbuf) && (!isalnum(*pp)) ) {
      *pp = '\0';
      pp--;
    }
    
    // Add this mapping to the map that will be used
    try {
      hdr2cgimap[keybuf] = parmbuf;
    } catch ( ... ) {
      eDest.Emsg("Config", "Can't insert new header2cgi rule. key: '", keybuf, "'");
      return 1;
    }
    
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

  if (xrdctxVer){ HTTPS_ALERT("cadir","tlsca",false); }
  return 0;
}

/******************************************************************************/
/*                      x s s l c i p h e r f i l t e r                       */
/******************************************************************************/

/* Function: xsslcipherfilter

   Purpose:  To parse the directive: cipherfilter <filter>

             <filter>    the filter string to be used when generating
                         the SSL cipher list

   Output: 0 upon success or !0 upon failure.
 */

int XrdHttpProtocol::xsslcipherfilter(XrdOucStream & Config) {
  char *val;

  // Get the filter string
  //
  val = Config.GetWord();
  if (!val || !val[0]) {
    eDest.Emsg("Config", "SSL cipherlist filter string not specified");
    return 1;
  }

  // Record the filter string
  //
  if (sslcipherfilter) free(sslcipherfilter);
  sslcipherfilter = strdup(val);

  return 0;
}

/******************************************************************************/
/*                             x t l s r e u s e                              */
/******************************************************************************/

/* Function: xtlsreuse

   Purpose:  To parse the directive: tlsreuse {on | off}

   Output: 0 upon success or 1 upon failure.
 */

int XrdHttpProtocol::xtlsreuse(XrdOucStream & Config) {

  char *val;

// Get the argument
//
   val = Config.GetWord();
   if (!val || !val[0])
      {eDest.Emsg("Config", "tlsreuse argument not specified"); return 1;}

// If it's off, we set it off
//
   if (!strcmp(val, "off"))
      {tlsCache = XrdTlsContext::scOff;
       return 0;
      }

// If it's on we set it on.
//
   if (!strcmp(val, "on"))
      {tlsCache = XrdTlsContext::scSrvr;
       return 0;
      }

// Bad argument
//
   eDest.Emsg("config", "invalid tlsreuse parameter -", val);
   return 1;
}
  
/******************************************************************************/
/*                                x t r a c e                                 */
/******************************************************************************/

/* Function: xtrace

   Purpose:  To parse the directive: trace <events>

             <events> the blank separated list of events to trace. Trace
                      directives are cumulative.

   Output: 0 upon success or 1 upon failure.
 */

int XrdHttpProtocol::xtrace(XrdOucStream & Config) {

  char *val;

  static struct traceopts {
    const char *opname;
    int opval;
  } tropts[] = {
    {"all", TRACE_ALL},
    {"auth", TRACE_AUTH},
    {"debug", TRACE_DEBUG},
    {"mem", TRACE_MEM},
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
  XrdHttpTrace.What = trval;
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

  if (!Bridge) return -1;
  b = Bridge->Run((char *) &CurrentReq.xrdreq, fname, l);
  if (!b) {
    return -1;
  }


  return 0;
}

/******************************************************************************/
/*                              d o C h k s u m                               */
/******************************************************************************/
  
int XrdHttpProtocol::doChksum(const XrdOucString &fname) {
  size_t length;
  memset(&CurrentReq.xrdreq, 0, sizeof (ClientRequest));
  CurrentReq.xrdreq.query.requestid = htons(kXR_query);
  CurrentReq.xrdreq.query.infotype = htons(kXR_Qcksum);
  memset(CurrentReq.xrdreq.query.reserved1, '\0', sizeof(CurrentReq.xrdreq.query.reserved1));
  memset(CurrentReq.xrdreq.query.fhandle, '\0', sizeof(CurrentReq.xrdreq.query.fhandle));
  memset(CurrentReq.xrdreq.query.reserved2, '\0', sizeof(CurrentReq.xrdreq.query.reserved2));
  length = fname.length() + 1;
  CurrentReq.xrdreq.query.dlen = htonl(length);

  if (!Bridge) return -1;

  return Bridge->Run(reinterpret_cast<char *>(&CurrentReq.xrdreq), const_cast<char *>(fname.c_str()), length) ? 0 : -1;
}


static XrdVERSIONINFODEF(compiledVer, XrdHttpProtocolTest, XrdVNUMBER, XrdVERSION);

// Loads the SecXtractor plugin, if available
int XrdHttpProtocol::LoadSecXtractor(XrdSysError *myeDest, const char *libName,
                                     const char *libParms) {
  
  
  // We don't want to load it more than once
  if (secxtractor) return 1;
  
    XrdOucPinLoader myLib(myeDest, &compiledVer, "secxtractorlib", libName);
    XrdHttpSecXtractor *(*ep)(XrdHttpSecXtractorArgs);

    // Get the entry point of the object creator
    //
    ep = (XrdHttpSecXtractor *(*)(XrdHttpSecXtractorArgs))(myLib.Resolve("XrdHttpGetSecXtractor"));
    if (ep && (secxtractor = ep(myeDest, NULL, libParms))) return 0;
    myLib.Unload();
    return 1;
}

/******************************************************************************/
/*                        L o a d E x t H a n d l e r                         */
/******************************************************************************/

int XrdHttpProtocol::LoadExtHandler(std::vector<extHInfo> &hiVec,
                                    const char *cFN, XrdOucEnv &myEnv) {

  // Add the pointer to the cadir and the cakey to the environment.
  //
  if (sslcadir) myEnv.Put("http.cadir", sslcadir);
  if (sslcafile) myEnv.Put("http.cafile", sslcafile);
  if (sslcert)  myEnv.Put("http.cert",  sslcert);
  if (sslkey)   myEnv.Put("http.key"  , sslkey);

  // Load all of the specified external handlers.
  //
  for (int i = 0; i < (int)hiVec.size(); i++)
      if (LoadExtHandler(&eDest, hiVec[i].extHPath.c_str(), cFN,
                         hiVec[i].extHParm.c_str(), &myEnv,
                         hiVec[i].extHName.c_str())) return 1;

  return 0;
}
  
// Loads the external handler plugin, if available
int XrdHttpProtocol::LoadExtHandler(XrdSysError *myeDest, const char *libName,
                                    const char *configFN, const char *libParms,
                                    XrdOucEnv *myEnv, const char *instName) {
  
  
  // This function will avoid loading doubles. No idea why this happens
  if (ExtHandlerLoaded(instName)) {
    eDest.Emsg("Config", "Instance name already present for an http external handler plugin.");
    return 1;
  }
  if (exthandlercnt >= MAX_XRDHTTPEXTHANDLERS) {
    eDest.Emsg("Config", "Cannot load one more exthandler. Max is 4");
    return 1;
  }
  
  XrdOucPinLoader myLib(myeDest, &compiledVer, "exthandlerlib", libName);
  XrdHttpExtHandler *(*ep)(XrdHttpExtHandlerArgs);
  
  // Get the entry point of the object creator
  //
  ep = (XrdHttpExtHandler *(*)(XrdHttpExtHandlerArgs))(myLib.Resolve("XrdHttpGetExtHandler"));

  XrdHttpExtHandler *newhandler;
  if (ep && (newhandler = ep(myeDest, configFN, libParms, myEnv))) {
    
    // Handler has been loaded, it's the last one in the list
    strncpy( exthandler[exthandlercnt].name,  instName, 16 );
    exthandler[exthandlercnt].name[15] = '\0';
    exthandler[exthandlercnt++].ptr = newhandler;
    
    return 0;
  }

  myLib.Unload();
  return 1;
}



// Tells if we have already loaded a certain exthandler. Try to
// privilege speed, as this func may be invoked pretty often
bool XrdHttpProtocol::ExtHandlerLoaded(const char *handlername) {
  for (int i = 0; i < exthandlercnt; i++) {
    if ( !strncmp(exthandler[i].name, handlername, 15) ) {
      return true;
    }
  }
  return false;
}

// Locates a matching external handler for a given request, if available. Try to
// privilege speed, as this func is invoked for every incoming request
XrdHttpExtHandler * XrdHttpProtocol::FindMatchingExtHandler(const XrdHttpReq &req) {
  
  for (int i = 0; i < exthandlercnt; i++) {
    if (exthandler[i].ptr->MatchesPath(req.requestverb.c_str(), req.resource.c_str())) {
      return exthandler[i].ptr;
    }
  }
  return NULL;
}
