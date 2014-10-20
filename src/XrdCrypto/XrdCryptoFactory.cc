/******************************************************************************/
/*                                                                            */
/*                  X r d C r y p t o F a c t o r y . c c                     */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Gerri Ganis for CERN                                         */
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

/* ************************************************************************** */
/*                                                                            */
/* Abstract interface for a crypto factory                                    */
/* Allows to plug-in modules based on different crypto implementation         */
/* (OpenSSL, Botan, ...)                                                      */
/*                                                                            */
/* ************************************************************************** */
#include <string.h>
#include <dlfcn.h>

#include "XrdCrypto/XrdCryptoAux.hh"
#include "XrdCrypto/XrdCryptoTrace.hh"
#include "XrdCrypto/XrdCryptoFactory.hh"
#include "XrdCrypto/XrdCryptolocalFactory.hh"

#include "XrdOuc/XrdOucHash.hh"
#include "XrdOuc/XrdOucPinLoader.hh"
#include "XrdSys/XrdSysPlatform.hh"

#include "XrdVersion.hh"
  
//
// For error logging
static XrdSysError eDest(0,"cryptofactory_");

// We have always an instance of the simple RSA implementation
static XrdCryptolocalFactory localCryptoFactory;

//____________________________________________________________________________
XrdCryptoFactory::XrdCryptoFactory(const char *n, int id)
{
   // Constructor (only called by derived classes).

   if (n) {
      int l = strlen(n);
      l = (l > (MAXFACTORYNAMELEN - 1)) ? (MAXFACTORYNAMELEN - 1) : l;  
      strncpy(name,n,l);
      name[l] = 0;  // null terminated
   }
   fID = id;
}

//______________________________________________________________________________
void XrdCryptoFactory::SetTrace(kXR_int32)
{
   // Set flags for tracing

   ABSTRACTMETHOD("XrdCryptoFactory::SetTrace");
}

//______________________________________________________________________________
bool XrdCryptoFactory::operator==(const XrdCryptoFactory factory)
{
   // Compare name of 'factory' to local name: return 1 if matches, 0 if not

   if (!strcmp(factory.Name(),Name()))
      return 1;
   return 0;
}

//______________________________________________________________________________
XrdCryptoKDFunLen_t XrdCryptoFactory::KDFunLen()
{
   // Return an instance of an implementation of a Key Der function length.

   ABSTRACTMETHOD("XrdCryptoFactory::KDFunLen");
   return 0;
}

//______________________________________________________________________________
XrdCryptoKDFun_t XrdCryptoFactory::KDFun()
{
   // Return an instance of an implementation of a Key Derivation function.

   ABSTRACTMETHOD("XrdCryptoFactory::KDFun");
   return 0;
}

//______________________________________________________________________________
bool XrdCryptoFactory::SupportedCipher(const char *)
{
   // Returns true id specified cipher is supported by the implementation

   ABSTRACTMETHOD("XrdCryptoFactory::SupportedCipher");
   return 0;
}

//______________________________________________________________________________
XrdCryptoCipher *XrdCryptoFactory::Cipher(const char *, int)
{
   // Return an instance of an implementation of XrdCryptoCipher.

   ABSTRACTMETHOD("XrdCryptoFactory::Cipher");
   return 0;
}

//______________________________________________________________________________
XrdCryptoCipher *XrdCryptoFactory::Cipher(const char *, int, const char *, 
                                          int, const char *)
{
   // Return an instance of an implementation of XrdCryptoCipher.

   ABSTRACTMETHOD("XrdCryptoFactory::Cipher");
   return 0;
}

//______________________________________________________________________________
XrdCryptoCipher *XrdCryptoFactory::Cipher(XrdSutBucket *)
{
   // Return an instance of an implementation of XrdCryptoCipher.

   ABSTRACTMETHOD("XrdCryptoFactory::Cipher");
   return 0;
}

//______________________________________________________________________________
XrdCryptoCipher *XrdCryptoFactory::Cipher(int, char *, int, const char *)
{
   // Return an instance of an implementation of XrdCryptoCipher.

   ABSTRACTMETHOD("XrdCryptoFactory::Cipher");
   return 0;
}

//______________________________________________________________________________
XrdCryptoCipher *XrdCryptoFactory::Cipher(const XrdCryptoCipher &)
{
   // Return an instance of an implementation of XrdCryptoCipher.

   ABSTRACTMETHOD("XrdCryptoFactory::Cipher");
   return 0;
}

//______________________________________________________________________________
bool XrdCryptoFactory::SupportedMsgDigest(const char *)
{
   // Returns true id specified digest is supported by the implementation

   ABSTRACTMETHOD("XrdCryptoFactory::SupportedMsgDigest");
   return 0;
}

//______________________________________________________________________________
XrdCryptoMsgDigest *XrdCryptoFactory::MsgDigest(const char *)
{
   // Return an instance of an implementation of XrdCryptoMsgDigest.

   ABSTRACTMETHOD("XrdCryptoFactory::MsgDigest");
   return 0;
}

//______________________________________________________________________________
XrdCryptoRSA *XrdCryptoFactory::RSA(int, int)
{
   // Return an instance of an implementation of XrdCryptoRSA.

   ABSTRACTMETHOD("XrdCryptoFactory::RSA");
   return 0;
}

//______________________________________________________________________________
XrdCryptoRSA *XrdCryptoFactory::RSA(const char *, int)
{
   // Return an instance of an implementation of XrdCryptoRSA.

   ABSTRACTMETHOD("XrdCryptoFactory::RSA");
   return 0;

}

//______________________________________________________________________________
XrdCryptoRSA *XrdCryptoFactory::RSA(const XrdCryptoRSA &)
{
   // Return an instance of an implementation of XrdCryptoRSA.

   ABSTRACTMETHOD("XrdCryptoFactory::RSA ("<<this<<")");
   return 0;
}

//______________________________________________________________________________
XrdCryptoX509 *XrdCryptoFactory::X509(const char *, const char *)
{
   // Return an instance of an implementation of XrdCryptoX509.

   ABSTRACTMETHOD("XrdCryptoFactory::X509");
   return 0;
}

//______________________________________________________________________________
XrdCryptoX509 *XrdCryptoFactory::X509(XrdSutBucket *)
{
   // Init XrdCryptoX509 from a bucket

   ABSTRACTMETHOD("XrdCryptoFactory::X509");
   return 0;
}

//______________________________________________________________________________
XrdCryptoX509Crl *XrdCryptoFactory::X509Crl(const char *, int)
{
   // Return an instance of an implementation of XrdCryptoX509Crl.

   ABSTRACTMETHOD("XrdCryptoFactory::X509Crl");
   return 0;
}

//______________________________________________________________________________
XrdCryptoX509Crl *XrdCryptoFactory::X509Crl(XrdCryptoX509 *)
{
   // Return an instance of an implementation of XrdCryptoX509Crl.

   ABSTRACTMETHOD("XrdCryptoFactory::X509Crl");
   return 0;
}

//______________________________________________________________________________
XrdCryptoX509Req *XrdCryptoFactory::X509Req(XrdSutBucket *)
{
   // Return an instance of an implementation of XrdCryptoX509Req.

   ABSTRACTMETHOD("XrdCryptoFactory::X509Req");
   return 0;
}

//______________________________________________________________________________
XrdCryptoX509VerifyCert_t XrdCryptoFactory::X509VerifyCert()
{
   // Return an instance of an implementation of a verification
   // function for X509 certificate.

   ABSTRACTMETHOD("XrdCryptoFactory::X509VerifyCert");
   return 0;
}

//______________________________________________________________________________
XrdCryptoX509VerifyChain_t XrdCryptoFactory::X509VerifyChain()
{
   // Return an instance of an implementation of a verification
   // function for X509 certificate chains.

   ABSTRACTMETHOD("XrdCryptoFactory::X509VerifyChain");
   return 0;
}

//______________________________________________________________________________
XrdCryptoX509ExportChain_t XrdCryptoFactory::X509ExportChain()
{
   // Return an instance of an implementation of a function
   // to export a X509 certificate chain.

   ABSTRACTMETHOD("XrdCryptoFactory::X509ExportChain");
   return 0;
}

//______________________________________________________________________________
XrdCryptoX509ChainToFile_t XrdCryptoFactory::X509ChainToFile()
{
   // Return an instance of an implementation of a function
   // to dump a X509 certificate chain to a file.

   ABSTRACTMETHOD("XrdCryptoFactory::X509ChainToFile");
   return 0;
}

//______________________________________________________________________________
XrdCryptoX509ParseFile_t XrdCryptoFactory::X509ParseFile()
{
   // Return an instance of an implementation of a function
   // to parse a file supposed to contain for X509 certificates.

   ABSTRACTMETHOD("XrdCryptoFactory::X509ParseFile");
   return 0;
}

//______________________________________________________________________________
XrdCryptoX509ParseBucket_t XrdCryptoFactory::X509ParseBucket()
{
   // Return an instance of an implementation of a function
   // to parse a bucket supposed to contain for X509 certificates.

   ABSTRACTMETHOD("XrdCryptoFactory::X509ParseBucket");
   return 0;
}

//______________________________________________________________________________
XrdCryptoProxyCertInfo_t XrdCryptoFactory::ProxyCertInfo()
{
   // Check if the proxyCertInfo extension exists

   ABSTRACTMETHOD("XrdCryptoFactory::ProxyCertInfo");
   return 0;
}

//______________________________________________________________________________
XrdCryptoSetPathLenConstraint_t XrdCryptoFactory::SetPathLenConstraint()
{
   // Set the path length constraint

   ABSTRACTMETHOD("XrdCryptoFactory::SetPathLenConstraint");
   return 0;
}

//______________________________________________________________________________
XrdCryptoX509CreateProxy_t XrdCryptoFactory::X509CreateProxy()
{
   // Create a proxy certificate

   ABSTRACTMETHOD("XrdCryptoFactory::X509CreateProxy");
   return 0;
}

//______________________________________________________________________________
XrdCryptoX509CreateProxyReq_t XrdCryptoFactory::X509CreateProxyReq()
{
   // Create a proxy request

   ABSTRACTMETHOD("XrdCryptoFactory::X509CreateProxyReq");
   return 0;
}

//______________________________________________________________________________
XrdCryptoX509SignProxyReq_t XrdCryptoFactory::X509SignProxyReq()
{
   // Sign a proxy request

   ABSTRACTMETHOD("XrdCryptoFactory::X509SignProxyReq");
   return 0;
}

//______________________________________________________________________________
XrdCryptoX509GetVOMSAttr_t XrdCryptoFactory::X509GetVOMSAttr()
{
   // Get VOMS attributes, if any

   ABSTRACTMETHOD("XrdCryptoFactory::X509GetVOMSAttr");
   return 0;
}


/* ************************************************************************** */
/*                                                                            */
/*                    G e t C r y p t o F a c t o r y                         */
/*                                                                            */
/* ************************************************************************** */

//
// Structure for the local record
typedef struct {
   XrdCryptoFactory *factory;
   char              factoryname[MAXFACTORYNAMELEN];
   bool              status; 
} FactoryEntry;

//____________________________________________________________________________
XrdCryptoFactory *XrdCryptoFactory::GetCryptoFactory(const char *factoryid)
{
   // Static method to load/locate the crypto factory named factoryid
 
   static XrdVERSIONINFODEF(myVer,cryptoloader,XrdVNUMBER,XrdVERSION);
   static FactoryEntry  *factorylist = 0;
   static int            factorynum = 0;
   static XrdOucHash<XrdOucPinLoader> plugins;
   XrdCryptoFactory     *(*efact)();
   XrdCryptoFactory *factory;
   char factobjname[80], libfn[80];
   EPNAME("Factory::GetCryptoFactory");

   //
   // The id must be defined
   if (!factoryid || !strlen(factoryid)) {
      PRINT("crypto factory ID ("<<factoryid<<") undefined");
      return 0;
   }

   //
   // If the local simple implementation is required return the related pointer
   if (!strcmp(factoryid,"local")) {
      PRINT("local crypto factory requested");
      return &localCryptoFactory;
   }

   // 
   // Check if already loaded
   if (factorynum) {
      int i = 0;
      for ( ; i < factorynum; i++ ) {
         if (!strcmp(factoryid,factorylist[i].factoryname)) {
            if (factorylist[i].status) {
               DEBUG(factoryid <<" crypto factory object already loaded ("
                               << factorylist[i].factory << ")");
               return factorylist[i].factory;
            } else {
               DEBUG("previous attempt to load crypto factory "
                     <<factoryid<<" failed - do nothing");
               return 0;
            }
         }
      }
   }

   //
   // Create new entry for this factory in the local record
   FactoryEntry *newfactorylist = new FactoryEntry[factorynum+1];
   if (newfactorylist) {
      int i = 0;
      for ( ; i < factorynum; i++ ) {
         newfactorylist[i].factory = factorylist[i].factory;
         newfactorylist[i].status = factorylist[i].status;
         strcpy(newfactorylist[i].factoryname,factorylist[i].factoryname);
      }
      newfactorylist[i].factory = 0;
      newfactorylist[i].status = 0;
      strcpy(newfactorylist[i].factoryname,factoryid);

      // Destroy previous vector
      if (factorylist) delete[] factorylist;

      // Update local list
      factorylist = newfactorylist;
      factorynum++;
   } else
      PRINT("cannot create local record of loaded crypto factories");

   //
   // Try loading: name of routine to load
   sprintf(factobjname, "XrdCrypto%sFactoryObject", factoryid);
   
   // Create or attach to the plug-in instance
   XrdOucPinLoader *plug = plugins.Find(factoryid);
   if (!plug) {
      // Create one and add it to the list
      snprintf(libfn, sizeof(libfn)-1, "libXrdCrypto%s.so", factoryid);
      libfn[sizeof(libfn)-1] = '\0';
      
      plug = new XrdOucPinLoader(&myVer, "cryptolib", libfn);
      plugins.Add(factoryid, plug);
   }
   if (!plug) {
      PRINT("problems opening shared library " << libfn);
      return 0;
   }
   DEBUG("shared library '" << libfn << "' loaded");

   // Get the function
   if (!(efact = (XrdCryptoFactory *(*)()) plug->Resolve(factobjname))) {
      PRINT(plug->LastMsg());
      PRINT("problems finding crypto factory object creator " << factobjname);
      return 0;
   }
 
   //
   // Get the factory object
   if (!(factory = (*efact)())) {
      PRINT("problems creating crypto factory object");
      return 0;
   }

   //
   // Update local record
   factorylist[factorynum-1].factory = factory;
   factorylist[factorynum-1].status = 1;

   return factory;
}
