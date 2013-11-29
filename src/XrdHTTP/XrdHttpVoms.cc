//------------------------------------------------------------------------------
// This file is part of XrdHTTP: A pragmatic implementation of the
// HTTP/WebDAV protocol for the Xrootd framework
//
// Copyright (c) 2013 by European Organization for Nuclear Research (CERN)
// Author: Fabrizio Furano <furano@cern.ch>
// File Date: Nov 2013
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





/** @file  XrdHttpVoms.cc
 * @brief  Security extractor plugin implementation, based on the voms libraries
 * @author Fabrizio Furano
 * @date   November 2013
 *
 *
 *
 */


#include <errno.h>

#include <voms/vomsssl.h>
#include <voms/voms_api.h>

#include "XrdSys/XrdSysError.hh"
#include "XrdHttpSecXtractor.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "Xrd/XrdLink.hh"



extern "C" {
  extern int proxy_app_verify_callback(X509_STORE_CTX *ctx, void *empty);
}

class XrdHttpVOMS : public XrdHttpSecXtractor
{
public:

    // Extract security info from the link instaaance, and use it to populate
    // the given XrdSec instance
    virtual int GetSecData(XrdLink *, XrdSecEntity &, SSL *);

    // Initializes an ssl ctx
    virtual int Init(SSL_CTX *, int);

    XrdHttpVOMS(XrdSysError *);

private:


    XrdSysError *eDest;

};

/******************************************************************************/
/*                        I m p l e m e n t a t i o n                         */
/******************************************************************************/

#define TRACELINK lp


// Trace flags
//
#define TRACE_ALL       0x0fff
#define TRACE_DEBUG     0x0001
#define TRACE_EMSG      0x0002
#define TRACE_FS        0x0004
#define TRACE_LOGIN     0x0008
#define TRACE_MEM       0x0010
#define TRACE_REQ       0x0020
#define TRACE_REDIR     0x0040
#define TRACE_RSP       0x0080
#define TRACE_SCHED     0x0100
#define TRACE_STALL     0x0200

XrdOucTrace *XrdVomsTrace;
const char *XrdVomsTraceID;

#define TRACEI(act, x) \
   if (XrdVomsTrace->What & TRACE_ ## act) \
      {XrdVomsTrace->Beg(XrdVomsTraceID,TRACELINK->ID); cerr <<x; XrdVomsTrace->End();}


/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdHttpVOMS::XrdHttpVOMS(XrdSysError *erp): XrdHttpSecXtractor()
{
    //eDest = erp;
    //eDest->logger(erp->logger());
    XrdVomsTrace = new XrdOucTrace(eDest);
    XrdVomsTrace->What = TRACE_ALL;
}

int XrdHttpVOMS::GetSecData(XrdLink *lp, XrdSecEntity &sec, SSL *ssl) {
    vomsdata vmd;
    voms vm;
    std::vector<std::string> fqans;

    X509 *peer_cert;
    STACK_OF(X509) *peer_chain;


    peer_cert = SSL_get_peer_certificate(ssl);

    TRACEI(DEBUG, " SSL_get_peer_certificate returned :" << peer_cert);
    if (peer_cert && peer_cert->name) {

      TRACEI(DEBUG, " Setting Username :" << peer_cert->name);
      lp->setID(peer_cert->name, 0);

      // Here we should fill the SecEntity instance with the DN and the voms stuff
      sec.name = strdup((char *) peer_cert->name);

    }

    peer_chain = SSL_get_peer_cert_chain(ssl);
    TRACEI(DEBUG, " SSL_get_peer_cert_chain :" << peer_chain);


    if (peer_chain && peer_cert) {
      if (vmd.Retrieve(peer_cert, peer_chain, RECURSE_CHAIN)) {
        /* Se ritorna true allora la verifica della firma e' andata a buon
           fine  */

        /*  Questo ti da la lista di tutti gli fqan della VO primaria
            (la prima in voms-proxy-init --voms <vo> per intenderci. */


        if (vmd.DefaultData(vm)) {
          fqans = vm.fqan;
          sec.vorg = strdup(vm.voname.c_str());
          for (unsigned int i = 0; i < fqans.size(); i++) {
            TRACEI(DEBUG, " fqan :" << fqans[i]);
          }
          sec.role = strdup(fqans[0].c_str());
          TRACEI(DEBUG, " Setting VO: " << sec.vorg << " roles :" << sec.role);

        }


        /*  Per accedere invece a tutte le VO (se ci sono state piu' VO)
            usa: */
        //
        //        std::vector<std::string> fqans;
        //        for (std::vector<voms>::iterator i = vmd.data().begin(); i !=
        //                vmd.data().end(); i++) {
        //            fqans.insert(i->fqan.begin(), i->fqan.end());
        //        }
      } else
        TRACEI(DEBUG, " voms info retrieval failed: " << vmd.ErrorMessage());
    }

    if (peer_cert) X509_free(peer_cert);

    //if (peer_chain) sk_X509_pop_free(peer_chain, X509_free);
    peer_chain = 0;

    return 0;
}

int XrdHttpVOMS::Init(SSL_CTX *sslctx, int mydebug) {
    SSL_CTX_set_cert_verify_callback(sslctx, proxy_app_verify_callback, 0);

    XrdVomsTrace->What = mydebug;


    return 0;
}

/******************************************************************************/
/*                    X r d H t t p G e t S e c X t r a c t o r               */
/******************************************************************************/

XrdHttpSecXtractor *XrdHttpGetSecXtractor(XrdHttpSecXtractorArgs)
{
    return (XrdHttpSecXtractor *)new XrdHttpVOMS(eDest);
}
