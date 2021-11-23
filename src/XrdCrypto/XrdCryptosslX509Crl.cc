/******************************************************************************/
/*                                                                            */
/*                X r d C r y p t o s s l X 5 0 9 C r l. c c                  */
/*                                                                            */
/* (c) 2005 G. Ganis , CERN                                                   */
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

/* ************************************************************************** */
/*                                                                            */
/* OpenSSL implementation of XrdCryptoX509Crl                                 */
/*                                                                            */
/* ************************************************************************** */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <ctime>

#include "XrdCrypto/XrdCryptosslRSA.hh"
#include "XrdCrypto/XrdCryptosslX509Crl.hh"
#include "XrdCrypto/XrdCryptosslAux.hh"
#include "XrdCrypto/XrdCryptosslTrace.hh"

#include <openssl/bn.h>
#include <openssl/pem.h>

#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define X509_REVOKED_get0_revocationDate(x) (x)->revocationDate
#define X509_REVOKED_get0_serialNumber(x) (x)->serialNumber
#define X509_CRL_get0_lastUpdate X509_CRL_get_lastUpdate
#define X509_CRL_get0_nextUpdate X509_CRL_get_nextUpdate
#endif

//_____________________________________________________________________________
XrdCryptosslX509Crl::XrdCryptosslX509Crl(const char *cf, int opt)
                 : XrdCryptoX509Crl()
{
   // Constructor certificate from file 'cf'.
   EPNAME("X509Crl::XrdCryptosslX509Crl_file");

   // Make sure file name is defined;
   if (opt == 0) {
      if (Init(cf) != 0) {
         DEBUG("could not initialize the CRL from "<<cf);
         return;
      }
   } else {
      if (InitFromURI(cf, 0) != 0) {
         DEBUG("could not initialize the CRL from URI"<<cf);
         return;
      }
   }
}

//_____________________________________________________________________________
XrdCryptosslX509Crl::XrdCryptosslX509Crl(FILE *fc, const char *cf)
{
   // Constructe CRL from a FILE handle `fc` with (assumed) filename `cf`.
   EPNAME("X509Crl::XrdCryptosslX509Crl_file");

   if (Init(fc, cf)) {
      DEBUG("could not initialize the CRL from " << cf);
      return;
   }
}

//_____________________________________________________________________________
XrdCryptosslX509Crl::XrdCryptosslX509Crl(XrdCryptoX509 *cacert)
                 : XrdCryptoX509Crl()
{
   // Constructor certificate from CA certificate 'cacert'. This constructor
   // extracts the information about the location of the CRL cerificate from the
   // CA certificate extension 'crlDistributionPoints', downloads the file and
   // loads it in the cache
   EPNAME("X509Crl::XrdCryptosslX509Crl_CA");

   // The CA certificate must be defined
   if (!cacert || cacert->type != XrdCryptoX509::kCA) {
      DEBUG("the CA certificate is undefined or not CA! ("<<cacert<<")");
      return;
   }

   // Get the extension
   X509_EXTENSION *crlext = (X509_EXTENSION *) cacert->GetExtension("crlDistributionPoints");
   if (!crlext) {
      DEBUG("extension 'crlDistributionPoints' not found in the CA certificate");
      return;
   }

   // Bio for exporting the extension
   BIO *bext = BIO_new(BIO_s_mem());
   ASN1_OBJECT *obj = X509_EXTENSION_get_object(crlext);
   i2a_ASN1_OBJECT(bext, obj);
   X509V3_EXT_print(bext, crlext, 0, 4);
   // data length
   char *cbio = 0;
   int lbio = (int) BIO_get_mem_data(bext, &cbio);
   char *buf = (char *) malloc(lbio+1);
   // Read key from BIO to buf
   memcpy(buf, cbio, lbio);
   buf[lbio] = 0;
   BIO_free(bext);
   // Save it
   XrdOucString uris(buf);
   free(buf);

   DEBUG("URI string: "<< uris);

   XrdOucString uri;
   int from = 0;
   while ((from = uris.tokenize(uri, from, ' ')) != -1) {
      if (uri.beginswith("URI:")) {
         uri.replace("URI:","");
         uri.replace("\n","");
         if (InitFromURI(uri.c_str(), cacert->SubjectHash()) == 0) {
            crluri = uri;
            // We are done
            break;
         }
      }
   }
}

//_____________________________________________________________________________
XrdCryptosslX509Crl::~XrdCryptosslX509Crl()
{
   // Destructor

   // Cleanup CRL
   if (crl)
      X509_CRL_free(crl);
}

//_____________________________________________________________________________
int XrdCryptosslX509Crl::Init(const char *cf)
{
   // Load a CRL from an open file handle; for debugging purposes,
   // we assume it's loaded from file named `cf`.
   EPNAME("X509Crl::Init");

   // Make sure file name is defined;
   if (!cf) {
      DEBUG("file name undefined");
      return -1;
   }
   // Make sure file exists;
   struct stat st;
   if (stat(cf, &st) != 0) {
      if (errno == ENOENT) {
         DEBUG("file "<<cf<<" does not exist - do nothing");
      } else {
         DEBUG("cannot stat file "<<cf<<" (errno: "<<errno<<")");
      }
      return -1;
   }
   //
   // Open file in read mode
   FILE *fc = fopen(cf, "r");
   if (!fc) {
      DEBUG("cannot open file "<<cf<<" (errno: "<<errno<<")");
      return -1;
   }

   auto rval = Init(fc, cf);

   //
   // Close the file
   fclose(fc);

   return rval;
}


//_____________________________________________________________________________
int XrdCryptosslX509Crl::Init(FILE *fc, const char *cf)
{
   // Constructor certificate from file 'cf'.
   // Return 0 on success, -1 on failure
   EPNAME("X509Crl::Init");

   //
   // Read the content:
   if (!PEM_read_X509_CRL(fc, &crl, 0, 0)) {
      DEBUG("Unable to load CRL from file");
      return -1;
   }

   //
   // Notify
   DEBUG("CRL successfully loaded from "<< cf);

   //
   // Save source file name
   srcfile = cf;
   //
   // Init some of the private members (the others upon need)
   Issuer();
   //
   // Load into cache
   LoadCache();
   //
   // Done
   return 0;
}

//_____________________________________________________________________________
int XrdCryptosslX509Crl::InitFromURI(const char *uri, const char *hash)
{
   // Initialize the CRL taking the file indicated by URI. Download and
   // reformat the file first.
   // Returns 0 on success, -1 on failure.
   EPNAME("X509Crl::InitFromURI");

   // Make sure file name is defined;
   if (!uri) {
      DEBUG("uri undefined");
      return -1;
   }
   XrdOucString u(uri), h(hash);
   if (h == "") {
      int isl = u.rfind('/');
      if (isl != STR_NPOS) h.assign(u, isl + 1);
   }
   if (h == "") h = "hashtmp";

   // Create local output file path
   XrdOucString outtmp(getenv("TMPDIR")), outpem;
   if (outtmp.length() <= 0) outtmp = "/tmp";
   if (!outtmp.endswith("/")) outtmp += "/";
   outtmp += h;
   outtmp += ".crltmp";

   // Prepare 'wget' command
   XrdOucString cmd("wget ");
   cmd += uri;
   cmd += " -O ";
   cmd += outtmp;
   
   // Execute 'wget'
   DEBUG("executing ... "<<cmd);
   if (system(cmd.c_str()) == -1) {
      DEBUG("'system' could not fork to execute command '"<<cmd<<"'");
      return -1;
   }
   struct stat st;
   if (stat(outtmp.c_str(), &st) != 0) {
      DEBUG("did not manage to get the CRL file from "<<uri);
      return -1;
   }
   outpem = outtmp;

   // Find out the file type
   int needsopenssl = GetFileType(outtmp.c_str());
   if (needsopenssl < 0) {
      DEBUG("did not manage to coorectly parse "<<outtmp);
      return -1;
   }

   if (needsopenssl > 0) {
      // Put it in PEM format
      outpem.replace(".crltmp", ".pem");
      cmd = "openssl crl -inform DER -in ";
      cmd += outtmp;
      cmd += " -out ";
      cmd += outpem;
      cmd += " -text";

      // Execute 'openssl crl'
      DEBUG("executing ... "<<cmd);
      if (system(cmd.c_str()) == -1) {
         DEBUG("system: problem executing: "<<cmd);
         return -1;
      }

      // Cleanup the temporary files
      if (unlink(outtmp.c_str()) != 0) {
         DEBUG("problems removing "<<outtmp);
      }
   }

   // Make sure the file is there
   if (stat(outpem.c_str(), &st) != 0) {
      DEBUG("did not manage to change format from DER to PEM ("<<outpem<<")");
      return -1;
   }

   // Now init from the new file
   if (Init(outpem.c_str()) != 0) {
      DEBUG("could not initialize the CRL from "<<outpem);
      return -1;
   }

   // Cleanup the temporary files
   unlink(outpem.c_str());

   //
   // Done
   return 0;
}

//_____________________________________________________________________________
bool XrdCryptosslX509Crl::ToFile(FILE *fh)
{
   // Write the CRL's contents to a file in the PEM format.
   EPNAME("ToFile");

   if (!crl) {
      DEBUG("CRL object invalid; cannot write to a file");
      return false;
   }

   if (PEM_write_X509_CRL(fh, crl) == 0) {
      DEBUG("Unable to write CRL to file");
      return false;
   }

   //
   // Notify
   DEBUG("CRL successfully written to file");

   return true;
}

//_____________________________________________________________________________
int XrdCryptosslX509Crl::GetFileType(const char *crlfn)
{
   // Try to understand if file 'crlfn' is in DER (binary) or PEM (ASCII)
   // format (assume that is not ASCII is a DER).
   // Return 1 if not-PEM, 0 if PEM, -1 if any error occurred
   EPNAME("GetFileType");

   if (!crlfn || strlen(crlfn) <= 0) {
      PRINT("file name undefined!");
      return -1;
   }

   char line[1024] = {0};
   FILE *f = fopen(crlfn, "r");
   if (!f) {
      PRINT("could not open file "<<crlfn<<" - errno: "<<(int)errno);
      return -1;
   }

   int rc = 1;
   while (fgets(line, 1024, f)) {
      // Skip empty lines at beginning
      if (line[0] == '\n') continue;
      // Analyse line for '-----BEGIN X509 CRL-----'
      if (strstr(line, "BEGIN X509 CRL")) rc = 0;
      break;
   }
   // Close the files
   fclose(f);
   // Done
   return rc;
}

//_____________________________________________________________________________
int XrdCryptosslX509Crl::LoadCache()
{
   // Load relevant info into the cache
   // Return 0 if ok, -1 in case of error
   EPNAME("LoadCache");

   // The CRL must exists
   if (!crl) {
      DEBUG("CRL undefined");
      return -1;
   }

   // Parse CRL
#if OPENSSL_VERSION_NUMBER >= 0x10000000L
   STACK_OF(X509_REVOKED *) rsk = X509_CRL_get_REVOKED(crl);
#else /* OPENSSL */
   STACK_OF(X509_REVOKED *) *rsk = X509_CRL_get_REVOKED(crl);
#endif /* OPENSSL */
   if (!rsk) {
      DEBUG("could not get stack of revoked instances");
      return -1;
   }

   // Number of revocations
#if OPENSSL_VERSION_NUMBER >= 0x10000000L
   nrevoked = sk_X509_REVOKED_num(rsk);
#else /* OPENSSL */
   nrevoked = sk_num(rsk);
#endif /* OPENSSL */
   DEBUG(nrevoked << "certificates have been revoked");
   if (nrevoked <= 0) {
      DEBUG("no valid certificate has been revoked - nothing to do");
      return 0;
   }

   // Get serial numbers of revoked certificates
   char *tagser = 0;
   int i = 0;
   for (; i < nrevoked; i++ ){
#if OPENSSL_VERSION_NUMBER >= 0x10000000L
      X509_REVOKED *rev = sk_X509_REVOKED_value(rsk,i);
#else /* OPENSSL */
      X509_REVOKED *rev = (X509_REVOKED *)sk_value(rsk,i);
#endif /* OPENSSL */
      if (rev) {
         BIGNUM *bn = BN_new();
         ASN1_INTEGER_to_BN(X509_REVOKED_get0_serialNumber(rev), bn);
         tagser = BN_bn2hex(bn);
         BN_free(bn);
         TRACE(Dump, "certificate with serial number: "<<tagser<<
                     "  has been revoked");
         // Add to the cache
         bool rdlock = false;
         XrdSutCacheEntry *cent = cache.Get((const char *)tagser, rdlock);
         if (!cent) {
            DEBUG("problems getting entry in the cache");
            return -1;
         }
         // Add revocation date
         cent->mtime = XrdCryptosslASN1toUTC(X509_REVOKED_get0_revocationDate(rev));
         // Set status
         cent->mtime = kCE_ok;
         // Release the string for the serial number
         OPENSSL_free(tagser);
         // Unlock the entry
         cent->rwmtx.UnLock();
      }
   }

   return 0;
}

//_____________________________________________________________________________
time_t XrdCryptosslX509Crl::LastUpdate()
{
   // Time of last update

   // If we do not have it already, try extraction
   if (lastupdate < 0) {
      // Make sure we have a CRL
      if (crl)
         // Extract UTC time in secs from Epoch
         lastupdate = XrdCryptosslASN1toUTC(X509_CRL_get0_lastUpdate(crl));
   }
   // return what we have
   return lastupdate;
}

//_____________________________________________________________________________
time_t XrdCryptosslX509Crl::NextUpdate()
{
   // Time of next update

   // If we do not have it already, try extraction
   if (nextupdate < 0) {
      // Make sure we have a CRL
      if (crl)
         // Extract UTC time in secs from Epoch
         nextupdate = XrdCryptosslASN1toUTC(X509_CRL_get0_nextUpdate(crl));
   }
   // return what we have
   return nextupdate;
}

//_____________________________________________________________________________
const char *XrdCryptosslX509Crl::Issuer()
{
   // Return issuer name
   EPNAME("X509Crl::Issuer");

   // If we do not have it already, try extraction
   if (issuer.length() <= 0) {

      // Make sure we have a CRL
      if (!crl) {
         DEBUG("WARNING: no CRL available - cannot extract issuer name");
         return (const char *)0;
      }

      // Extract issuer name
      XrdCryptosslNameOneLine(X509_CRL_get_issuer(crl), issuer);
   }

   // return what we have
   return (issuer.length() > 0) ? issuer.c_str() : (const char *)0;
}

//_____________________________________________________________________________
const char *XrdCryptosslX509Crl::IssuerHash(int alg)
{
   // Return hash of issuer name
   // Use default algorithm (X509_NAME_hash) for alg = 0, old algorithm
   // (for v>=1.0.0) when alg = 1
   EPNAME("X509::IssuerHash");

#if (OPENSSL_VERSION_NUMBER >= 0x10000000L && !defined(__APPLE__))
   if (alg == 1) {
      // md5 based
      if (issueroldhash.length() <= 0) {
         // Make sure we have a certificate
         if (crl) {
            char chash[30] = {0};
            snprintf(chash, sizeof(chash),
                     "%08lx.0",X509_NAME_hash_old(X509_CRL_get_issuer(crl)));
            issueroldhash = chash;
         } else {
            DEBUG("WARNING: no certificate available - cannot extract issuer hash (md5)");
         }
      }
      // return what we have
      return (issueroldhash.length() > 0) ? issueroldhash.c_str() : (const char *)0;
   }
#else
   if (alg == 1) { }
#endif

   // If we do not have it already, try extraction
   if (issuerhash.length() <= 0) {

      // Make sure we have a certificate
      if (crl) {
         char chash[30] = {0};
         snprintf(chash, sizeof(chash),
                  "%08lx.0",X509_NAME_hash(X509_CRL_get_issuer(crl)));
         issuerhash = chash;
      } else {
         DEBUG("WARNING: no certificate available - cannot extract issuer hash (default)");
      }
   }

   // return what we have
   return (issuerhash.length() > 0) ? issuerhash.c_str() : (const char *)0;
}

//_____________________________________________________________________________
bool XrdCryptosslX509Crl::Verify(XrdCryptoX509 *ref)
{
   // Verify certificate signature with pub key of ref cert

   // We must have been initialized
   if (!crl)
      return 0;

   // We must have something to check with
   X509 *r = ref ? (X509 *)(ref->Opaque()) : 0;
   EVP_PKEY *rk = r ? X509_get_pubkey(r) : 0;
   if (!rk)
      return 0;

   // Ok: we can verify
   return (X509_CRL_verify(crl, rk) > 0);
}

//_____________________________________________________________________________
bool XrdCryptosslX509Crl::IsRevoked(int serialnumber, int when)
{
   // Check if certificate with serialnumber is in the
   // list of revocated certificates
   EPNAME("IsRevoked");

   // Reference time
   int now = (when > 0) ? when : time(0);

   // Warn if CRL should be updated
   if (now > NextUpdate()) {
      DEBUG("WARNING: CRL is expired: you should download the updated one");
   }

   // We must have something to check against
   if (nrevoked <= 0) {
      DEBUG("No certificate in the list");
      return 0;
   }

   // Ok, build the tag
   char tagser[20] = {0};
   sprintf(tagser,"%x",serialnumber);

   // Look into the cache
   XrdSutCacheEntry *cent = cache.Get((const char *)tagser);
   if (cent && cent->status == kCE_ok) {
      // Check the revocation time
      if (now > cent->mtime) {
         DEBUG("certificate "<<tagser<<" has been revoked");
         cent->rwmtx.UnLock();
         return 1;
      }
      cent->rwmtx.UnLock();
   }

   // Certificate not revoked
   return 0;
}

//_____________________________________________________________________________
bool XrdCryptosslX509Crl::IsRevoked(const char *sernum, int when)
{
   // Check if certificate with 'sernum' is in the
   // list of revocated certificates
   EPNAME("IsRevoked");

   // Reference time
   int now = (when > 0) ? when : time(0);

   // Warn if CRL should be updated
   if (now > NextUpdate()) {
      DEBUG("WARNING: CRL is expired: you should download the updated one");
   }

   // We must have something to check against
   if (nrevoked <= 0) {
      DEBUG("No certificate in the list");
      return 0;
   }

   // Look into the cache
   XrdSutCacheEntry *cent = cache.Get((const char *)sernum);
   if (cent && cent->status == kCE_ok) {
      // Check the revocation time
      if (now > cent->mtime) {
         DEBUG("certificate "<<sernum<<" has been revoked");
         cent->rwmtx.UnLock();
         return 1;
      }
      cent->rwmtx.UnLock();
   }

   // Certificate not revoked
   return 0;
}

//_____________________________________________________________________________
void XrdCryptosslX509Crl::Dump()
{
   // Dump content
   EPNAME("X509Crl::Dump");

   // Time strings
   struct tm tst;
   char stbeg[256] = {0};
   time_t tbeg = LastUpdate();
   localtime_r(&tbeg,&tst);
   asctime_r(&tst,stbeg);
   stbeg[strlen(stbeg)-1] = 0;
   char stend[256] = {0};
   time_t tend = NextUpdate();
   localtime_r(&tend,&tst);
   asctime_r(&tst,stend);
   stend[strlen(stend)-1] = 0;

   PRINT("+++++++++++++++ X509 CRL dump +++++++++++++++++++++++");
   PRINT("+");
   PRINT("+ File:    "<<ParentFile());
   PRINT("+");
   PRINT("+ Issuer:  "<<Issuer());
   PRINT("+ Issuer hash:  "<<IssuerHash(0));
   PRINT("+");
   if (IsExpired()) {
      PRINT("+ Validity: (expired!)");
   } else {
      PRINT("+ Validity:");
   }
   PRINT("+ LastUpdate:  "<<tbeg<<" UTC - "<<stbeg);
   PRINT("+ NextUpdate:  "<<tend<<" UTC - "<<stend);
   PRINT("+");
   PRINT("+ Number of revoked certificates: "<<nrevoked);
   PRINT("+");
   PRINT("+++++++++++++++++++++++++++++++++++++++++++++++++");
}
