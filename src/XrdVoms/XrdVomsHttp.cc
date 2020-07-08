/******************************************************************************/
/*                                                                            */
/*                        X r d V o m s H t t p . c c                         */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC02-76-SFO0515 with the Deprtment of Energy             */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

// This code is based on

#include "XrdVomsFun.hh"

/** @brief This code is based on the basic architecture shown in
 *
 *  @file   XrdHttpVoms.cc
 *  @author Fabrizio Furano
 *  @date   November 2013
 */

/******************************************************************************/
/*                              I n c l u d e s                               */
/******************************************************************************/
  
#include "XrdVersion.hh" 
#include "XrdHttp/XrdHttpSecXtractor.hh"
#include "XrdSec/XrdSecInterface.hh"

#include "XrdVoms.hh"

/******************************************************************************/
/*               C l a s s   X r d X r o o t d V o m s H t t p                */
/******************************************************************************/
  
class XrdVomsHttp : public XrdHttpSecXtractor
{
public:

    // Extract security info from the link instance, and use it to populate
    // the given XrdSec instance
    //
    virtual int GetSecData(XrdLink *, XrdSecEntity &, SSL *);

    // Initializes an ssl ctx
    //
    virtual int Init(SSL_CTX *, int) {return 0;}

    
    virtual int InitSSL(SSL *ssl, char *cadir) {return 0;}
    virtual int FreeSSL(SSL *) {return 0;}
    
    XrdVomsHttp(XrdSysError *erp, XrdVomsFun &vFun)
               : vomsFun(vFun), eDest(erp) {};

private:

    XrdVomsFun  &vomsFun;
    XrdSysError *eDest;
};

/******************************************************************************/
/*                            G e t S e c D a t a                             */
/******************************************************************************/
  
int XrdVomsHttp::GetSecData(XrdLink *lp, XrdSecEntity &sec, SSL *ssl)
{
   Voms_x509_in_t xCerts;
   int rc;

// Make sure the certs have been verified. Note that HTTP doesn't do well if
// we return failure. So, we always return success as there will be no entity.
//
//
   if (SSL_get_verify_result(ssl) != X509_V_OK) return 0;

// Get the certs
//
   xCerts.cert  = SSL_get_peer_certificate(ssl);
   if (!xCerts.cert) return 0;
   xCerts.chain = SSL_get_peer_cert_chain(ssl);

// The API calls for the cert member in the SecEntity point to the certs
//
   sec.creds = (char *)&xCerts;

// Do the voms tango now and upon success pretend we are "gsi" authentication
//
   if (!(rc = vomsFun.VOMSFun(sec))) strcpy(sec.prot, "gsi");

// Free the x509 cert the chain will stick arround until the session is freed
//
   X509_free(xCerts.cert);

// All done
//
   sec.creds = 0;
   return rc;
}

/******************************************************************************/
/*                 X r d H t t p G e t S e c X t r a c t o r                  */
/******************************************************************************/
  
XrdHttpSecXtractor *XrdHttpGetSecXtractor(XrdHttpSecXtractorArgs)
{

// First step it get a new VomsFun object
//
   XrdVomsFun *vomsFun = new XrdVomsFun(*eDest);

// Initialize it using the parameters supplied
//
   if (vomsFun->VOMSInit(parms) < 0)
      {delete vomsFun;
       return 0;
      }

// We will always use a stack of x509 certs, make sure that is what the
// voms fund will actually think it wants.
//
   vomsFun->SetCertFmt(XrdVomsFun::gCertX509);

// Now return the interface object
//
   return (XrdHttpSecXtractor *)new XrdVomsHttp(eDest, *vomsFun);
}

/******************************************************************************/
/*                   V e r s i o n   I n f o r m a t i o n                    */
/******************************************************************************/
  
// This is the macro that declares the xrootd version this plugin uses.
// We only need to pass the name of the hook function and a name for logging.
// The version numbers actually are taken automatically at compile time.
//
XrdVERSIONINFO(XrdHttpGetSecXtractor,XrdVomsHttp)

