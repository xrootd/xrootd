/******************************************************************************/
/*                                                                            */
/*                      X r d S e c g s i t e s t . c c                       */
/*                                                                            */
/* (c) 2005, G. Ganis / CERN                                                  */
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
/*                                                                            */
/******************************************************************************/

//
//  Test program for XrdSecgsi
//

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/types.h>
#include <pwd.h>

#include "XrdOuc/XrdOucString.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysError.hh"

#include "XrdSut/XrdSutAux.hh"

#include "XrdCrypto/XrdCryptoAux.hh"
#include "XrdCrypto/XrdCryptoFactory.hh"
#include "XrdCrypto/XrdCryptoX509.hh"
#include "XrdCrypto/XrdCryptoX509Req.hh"
#include "XrdCrypto/XrdCryptoX509Chain.hh"
#include "XrdCrypto/XrdCryptoX509Crl.hh"

#include "XrdCrypto/XrdCryptosslAux.hh"

#include "XrdCrypto/XrdCryptogsiX509Chain.hh"

#include "XrdSecgsi/XrdSecgsiTrace.hh"

#include <openssl/x509v3.h>
#include <openssl/x509.h>

//
// Globals 

// #define PRINT(x) {cerr <<x <<endl;}
XrdCryptoFactory *gCryptoFactory = 0;

XrdOucString EEcert = "";
XrdOucString EEkey = "";
XrdOucString PXcert = "";
XrdOucString PPXcert = "";
XrdOucString CAdir = "/etc/grid-security/certificates/";
int          CAnum = 0;
XrdOucString CAcert[5];
int          Dbg = 0;
int          Help = 0;

//
// For error logging and tracing
static XrdSysLogger Logger;
static XrdSysError eDest(0,"gsitest_");
XrdOucTrace *gsiTrace = 0;

#define PRTWIDTH 80
static void pdots(const char *t, bool ok = 1)
{
   unsigned int i = 0;
   unsigned int l = (t) ? strlen (t) : 0;
   unsigned int np = PRTWIDTH - l - 8;
   printf("|| %s ", t);
   for (; i < np ; i++) { printf("."); }
   printf("  %s\n", (ok ? "PASSED" : "FAILED"));
}
static void pline(const char *t)
{
   unsigned int i = 0;
   unsigned int l = (t) ? strlen (t) : 0;
   unsigned int np = PRTWIDTH - l - 3;
   if (l > 0) {
      printf("|| %s ---", t);
   } else {
      printf("|| ----");
   }
   for (; i < np ; i++) { printf("-"); }
   printf("\n");
}

static void printHelp()
{
   printf(" \n");
   printf(" Basic test program for crypto functionality in relation to GSI.\n");
   printf(" The program needs access to a user certificate file and its private key, and the related\n");
   printf(" CA file(s); the CRL is downloaded using the information found in the CA certificate.\n");
   printf(" The location of the files are the standard ones and they can modified by the standard\n");
   printf(" environment variables:\n");
   printf(" \n");
   printf("      X509_USER_CERT  [$HOME/.globus/usercert.pem]       user certificate\n");
   printf("      X509_USER_KEY   [$HOME/.globus/userkey.pem]        user private key\n");
   printf("      X509_USER_PROXY [/tmp/x509up_u<uid>]               user proxy\n");
   printf("      X509_CERT_DIR   [/etc/grid-security/certificates/] CA certificates and CRL directories\n");
   printf(" \n");
   printf(" Usage:\n");
   printf("      xrdgsitest [-v,--verbose] [-h,--help] \n");
   printf(" \n");
   printf("      -h, --help             Print this screen\n");
   printf("      -v, --verbose          Dump all details\n");
   printf(" \n");
   printf(" The output is a list of PASSED/FAILED test, interleaved with details when the verbose option\n");
   printf(" is chosen.\n");
   printf(" \n");
}

int main( int argc, char **argv )
{
   // Test implemented functionality
   EPNAME("main");
   char cryptomod[64] = "ssl";
   char outname[256] = {0};

   // Basic argument parsing
   int i = 1;
   for (; i < argc; i++) {
      // Verbosity level
      if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) Dbg = 1;
      if (!strcmp(argv[i], "-vv")) Dbg = 2;
      // Help
      if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) Help = 1;
   }

   // Print help if required
   if (Help) {
      printHelp();
      exit(0);
   }

   //
   // Initiate error logging and tracing
   eDest.logger(&Logger);
   if (!gsiTrace)
      gsiTrace = new XrdOucTrace(&eDest);
   if (gsiTrace && Dbg > 0) {
      // Medium level
      gsiTrace->What |= (TRACE_Authen | TRACE_Debug);
   }
   //
   // Set debug flags in other modules
   kXR_int32 tracesut = (Dbg > 0) ? sutTRACE_Debug : 0;
   kXR_int32 tracecrypto = (Dbg > 0) ? cryptoTRACE_Debug : 0;
   XrdSutSetTrace(tracesut);
   XrdCryptoSetTrace(tracecrypto);

   //
   // Determine application name
   char *p = argv[0];
   int k = strlen(argv[0]);
   while (k--)
      if (p[k] == '/') break;
   strcpy(outname,p+k+1);

   //
   // Load the crypto factory
   if (!(gCryptoFactory = XrdCryptoFactory::GetCryptoFactory(cryptomod))) {
      pdots("  Cannot instantiate factory", 0);
      exit(1);
   }
   if (Dbg > 0)
      gCryptoFactory->SetTrace(cryptoTRACE_Debug);

   pline("");
   pline("Crypto functionality tests for GSI");
   pline("");

   //
   // Find out the username and locate the relevant certificates and directories
   struct passwd *pw = getpwuid(geteuid());
   if (!pw) {
      pdots("  Could not resolve user info - exit", 0);
      exit(1);
   }
   NOTIFY("effective user is : "<<pw->pw_name<<", $HOME : "<<pw->pw_dir);

   //
   // User certificate
   EEcert = pw->pw_dir;
   EEcert += "/.globus/usercert.pem";
   if (getenv("X509_USER_CERT")) EEcert = getenv("X509_USER_CERT");
   NOTIFY("user EE certificate: "<<EEcert);
   XrdCryptoX509 *xEE = gCryptoFactory->X509(EEcert.c_str());
   if (xEE) {
      if (Dbg > 0) xEE->Dump();
   } else {
      pdots("  Problems loading user EE cert", 0);
   }
   if (xEE) pdots("Loading EEC", 1);

   //
   // User key
   EEkey = pw->pw_dir;
   EEkey += "/.globus/userkey.pem";
   if (getenv("X509_USER_KEY")) EEkey = getenv("X509_USER_KEY");
   NOTIFY("user EE key: "<<EEkey);
   //
   // User Proxy certificate
   PXcert = "/tmp/x509up_u";
   PXcert += (int) pw->pw_uid;
   if (getenv("X509_USER_PROXY")) PXcert = getenv("X509_USER_PROXY");
   NOTIFY("user proxy certificate: "<<PXcert);
   XrdCryptoX509 *xPX = gCryptoFactory->X509(PXcert.c_str());
   if (xPX) {
      if (Dbg > 0) xPX->Dump();
   } else {
      pdots("  Problems loading user proxy cert", 0);
   }
   if (xPX) pdots("Loading User Proxy", 1);

   //
   pline("");
   pline("Recreate the proxy certificate");
   XrdProxyOpt_t *pxopt = 0;   // defaults
   XrdCryptogsiX509Chain *cPXp = new XrdCryptogsiX509Chain();
   XrdCryptoRSA *kPXp = 0;
   XrdCryptoX509 *xPXp = 0;
   X509_EXTENSION *ext = 0;
   int prc = gCryptoFactory->X509CreateProxy()(EEcert.c_str(), EEkey.c_str(),
                                             pxopt, cPXp, &kPXp, PXcert.c_str());
   if (prc == 0) {
      if (Dbg > 0) cPXp->Dump();
      if ((xPXp = (XrdCryptoX509 *)(cPXp->Begin()))) {
         pdots("Recreating User Proxy", 1);
         if ((ext = (X509_EXTENSION *)(xPXp->GetExtension("1.3.6.1.4.1.3536.1.222")))) {
            pdots("proxyCertInfo extension OK", 1);
         }
      }
   } else {
      pdots("Recreating User Proxy", 0);
      exit(1);
   }

   //
   pline("");
   pline("Load CA certificates");
   // Load CA certificates now
   XrdCryptoX509 *xCA[5], *xCAref = 0;
   if (getenv("X509_CERT_DIR")) CAdir = getenv("X509_CERT_DIR");
   if (!CAdir.endswith("/")) CAdir += "/";
   XrdCryptoX509 *xc = xEE;
   bool rCAfound = 0;
   int nCA = 0;
   while (!rCAfound && nCA < 5) {
      CAcert[nCA] = CAdir;
      CAcert[nCA] += xc->IssuerHash();
      NOTIFY("issuer CA certificate path "<<CAcert[nCA]);
      xCA[nCA] = gCryptoFactory->X509(CAcert[nCA].c_str());
      if (xCA[nCA]) {
         if (Dbg > 0) xCA[nCA]->Dump();
         pdots("Loading CA certificate", 1);
      } else {
         pdots("Loading CA certificate", 0);
      }
      // Check if self-signed
      if (!strcmp(xCA[nCA]->IssuerHash(), xCA[nCA]->SubjectHash())) {
         rCAfound = 1;
         break;
      }
      // If not, parse the issuer ...
      xc = xCA[nCA];
      nCA++;
   }

   //
   pline("");
   pline("Testing ParseFile");
   XrdCryptoX509ParseFile_t ParseFile = gCryptoFactory->X509ParseFile();
   XrdCryptoRSA *key = 0;
   XrdCryptoX509Chain *chain = new XrdCryptoX509Chain();
   if (ParseFile) {
      int nci = (*ParseFile)(PXcert.c_str(), chain);
      if (!(key = chain->Begin()->PKI())) {
         pdots("getting PKI", 0);
      }
      NOTIFY(nci <<" certificates found parsing file");
      if (Dbg > 0) chain->Dump();
      int jCA = nCA + 1;
      while (jCA--) {
         chain->PushBack(xCA[jCA]);
      }
      if (Dbg > 0) chain->Dump();
      int rorc = chain->Reorder();
      if (rCAfound) {
         if (Dbg > 0) chain->Dump();
         pdots("Chain reorder: ", (rorc != -1));
         XrdCryptoX509Chain::EX509ChainErr ecod = XrdCryptoX509Chain::kNone;
         int verc = chain->Verify(ecod);
         pdots("Chain verify: ", verc);
      } else {
         pdots("Full CA chain verification", 0);
      }
   } else {
      pdots("attaching to X509ParseFile", 0);
      exit (1);
   }

   //
   pline("");
   pline("Testing ExportChain");
   XrdCryptoX509ExportChain_t ExportChain = gCryptoFactory->X509ExportChain();
   XrdSutBucket *chainbck = 0;
   if (ExportChain) {
      chainbck = (*ExportChain)(chain, 0);
      pdots("Attach to X509ExportChain", 1);
   } else {
      pdots("Attach to X509ExportChain", 0);
      exit (1);
   }
   //
   pline("");
   pline("Testing Chain Import");
   XrdCryptoX509ParseBucket_t ParseBucket = gCryptoFactory->X509ParseBucket();
   if (!ParseBucket) pdots("attaching to X509ParseBucket", 0);
   // Init new chain with CA certificate 
   int jCA = nCA;
   XrdCryptoX509Chain *CAchain = new XrdCryptoX509Chain(xCA[jCA]);
   while (jCA) { CAchain->PushBack(xCA[--jCA]); }
   if (ParseBucket && CAchain) {
      int nci = (*ParseBucket)(chainbck, CAchain);
      NOTIFY(nci <<" certificates found parsing bucket");
      if (Dbg > 0) CAchain->Dump();
      int rorc = CAchain->Reorder();
      pdots("Chain reorder: ", (rorc != -1));
      if (Dbg > 0) CAchain->Dump();
      XrdCryptoX509Chain::EX509ChainErr ecod = XrdCryptoX509Chain::kNone;
      int verc = CAchain->Verify(ecod);
      pdots("Chain verify: ", verc);
   } else {
      pdots("creating new X509Chain", 0);
      exit (1);
   }

   //
   pline("");
   pline("Testing GSI chain import and verification");
   // Init new GSI chain with CA certificate 
   jCA = nCA;
   XrdCryptogsiX509Chain *GSIchain = new XrdCryptogsiX509Chain(xCA[jCA], gCryptoFactory);
   while (jCA) { GSIchain->PushBack(xCA[--jCA]); }
   if (ParseBucket && GSIchain) {
      int nci = (*ParseBucket)(chainbck, GSIchain);
      NOTIFY(nci <<" certificates found parsing bucket");
      if (Dbg > 0) GSIchain->Dump();
      XrdCryptoX509Chain::EX509ChainErr ecod = XrdCryptoX509Chain::kNone;
      x509ChainVerifyOpt_t vopt = { kOptsRfc3820, 0, -1, 0};
      int verc = GSIchain->Verify(ecod, &vopt);
      pdots("GSI chain verify: ", verc);
      if (!verc) NOTIFY("GSI chain verify ERROR: "<<GSIchain->LastError());
      if (Dbg > 0) GSIchain->Dump();
   } else {
      pdots("Creating new gsiX509Chain", 0);
      exit (1);
   }

   //
   pline("");
   pline("Testing GSI chain copy");
   // Init new GSI chain with CA certificate 
   XrdCryptogsiX509Chain *GSInew = new XrdCryptogsiX509Chain(GSIchain, gCryptoFactory);
   if (GSInew) {
      if (Dbg > 0) GSInew->Dump();
      XrdCryptoX509Chain::EX509ChainErr ecod = XrdCryptoX509Chain::kNone;
      x509ChainVerifyOpt_t vopt = { kOptsRfc3820, 0, -1, 0};
      int verc = GSInew->Verify(ecod, &vopt);
      if (!verc) NOTIFY("GSI chain copy verify ERROR: "<<GSInew->LastError());
      pdots("GSI chain verify: ", verc);
      if (Dbg > 0) GSInew->Dump();
   } else {
      pdots("Creating new gsiX509Chain with copy", 0);
      exit (1);
   }

   //
   pline("");
   pline("Testing Cert verification");
   XrdCryptoX509VerifyCert_t VerifyCert = gCryptoFactory->X509VerifyCert();
   if (VerifyCert) {
      bool ok;
      jCA = nCA;
      while (jCA >= 0) {
         ok = xEE->Verify(xCA[jCA]);
         NOTIFY( ": verify cert: EE signed by CA? " <<ok<<" ("<<xCA[jCA]->Subject()<<")");
         if (ok) xCAref = xCA[jCA];
         jCA--;
      }
      pdots("verify cert: EE signed by CA", (xCAref ? 1 : 0));
      ok = xPX->Verify(xEE);
      pdots("verify cert: PX signed by EE", ok);
      jCA = nCA;
      bool refok = 0;
      while (jCA >= 0) {
         ok = xPX->Verify(xCA[jCA]);
         NOTIFY( ": verify cert: PX signed by CA? " <<ok<<" ("<<xCA[jCA]->Subject()<<")");
         if (!refok && ok) refok = 1;
         jCA--;
      }
      pdots("verify cert: PX not signed by CA", !refok);
   } else {
      pdots("Attaching to X509VerifyCert", 0);
      exit (1);
   }


   //
   pline("");
   pline("Testing request creation");
   XrdCryptoX509Req *rPXp = 0;
   XrdCryptoRSA *krPXp = 0;
   prc = gCryptoFactory->X509CreateProxyReq()(xPX, &rPXp, &krPXp);
   if (prc == 0) {
      pdots("Creating request", 1);
      if (Dbg > 0) rPXp->Dump();
   } else {
      pdots("Creating request", 0);
      exit(1);
   }

   //
   pline("");
   pline("Testing request signature");
   XrdCryptoX509 *xPXpp = 0;
   prc = gCryptoFactory->X509SignProxyReq()(xPX, kPXp, rPXp, &xPXpp);
   if (prc == 0) {
      if (Dbg > 0) xPXpp->Dump();
      xPXpp->SetPKI((XrdCryptoX509data) krPXp->Opaque());
      bool extok = 0;
      if ((ext = (X509_EXTENSION *)xPXpp->GetExtension(gsiProxyCertInfo_OID))) extok = 1;
      pdots("Check proxyCertInfo extension", extok);
   } else {
      pdots("Signing request", 0);
      exit(1);
   }

   //
   pline("");
   pline("Testing export of signed proxy");
   PPXcert = PXcert;
   PPXcert += "p";
   NOTIFY(": file for signed proxy chain: "<<PPXcert);
   XrdCryptoX509ChainToFile_t ChainToFile = gCryptoFactory->X509ChainToFile();
   // Init the proxy chain 
   XrdCryptoX509Chain *PXchain = new XrdCryptoX509Chain(xPXpp);
   PXchain->PushBack(xPX);
   PXchain->PushBack(xEE);
   if (ChainToFile && PXchain) {
      if ((*ChainToFile)(PXchain, PPXcert.c_str()) != 0) {
         NOTIFY(": problems saving signed proxy chain to file: "<<PPXcert);
         pdots("Saving signed proxy chain to file", 0);
      } else {
         pdots("Saving signed proxy chain to file", 1);        
      }
   } else {
      pdots("Creating new X509Chain", 0);
      exit (1);
   }

   //
   pline("");
   pline("Testing CRL identification");
   X509_EXTENSION *crlext = 0;
   if (xCAref) {
      if ((crlext = (X509_EXTENSION *)xCAref->GetExtension("crlDistributionPoints"))) {
         pdots("Check CRL distribution points extension OK", 1);
      } else {
         pdots("Getting extension", 0);
      }
   }

   //
   pline("");
   pline("Testing CRL loading");
   XrdCryptoX509Crl *xCRL1 = gCryptoFactory->X509Crl(xCAref);
   if (xCRL1) {
      if (Dbg > 0) xCRL1->Dump();
      pdots("Loading CA1 crl", 1);
      // Verify CRL signature
      bool crlsig = 0, xsig = 0;
      for (jCA = 0; jCA <= nCA; jCA++) {
         xsig = xCRL1->Verify(xCA[jCA]);
         NOTIFY( ": CRL signature OK? "<<xsig<<" ("<<xCA[jCA]->Subject()<<")");
         if (!crlsig && xsig) crlsig = 1;
      }
      pdots("CRL signature OK", crlsig);
      // Verify a serial number
      bool snrev = xCRL1->IsRevoked(25, 0);
      NOTIFY( ": SN: 25 revoked? "<<snrev);
      // Verify another serial number
      snrev = xCRL1->IsRevoked(0x20, 0);
      NOTIFY( ": SN: 32 revoked? "<<snrev);
   } else {
      pdots("Loading CA1 crl", 0);
   }

   pline("");
   exit(0);
}
