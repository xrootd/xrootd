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

#include "XrdHttpProtocol.hh"
#include "XrdHttpTrace.hh"
#include "XrdHttpSecXtractor.hh"
#include "Xrd/XrdLink.hh"
#include "XrdCrypto/XrdCryptoX509Chain.hh"
#include "XrdCrypto/XrdCryptosslAux.hh"
#include "XrdCrypto/XrdCryptoFactory.hh"
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

/******************************************************************************/
/*                          I n i t S e c u r i t y                           */
/******************************************************************************/

bool XrdHttpProtocol::InitSecurity() {
#ifdef HAVE_XRDCRYPTO
  // Borrow the initialization of XrdCryptossl, in order to share the
  // OpenSSL threading bits
  if (!(myCryptoFactory = XrdCryptoFactory::GetCryptoFactory("ssl"))) {
    eDest.Say("Error instantiating crypto factory ssl", "");
    return false;
  }
#endif

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

// All done
//
   return true;
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

  if (!dn) {
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
  TRACEI(DEBUG, " Subject name is : '" << SecEntity.moninfo << "'");
  // X509Chain doesn't assume it owns the underlying certs unless
  // you explicitly invoke the Cleanup method
  chain.Cleanup();

  if (GetVOMSData(lp)) {
    TRACEI(DEBUG, " No VOMS information for DN: " << SecEntity.moninfo);

    if (isRequiredXtractor) {
      eDest.Emsg(epname, "Failed extracting required VOMS info for DN: ",
                 SecEntity.moninfo);
      return 1;
    }
  }

  return HandleGridMap(lp);
}


/******************************************************************************/
/*                          H a n d l e G r i d M a p                         */
/******************************************************************************/

int
XrdHttpProtocol::HandleGridMap(XrdLink* lp)
{
  EPNAME("HandleGridMap");
  char bufname[256];

  if (servGMap) {
    int mape = servGMap->dn2user(SecEntity.moninfo, bufname, sizeof(bufname), 0);
    if ( !mape && SecEntity.moninfo[0] ) {
      TRACEI(DEBUG, " Mapping name: '" << SecEntity.moninfo << "' --> " << bufname);
      if (SecEntity.name) free(SecEntity.name);
      SecEntity.name = strdup(bufname);
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
        strcpy(bufname2, "unknown-\0"); // note it's 9 chars
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
