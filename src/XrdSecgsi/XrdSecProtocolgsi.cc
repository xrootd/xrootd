// $Id$
/******************************************************************************/
/*                                                                            */
/*                 X r d S e c P r o t o c o l g s i . c c                    */
/*                                                                            */
/* (c) 2005 G. Ganis / CERN                                                   */
/*                                                                            */
/******************************************************************************/

#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <iostream.h>
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <sys/param.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#include <XrdOuc/XrdOucLogger.hh>
#include <XrdOuc/XrdOucError.hh>
#include <XrdOuc/XrdOucStream.hh>

#include <XrdSut/XrdSutCache.hh>

#include <XrdCrypto/XrdCryptoMsgDigest.hh>
#include <XrdCrypto/XrdCryptosslgsiAux.hh>

#include <XrdSecgsi/XrdSecProtocolgsi.hh>
#include <XrdSecgsi/XrdSecgsiTrace.hh>

/******************************************************************************/
/*                           S t a t i c   D a t a                            */
/******************************************************************************/
  
static String Prefix   = "xrd";
static String ProtoID  = XrdSecPROTOIDENT;
static const kXR_int32 Version = XrdSecgsiVERSION;

static const char *gsiClientSteps[] = {
   "kXGC_none",
   "kXGC_certreq",
   "kXGC_cert",
   "kXGC_reserved"
};

static const char *gsiServerSteps[] = {
   "kXGS_none",
   "kXGS_init",
   "kXGS_cert",
   "kXGS_reserved"
};

static const char *gGSErrStr[] = {
   "ErrParseBuffer",               // 10000
   "ErrDecodeBuffer",              // 10001
   "ErrLoadCrypto",                // 10002
   "ErrBadProtocol",               // 10003
   "ErrCreateBucket",              // 10004
   "ErrDuplicateBucket",           // 10005
   "ErrCreateBuffer",              // 10006
   "ErrSerialBuffer",              // 10007
   "ErrGenCipher",                 // 10008
   "ErrExportPuK",                 // 10009
   "ErrEncRndmTag",                // 10010
   "ErrBadRndmTag",                // 10011
   "ErrNoRndmTag",                 // 10012
   "ErrNoCipher",                  // 10013
   "ErrNoCreds",                   // 10014
   "ErrBadOpt",                    // 10015
   "ErrMarshal",                   // 10016
   "ErrUnmarshal",                 // 10017
   "ErrSaveCreds",                 // 10018
   "ErrNoBuffer",                  // 10019
   "ErrRefCipher",                 // 10020
   "ErrNoPublic",                  // 10021
   "ErrAddBucket",                 // 10022
   "ErrFinCipher",                 // 10023
   "ErrInit",                      // 10024
   "ErrBadCreds",                  // 10025
   "ErrError"                      // 10026  
};

// Masks for options
static const short kOptsServer  = 0x0001;
// One day in secs
static const int kOneDay = 86400; 

/******************************************************************************/
/*                     S t a t i c   C l a s s   D a t a                      */
/******************************************************************************/

XrdOucMutex XrdSecProtocolgsi::gsiContext;
String XrdSecProtocolgsi::CAdir    = "/etc/grid-security/certificates/";
String XrdSecProtocolgsi::CRLdir   = "/etc/grid-security/certificates/";
String XrdSecProtocolgsi::DefCRLext= ".r0";
String XrdSecProtocolgsi::SrvCert  = "/etc/grid-security/root/rootcert.pem";
String XrdSecProtocolgsi::SrvKey   = "/etc/grid-security/root/rootkey.pem";
String XrdSecProtocolgsi::UsrProxy = "/tmp/x509up_u";
String XrdSecProtocolgsi::UsrCert  = "/.globus/usercert.pem";
String XrdSecProtocolgsi::UsrKey   = "/.globus/userkey.pem";;
String XrdSecProtocolgsi::PxyValid = "12:00";
int    XrdSecProtocolgsi::DepLength= 0;
int    XrdSecProtocolgsi::DefBits  = 512;
int    XrdSecProtocolgsi::CRLCheck = 1;
String XrdSecProtocolgsi::DefCrypto= "ssl";
String XrdSecProtocolgsi::DefCipher= "aes-128-cbc:bf-cbc:des-ede3-cbc";
String XrdSecProtocolgsi::DefMD    = "sha1:md5";
String XrdSecProtocolgsi::DefError = "invalid credentials ";
//
// Crypto related info
int  XrdSecProtocolgsi::ncrypt    = 0;                 // Number of factories
XrdCryptoFactory *XrdSecProtocolgsi::cryptF[XrdCryptoMax] = {0};   // their hooks 
int  XrdSecProtocolgsi::cryptID[XrdCryptoMax] = {0};   // their IDs 
String XrdSecProtocolgsi::cryptName[XrdCryptoMax] = {0}; // their names 
XrdCryptoCipher *XrdSecProtocolgsi::refcip[XrdCryptoMax] = {0};    // ref for session ciphers 
//
// Caches
XrdSutCache XrdSecProtocolgsi::cacheCA;   // CA info
XrdSutCache XrdSecProtocolgsi::cacheCert; // Server certificates info
XrdSutCache XrdSecProtocolgsi::cachePxy;  // Client proxies
//
// Running options / settings
int  XrdSecProtocolgsi::Debug       = 0; // [CS] Debug level
bool XrdSecProtocolgsi::Server      = 1; // [CS] If server mode 
int  XrdSecProtocolgsi::TimeSkew    = 300; // [CS] Allowed skew in secs for time stamps 
//
// Debug an tracing
XrdOucError    XrdSecProtocolgsi::eDest(0, "secgsi_");
XrdOucLogger   XrdSecProtocolgsi::Logger;
XrdOucTrace   *XrdSecProtocolgsi::GSITrace = 0;

XrdOucTrace *gsiTrace = 0;

/******************************************************************************/
/*                    S t a t i c   F u n c t i o n s                         */
/******************************************************************************/
//_____________________________________________________________________________
static const char *ClientStepStr(int kclt)
{
   // Return string with client step  
   static const char *ukn = "Unknown";

   kclt = (kclt < 0) ? 0 : kclt;
   kclt = (kclt > kXGC_reserved) ? 0 : kclt;
   kclt = (kclt >= kXGC_certreq) ? (kclt - kXGC_certreq + 1) : kclt;

   if (kclt < 0 || kclt > (kXGC_reserved - kXGC_certreq + 1))
      return ukn;  
   else
      return gsiClientSteps[kclt];
}

//_____________________________________________________________________________
static const char *ServerStepStr(int ksrv)
{
   // Return string with server step  
   static const char *ukn = "Unknown";

   ksrv = (ksrv < 0) ? 0 : ksrv;
   ksrv = (ksrv > kXGS_reserved) ? 0 : ksrv;
   ksrv = (ksrv >= kXGS_init) ? (ksrv - kXGS_init + 1) : ksrv;

   if (ksrv < 0 || ksrv > (kXGS_reserved - kXGS_init + 1))
      return ukn;  
   else
      return gsiServerSteps[ksrv];
}

/******************************************************************************/
/*       P r o t o c o l   I n i t i a l i z a t i o n   M e t h o d s        */
/******************************************************************************/


//_____________________________________________________________________________
XrdSecProtocolgsi::XrdSecProtocolgsi(int opts, const char *hname,
                                     const struct sockaddr *ipadd)
{
   // Default constructor
   EPNAME("XrdSecProtocolgsi");

   if (QTRACE(Authen)) { PRINT("constructing: "<<this); }

   // Create instance of the handshake vars
   if ((hs = new gsiHSVars())) {
      // Update time stamp
      hs->TimeStamp = time(0);
      // Local handshake variables
      hs->CryptoMod = "";       // crypto module in use
      hs->RemVers = -1;         // Version run by remote counterpart
      hs->Rcip = 0;             // reference cipher
      hs->Cbck = 0;             // Bucket with the certificate in export form
      hs->ID = "";              // Handshake ID (dummy for clients)
      hs->Cref = 0;             // Cache reference
      hs->Pent = 0;             // Pointer to relevant file entry
      hs->Chain = 0;            // Chain to be eventually verified
      hs->Crl = 0;              // Pointer to CRL, if required
      hs->PxyChain = 0;         // Proxy Chain on clients
      hs->RtagOK = 0;           // Rndm tag checked / not checked
      hs->Tty = (isatty(0) == 0 || isatty(1) == 0) ? 0 : 1;
      hs->LastStep = 0;         // Step required at previous iteration
   } else {
      DEBUG("could not create handshake vars object");
   }

   // Set protocol ID
   strncpy(Entity.prot, XrdSecPROTOIDENT, sizeof(Entity.prot));

   // Set host name
   if (hname) {
      Entity.host = strdup(hname);
   } else {
      DEBUG("warning: host name undefined");
   }
   // Set host addr
   memcpy(&hostaddr, ipadd, sizeof(hostaddr));

   // Init session variables
   sessionCF = 0;
   sessionKey = 0;
   bucketKey = 0;
   sessionMD = 0;
   sessionKsig = 0;
   sessionKver = 0;

   //
   // Notify, if required
   DEBUG("constructing: host: "<<hname);
   DEBUG("p: "<<XrdSecPROTOIDENT<<", plen: "<<XrdSecPROTOIDLEN);
   //
   // basic settings
   options  = opts;

   //
   // Notify, if required
   if (Server) {
      DEBUG("mode: server");
   } else {
      DEBUG("mode: client");
   }

   // We are done
   String vers = Version;
   vers.insert('.',vers.length()-2);
   vers.insert('.',vers.length()-5);
   DEBUG("object created: v"<<vers.c_str());
}

//_____________________________________________________________________________
char *XrdSecProtocolgsi::Init(gsiOptions opt, XrdOucErrInfo *erp)
{
   // Static method to the configure the static part of the protocol
   // Called once by XrdSecProtocolgsiInit
   EPNAME("Init");
   char *Parms = 0;
   //
   // Time stamp of initialization
   int timestamp = (int)time(0);
   //
   // Debug an tracing
   Debug = (opt.debug > -1) ? opt.debug : Debug;
   // Initiate error logging and tracing
   eDest.logger(&Logger);
   GSITrace    = new XrdOucTrace(&eDest);
   // Set debug mask ... also for auxilliary libs
   int trace = 0;
   if (Debug >= 3) {
      trace = cryptoTRACE_Dump;
      GSITrace->What |= TRACE_Authen;
      GSITrace->What |= TRACE_Debug;
   } else if (Debug >= 1) {
      trace = cryptoTRACE_Debug;
      GSITrace->What = TRACE_Debug;
   }
   gsiTrace = GSITrace;
   // ... also for auxilliary libs
   XrdSutSetTrace(trace);
   XrdCryptoSetTrace(trace);

   //
   // Operation mode
   Server = (opt.mode == 's');

   //
   // Check existence of CA directory
   struct stat st;
   if (opt.certdir)
      CAdir = opt.certdir;
   if (stat(CAdir.c_str(),&st) == -1) {
      if (errno == ENOENT) {
         ErrF(erp,kGSErrError,"CA directory non existing:",CAdir.c_str());
         PRINT(erp->getErrText());
      } else {
         ErrF(erp,kGSErrError,"cannot stat CA directory:",CAdir.c_str());
         PRINT(erp->getErrText());
      }
      return Parms;
   }
   if (!(CAdir.endswith('/'))) CAdir += '/';
   DEBUG("using CA dir: "<<CAdir);

   //
   // CRL check level
   //
   //    0   do not care
   //    1   use if available
   //    2   require
   //    3   require not expired
   //
   if (opt.crl >= 0 && opt.crl <= 3)
      CRLCheck = opt.crl;
   DEBUG("option CRLCheck: "<<CRLCheck);

   //
   // Check existence of CRL directory
   if (opt.crldir) {
      CRLdir = opt.crldir;
      if (CRLCheck > 0) {
         if (stat(CRLdir.c_str(),&st) == -1) {
            if (CRLCheck > 1) {
               if (errno == ENOENT) {
                  ErrF(erp,kGSErrError,"CRL directory non existing:",CRLdir.c_str());
                  PRINT(erp->getErrText());
               } else {
                  ErrF(erp,kGSErrError,"cannot stat CRL directory:",CRLdir.c_str());
                  PRINT(erp->getErrText());
               }
               return Parms;
            } else {
               DEBUG("CRL dir: "<<CRLdir<<" cannot be stat: use defaults");
               // Use CAdir
               CRLdir = CAdir;
            }
         }
      }
   } else {
      // Use CAdir
      CRLdir = CAdir;
   }
   if (CRLCheck > 0) {
      if (!(CRLdir.endswith('/'))) CRLdir += '/';
      DEBUG("using CRL dir: "<<CRLdir);
   }

   //
   // Default extension for CRL files
   if (opt.crlext)
      DefCRLext = opt.crlext;

   //
   // Server specific options
   if (Server) {
      //
      // List of supported / wanted crypto modules
      if (opt.clist)
         DefCrypto = opt.clist;
      //
      // List of crypto modules
      String cryptlist(DefCrypto,0,-1,64);
      // 
      // Load crypto modules
      XrdSutPFEntry ent;
      XrdCryptoFactory *cf = 0;
      if (cryptlist.length()) {
         String ncpt = "";
         int from = 0;
         while ((from = cryptlist.tokenize(ncpt, from, '|')) != -1) {
            if (ncpt.length() > 0 && ncpt[0] != '-') {
               // Try loading 
               if ((cf = XrdCryptoFactory::GetCryptoFactory(ncpt.c_str()))) {
                  // Add it to the list
                  cryptF[ncrypt] = cf;
                  cryptID[ncrypt] = cf->ID();
                  cryptName[ncrypt].insert(cf->Name(),0,strlen(cf->Name())+1);
                  cf->SetTrace(trace);
                  // Ref cipher
                  if (!(refcip[ncrypt] = cf->Cipher(0,0,0))) {
                     PRINT("ref cipher for module "<<ncpt<<
                           " cannot be instantiated : disable");
                     from -= ncpt.length();
                     cryptlist.erase(ncpt);
                  } else {
                     ncrypt++;
                     if (ncrypt >= XrdCryptoMax) {
                        PRINT("max number of crypto modules ("
                              << XrdCryptoMax <<") reached ");
                        break;
                     }
                  }
               } else {
                  PRINT("cannot instantiate crypto factory "<<ncpt<<
                           ": disable");
                  from -= ncpt.length();
                  cryptlist.erase(ncpt);
               }
            }
         }
      }
      //
      // We need at least one valid crypto module
      if (ncrypt <= 0) {
         ErrF(erp,kGSErrInit,"could not find any valid crypto module");
         PRINT(erp->getErrText());
         return Parms;
      }
      //
      // Load CA info into cache
      if (LoadCADir(timestamp) != 0) {
         ErrF(erp,kGSErrError,"problems loading CA info into cache");
         PRINT(erp->getErrText());
         return Parms;
      }
      if (QTRACE(Authen)) { cacheCA.Dump(); }

      //
      // List of supported / wanted ciphers
      if (opt.cipher)
         DefCipher = opt.cipher;
      // make sure we support all of them
      String cip = "";
      int from = 0;
      while ((from = DefCipher.tokenize(cip, from, ':')) != -1) {
         if (cip.length() > 0) {
            int i = 0;
            for (; i < ncrypt; i++) {
               if (!(cryptF[i]->SupportedCipher(cip.c_str()))) {
                  // Not supported: drop from the list
                  PRINT("cipher type not supported ("<<cip<<") - disabling");
                  from -= cip.length();
                  DefCipher.erase(cip);
               }
            }
         }
      }

      //
      // List of supported / wanted Message Digest
      if (opt.md)
         DefMD = opt.md;
      // make sure we support all of them
      String md = "";
      from = 0;
      while ((from = DefMD.tokenize(md, from, ':')) != -1) {
         if (md.length() > 0) {
            int i = 0;
            for (; i < ncrypt; i++) {
               if (!(cryptF[i]->SupportedMsgDigest(md.c_str()))) {
                  // Not supported: drop from the list
                  PRINT("MD type not supported ("<<md<<") - disabling");
                  from -= md.length();
                  DefMD.erase(md);
               }
            }
         }
      }

      //
      // Load server certificate and key
      if (opt.cert)
         SrvCert = opt.cert;
      if (opt.key)
         SrvKey = opt.key;
      //
      // Init cache for certificates (we allow for more instances of
      // the same certificate, one for each different crypto module
      // available; this may evetually not be strictly needed.)
      if (cacheCert.Init(10) != 0) {
         ErrF(erp,kGSErrError,"problems init cache for certificates");
         PRINT(erp->getErrText());
         return Parms;
      }
      int i = 0;
      String certcalist = "";   // list of CA for server certificates
      for (; i<ncrypt; i++) {
         // Check normal certificates
         XrdCryptoX509 *xsrv = cryptF[i]->X509(SrvCert.c_str(),SrvKey.c_str());
         if (xsrv) {
            // Must be of EEC type
            if (xsrv->type != XrdCryptoX509::kEEC) {
               PRINT("problems loading srv cert: not EEC but: "<<xsrv->Type());
               continue;
            }
            // Must be valid
            if (!(xsrv->IsValid())) {
               PRINT("problems loading srv cert: invalid");
               continue;
            }
            // PKI must have been successfully initialized
            if (!xsrv->PKI() || xsrv->PKI()->status != XrdCryptoRSA::kComplete) {
               PRINT("problems loading srv cert: invalid PKI");
               continue;
            }
            // Must be exportable
            XrdSutBucket *xbck = xsrv->Export();
            if (!xbck) {
               PRINT("problems loading srv cert: cannot export into bucket");
               continue;
            }
            // Ok: save it into the cache
            String tag = cryptF[i]->Name();
            XrdSutPFEntry *cent = cacheCert.Add(tag.c_str());
            if (cent) {
               cent->status = kPFE_ok;
               cent->cnt = 0;
               cent->mtime = xsrv->NotAfter(); // expiration time
               // Save pointer to certificate
               cent->buf1.buf = (char *)xsrv;
               cent->buf1.len = 1;  // just a flag
               // Save pointer to key
               cent->buf2.buf = (char *)(xsrv->PKI());
               cent->buf2.len = 1;  // just a flag
               // Save pointer to bucket
               cent->buf3.buf = (char *)(xbck);
               cent->buf3.len = 1;  // just a flag
               // Save CA hash in list to communicate to clients
               if (certcalist.find(xsrv->IssuerHash()) == STR_NPOS) {
                  if (certcalist.length() > 0)
                     certcalist += "|";
                  certcalist += xsrv->IssuerHash();
               }
            }
         }
      }
      // Rehash cache
      cacheCert.Rehash(1);
      //
      // We must have got at least one valid certificate
      if (cacheCert.Empty()) {
         ErrF(erp,kGSErrError,"no valid server certificate found");
         PRINT(erp->getErrText());
         return Parms;
      }
      if (QTRACE(Authen)) { cacheCert.Dump(); }

      DEBUG("CA list: "<<certcalist);
      //
      // Parms in the form:
      //     &P=gsi,v:<version>,c:<cryptomod>,ca:<list_of_srv_cert_ca>
      Parms = new char[cryptlist.length()+3+12+certcalist.length()+5];
      if (Parms) {
         sprintf(Parms,"v:%d,c:%s,ca:%s",
                       Version,cryptlist.c_str(),certcalist.c_str());
      } else {
         ErrF(erp,kGSErrInit,"no system resources for 'Parms'");
         PRINT(erp->getErrText());
      }

      // Some notification
      DEBUG("available crypto modules: "<<cryptlist);
      DEBUG("issuer CAs of server certs (hashes): "<<certcalist);
   }

   //
   // Client specific options
   if (!Server) {
      //
      // Init cache for CA certificate info. Clients will fill it
      // upon need
      if (cacheCA.Init(100) != 0) {
         ErrF(erp,kGSErrError,"problems init cache for CA info");
         PRINT(erp->getErrText());
         return Parms;
      }
      //
      // Init cache for proxies (in the future we may allow
      // users to use more certificates, depending on the context)
      if (cachePxy.Init(2) != 0) {
         ErrF(erp,kGSErrError,"problems init cache for proxies");
         PRINT(erp->getErrText());
         return Parms;
      }
      // use default dir $(HOME)/.<prefix>
      struct passwd *pw = getpwuid(getuid());
      if (!pw) {
         DEBUG("WARNING: cannot get user information (uid:"<<getuid()<<")");
      }
      //
      // Define user proxy file
      if (opt.proxy) {
         UsrProxy = opt.proxy;
      } else {
         if (pw)
            UsrProxy += (int)(pw->pw_uid);
      }
      // Define user certificate file
      if (opt.cert) {
         UsrCert = opt.cert;
      } else {
         if (pw)
            UsrCert.insert(pw->pw_dir,0);
      }
      // Define user private key file
      if (opt.key) {
         UsrKey = opt.key;
      } else {
         if (pw)
            UsrKey.insert(pw->pw_dir,0);
      }
      // Define proxy validity at renewal
      if (opt.valid)
         PxyValid = opt.valid;
      // Set depth of signature path
      if (opt.deplen != DepLength)
         DepLength = opt.deplen;
      // Set number of bits for proxy key
      if (opt.bits > DefBits)
         DefBits = opt.bits;
      //
      // Notify
      DEBUG("using certificate file:         "<<UsrCert);
      DEBUG("using private key file:         "<<UsrKey);
      DEBUG("proxy: file:                    "<<UsrProxy);
      DEBUG("proxy: validity:                "<<PxyValid);
      DEBUG("proxy: depth of signature path: "<<DepLength);
      DEBUG("proxy: bits in key:             "<<DefBits);

      // We are done
      Parms = (char *)"";
   }

   // We are done
   return Parms;
}

/******************************************************************************/
/*                                D e l e t e                                 */
/******************************************************************************/
void XrdSecProtocolgsi::Delete()
{
   // Deletes the protocol
   if (Entity.host) free(Entity.host);
   // Cleanup the handshake variables, if still there
   SafeDelete(hs);
   delete this;
}


/******************************************************************************/
/*       E n c r y p t i o n  R e l a t e d   M e t h o d s                   */
/******************************************************************************/

//_____________________________________________________________________________
int XrdSecProtocolgsi::Encrypt(const char *inbuf,  // Data to be encrypted
                               int inlen,          // Length of data in inbuff
                               XrdSecBuffer **outbuf)  // Returns encrypted data
{
   // Encrypt data in inbuff and place it in outbuff.
   //
   // Returns: < 0 Failed, the return value is -errno of the reason. Typically,
   //              -EINVAL    - one or more arguments are invalid.
   //              -ENOTSUP   - encryption not supported by the protocol
   //              -EOVERFLOW - outbuff is too small to hold result
   //              -ENOENT    - Context not initialized
   //          = 0 Success, outbuff contains a pointer to the encrypted data.
   //
   EPNAME("Encrypt");

   // We must have a key
   if (!sessionKey)
      return -ENOENT;

   // And something to encrypt
   if (!inbuf || inlen <= 0 || !outbuf)
      return -EINVAL;

   // Get output buffer
   char *buf = (char *)malloc(sessionKey->EncOutLength(inlen));
   if (!buf)
      return -ENOMEM;

   // Encrypt
   int len = sessionKey->Encrypt(inbuf, inlen, buf);
   if (len <= 0) {
      free(buf);
      return -EINVAL;
   }

   // Create and fill output buffer
   *outbuf = new XrdSecBuffer(buf, len);

   // We are done
   DEBUG("encrypted buffer has "<<len<<" bytes");
   return 0;
}

//_____________________________________________________________________________
int XrdSecProtocolgsi::Decrypt(const char *inbuf,  // Data to be decrypted
                               int inlen,          // Length of data in inbuff
                               XrdSecBuffer **outbuf)  // Returns decrypted data
{
   // Decrypt data in inbuff and place it in outbuff.
   //
   // Returns: < 0 Failed,the return value is -errno (see Encrypt).
   //          = 0 Success, outbuff contains a pointer to the encrypted data.
   EPNAME("Decrypt");

   // We must have a key
   if (!sessionKey)
      return -ENOENT;

   // And something to decrypt
   if (!inbuf || inlen <= 0 || !outbuf)
      return -EINVAL;

   // Get output buffer
   char *buf = (char *)malloc(sessionKey->DecOutLength(inlen));
   if (!buf)
      return -ENOMEM;

   // Decrypt
   int len = sessionKey->Decrypt(inbuf, inlen, buf);
   if (len <= 0) {
      free(buf);
      return -EINVAL;
   }

   // Create and fill output buffer
   *outbuf = new XrdSecBuffer(buf, len);

   // We are done
   DEBUG("decrypted buffer has "<<len<<" bytes");
   return 0;
}

//_____________________________________________________________________________
int XrdSecProtocolgsi::Sign(const char  *inbuf,   // Data to be signed
                            int inlen,    // Length of data to be signed
                            XrdSecBuffer **outbuf)   // Buffer for the signature
{
   // Sign data in inbuff and place the signature in outbuf.
   //
   // Returns: < 0 Failed, returned value is -errno (see Encrypt).
   //          = 0 Success, the return value is the length of the signature
   //              placed in outbuf.
   //
   EPNAME("Sign");

   // We must have a PKI and a digest
   if (!sessionKsig || !sessionMD)
      return -ENOENT;

   // And something to sign
   if (!inbuf || inlen <= 0 || !outbuf)
      return -EINVAL;
   
   // Reset digest
   sessionMD->Reset(0);
   
   // Calculate digest
   sessionMD->Update(inbuf, inlen);
   sessionMD->Final();

   // Output length
   int lmax = sessionKsig->GetOutlen(sessionMD->Length());
   char *buf = (char *)malloc(lmax);
   if (!buf)
      return -ENOMEM;

   // Sign
   int len = sessionKsig->EncryptPrivate(sessionMD->Buffer(),
                                         sessionMD->Length(),
                                         buf, lmax);
   if (len <= 0) {
      free(buf);
      return -EINVAL;
   }

   // Create and fill output buffer
   *outbuf = new XrdSecBuffer(buf, len);

   // We are done
   DEBUG("signature has "<<len<<" bytes");
   return 0;
}

//_____________________________________________________________________________
int XrdSecProtocolgsi::Verify(const char  *inbuf,   // Data to be verified
                              int  inlen,           // Length of data in inbuf
                              const char  *sigbuf,  // Buffer with signature
                              int  siglen)          // Length of signature
{
   // Verify a signature
   //
   // Returns: < 0 Failed, returned value is -errno (see Encrypt).
   //          = 0 Signature matches the value in inbuff.
   //          > 0 Failed to verify, signature does not match inbuff data.
   //
   EPNAME("Verify");

   // We must have a PKI and a digest
   if (!sessionKver || !sessionMD)
      return -ENOENT;

   // And something to verify
   if (!inbuf || inlen <= 0 || !sigbuf || siglen <= 0)
      return -EINVAL;
   
   // Reset digest
   sessionMD->Reset(0);
   
   // Calculate digest
   sessionMD->Update(inbuf, inlen);
   sessionMD->Final();

   // Output length
   int lmax = sessionKver->GetOutlen(siglen);
   char *buf = new char[lmax];
   if (!buf)
      return -ENOMEM;

   // Decrypt signature
   int len = sessionKver->DecryptPublic(sigbuf, siglen, buf, lmax);
   if (len <= 0) {
      delete[] buf;
      return -EINVAL;
   }

   // Verify signature
   bool bad = 1;
   if (len == sessionMD->Length()) {
      if (!strncmp(buf, sessionMD->Buffer(), len)) {
         // Signature matches
         bad = 0;
         DEBUG("signature successfully verified");
      }
   }

   // Cleanup
   if (buf) delete[] buf;

   // We are done
   return ((bad) ? 1 : 0);
}

//_____________________________________________________________________________
int XrdSecProtocolgsi::getKey(char *kbuf, int klen)
{
   // Get the current encryption key
   //
   // Returns: < 0 Failed, returned value if -errno (see Encrypt)
   //         >= 0 The size of the encyption key. The supplied buffer of length
   //              size hold the key. If the buffer address is 0, only the 
   //              size of the key is returned.
   //
   EPNAME("getKey");

   // Check if we have to serialize the key
   if (!bucketKey) {

      // We must have a key for that
      if (!sessionKey)
         // Invalid call
         return -ENOENT;
      
      // Create bucket
      bucketKey = sessionKey->AsBucket();
   }

   // Prepare output now, if we have any
   if (bucketKey) {
      // If are asked only the size, we are done
      if (kbuf == 0)
         return bucketKey->size;
   
      // Check the size of the buffer
      if (klen < bucketKey->size)
         // Too small
         return -EOVERFLOW;

      // Copy the buffer
      memcpy(kbuf, bucketKey->buffer, bucketKey->size);

      // We are done
      DEBUG("session key exported");
      return bucketKey->size;
   }

   // Key exists but we could export it in bucket format
   return -ENOMEM;
}

//_____________________________________________________________________________
int XrdSecProtocolgsi::setKey(char *kbuf, int klen)
{
   // Set the current encryption key
   //
   // Returns: < 0 Failed, returned value if -errno (see Encrypt)
   //            0 The new key has been set.
   //
   EPNAME("setKey");

   // Make sur that we can initialize the new key
   if (!kbuf || klen <= 0) 
      // Invalid inputs
      return -EINVAL;

   if (!sessionCF) 
      // Invalid context
      return -ENOENT;

   // Put the buffer key into a bucket
   XrdSutBucket *bck = new XrdSutBucket();
   if (!bck)
      // Cannot get buffer: out-of-resources?
      return -ENOMEM;
   // Set key buffer
   bck->SetBuf(kbuf, klen);

   // Init a new cipher from the bucket
   XrdCryptoCipher *newKey = sessionCF->Cipher(bck);
   if (!newKey) {
      SafeDelete(bck);
      return -ENOMEM;
   }

   // Delete current key
   SafeDelete(sessionKey);

   // Set the new key
   sessionKey = newKey;

   // Cleanup
   SafeDelete(bck);

   // Ok 
   DEBUG("session key update");
   return 0;
}

/******************************************************************************/
/*             C l i e n t   O r i e n t e d   F u n c t i o n s              */
/******************************************************************************/
/******************************************************************************/
/*                        g e t C r e d e n t i a l s                         */
/******************************************************************************/

XrdSecCredentials *XrdSecProtocolgsi::getCredentials(XrdSecParameters *parm,
                                                     XrdOucErrInfo    *ei)
{
   // Query client for the password; remote username and host
   // are specified in 'parm'. File '.rootnetrc' is checked. 
   EPNAME("getCredentials");

   // Handshake vars conatiner must be initialized at this point
   if (!hs)
      return ErrC(ei,0,0,0,kGSErrError,
                  "handshake var container missing","getCredentials");
   //
   // Nothing to do if buffer is empty
   if (!parm || !(parm->buffer) || parm->size <= 0) {
      if (hs->Iter == 0) 
         return ErrC(ei,0,0,0,kGSErrNoBuffer,"parm empty","getCredentials");
      else
         return (XrdSecCredentials *)0;
   }

   // Count interations
   (hs->Iter)++;

   // Update time stamp
   hs->TimeStamp = time(0);

   // Local vars
   int step = 0;
   int nextstep = 0;
   const char *stepstr = 0;
   char *bpub = 0;
   int lpub = 0;
   String CryptList = "";
   String Host = "";
   String RemID = "";
   String Emsg;
   String specID = "";
   String issuerHash = "";
   // Buffer / Bucket related
   XrdSutBuffer *bpar   = 0;  // Global buffer
   XrdSutBuffer *bmai   = 0;  // Main buffer

   // 
   // Unlocks automatically returning
   XrdOucMutexHelper gsiGuard(&gsiContext);
   //
   // Decode received buffer
   if (!(bpar = new XrdSutBuffer((const char *)parm->buffer,parm->size)))
      return ErrC(ei,0,0,0,kGSErrDecodeBuffer,"global",stepstr);
   //
   // Check protocol ID name
   if (strcmp(bpar->GetProtocol(),XrdSecPROTOIDENT))
      return ErrC(ei,bpar,bmai,0,kGSErrBadProtocol,stepstr);
   //
   // The step indicates what we are supposed to do
   step = (bpar->GetStep()) ? bpar->GetStep() : kXGS_init;
   stepstr = ServerStepStr(step);
   // Dump, if requested
   if (QTRACE(Authen)) {
      bpar->Dump(stepstr);
   }
   //
   // Parse input buffer
   if (ParseClientInput(bpar, &bmai, Emsg) == -1) {
      DEBUG(Emsg<<" CF: "<<sessionCF);
      return ErrC(ei,bpar,bmai,0,kGSErrParseBuffer,Emsg.c_str(),stepstr);
   }
   //
   // Version
   DEBUG("version run by server: "<< hs->RemVers);
   //
   // Check random challenge
   if (!CheckRtag(bmai, Emsg))
      return ErrC(ei,bpar,bmai,0,kGSErrBadRndmTag,Emsg.c_str(),stepstr);
   //
   // Now action depens on the step
   nextstep = kXGC_none;
   switch (step) {

   case kXGS_init:
      //
      // Add bucket with cryptomod to the global list
      // (This must be always visible from now on)
      if (bpar->AddBucket(hs->CryptoMod,kXRS_cryptomod) != 0)
         return ErrC(ei,bpar,bmai,0,
              kGSErrCreateBucket,XrdSutBuckStr(kXRS_cryptomod),stepstr);
      //
      // Add bucket with our version to the main list
      if (bpar->MarshalBucket(kXRS_version,(kXR_int32)(hs->RemVers)) != 0)
         return ErrC(ei,bpar,bmai,0, kGSErrCreateBucket,
                XrdSutBuckStr(kXRS_version),"global",stepstr);
      //
      // Add our issuer hash
      issuerHash = hs->PxyChain->Begin()->IssuerHash();
      if (bpar->AddBucket(issuerHash,kXRS_issuer_hash) != 0)
            return ErrC(ei,bpar,bmai,0, kGSErrCreateBucket,
                        XrdSutBuckStr(kXRS_issuer_hash),stepstr);
      //
      nextstep = kXGC_certreq;
      break;

   case kXGS_cert:
      //
      // We must have a session cipher at this point
      if (!(sessionKey))
         return ErrC(ei,bpar,bmai,0,
              kGSErrNoCipher,"session cipher",stepstr);

      //
      // Extract buffer with public info for the cipher agreement
      if (!(bpub = sessionKey->Public(lpub))) 
         return ErrC(ei,bpar,bmai,0,
                     kGSErrNoPublic,"session",stepstr);
      //
      // Add it to the global list
      if (bpar->UpdateBucket(bpub,lpub,kXRS_puk) != 0)
         return ErrC(ei,bpar,bmai,0, kGSErrAddBucket,
                     XrdSutBuckStr(kXRS_puk),"global",stepstr);
      //
      // Add the proxy certificate
      bmai->AddBucket(hs->Cbck);
      //
      nextstep = kXGC_cert;
      break;

   default:
      return ErrC(ei,bpar,bmai,0, kGSErrBadOpt,stepstr);
   }
   //
   // Serialize and encrypt
   if (AddSerialized('c', nextstep, hs->ID,
                     bpar, bmai, kXRS_main, sessionKey) != 0) {
      // Remove to avoid destruction
      bmai->Remove(hs->Cbck);
      return ErrC(ei,bpar,bmai,0,
                  kGSErrSerialBuffer,"main",stepstr);
   }
   //
   // Serialize the global buffer
   char *bser = 0;
   int nser = bpar->Serialized(&bser,'f');

   if (QTRACE(Authen)) {
      bpar->Dump(ClientStepStr(bpar->GetStep()));
      bmai->Dump("Main OUT");
   }
   //
   // Remove to avoid destruction
   bmai->Remove(hs->Cbck);
   //
   // We may release the buffers now
   REL2(bpar,bmai);
   //
   // Return serialized buffer
   if (nser > 0) {
      DEBUG("returned " << nser <<" bytes of credentials");
      return new XrdSecCredentials(bser, nser);
   } else {
      DEBUG("problems with final serialization");
      return (XrdSecCredentials *)0;
   }
}

/******************************************************************************/
/*               S e r v e r   O r i e n t e d   M e t h o d s                */
/******************************************************************************/
/******************************************************************************/
/*                          A u t h e n t i c a t e                           */
/******************************************************************************/

int XrdSecProtocolgsi::Authenticate(XrdSecCredentials *cred,
                                    XrdSecParameters **parms,
                                    XrdOucErrInfo     *ei)
{
   //
   // Check if we have any credentials or if no credentials really needed.
   // In either case, use host name as client name
   EPNAME("Authenticate");

   //
   // If cred buffer is two small or empty assume host protocol
   if (cred->size <= (int)XrdSecPROTOIDLEN || !cred->buffer) {
      strncpy(Entity.prot, "host", sizeof(Entity.prot));
      return 0;
   }

   // Handshake vars conatiner must be initialized at this point
   if (!hs)
      return ErrS(hs->ID,ei,0,0,0,kGSErrError,
                  "handshake var container missing",
                  "protocol initialization problems");

   // Update time stamp
   hs->TimeStamp = time(0);

   //
   // ID of this handshaking
   hs->ID = Entity.tident;
   DEBUG("handshaking ID: " << hs->ID);

   // Local vars 
   int kS_rc = kgST_more;
   int step = 0;
   int nextstep = 0;
   char *bpub = 0;
   int lpub = 0;
   const char *stepstr = 0;
   String Message;
   String CryptList;
   String Host;
   String SrvPuKExp;
   String Salt;
   String RndmTag;
   String ClntMsg(256);
   // Buffer related
   XrdSutBuffer    *bpar = 0;  // Global buffer
   XrdSutBuffer    *bmai = 0;  // Main buffer

   //
   // Unlocks automatically returning
   XrdOucMutexHelper gsiGuard(&gsiContext);
   //
   // Decode received buffer
   if (!(bpar = new XrdSutBuffer((const char *)cred->buffer,cred->size)))
      return ErrS(hs->ID,ei,0,0,0,kGSErrDecodeBuffer,"global",stepstr);
   //
   // Check protocol ID name
   if (strcmp(bpar->GetProtocol(),XrdSecPROTOIDENT))
      return ErrS(hs->ID,ei,bpar,bmai,0,kGSErrBadProtocol,stepstr);
   //
   // The step indicates what we are supposed to do
   step = bpar->GetStep();
   stepstr = ClientStepStr(step);
   // Dump, if requested
   if (QTRACE(Authen)) {
      bpar->Dump(stepstr);
   }
   //
   // Parse input buffer
   if (ParseServerInput(bpar, &bmai, ClntMsg) == -1) {
      DEBUG(ClntMsg);
      return ErrS(hs->ID,ei,bpar,bmai,0,kGSErrParseBuffer,ClntMsg.c_str(),stepstr);
   }
   //
   // Version
   DEBUG("version run by client: "<< hs->RemVers);
   //
   // Dump, if requested
   if (QTRACE(Authen)) {
      if (bmai)
         bmai->Dump("main IN");
   }
   //
   // Check random challenge
   if (!CheckRtag(bmai, ClntMsg))
      return ErrS(hs->ID,ei,bpar,bmai,0,kGSErrBadRndmTag,stepstr,ClntMsg.c_str());
   //
   // Now action depens on the step
   switch (step) {

   case kXGC_certreq:
      //
      // Client required us to send our certificate and cipher public part:
      // add first this last one.
      // Extract buffer with public info for the cipher agreement
      if (!(bpub = hs->Rcip->Public(lpub))) 
         return ErrS(hs->ID,ei,bpar,bmai,0, kGSErrNoPublic,
                                         "session",stepstr);
      //
      // Add it to the global list
      if (bpar->AddBucket(bpub,lpub,kXRS_puk) != 0)
         return ErrS(hs->ID,ei,bpar,bmai,0, kGSErrAddBucket,
                                            "main",stepstr);
      //
      // Add bucket with list of supported ciphers
      if (bpar->AddBucket(DefCipher,kXRS_cipher_alg) != 0)
         return ErrS(hs->ID,ei,bpar,bmai,0,
              kGSErrAddBucket,XrdSutBuckStr(kXRS_cipher_alg),stepstr);
      //
      // Add bucket with list of supported MDs
      if (bpar->AddBucket(DefMD,kXRS_md_alg) != 0)
         return ErrS(hs->ID,ei,bpar,bmai,0,
              kGSErrAddBucket,XrdSutBuckStr(kXRS_md_alg),stepstr);
      //
      // Add the server certificate
      bpar->AddBucket(hs->Cbck);

      // We are done for the moment
      nextstep = kXGS_cert;
      break;

   case kXGC_cert:
      //
      // Client sent its own credentials: their are checked in
      // ParseServerInput, so if we are here they are OK
      // Nothing more to do, except setting the client DN
      if (hs->Chain->EECname()) {
         Entity.name = strdup(hs->Chain->EECname());
      } else {
         DEBUG("WARNING: DN missing: corruption? ");
      }

      kS_rc = kgST_ok;
      nextstep = kXGS_none;
      break;

   default:
      return ErrS(hs->ID,ei,bpar,bmai,0, kGSErrBadOpt, stepstr);
   }

   if (kS_rc == kgST_more) {
      //
      // Add message to client
      if (ClntMsg.length() > 0)
         if (bmai->AddBucket(ClntMsg,kXRS_message) != 0) {
            DEBUG("problems adding bucket with message for client");
         }
      // 
      // Serialize, encrypt and add to the global list
      if (AddSerialized('s', nextstep, hs->ID,
                        bpar, bmai, kXRS_main, sessionKey) != 0) {
         // Remove to avoid destruction
         bpar->Remove(hs->Cbck);
         return ErrS(hs->ID,ei,bpar,bmai,0, kGSErrSerialBuffer,
                     "main / session cipher",stepstr);
      }
      //
      // Serialize the global buffer
      char *bser = 0;
      int nser = bpar->Serialized(&bser,'f');
      //
      // Dump, if requested
      if (QTRACE(Authen)) {
         bpar->Dump(ServerStepStr(bpar->GetStep()));
         bmai->Dump("Main OUT");
      }
      //
      // Create buffer for client
      *parms = new XrdSecParameters(bser,nser);
      //
      // Remove to avoid destruction
      bpar->Remove(hs->Cbck);

   } else {
      //
      // Remove to avoid destruction
      bpar->Remove(hs->Cbck);
      //
      // Cleanup handshake vars
      SafeDelete(hs);
   }
   //
   // We may release the buffers now
   REL2(bpar,bmai);
   //
   // All done
   return kS_rc;
}

/******************************************************************************/
/*              X r d S e c P r o t o c o l p w d I n i t                     */
/******************************************************************************/
  
extern "C"
{
char *XrdSecProtocolgsiInit(const char mode,
                            const char *parms, XrdOucErrInfo *erp)
{
   // One-time protocol initialization, filling the static flags and options
   // of the protocol.
   // For clients (mode == 'c') we use values in envs.
   // For servers (mode == 's') the command line options are passed through
   // parms.
   gsiOptions opts;
   char *rc = (char *)"";
   
   //
   // Clients first
   if (mode == 'c') {
      //
      // Decode envs:
      //             "XrdSecDEBUG"           debug flag ("0","1","2","3")
      //             "XrdSecGSICADIR"        full path to an alternative path
      //                                     containing the CA info
      //                                     [/etc/grid-security/certificates]
      //             "XrdSecGSICRLDIR"       full path to an alternative path
      //                                     containing the CRL info
      //                                     [/etc/grid-security/certificates]
      //             "XrdSecGSICRLEXT"       default extension of CRL files [.r0]
      //             "XrdSecGSIUSERCERT"     full path to an alternative file
      //                                     containing the user certificate
      //                                     [$HOME/.globus/usercert.pem]
      //             "XrdSecGSIUSERKEY"      full path to an alternative file
      //                                     containing the user key
      //                                     [$HOME/.globus/userkey.pem]
      //             "XrdSecGSIUSERPROXY"    full path to an alternative file
      //                                     containing the user proxy
      //                                     [/tmp/x509up_u<uid>]
      //             "XrdSecGSIPROXYVALID"   validity of proxies in the 
      //                                     grid-proxy-init format
      //                                     ["12:00", i.e. 12 hours]
      //             "XrdSecGSIPROXYDEPLEN"  depth of signature path for proxies;
      //                                     use -1 for unlimited [0]
      //             "XrdSecGSIPROXYKEYBITS" bits in PKI for proxies [512]
      //             "XrdSecGSICRLCHECK"     CRL check level: 0 don't care,
      //                                     1 use if available, 2 require,
      //                                     3 require non-expired CRL [2] 
      //
      opts.mode = mode;
      // debug
      char *cenv = getenv("XrdSecDEBUG");
      if (cenv)
         if (cenv[0] >= 49 && cenv[0] <= 51) opts.debug = atoi(cenv);  
      else
         if ((cenv = getenv("XRDDEBUG"))) opts.debug = 1;

      // directory with CA certificates
      cenv = getenv("XrdSecGSICADIR");
      if (cenv)
         opts.certdir = strdup(cenv);

      // directory with CRL info
      cenv = getenv("XrdSecGSICRLDIR");
      if (cenv)
         opts.crldir = strdup(cenv);

      // Default extension CRL files
      cenv = getenv("XrdSecGSICRLEXT");
      if (cenv)
         opts.crlext = strdup(cenv);

      // file with user cert
      cenv = getenv("XrdSecGSIUSERCERT");
      if (cenv)
         opts.cert = strdup(cenv);  

      // file with user key
      cenv = getenv("XrdSecGSIUSERKEY");
      if (cenv)
         opts.key = strdup(cenv);

      // file with user proxy
      cenv = getenv("XrdSecGSIUSERPROXY");
      if (cenv)
         opts.proxy = strdup(cenv);

      // file with user proxy
      cenv = getenv("XrdSecGSIPROXYVALID");
      if (cenv)
         opts.valid = strdup(cenv);

      // Depth of signature path for proxies
      cenv = getenv("XrdSecGSIPROXYDEPLEN");
      if (cenv)
         opts.deplen = atoi(cenv);

      // Key Bit length 
      cenv = getenv("XrdSecGSIPROXYKEYBITS");
      if (cenv)
         opts.bits = atoi(cenv);

      // CRL check level 
      cenv = getenv("XrdSecGSICRLCHECK");
      if (cenv)
         opts.crl = atoi(cenv);
      //
      // Setup the object with the chosen options
      rc = XrdSecProtocolgsi::Init(opts,erp);

      // Some cleanup
      if (opts.certdir) free(opts.certdir);
      if (opts.crldir) free(opts.crldir);
      if (opts.crlext) free(opts.crlext);
      if (opts.cert) free(opts.cert);
      if (opts.key) free(opts.key);
      if (opts.proxy) free(opts.proxy);
      if (opts.valid) free(opts.valid);

      // We are done
      return rc;
   }

   //
   // Server initialization
   // 
   // Duplicate the parms
   char parmbuff[1024];
   if (parms) 
      strlcpy(parmbuff, parms, sizeof(parmbuff));
   else {
      char *msg = (char *)"parameters not specified.";
      if (erp) 
         erp->setErrInfo(EINVAL, msg);
      else 
         cerr <<msg <<endl;
      return (char *)0;
   }
   //
   // The tokenizer
   XrdOucTokenizer inParms(parmbuff);

   //
   // Decode parms:
   // for servers:
   //              [-d:<debug_level>]
   //              [-c:[-]ssl[:[-]<CryptoModuleName]]
   //              [-certdir:<dir_with_CA_info>]
   //              [-crldir:<dir_with_CRL_info>]
   //              [-crlext:<default_extension_CRL_files>]
   //              [-cert:<path_to_server_certificate>]
   //              [-key:<path_to_server_key>]
   //              [-cipher:<list_of_supported_ciphers>]
   //              [-md:<list_of_supported_digests>]
   //              [-crl:<crl_check_level>]
   //
   int debug = -1;
   String clist = "";
   String certdir = "";
   String crldir = "";
   String crlext = "";
   String cert = "";
   String key = "";
   String cipher = "";
   String md = "";
   int crl = 2;
   char *op = 0;
   if (inParms.GetLine()) { 
      while ((op = inParms.GetToken())) {
         if (!strncmp(op, "-d:",3))
            debug = atoi(op+3);
         if (!strncmp(op, "-c:",3))
            clist = (const char *)(op+3);
         if (!strncmp(op, "-certdir:",9))
            certdir = (const char *)(op+9);
         if (!strncmp(op, "-crldir:",8))
            crldir = (const char *)(op+8);
         if (!strncmp(op, "-crlext:",8))
            crlext = (const char *)(op+8);
         if (!strncmp(op, "-cert:",6))
            cert = (const char *)(op+6);
         if (!strncmp(op, "-key:",5))
            key = (const char *)(op+5);
         if (!strncmp(op, "-cipher:",8))
            cipher = (const char *)(op+8);
         if (!strncmp(op, "-md:",4))
            md = (const char *)(op+4);
         if (!strncmp(op, "-crl:",5))
            crl = atoi(op+5);
      }
   }
      
   //
   // Build the option object
   opts.debug = debug;
   opts.mode = 's';
   opts.crl = crl;
   if (clist.length() > 0)
      opts.clist = (char *)clist.c_str();
   if (certdir.length() > 0)
      opts.certdir = (char *)certdir.c_str();
   if (crldir.length() > 0)
      opts.crldir = (char *)crldir.c_str();
   if (crlext.length() > 0)
      opts.crlext = (char *)crlext.c_str();
   if (cert.length() > 0)
      opts.cert = (char *)cert.c_str();
   if (key.length() > 0)
      opts.key = (char *)key.c_str();
   if (cipher.length() > 0)
      opts.cipher = (char *)cipher.c_str();
   if (md.length() > 0)
      opts.md = (char *)md.c_str();
   //
   // Setup the plug-in with the chosen options
   return XrdSecProtocolgsi::Init(opts,erp);
}}


/******************************************************************************/
/*              X r d S e c P r o t o c o l p w d O b j e c t                 */
/******************************************************************************/
  
extern "C"
{
XrdSecProtocol *XrdSecProtocolgsiObject(const char              mode,
                                        const char             *hostname,
                                        const struct sockaddr  &netaddr,
                                        const char             *parms,
                                        XrdOucErrInfo    *erp)
{
   XrdSecProtocolgsi *prot;
   int options = XrdSecNOIPCHK;

   //
   // Get a new protocol object
   if (!(prot = new XrdSecProtocolgsi(options,hostname,&netaddr))) {
      char *msg = (char *)"Secgsi: Insufficient memory for protocol.";
      if (erp) 
         erp->setErrInfo(ENOMEM, msg);
      else 
         cerr <<msg <<endl;
      return (XrdSecProtocol *)0;
   }
   //
   // We are done
   if (!erp)
      cerr << "protocol object instantiated" << endl;
   return prot;
}}

 
/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/

//_________________________________________________________________________
int XrdSecProtocolgsi::AddSerialized(char opt, kXR_int32 step, String ID,
                                     XrdSutBuffer *bls, XrdSutBuffer *buf,
                                     kXR_int32 type,
                                     XrdCryptoCipher *cip)
{
   // Serialize buf, and add it encrypted to bls as bucket type
   // Cipher cip is used if defined; else PuK rsa .
   // If both are undefined the buffer is just serialized and added.
   EPNAME("AddSerialized");

   if (!bls || !buf || (opt != 0 && opt != 'c' && opt != 's')) {
      DEBUG("invalid inputs ("
            <<bls<<","<<buf<<","<<opt<<")"
            <<" - type: "<<XrdSutBuckStr(type));
      return -1;
   }

   //
   // Add step to indicate the counterpart what we send
   if (step > 0) {
      bls->SetStep(step);
      buf->SetStep(step);
      hs->LastStep = step;
   }

   //
   // If a random tag has been sent and we have a session cipher,
   // we sign it
   XrdSutBucket *brt = buf->GetBucket(kXRS_rtag);
   if (brt && sessionKsig) {
      //
      // Encrypt random tag with session cipher
      if (sessionKsig->EncryptPrivate(*brt) == 0) {
         DEBUG("error encrypting random tag");
         return -1;
      }
      //
      // Update type
      brt->type = kXRS_signed_rtag;
   }
   //
   // Add an random challenge: if a next exchange is required this will
   // allow to prove authenticity of counter part
   //
   // Generate new random tag and create a bucket
   String RndmTag;
   XrdSutRndm::GetRndmTag(RndmTag);
   //
   // Get bucket
   brt = 0;
   if (!(brt = new XrdSutBucket(RndmTag,kXRS_rtag))) {
      DEBUG("error creating random tag bucket");
      return -1;
   }
   buf->AddBucket(brt);
   //
   // Get cache entry
   if (!hs->Cref) {
      DEBUG("cache entry not found: protocol error");
      return -1;
   }
   //
   // Add random tag to the cache and update timestamp
   hs->Cref->buf1.SetBuf(brt->buffer,brt->size);      
   hs->Cref->mtime = (kXR_int32)hs->TimeStamp;
   //
   // Now serialize the buffer ...
   char *bser = 0;
   int nser = buf->Serialized(&bser);
   //
   // Update bucket with this content
   XrdSutBucket *bck = 0;;
   if (!(bck = bls->GetBucket(type))) {
      // or create new bucket, if not existing
      if (!(bck = new XrdSutBucket(bser,nser,type))) {
         DEBUG("error creating bucket "
               <<" - type: "<<XrdSutBuckStr(type));
         return -1;
      }
      //
      // Add the bucket to the list
      bls->AddBucket(bck);      
   } else {
      bck->Update(bser,nser);
   }
   //
   // Encrypted the bucket
   if (cip) {
      if (cip->Encrypt(*bck) == 0) {
         DEBUG("error encrypting bucket - cipher "
               <<" - type: "<<XrdSutBuckStr(type));
         return -1;
      }
   }
   // We are done
   return 0;
}

//_________________________________________________________________________
int XrdSecProtocolgsi::ParseClientInput(XrdSutBuffer *br, XrdSutBuffer **bm,
                                        String &emsg)
{
   // Parse received buffer b,
   // Result used to fill the handshake local variables
   EPNAME("ParseClientInput");

   // Space for pointer to main buffer must be already allocated
   if (!br || !bm) {
      DEBUG("invalid inputs ("<<br<<","<<bm<<")");
      emsg = "invalid inputs";
      return -1;
   }

   //
   // Get the step
   XrdSutBucket *bck = 0;

   // If first call, not much to do
   if (!br->GetNBuckets()) {
      //
      // Create the main buffer as a copy of the buffer received
      if (!((*bm) = new XrdSutBuffer(br->GetProtocol(),br->GetOptions()))) {
         emsg = "error instantiating main buffer";
         return -1;
      }
      //
      // Extract server version from options
      String opts = br->GetOptions();
      int ii = opts.find("v:");
      if (ii >= 0) {
         String sver(opts,ii+2);
         sver.erase(sver.find(','));
         hs->RemVers = atoi(sver.c_str());
      } else {
         hs->RemVers = Version;
         emsg = "server version information not found in options:"
                " assume same as local";
      }
      //
      // Create cache
      if (!(hs->Cref = new XrdSutPFEntry("c"))) {
         emsg = "error creating cache";
         return -1;
      }
      //
      // Save server version in cache
      hs->Cref->status = hs->RemVers;
      //
      // Extract list of crypto modules 
      String clist;
      ii = opts.find("c:");
      if (ii >= 0) {
         clist.assign(opts, ii+2);
         clist.erase(clist.find(','));
      } else {
         DEBUG("Crypto list missing: protocol error? (use defaults)");
         clist = DefCrypto;
      }
      // Parse the list loading the first we can
      if (ParseCrypto(clist) != 0) {
         emsg = "cannot find / load crypto requested modules :";
         emsg += clist;
         return -1;
      }
      //
      // Extract server certificate CA hashes
      String srvca;
      ii = opts.find("ca:");
      if (ii >= 0) {
         srvca.assign(opts, ii+3);
         srvca.erase(srvca.find(','));
      }
      // Parse the list loading the first we can
      if (ParseCAlist(srvca) != 0) {
         emsg = "unknown CA: cannot verify server certificate";
         return -1;
      }
      //
      // Load / Attach-to user proxies
      ProxyIn_t pi = {UsrCert.c_str(), UsrKey.c_str(), CAdir.c_str(),
                      UsrProxy.c_str(), PxyValid.c_str(),
                      DepLength, DefBits};
      ProxyOut_t po = {hs->PxyChain, sessionKsig, hs->Cbck };
      if (QueryProxy(1, &cachePxy, "Proxy:0",
                     sessionCF, hs->TimeStamp, &pi, &po) != 0) {
         emsg = "error getting user proxies";
         return -1;
      }
      // Save the result
      hs->PxyChain = po.chain;
      hs->Cbck = po.cbck;
      if (!(sessionKsig = sessionCF->RSA(*(po.ksig)))) {
         emsg = "could not get a copy of the signing key:";
         return -1;
      }
      //
      // And we are done;
      return 0;
   }
   //
   // make sure the cache is still there
   if (!hs->Cref) {
      emsg = "cache entry not found";
      return -1;
   }
   //
   // make sure is not too old
   int reftime = hs->TimeStamp - TimeSkew;
   if (hs->Cref->mtime < reftime) {
      emsg = "cache entry expired";
      // Remove: should not be checked a second time
      SafeDelete(hs->Cref);
      return -1;
   }
   //
   // Get from cache version run by server
   hs->RemVers = hs->Cref->status;

   //
   // Extract list of cipher algorithms supported by the server
   String cip = "";
   if ((bck = br->GetBucket(kXRS_cipher_alg))) {
      String ciplist;
      bck->ToString(ciplist);
      // Parse the list
      int from = 0;
      while ((from = ciplist.tokenize(cip, from, ':')) != -1) {
         if (cip.length() > 0) 
            if (sessionCF->SupportedCipher(cip.c_str()))
               break;
         cip = "";
      }
      if (cip.length() > 0)
         // COmmunicate to server
         br->UpdateBucket(cip, kXRS_cipher_alg);
   } else {
      DEBUG("WARNING: list of ciphers supported by server missing"
            " - using default");
   }

   //
   // Extract server public part for session cipher
   if (!(bck = br->GetBucket(kXRS_puk))) {
      emsg = "server public part for session cipher missing";
      return -1;
   }
   //
   // Initialize session cipher
   SafeDelete(sessionKey);
   if (!(sessionKey =
         sessionCF->Cipher(0,bck->buffer,bck->size,cip.c_str()))) {
            DEBUG("could not instantiate session cipher "
                  "using cipher public info from server");
            emsg = "could not instantiate session cipher ";
   }
   //
   // Extract server certificate
   if (!(bck = br->GetBucket(kXRS_x509))) {
      emsg = "server certificate missing";
      return -1;
   }
   //
   // Finalize chain: get a copy of it (we do not touch the reference)
   hs->Chain = new X509Chain(hs->Chain);
   if (!(hs->Chain)) {
      emsg = "cannot suplicate reference chain";
      return -1;
   }
   // Get hook to parsing function
   XrdCryptoX509ParseBucket_t ParseBucket = sessionCF->X509ParseBucket();
   if (!ParseBucket) {
      emsg = "cannot attach to ParseBucket function!";
      return -1;
   }
   // Parse bucket
   int nci = (*ParseBucket)(bck, hs->Chain);
   if (nci != 1) {
      emsg += nci;
      emsg += " vs 1 expected)";
      return -1;
   }
   //
   // Verify the chain
   x509ChainVerifyOpt_t vopt = { 0, hs->TimeStamp, -1, hs->Crl};
   XrdCryptoX509Chain::EX509ChainErr ecode = XrdCryptoX509Chain::kNone;
   if (!(hs->Chain->Verify(ecode, &vopt))) {
      emsg = "certificate chain verification failed: ";
      emsg += hs->Chain->LastError();
      return -1;
   }
   //
   // Extract the server key
   sessionKver = sessionCF->RSA(*(hs->Chain->End()->PKI()));
   if (!sessionKver || !sessionKver->IsValid()) {
      emsg = "server certificate contains an invalid key";
      return -1;
   }

   // Deactivate what not needed any longer
   br->Deactivate(kXRS_puk);
   br->Deactivate(kXRS_x509);

   //
   // Extract list of MD algorithms supported by the server
   String md = "";
   if ((bck = br->GetBucket(kXRS_md_alg))) {
      String mdlist;
      bck->ToString(mdlist);
      // Parse the list
      int from = 0;
      while ((from = mdlist.tokenize(md, from, ':')) != -1) {
         if (md.length() > 0) 
            if (sessionCF->SupportedMsgDigest(md.c_str()))
               break;
         md = "";
      }
   } else {
      DEBUG("WARNING: list of digests supported by server missing"
            " - using default");
      md = "md5";
   }
   if (!(sessionMD = sessionCF->MsgDigest(md.c_str()))) {
      emsg = "could not instantiate digest object";
      return -1;
   }
   // Communicate choice to server
   br->UpdateBucket(md, kXRS_md_alg);

   //
   // Extract the main buffer (it contains the random challenge
   // and will contain our credentials encrypted)
   XrdSutBucket *bckm = 0;
   if (!(bckm = br->GetBucket(kXRS_main))) {
      emsg = "main buffer missing";
      return -1;
   }

   //
   // Deserialize main buffer
   if (!((*bm) = new XrdSutBuffer(bckm->buffer,bckm->size))) {
      emsg = "error deserializing main buffer";
      return -1;
   }

   // We are done
   return 0;
}

//_________________________________________________________________________
int XrdSecProtocolgsi::ParseServerInput(XrdSutBuffer *br, XrdSutBuffer **bm,
                                        String &cmsg)
{
   // Parse received buffer b, extracting and decrypting the main 
   // buffer *bm and extracting the session 
   // cipher, random tag buckets and user name, if any.
   // Results used to fill the local handshake variables
   EPNAME("ParseServerInput");

   // Space for pointer to main buffer must be already allocated
   if (!br || !bm) {
      DEBUG("invalid inputs ("<<br<<","<<bm<<")");
      cmsg = "invalid inputs";
      return -1;
   }
   XrdSutBucket *bck = 0;
   XrdSutBucket *bckm = 0;
   //
   // Get the step
   int step = br->GetStep();
   //
   // Extract the main buffer 
   if (!(bckm = br->GetBucket(kXRS_main))) {
      cmsg = "main buffer missing";
      return -1;
   }
   //
   // The first iteration we just need to check if we can verify
   // the client identity
   if (step == kXGC_certreq) {
      //
      // Extract bucket with crypto module
      if (!(bck = br->GetBucket(kXRS_cryptomod))) {
         cmsg = "crypto module specification missing";
         return -1;
      }
      String cmod;
      bck->ToString(cmod);
      // Parse the list loading the first we can
      if (ParseCrypto(cmod) != 0) {
         cmsg = "cannot find / load crypto requested module :";
         cmsg += cmod;
         return -1;
      }
      //
      // Get version run by client, if there
      if (br->UnmarshalBucket(kXRS_version,hs->RemVers) != 0) {
         hs->RemVers = Version;
         cmsg = "client version information not found in options:"
                " assume same as local";
      } else {
         br->Deactivate(kXRS_version);
      }
      //
      // Extract bucket with client issuer hash
      if (!(bck = br->GetBucket(kXRS_issuer_hash))) {
         cmsg = "client issuer hash missing";
         return -1;
      }
      String cahash;
      bck->ToString(cahash);
      //
      // Check if we know it
      if (ParseCAlist(cahash) != 0) {
         cmsg = "unknown CA: cannot verify client credentials";
         return -1;
      }
      // Find our certificate in cache
      XrdSutPFEntry *cent = 0;
      if (!(cent = cacheCert.Get(sessionCF->Name()))) {
         cmsg = "cannot find certificate: corruption?";
         return -1;
      }
      // Check validity and run renewal for proxies or fail
      if (cent->mtime < hs->TimeStamp) {
         if (cent->status == kPFE_special) {
            // Try init proxies
            ProxyIn_t pi = {SrvCert.c_str(), SrvKey.c_str(), CAdir.c_str(),
                            UsrProxy.c_str(), PxyValid.c_str(), 0, 512};
            X509Chain *ch = 0;
            XrdCryptoRSA *k = 0;
            XrdSutBucket *b = 0;
            ProxyOut_t po = {ch, k, b };
            if (QueryProxy(0, &cacheCert, sessionCF->Name(),
                           sessionCF, hs->TimeStamp, &pi, &po) != 0) {
               cmsg = "proxy expired and cannot be renewed";
               return -1;
            }
         } else {
            cmsg = "certificate has expired - go and get a new one";
            return -1;
         }
      }


      // Fill some relevant handshake variables
      sessionKsig = sessionCF->RSA(*((XrdCryptoRSA *)(cent->buf2.buf)));
      hs->Cbck = (XrdSutBucket *)(cent->buf3.buf);

      // Create a handshake cache 
      if (!(hs->Cref = new XrdSutPFEntry(hs->ID.c_str()))) {
         cmsg = "cannot create cache entry";
         return -1;
      }
      //
      // Deserialize main buffer
      if (!((*bm) = new XrdSutBuffer(bckm->buffer,bckm->size))) {
         cmsg = "error deserializing main buffer";
         return -1;
      }

      // Deactivate what not need any longer
      br->Deactivate(kXRS_issuer_hash);

      // We are done
      return 0;
   }

   //
   // Extract cipher algorithm chosen by the client
   String cip = "";
   if ((bck = br->GetBucket(kXRS_cipher_alg))) {
      bck->ToString(cip);
      // Parse the list
      if (DefCipher.find(cip) == -1) {
         cmsg = "unsupported cipher chosen by the client";
         return -1;
      }
      // Deactivate the bucket
      br->Deactivate(kXRS_cipher_alg);
   } else {
      DEBUG("WARNING: client choice for cipher missing"
            " - using default");
   }

   // Second iteration: first get the session cipher
   if ((bck = br->GetBucket(kXRS_puk))) {
      //
      // Cleanup
      SafeDelete(sessionKey);
      //
      // Prepare cipher agreement: make sure we have the reference cipher
      if (!hs->Rcip) {
         cmsg = "reference cipher missing";
         return -1;
      }
      // Prepare cipher agreement: get a copy of the reference cipher
      if (!(sessionKey = sessionCF->Cipher(*(hs->Rcip)))) {
         cmsg = "cannot get reference cipher";
         return -1;
      }
      //
      // Instantiate the session cipher 
      if (!(sessionKey->Finalize(bck->buffer,bck->size,cip.c_str()))) {
         cmsg = "cannot finalize session cipher";
         return -1;
      }
      //
      // We need it only once
      br->Deactivate(kXRS_puk);
   }

   //
   // Decrypt the main buffer with the session cipher, if available
   if (sessionKey) {
      if (!(sessionKey->Decrypt(*bckm))) {
         cmsg = "error decrypting main buffer with session cipher";
         return -1;
      }
   }
   //
   // Deserialize main buffer
   if (!((*bm) = new XrdSutBuffer(bckm->buffer,bckm->size))) {
      cmsg = "error deserializing main buffer";
      return -1;
   }
   //
   // Get version run by client, if there
   if (hs->RemVers == -1) {
      if ((*bm)->UnmarshalBucket(kXRS_version,hs->RemVers) != 0) {
         hs->RemVers = Version;
         cmsg = "client version information not found in options:"
                " assume same as local";
      } else {
        (*bm)->Deactivate(kXRS_version);
      }
   }

   //
   // Get cache entry 
   if (!hs->Cref) {
      cmsg = "session cache has gone";
      return -1;
   }
   //
   // make sure cache is not too old
   int reftime = hs->TimeStamp - TimeSkew;
   if (hs->Cref->mtime < reftime) {
      cmsg = "cache entry expired";
      SafeDelete(hs->Cref);
      return -1;
   }

   //
   // Extract the client certificate
   if (!(bck = (*bm)->GetBucket(kXRS_x509))) {
      cmsg = "client certificate missing";
      SafeDelete(hs->Cref);
      return -1;
   }
   //
   // Finalize chain: get a copy of it (we do not touch the reference)
   hs->Chain = new X509Chain(hs->Chain);
   if (!(hs->Chain)) {
      cmsg = "cannot suplicate reference chain";
      return -1;
   }
   // Get hook to parsing function
   XrdCryptoX509ParseBucket_t ParseBucket = sessionCF->X509ParseBucket();
   if (!ParseBucket) {
      cmsg = "cannot attach to ParseBucket function!";
      return -1;
   }
   // Parse bucket
   int nci = (*ParseBucket)(bck, hs->Chain);
   if (nci < 2) {
      cmsg = "wrong number of certificates in received bucket (";
      cmsg += nci;
      cmsg += " > 1 expected)";
      return -1;
   }
   //
   // Verify the chain
   x509ChainVerifyOpt_t vopt = { 0, hs->TimeStamp, -1, hs->Crl};
   XrdCryptoX509Chain::EX509ChainErr ecode = XrdCryptoX509Chain::kNone;
   if (!(hs->Chain->Verify(ecode, &vopt))) {
      cmsg = "certificate chain verification failed: ";
      cmsg += hs->Chain->LastError();
      return -1;
   }

   //
   // Extract the client public key
   sessionKver = sessionCF->RSA(*(hs->Chain->End()->PKI()));
   if (!sessionKver || !sessionKver->IsValid()) {
      cmsg = "server certificate contains an invalid key";
      return -1;
   }
   // Deactivate certificate buffer 
   (*bm)->Deactivate(kXRS_x509);

   //
   // Extract the MD algorithm chosen by the client
   String md = "";
   if ((bck = br->GetBucket(kXRS_md_alg))) {
      String mdlist;
      bck->ToString(md);
      // Parse the list
      if (DefMD.find(md) == -1) {
         cmsg = "unsupported MD chosen by the client";
         return -1;
      }
      // Deactivate
      br->Deactivate(kXRS_md_alg);
   } else {
      DEBUG("WARNING: client choice for digests missing"
            " - using default");
      md = "md5";
   }
   if (!(sessionMD = sessionCF->MsgDigest(md.c_str()))) {
      cmsg = "could not instantiate digest object";
      return -1;
   }

   //
   // We are done
   return 0;
}

//__________________________________________________________________
void XrdSecProtocolgsi::ErrF(XrdOucErrInfo *einfo, kXR_int32 ecode,
                             const char *msg1, const char *msg2,
                             const char *msg3)
{
   // Filling the error structure
   EPNAME("ErrF");

   char *msgv[12];
   int k, i = 0, sz = strlen("Secgsi");

   //
   // Code message, if any
   int cm = (ecode >= kGSErrParseBuffer && 
             ecode <= kGSErrError) ? (ecode-kGSErrParseBuffer) : -1;
   const char *cmsg = (cm > -1) ? gGSErrStr[cm] : 0;

   //
   // Build error message array
              msgv[i++] = (char *)"Secgsi";     //0
   if (cmsg) {msgv[i++] = (char *)": ";         //1
              msgv[i++] = (char *)cmsg;         //2
              sz += strlen(msgv[i-1]) + 2;
             }
   if (msg1) {msgv[i++] = (char *)": ";         //3
              msgv[i++] = (char *)msg1;         //4
              sz += strlen(msgv[i-1]) + 2;
             }
   if (msg2) {msgv[i++] = (char *)": ";         //5
              msgv[i++] = (char *)msg2;         //6
              sz += strlen(msgv[i-1]) + 2;
             }
   if (msg3) {msgv[i++] = (char *)": ";         //7
              msgv[i++] = (char *)msg3;         //8
              sz += strlen(msgv[i-1]) + 2;
             }

   // save it (or print it)
   if (einfo) {
      einfo->setErrInfo(ecode, (const char **)msgv, i);
   }
   if (QTRACE(Debug)) {
      char *bout = new char[sz+10];
      if (bout) {
         bout[0] = 0;
         for (k = 0; k < i; k++)
            sprintf(bout,"%s%s",bout,msgv[k]);
         DEBUG(bout);
      } else {
         for (k = 0; k < i; k++)
            DEBUG(msgv[k]);
      }
   }
}

//__________________________________________________________________
XrdSecCredentials *XrdSecProtocolgsi::ErrC(XrdOucErrInfo *einfo,
                                           XrdSutBuffer *b1,
                                           XrdSutBuffer *b2,
                                           XrdSutBuffer *b3,
                                           kXR_int32 ecode,
                                           const char *msg1,
                                           const char *msg2,
                                           const char *msg3)
{
   // Error logging client method

   // Fill the error structure
   ErrF(einfo, ecode, msg1, msg2, msg3);

   // Release buffers
   REL3(b1,b2,b3);

   // We are done
   return (XrdSecCredentials *)0;
}

//__________________________________________________________________
int XrdSecProtocolgsi::ErrS(String ID, XrdOucErrInfo *einfo,
                            XrdSutBuffer *b1, XrdSutBuffer *b2,
                            XrdSutBuffer *b3, kXR_int32 ecode,
                            const char *msg1, const char *msg2,
                            const char *msg3)
{
   // Error logging server method

   // Fill the error structure
   ErrF(einfo, ecode, msg1, msg2, msg3);

   // Release buffers
   REL3(b1,b2,b3);

   // We are done
   return kgST_error;
}

//______________________________________________________________________________
bool XrdSecProtocolgsi::CheckRtag(XrdSutBuffer *bm, String &emsg)
{
   // Check random tag signature if it was sent with previous packet
   EPNAME("CheckRtag");

   // Make sure we got a buffer
   if (!bm) {
      emsg = "Buffer not defined";
      return 0;
   }
   //
   // If we sent out a random tag check it signature
   if (hs->Cref && hs->Cref->buf1.len > 0) {
      XrdSutBucket *brt = 0;
      if ((brt = bm->GetBucket(kXRS_signed_rtag))) {
         // Make sure we got the right key to decrypt
         if (!(sessionKver)) {
            emsg = "Session cipher undefined";
            return 0;
         }
         // Decrypt it with the counter part public key
         if (!(sessionKver->DecryptPublic(*brt))) {
            emsg = "error decrypting random tag with public key";
            return 0;
         }
      } else {
         emsg = "random tag missing - protocol error";
         return 0;
      } 
      //
      // Random tag cross-check: content
      if (memcmp(brt->buffer,hs->Cref->buf1.buf,hs->Cref->buf1.len)) {
         emsg = "random tag content mismatch";
         SafeDelete(hs->Cref);
         // Remove: should not be checked a second time
         return 0;
      }
      //
      // Reset the cache entry but we will not use the info a second time
      memset(hs->Cref->buf1.buf,0,hs->Cref->buf1.len);
      hs->Cref->buf1.SetBuf();
      //
      // Flag successful check
      hs->RtagOK = 1;
      bm->Deactivate(kXRS_signed_rtag);
      DEBUG("Random tag successfully checked");
   } else {
      DEBUG("Nothing to check");
   }

   // We are done
   return 1;
}

//______________________________________________________________________________
int XrdSecProtocolgsi::LoadCADir(int timestamp)
{
   // Scan cadir for valid CA certificates and load them in memory
   // in cache ca.
   // Return 0 if ok, -1 if problems
   EPNAME("LoadCADir");

   // Some global statics
   int opt = XrdSecProtocolgsi::CRLCheck;
   String cadir = XrdSecProtocolgsi::CAdir;
   XrdSutCache *ca = &(XrdSecProtocolgsi::cacheCA);

   // Open directory
   DIR *dd = opendir(cadir.c_str());
   if (!dd) {
      DEBUG("could not open directory: "<<cadir<<" (errno: "<<errno<<")");
      return -1;
   }

   // Init cache
   if (!ca || ca->Init(100) != 0) {
      DEBUG("problems init cache for CA info");
      return -1;
   }
  
   // Read the content
   int i = 0;
   XrdCryptoX509ParseFile_t ParseFile = 0;
   String enam(cadir.length()+100); 
   struct dirent *dent = 0;
   while ((dent = readdir(dd))) {
      // entry name
      enam = cadir + dent->d_name;
      DEBUG("analysing entry "<<enam);
      // Try to init a chain: for each crypto factory
      for (i = 0; i < ncrypt; i++) {
         X509Chain *chain = new X509Chain();
         // Get the parse function
         ParseFile = cryptF[i]->X509ParseFile();
         int nci = (*ParseFile)(enam.c_str(), chain);
         bool ok = 0;
         XrdCryptoX509Crl *crl = 0;
         // Check what we got
         if (chain && nci == 1 && chain->CheckCA()) {
            // Get CRL, if required
            if (opt > 0)
               crl = LoadCRL(chain->Begin(), cryptF[i]);
            // Apply requirements
            if (opt < 2 || crl) {
               if (opt < 3 ||
                  (opt == 3 && crl && !(crl->IsExpired(timestamp)))) {
                  // Good CA
                  ok = 1;
               } else {
                  DEBUG("CRL is expired (opt: "<<opt<<")");
               }
            } else {
               DEBUG("CRL is missing (opt: "<<opt<<")");
            }
         }
         //
         if (ok) {
            // Save the chain: create the tag first
            String tag(chain->Begin()->SubjectHash());
            tag += ':';
            tag += cryptID[i];
            // Add to the cache
            XrdSutPFEntry *cent = ca->Add(tag.c_str());
            if (cent) {
               cent->buf1.buf = (char *)chain;
               cent->buf1.len = 1;      // Just a flag
               if (crl) {
                  cent->buf2.buf = (char *)crl;
                  cent->buf2.len = 1;      // Just a flag
               }
               cent->mtime = timestamp;
               cent->status = kPFE_ok;
               cent->cnt = 0;
            }
         } else {
            if (chain)
               chain->Cleanup();
            SafeDelete(chain);
            SafeDelete(crl);
         }
      }
   }

   // Close dir
   closedir(dd);

   // Rehash cache
   ca->Rehash(1);

   // We are done
   return 0;
}

//______________________________________________________________________________
XrdCryptoX509Crl *XrdSecProtocolgsi::LoadCRL(XrdCryptoX509 *xca,
                                             XrdCryptoFactory *CF)
{
   // Scan crldir for a valid CRL certificate associated to CA whose
   // certificate is xca. If the CRL is found and is valid according
   // to the chosen option, return its content in a X509Crl object.
   // Return 0 in any other case
   EPNAME("LoadCRL");
   XrdCryptoX509Crl *crl = 0;

   // The dir
   String crldir = XrdSecProtocolgsi::CRLdir;
   String crlext = XrdSecProtocolgsi::DefCRLext;

   // make sure we got what we need
   if (crldir.length() <= 0 || !xca || !CF) {
      DEBUG("Invalid inputs");
      return crl;
   }

   // Try first the target file
   String crlfile(crldir.length()+100); 
   // Get the CA hash
   String cahash = xca->SubjectHash();
   // Drop the extension (".0")
   String caroot(cahash, 0, cahash.find(".0")-1);
   // Add the default CRL extension and the dir
   crlfile = crldir + caroot;
   crlfile += crlext;
   DEBUG("target file: "<<crlfile);
   // Try to init a crl
   if ((crl = CF->X509Crl(crlfile.c_str()))) {
      // Verify issuer
      if (!(strcmp(crl->Issuer(),xca->Subject()))) {
         // Verify signature
         if (crl->Verify(xca)) {
            // Ok, we are done
            return crl;
         }
      }
   }

   // We need to parse the full dir: make sime cleanup first
   SafeDelete(crl);

   // Open directory
   DIR *dd = opendir(crldir.c_str());
   if (!dd) {
      DEBUG("could not open directory: "<<crldir<<" (errno: "<<errno<<")");
      return crl;
   }
  
   // Read the content
   struct dirent *dent = 0;
   while ((dent = readdir(dd))) {
      // Do not analyse the CA certificate
      if (!strcmp(cahash.c_str(),dent->d_name)) continue;
      // File name contain the root CA hash
      if (!strstr(dent->d_name,caroot.c_str())) continue;
      // candidate name
      crlfile = crldir + dent->d_name;
      DEBUG("analysing entry "<<crlfile);
      // Try to init a crl
      crl = CF->X509Crl(crlfile.c_str());
      if (!crl) continue;
      // Verify issuer
      if (strcmp(crl->Issuer(),xca->Subject())) {
         SafeDelete(crl);
         continue;
      }
      // Verify signature
      if (!(crl->Verify(xca))) {
         SafeDelete(crl);
         continue;
      }
      // Ok
      break;
   }

   // Close dir
   closedir(dd);

   // We are done
   return crl;
}

//______________________________________________________________________________
int XrdSecProtocolgsi::GetCA(const char *cahash)
{
   // Gets entry for CA with hash cahash for crypt factory cryptF[ic].
   // If not found in cache, try loading from <CAdir>/<cahash>.0 .
   // Return 0 if ok, -1 if not available, -2 if CRL not ok
   EPNAME("GetCA");

   // We nust have got a CA hash
   if (!cahash) {
      DEBUG("Invalid input ");
      return -1;
   }

   // The tag
   String tag(cahash,20);
   tag += ':';
   tag += sessionCF->ID();
   DEBUG("Querying cache for tag: "<<tag);

   // Try first the cache
   XrdSutPFEntry *cent = cacheCA.Get(tag.c_str());

   // If found, we are done
   if (cent) {
      hs->Chain = (X509Chain *)(cent->buf1.buf);
      hs->Crl = (XrdCryptoX509Crl *)(cent->buf2.buf);
      return 0;
   }

   // If not, prepare the file name   
   String fnam = CAdir + cahash; 
   DEBUG("trying to load CA certificate from "<<fnam);

   // Create chain
   hs->Chain = new X509Chain();
   if (!hs->Chain ) {
      DEBUG("could not create new GSI chain");
      return -1;
   }

   // Get the parse function
   XrdCryptoX509ParseFile_t ParseFile = sessionCF->X509ParseFile();
   if (ParseFile) {
      int nci = (*ParseFile)(fnam.c_str(), hs->Chain);
      bool ok = 0;
      if (nci == 1 && hs->Chain->CheckCA()) {

         // Get CRL, if required
         if (CRLCheck > 0)
            hs->Crl = LoadCRL(hs->Chain->Begin(), sessionCF);
         // Apply requirements
         if (CRLCheck < 2 || hs->Crl) {
            if (CRLCheck < 3 ||
               (CRLCheck == 3 &&
                hs->Crl && !(hs->Crl->IsExpired(hs->TimeStamp)))) {
               // Good CA
               ok = 1;
            } else {
               DEBUG("CRL is expired (CRLCheck: "<<CRLCheck<<")");
            }
         } else {
            DEBUG("CRL is missing (CRLCheck: "<<CRLCheck<<")");
         }
         //
         if (ok) {
            // Add to the cache
            cent = cacheCA.Add(tag.c_str());
            if (cent) {
               cent->buf1.buf = (char *)(hs->Chain);
               cent->buf1.len = 1;      // Just a flag
               if (hs->Crl) {
                  cent->buf2.buf = (char *)(hs->Crl);
                  cent->buf2.len = 1;      // Just a flag
               }
               cent->mtime = hs->TimeStamp;
               cent->status = kPFE_ok;
               cent->cnt = 0;
            }
         } else {
            return -2;
         }
      } else {
         DEBUG("certificate not found or invalid (nci: "<<nci<<", CA: "<<
               (int)(hs->Chain->CheckCA())<<")");
         return -1;
      }
   }

   // Rehash cache
   cacheCA.Rehash(1);

   // We are done
   return 0;
}

//______________________________________________________________________________
int XrdSecProtocolgsi::InitProxy(ProxyIn_t *pi, X509Chain *ch, XrdCryptoRSA **kp)
{
   // Invoke 'grid-proxy-init' via the shell to create a valid the proxy file
   // If the variable GLOBUS_LOCATION is defined it prepares the external shell
   // by sourcing $GLOBUS_LOCATION/etc/globus-user-env.sh .
   // Return 0 in cse of success, != 0 in any other case .
   EPNAME("InitProxy");
   int rc = 0;

   // We must be able to get an answer
   if (isatty(0) == 0 || isatty(1) == 0) {
      DEBUG("Not a tty: cannot prompt for proxies - do nothing ");
      return -1;
   }

#ifndef HASGRIDPROXYINIT
   //
   // Use internal function for proxy initialization
   //
   // Make sure we got a chain and a key to fill
   if (!ch || !kp) {
      DEBUG("chain or key container undefined");
      return -1;
   }
   //
   // Validity
   int valid = (pi->valid) ? XrdSutParseTime(pi->valid, 1) : -1;
   //
   // Options
   XrdProxyOpt_t pxopt = {pi->bits,    // bits in key
                          valid,       // duration validity in secs
                          pi->deplen}; // signature path depth
   //
   // Init now
   rc = XrdSslgsiX509CreateProxy(pi->cert, pi->key, &pxopt,
                                 ch, kp, pi->out);
#else
   // command string
   String cmd(kMAXBUFLEN);

   // Check if GLOBUS_LOCATION is defined
   if (getenv("GLOBUS_LOCATION"))
      cmd = "source $GLOBUS_LOCATION/etc/globus-user-env.sh;";

   // Add main command
   cmd += " grid-proxy-init";

   // Add user cert
   cmd += " -cert ";
   cmd += pi->cert;

   // Add user key
   cmd += " -key ";
   cmd += pi->key;

   // Add CA dir
   cmd += " -certdir ";
   cmd += pi->certdir;

   // Add validity
   if (pi->valid) {
      cmd += " -valid ";
      cmd += pi->valid;
   }

   // Add number of bits in key
   if (pi->bits > 512) {
      cmd += " -bits ";
      cmd += pi->bits;
   }

   // Add depth of signature path
   if (pi->deplen > -1) {
      cmd += " -path-length ";
      cmd += pi->deplen;
   }

   // Add output proxy coordinates
   if (pi->out) {
      cmd += " -out ";
      cmd += pi->out;
   }
   // Notify
   DEBUG("executing: " << cmd);

   // Execute
   rc = system(cmd.c_str());
   DEBUG("return code: "<< rc << " (0x"<<(int *)rc<<")");
#endif

   // We are done
   return rc;
}

//__________________________________________________________________________
int XrdSecProtocolgsi::ParseCAlist(String calist)
{
   // Parse received ca list, find the first available CA in the list
   // and return a chain initialized with such a CA.
   // If nothing found return 0.
   EPNAME("ParseCAlist");

   // Check inputs
   if (calist.length() <= 0) {
      DEBUG("nothing to parse");
      return -1;
   }
   DEBUG("parsing list: "<<calist);
 
   // Load module and define relevant pointers
   hs->Chain = 0;
   String cahash = "";
   // Parse list
   if (calist.length()) {
      int from = 0;
      while ((from = calist.tokenize(cahash, from, '|')) != -1) {
         // Check this hash
         if (cahash.length()) {
            // Get the CA chain
            if (GetCA(cahash.c_str()) == 0)
               return 0;
         }
      }
   }

   // We did not find it
   return -1;
}

//__________________________________________________________________________
int XrdSecProtocolgsi::ParseCrypto(String clist)
{
   // Parse crypto list clist, extracting the first available module
   // and getting a related local cipher and a related reference
   // cipher to be used to agree the session cipher; the local lists
   // crypto info is updated, if needed
   // The results are used to fill the handshake part of the protocol
   // instance.
   EPNAME("ParseCrypto");

   // Check inputs
   if (clist.length() <= 0) {
      DEBUG("empty list: nothing to parse");
      return -1;
   }
   DEBUG("parsing list: "<<clist);
 
   // Load module and define relevant pointers
   hs->CryptoMod = "";

   // Parse list
   int from = 0;
   while ((from = clist.tokenize(hs->CryptoMod, from, '|')) != -1) {
      // Check this module
      if (hs->CryptoMod.length() > 0) {
         DEBUG("found module: "<<hs->CryptoMod);
         // Load the crypto factory
         if ((sessionCF = 
              XrdCryptoFactory::GetCryptoFactory(hs->CryptoMod.c_str()))) {
            int fid = sessionCF->ID();
            int i = 0;
            // Retrieve the index in local table
            while (i < ncrypt) {
               if (cryptID[i] == fid) break;
               i++;
            }
            if (i >= ncrypt) {
               if (ncrypt == XrdCryptoMax) {
                  DEBUG("max number of crypto slots reached - do nothing");
                  return 0;
               } else {
                  // Add new entry
                  cryptF[i] = sessionCF;
                  cryptID[i] = fid;
                  ncrypt++;
               }
            }
            // On servers the ref cipher should be defined at this point
            hs->Rcip = refcip[i];
            // we are done
            return 0;
         }
      }
   }

   // Nothing found
   return -1;
}

//__________________________________________________________________________
int XrdSecProtocolgsi::QueryProxy(bool checkcache, XrdSutCache *cache,
                                  const char *tag, XrdCryptoFactory *cf,
                                  int timestamp, ProxyIn_t *pi, ProxyOut_t *po)
{
   // Query users proxies, initializing if needed
   EPNAME("QueryProxy");

   bool hasproxy = 0;
   // We may already loaded valid proxies
   XrdSutPFEntry *cent = 0;
   if (checkcache) {
      cent = cache->Get(tag);
      if (cent && cent->buf1.buf) {
         //
         po->chain = (X509Chain *)(cent->buf1.buf);
         // Check validity of the entry found (it may have expired)
         if (po->chain->CheckValidity(1, timestamp) == 0) {
            po->ksig = (XrdCryptoRSA *)(cent->buf2.buf);
            po->cbck = (XrdSutBucket *)(cent->buf3.buf);
            hasproxy = 1;
            return 0;
         } else {
            // Cleanup the chain
            po->chain->Cleanup();
            // Cleanup cache entry
            cent->buf1.buf = 0;
            cent->buf1.len = 0;
            // The key is deleted by the certificate destructor
            // Just reset the buffer
            cent->buf2.buf = 0;
            cent->buf2.len = 0;
            // and the related bucket
            if (cent->buf3.buf)
               delete (XrdSutBucket *)(cent->buf3.buf);
            cent->buf3.buf = 0;
            cent->buf3.len = 0;
         }
      }
   }
   //
   // We do not have good proxies, try load (user may have initialized
   // them in the meanwhile)
   // Create a new chain first, if needed
   if (!(po->chain))
      po->chain = new X509Chain();
   if (!(po->chain)) {
      DEBUG("cannot create new chain!");
      return -1;
   }
   int ntry = 2;
   bool parsefile = 1;
   XrdCryptoX509ParseFile_t ParseFile = 0;
   while (!hasproxy && ntry > 0) {

      // Try init as last option
      if (ntry == 1) {

         // Cleanup the chain
         po->chain->Cleanup();

         if (InitProxy(pi, po->chain, &(po->ksig)) != 0) {
            DEBUG("problems initializing proxy via external shell");
            ntry--;
            continue;
         }
#ifndef HASGRIDPROXYINIT
         // Chain is already loaded if we used the internal function
         // to initialize the proxies
         parsefile = 0;
         timestamp = (int)(time(0));
#endif
      }
      ntry--;

      if (parsefile) {
         if (!ParseFile) {
            if (!(ParseFile = cf->X509ParseFile())) {
               DEBUG("cannot attach to ParseFile function!");
               continue;
            }
         }
         
         // Parse the proxy file
         int nci = (*ParseFile)(pi->out, po->chain);
         if (nci < 2) {
            DEBUG("proxy files must have at least two certificates"
                  " (found: "<<nci<<")");
            continue;
         }
      }

      // Check validity in time
      if (po->chain->CheckValidity(1, timestamp) != 0) {
         DEBUG("proxy files contains expired certificates");
         continue;
      }

      // Reorder chain
      if (po->chain->Reorder() != 0) {
         DEBUG("proxy files contains inconsistent certificates");
         continue;
      }

      // Check key
      po->ksig = po->chain->End()->PKI();
      if (po->ksig->status != XrdCryptoRSA::kComplete) {
         DEBUG("proxy files contain invalid key pair");
         continue;
      }

      XrdCryptoX509ExportChain_t ExportChain = cf->X509ExportChain();
      if (!ExportChain) {
         DEBUG("cannot attach to ExportChain function!");
         continue;
      }

      // Create bucket for export
      po->cbck = (*ExportChain)(po->chain);
      if (!(po->cbck)) {
         DEBUG("could not create bucket for export");
         continue;
      }

      // Get attach an entry in cache
      if (!(cent = cache->Add(tag))) {
         DEBUG("could not create entry in cache");
         continue;
      }

      // Save info in cache
      cent->mtime = po->chain->End()->NotAfter(); // the expiring time
      cent->status = kPFE_special;  // distinguish from normal certs
      cent->cnt = 0;
      // The chain
      cent->buf1.buf = (char *)(po->chain);
      cent->buf1.len = 1;      // Just a flag
      // The key
      cent->buf2.buf = (char *)(po->chain->End()->PKI());
      cent->buf2.len = 1;      // Just a flag
      // The export bucket
      cent->buf3.buf = (char *)(po->cbck);
      cent->buf3.len = 1;      // Just a flag

      // Rehash cache
      cache->Rehash(1);

      // Set the positive flag
      hasproxy = 1;
   }

   // We are done
   if (!hasproxy) {
      // Some cleanup
      po->chain->Cleanup();
      SafeDelete(po->chain);
      SafeDelete(po->cbck);
      return -1;
   }
   return 0;
}
