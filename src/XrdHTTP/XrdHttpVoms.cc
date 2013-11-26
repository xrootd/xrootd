

// This file implements an instance of the XrdOucName2Name abstract class.

#include <errno.h>

#include <voms/vomsssl.h>
#include <voms/voms_api.h>

#include "XrdSys/XrdSysError.hh"
#include "XrdHttpSecXtractor.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdHttpTrace.hh"
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
    virtual int InitSSLCtx(SSL_CTX *);

    XrdHttpVOMS(XrdSysError *);

private:


    XrdSysError *eDest;

};

/******************************************************************************/
/*                        I m p l e m e n t a t i o n                         */
/******************************************************************************/

#define TRACELINK lp

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdHttpVOMS::XrdHttpVOMS(XrdSysError *erp): XrdHttpSecXtractor()
{
    //eDest = erp;
    //eDest->logger(erp->logger());
    XrdHttpTrace = new XrdOucTrace(eDest);
    XrdHttpTrace->What = TRACE_ALL;
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
          for (unsigned int i = 0; i < fqans.size(); i++) {
            TRACEI(DEBUG, " fqan :" << fqans[i]);
          }
          sec.vorg = strdup(fqans[0].c_str());
          TRACEI(DEBUG, " Setting main vorg :" << sec.vorg);

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
        TRACE(DEBUG, " voms info retrieval failed: " << vmd.ErrorMessage());
    }

    if (peer_cert) X509_free(peer_cert);

    //if (peer_chain) sk_X509_pop_free(peer_chain, X509_free);
    peer_chain = 0;

    return 0;
}

int XrdHttpVOMS::InitSSLCtx(SSL_CTX *sslctx) {
    SSL_CTX_set_cert_verify_callback(sslctx, proxy_app_verify_callback, 0);
    return 0;
}

/******************************************************************************/
/*                    X r d H t t p G e t S e c X t r a c t o r               */
/******************************************************************************/

XrdHttpSecXtractor *XrdHttpGetSecXtractor(XrdHttpSecXtractorArgs)
{
    return (XrdHttpSecXtractor *)new XrdHttpVOMS(eDest);
}
