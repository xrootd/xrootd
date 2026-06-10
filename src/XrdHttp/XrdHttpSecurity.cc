//------------------------------------------------------------------------------
// This file is part of XrdHTTP: A pragmatic implementation of the
// HTTP/WebDAV protocol for the Xrootd framework
//
// Copyright (c) 2020 by European Organization for Nuclear Research (CERN)
// Author: Fabrizio Furano <furano@cern.ch>
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

#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <openssl/evp.h>

#include "XrdHttpProtocol.hh"
#include "XrdHttpTrace.hh"
#include "XrdHttpSecXtractor.hh"
#include "XrdSec/XrdSecLoadSecurity.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "Xrd/XrdLink.hh"
#include "XrdCrypto/XrdCryptoX509Chain.hh"
#include "XrdCrypto/XrdCryptosslAux.hh"
#include "XrdCrypto/XrdCryptoFactory.hh"
#include "XrdSec/XrdSecEntityAttr.hh"
#include "XrdTls/XrdTlsPeerCerts.hh"
#include "XrdTls/XrdTlsContext.hh"
#include "XrdOuc/XrdOucGMap.hh"

namespace XrdHttpProtoInfo
{
  extern XrdTlsContext *xrdctx;
}

XrdOucGMap *XrdHttpProtocol::servGMap = 0;  // Grid mapping service
XrdCryptoFactory *XrdHttpProtocol::myCryptoFactory = 0;

// Static definitions
#define TRACELINK lp

namespace
{
const char *TraceID = "Security";
}

using namespace XrdHttpProtoInfo;

namespace
{

std::string bearerTokenKey(const char *tok, int tlen)
{
   unsigned char md[EVP_MAX_MD_SIZE];
   unsigned int mdLen = 0;
   if (!tok || tlen <= 0
   ||  !EVP_Digest(tok, static_cast<size_t>(tlen), md, &mdLen, EVP_sha256(), nullptr))
      return {};
   char hex[EVP_MAX_MD_SIZE * 2 + 1];
   for (unsigned int i = 0; i < mdLen; ++i)
      snprintf(hex + i * 2, sizeof(hex) - i * 2, "%02x", md[i]);
   return std::string(hex, mdLen * 2);
}

bool hasPrefix(const char *s, const char *end, const char *pfx)
{
   while (*pfx && s < end && *s == *pfx) {++s; ++pfx;}
   return !*pfx;
}

const char *stripBearerToken(const char *bTok, int &sz)
{
   const char *sTok = bTok;
   sz = 0;
   if (!sTok) return nullptr;

   const char *endPtr = sTok + strlen(sTok);
   while (sTok < endPtr && isspace(static_cast<unsigned char>(*sTok))) ++sTok;
   if (sTok >= endPtr) return nullptr;

   if ((endPtr - sTok) >= 9 && hasPrefix(sTok, endPtr, "Bearer%20")) sTok += 9;
   else if ((endPtr - sTok) >= 7 && hasPrefix(sTok, endPtr, "Bearer ")) sTok += 7;

   while (sTok < endPtr && isspace(static_cast<unsigned char>(*sTok))) ++sTok;
   if (sTok >= endPtr) return nullptr;

   while (endPtr > sTok && isspace(static_cast<unsigned char>(*(endPtr - 1)))) --endPtr;
   sz = static_cast<int>(endPtr - sTok);
   return (sz > 0 ? sTok : nullptr);
}

void copyEntityAttrs(XrdSecEntity &dst, const XrdSecEntity &src)
{
   for (const auto &key : src.eaAPI->Keys())
      {std::string val;
       if (src.eaAPI->Get(key, val)) dst.eaAPI->Add(key, val, true);
      }
}

bool extractBearerToken(const XrdHttpReq &req, std::string &token)
{
   const std::string &cgi = req.hdr2cgistr;
   const char *key = "authz=";
   size_t pos = cgi.find(key);
   if (pos != std::string::npos)
      {size_t start = pos + strlen(key);
       size_t end = cgi.find('&', start);
       token = (end == std::string::npos ? cgi.substr(start)
                                         : cgi.substr(start, end - start));
       return !token.empty();
      }

   for (const auto &hdr : req.allheaders)
      {if (!strcasecmp(hdr.first.c_str(), "authorization"))
          {token = hdr.second;
           return !token.empty();
          }
      }
   return false;
}

} // namespace

/******************************************************************************/
/*                          I n i t S e c u r i t y                           */
/******************************************************************************/

bool XrdHttpProtocol::InitSecurity() {
  // Borrow the initialization of XrdCryptossl, in order to share the
  // OpenSSL threading bits
  if (!(myCryptoFactory = XrdCryptoFactory::GetCryptoFactory("ssl"))) {
    eDest.Say("Error instantiating crypto factory ssl", "");
    return false;
  }

// If GRID map file was specified, load the plugin for it
//
   if (gridmap) {
     XrdOucString pars;
     if (XrdHttpTrace.What & TRACE_DEBUG) pars += "dbg|";

     if (!(servGMap = XrdOucgetGMap(&eDest, gridmap, pars.c_str()))) {
       eDest.Say("Error loading grid map file:", gridmap);
       return false;
     }
     TRACE(ALL, "using grid map file: "<< gridmap);
   }

// If a secxtractor was specified, load that too.
//
   if (secxtractor)
      {SSL_CTX *sslctx = (SSL_CTX*)xrdctx->Context(); // Need to avoid this!
       secxtractor->Init(sslctx, XrdHttpTrace.What);
      }

// Load the security framework when HTTP bearer OIDC is enabled.
//
   if (oidcHttpMode && !CIA)
      {if (!oidcConfigFN)
          {eDest.Say("Error: http.oidc requires a configuration file path");
           return false;
          }
       if (!(CIA = XrdSecLoadSecService(&eDest, oidcConfigFN)))
          {eDest.Say("Error loading security framework for http.oidc");
           return false;
          }
      }

// All done
//
   return true;
}

/******************************************************************************/
/*               H a n d l e O I D C A u t h e n t i c a t i o n              */
/******************************************************************************/

int
XrdHttpProtocol::HandleOidcAuthentication()
{
#undef  TRACELINK
#define TRACELINK Link

  if (!oidcHttpMode || !CIA) return 0;

  // Client-certificate identity is fixed for the TLS connection.
  if (SecEntity.name && strncmp(SecEntity.prot, "oidc", 4) != 0) return 0;

  std::string bearer;
  if (!extractBearerToken(CurrentReq, bearer))
     {if (oidcHttpMode == 2 && oidcBearerTokKey.empty())
         {TRACEI(REQ, " OIDC bearer token required but not provided.");
          SendSimpleResp(401, nullptr, nullptr, "Authentication required", 0, false);
          return 1;
         }
      return 0;
     }

  int tlen = 0;
  const char *tok = stripBearerToken(bearer.c_str(), tlen);
  if (!tok || tlen <= 0)
     {TRACEI(REQ, " OIDC bearer token malformed.");
      SendSimpleResp(401, nullptr, nullptr, "Authentication failed", 0, false);
      return 1;
     }

  const std::string tokKey = bearerTokenKey(tok, tlen);
  if (tokKey.empty())
     {TRACEI(REQ, " OIDC bearer token fingerprint failed.");
      SendSimpleResp(500, nullptr, nullptr, "Authentication failed", 0, false);
      return 1;
     }

  if (!oidcBearerTokKey.empty() && tokKey == oidcBearerTokKey) return 0;

  const int bsz = 5 + tlen + 1;
  std::vector<char> credBuf(static_cast<size_t>(bsz));
  strcpy(credBuf.data(), "oidc");
  memcpy(credBuf.data() + 5, tok, static_cast<size_t>(tlen));
  credBuf[static_cast<size_t>(5 + tlen)] = '\0';

  XrdSecCredentials cred;
  cred.buffer = credBuf.data();
  cred.size = bsz;

  XrdOucErrInfo eMsg;
  XrdSecProtocol *authProt = CIA->getProtocol(Link->Host(), *(Link->AddrInfo()),
                                            &cred, eMsg);
  if (!authProt)
     {int ec = 0;
      const char *et = eMsg.getErrText(ec);
      TRACEI(REQ, " OIDC protocol unavailable: " << (et && *et ? et : "unknown"));
      SendSimpleResp(401, nullptr, nullptr, "Authentication failed", 0, false);
      return 1;
     }

  XrdSecParameters *parm = nullptr;
  const int rc = authProt->Authenticate(&cred, &parm, &eMsg);
  if (parm) delete parm;

  if (rc != 0 || !CIA->PostProcess(authProt->Entity, eMsg))
     {int ec = 0;
      const char *et = eMsg.getErrText(ec);
      TRACEI(REQ, " OIDC token validation failed: " << (et && *et ? et : "unknown"));
      authProt->Delete();
      SendSimpleResp(401, nullptr, nullptr, "Authentication failed", 0, false);
      return 1;
     }

  if (!oidcBearerTokKey.empty() || Bridge)
     {if (Bridge && !Bridge->Disc())
         {TRACEI(REQ, " OIDC token changed but bridge is busy.");
          authProt->Delete();
          SendSimpleResp(503, nullptr, nullptr, "Authentication busy", 0, false);
          return 1;
         }
      Bridge = nullptr;
      DoingLogin = false;
      DoneSetInfo = false;
      if (!oidcBearerTokKey.empty())
         TRACEI(REQ, " OIDC bearer token changed; re-authenticating.");
     }

  if (SecEntity.name) free(SecEntity.name);
  SecEntity.name = authProt->Entity.name ? strdup(authProt->Entity.name) : nullptr;
  strncpy(SecEntity.prot, authProt->Entity.prot, sizeof(SecEntity.prot));
  copyEntityAttrs(SecEntity, authProt->Entity);
  authProt->Delete();

  oidcBearerTokKey = tokKey;
  TRACEI(REQ, " OIDC authenticated as: " << SecEntity.name);
  return 0;
}

/******************************************************************************/
/*                 H a n d l e A u t h e n t i c a t i o n                    */
/******************************************************************************/

int
XrdHttpProtocol::HandleAuthentication(XrdLink* lp)
{
  EPNAME("HandleAuthentication");
  int rc_ssl = SSL_get_verify_result(ssl);

  if (rc_ssl) { 
    TRACEI(DEBUG, " SSL_get_verify_result returned :" << rc_ssl);
    return 1;
  }

  XrdTlsPeerCerts pc(SSL_get_peer_certificate(ssl),SSL_get_peer_cert_chain(ssl));
  XrdCryptoX509Chain chain;

  if ((!pc.hasCert()) ||
      (myCryptoFactory && !myCryptoFactory->X509ParseStack()(&pc, &chain))) {
    TRACEI(DEBUG, "No certificate found in peer chain.");
    chain.Cleanup();
    return 0;
  }

  // Extract the DN for the current connection that will be used later on when
  // handling the gridmap file
  const char * dn = chain.EECname();
  const char * eechash = chain.EEChash();

  if (!dn || !eechash) {
    // X509Chain doesn't assume it owns the underlying certs unless
    // you explicitly invoke the Cleanup method
    TRACEI(DEBUG, "Failed to extract DN information.");
    chain.Cleanup();
    return 1;
  }

  if (SecEntity.moninfo) {
    free(SecEntity.moninfo);
  }

  SecEntity.moninfo = strdup(dn);
  TRACEI(DEBUG, " Subject name is : '" << SecEntity.moninfo << "'; hash is " << eechash);
  // X509Chain doesn't assume it owns the underlying certs unless
  // you explicitly invoke the Cleanup method

  if (GetVOMSData(lp)) {
    TRACEI(DEBUG, " No VOMS information for DN: " << SecEntity.moninfo);

    if (isRequiredXtractor) {
      eDest.Emsg(epname, "Failed extracting required VOMS info for DN: ",
                 SecEntity.moninfo);
      chain.Cleanup();
      return 1;
    }
  }

  auto retval = HandleGridMap(lp, eechash);
  chain.Cleanup();
  return retval;
}


/******************************************************************************/
/*                          H a n d l e G r i d M a p                         */
/******************************************************************************/

int
XrdHttpProtocol::HandleGridMap(XrdLink* lp, const char * eechash)
{
  EPNAME("HandleGridMap");
  char bufname[256];

  if (servGMap) {
    int mape = servGMap->dn2user(SecEntity.moninfo, bufname, sizeof(bufname), 0);
    if ( !mape && SecEntity.moninfo[0] ) {
      TRACEI(DEBUG, " Mapping name: '" << SecEntity.moninfo << "' --> " << bufname);
      if (SecEntity.name) free(SecEntity.name);
      SecEntity.name = strdup(bufname);
      SecEntity.eaAPI->Add("gridmap.name", "1", true);
    }
    else {
      TRACEI(ALL, " Mapping name: " << SecEntity.moninfo << " Failed. err: " << mape);

      if (isRequiredGridmap) {
        eDest.Emsg(epname, "Required gridmap mapping failed for DN:",
                   SecEntity.moninfo);
        return 1;
      }
    }
  }

  if (!SecEntity.name && !compatNameGeneration) {
    TRACEI(DEBUG, " Will fallback name to subject hash: " << eechash);
    SecEntity.name = strdup(eechash);
    return 0;
  }

  if (!SecEntity.name) {
    // Here we have the user DN, and try to extract an useful user name from it
    if (SecEntity.name) free(SecEntity.name);
    SecEntity.name = 0;
    // To set the name we pick the first CN of the certificate subject
    // and hope that it makes some sense, it usually does
    char *lnpos = strstr(SecEntity.moninfo, "/CN=");
    char bufname2[9];


    if (lnpos) {
      lnpos += 4;
      char *lnpos2 = index(lnpos, '/');
      if (lnpos2) {
        int l = ( lnpos2-lnpos < (int)sizeof(bufname) ? lnpos2-lnpos : (int)sizeof(bufname)-1 );
        strncpy(bufname, lnpos, l);
        bufname[l] = '\0';

        // Here we have the string in the buffer. Take the last 8 non-space characters
        size_t j = 8;
        strcpy(bufname2, "unknown-"); // note it's 8 chars + '\0' at the end
        for (int i = (int)strlen(bufname)-1; i >= 0; i--) {
          if (isalnum(bufname[i])) {
            j--;
            bufname2[j] = bufname[i];
            if (j == 0) break;
          }

        }

        SecEntity.name = strdup(bufname);
        TRACEI(DEBUG, " Setting link name: '" << bufname2+j << "'");
        lp->setID(bufname2+j, 0);
      }
    }
  }

  // If we could not find anything good, take the last 8 non-space characters of the main subject
  if (!SecEntity.name) {
    size_t j = 8;
    SecEntity.name = strdup("unknown-\0"); // note it's 9 chars
    for (int i = (int)strlen(SecEntity.moninfo)-1; i >= 0; i--) {
      if (isalnum(SecEntity.moninfo[i])) {
        j--;
        SecEntity.name[j] = SecEntity.moninfo[i];
        if (j == 0) break;
      }
    }
  }

  return 0;
}


/******************************************************************************/
/*                           G e t V O M S D a t a                            */
/******************************************************************************/

int XrdHttpProtocol::GetVOMSData(XrdLink *lp)
{
  TRACEI(DEBUG, " Extracting auth info.");

  // Invoke the Security exctractor plugin which will fill in the XrdSecEntity
  // with VOMS info, if VOMS is installed. If we have no sec extractor then do
  // nothing, just plain https will work.
  if (secxtractor) {
    // Note: this is kept for compatibility with XrdHttpVOMS which modified the
    // SecEntity.name filed
    char *savestr = 0;

    if (servGMap && SecEntity.name) {
      savestr = strdup(SecEntity.name);
    }

    int r = secxtractor->GetSecData(lp, SecEntity, ssl);

    if (servGMap && savestr) {
      if (SecEntity.name) free(SecEntity.name);
      SecEntity.name = savestr;
    }

    if (r) {
      TRACEI(ALL, " Certificate data extraction failed: " << SecEntity.moninfo
             << " Failed. err: " << r);
    }

    return r;
  }

  return 0;
}
