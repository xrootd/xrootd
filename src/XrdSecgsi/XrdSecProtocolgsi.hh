/******************************************************************************/
/*                                                                            */
/*                 X r d S e c P r o t o c o l g s i . h h                    */
/*                                                                            */
/* (c) 2005 G. Ganis / CERN                                                   */
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
#include <time.h>

#include "XrdNet/XrdNetAddrInfo.hh"

#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucGMap.hh"
#include "XrdOuc/XrdOucHash.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdOuc/XrdOucTokenizer.hh"

#include "XrdSys/XrdSysPthread.hh"

#include "XrdSec/XrdSecInterface.hh"
#include "XrdSecgsi/XrdSecgsiTrace.hh"

#include "XrdSut/XrdSutPFEntry.hh"
#include "XrdSut/XrdSutPFile.hh"
#include "XrdSut/XrdSutBuffer.hh"
#include "XrdSut/XrdSutRndm.hh"

#include "XrdCrypto/XrdCryptoAux.hh"
#include "XrdCrypto/XrdCryptoCipher.hh"
#include "XrdCrypto/XrdCryptoFactory.hh"
#include "XrdCrypto/XrdCryptoX509Crl.hh"

#include "XrdCrypto/XrdCryptogsiX509Chain.hh"

/******************************************************************************/
/*                               D e f i n e s                                */
/******************************************************************************/

typedef XrdOucString String;
typedef XrdCryptogsiX509Chain X509Chain;
  
#define XrdSecPROTOIDENT    "gsi"
#define XrdSecPROTOIDLEN    sizeof(XrdSecPROTOIDENT)
#define XrdSecgsiVERSION    10300
#define XrdSecNOIPCHK       0x0001
#define XrdSecDEBUG         0x1000
#define XrdCryptoMax        10

#define kMAXBUFLEN          1024

//
// Message codes either returned by server or included in buffers
enum kgsiStatus {
   kgST_error    = -1,      // error occured
   kgST_ok       =  0,      // ok
   kgST_more     =  1       // need more info
};

// Client steps
enum kgsiClientSteps {
   kXGC_none = 0,
   kXGC_certreq     = 1000, // 1000: request server certificate
   kXGC_cert,               // 1001: packet with (proxy) certificate
   kXGC_sigpxy,             // 1002: packet with signed proxy certificate
   kXGC_reserved            // 
};

// Server steps
enum kgsiServerSteps {
   kXGS_none = 0,
   kXGS_init       = 2000,   // 2000: fake code used the first time 
   kXGS_cert,                // 2001: packet with certificate 
   kXGS_pxyreq,              // 2002: packet with proxy req to be signed 
   kXGS_reserved             //
};

// Handshake options
enum kgsiHandshakeOpts {
   kOptsDlgPxy     = 1,      // 0x0001: Ask for a delegated proxy
   kOptsFwdPxy     = 2,      // 0x0002: Forward local proxy
   kOptsSigReq     = 4,      // 0x0004: Accept to sign delegated proxy
   kOptsSrvReq     = 8,      // 0x0008: Server request for delegated proxy
   kOptsPxFile     = 16,     // 0x0010: Save delegated proxies in file
   kOptsDelChn     = 32      // 0x0020: Delete chain
};

// Error codes
enum kgsiErrors {
   kGSErrParseBuffer = 10000,       // 10000
   kGSErrDecodeBuffer,              // 10001
   kGSErrLoadCrypto,                // 10002
   kGSErrBadProtocol,               // 10003
   kGSErrCreateBucket,              // 10004
   kGSErrDuplicateBucket,           // 10005
   kGSErrCreateBuffer,              // 10006
   kGSErrSerialBuffer,              // 10007
   kGSErrGenCipher,                 // 10008
   kGSErrExportPuK,                 // 10009
   kGSErrEncRndmTag,                // 10010
   kGSErrBadRndmTag,                // 10011
   kGSErrNoRndmTag,                 // 10012
   kGSErrNoCipher,                  // 10013
   kGSErrNoCreds,                   // 10014
   kGSErrBadOpt,                    // 10015
   kGSErrMarshal,                   // 10016
   kGSErrUnmarshal,                 // 10017
   kGSErrSaveCreds,                 // 10018
   kGSErrNoBuffer,                  // 10019
   kGSErrRefCipher,                 // 10020
   kGSErrNoPublic,                  // 10021
   kGSErrAddBucket,                 // 10022
   kGSErrFinCipher,                 // 10023
   kGSErrInit,                      // 10024
   kGSErrBadCreds,                  // 10025
   kGSErrError                      // 10026  
};

#define REL1(x)     { if (x) delete x; }
#define REL2(x,y)   { if (x) delete x; if (y) delete y; }
#define REL3(x,y,z) { if (x) delete x; if (y) delete y; if (z) delete z; }

#define SafeDelete(x) { if (x) delete x ; x = 0; }
#define SafeDelArray(x) { if (x) delete [] x ; x = 0; }
#define SafeFree(x) { if (x) free(x) ; x = 0; }

// External functions for generic mapping
typedef char *(*XrdSecgsiGMAP_t)(const char *, int);
typedef int (*XrdSecgsiAuthz_t)(XrdSecEntity &);
typedef int (*XrdSecgsiAuthzInit_t)(const char *);
typedef int (*XrdSecgsiAuthzKey_t)(XrdSecEntity &, char **);
// VOMS extraction
typedef XrdSecgsiAuthz_t XrdSecgsiVOMS_t;
typedef XrdSecgsiAuthzInit_t XrdSecgsiVOMSInit_t;
//
// This a small class to set the relevant options in one go
//
class XrdOucGMap;
class XrdOucTrace;
class gsiOptions {
public:
   short  debug;  // [cs] debug flag
   char   mode;   // [cs] 'c' or 's'
   char  *clist;  // [s] list of crypto modules ["ssl" ]
   char  *certdir;// [cs] dir with CA info [/etc/grid-security/certificates]
   char  *crldir; // [cs] dir with CRL info [/etc/grid-security/certificates]
   char  *crlext; // [cs] extension of CRL files [.r0]
   char  *cert;   // [s] server certificate [/etc/grid-security/root/rootcert.pem]
                  // [c] user certificate [$HOME/.globus/usercert.pem]
   char  *key;    // [s] server private key [/etc/grid-security/root/rootkey.pem]
                  // [c] user private key [$HOME/.globus/userkey.pem]
   char  *cipher; // [s] list of ciphers [aes-128-cbc:bf-cbc:des-ede3-cbc]
   char  *md;     // [s] list of MDs [sha1:md5]
   int    crl;    // [cs] check level of CRL's [1] 
   int    ca;     // [cs] verification level of CA's [1] 
   int    crlrefresh; // [cs] CRL refresh or expiration period in secs [1 day] 
   char  *proxy;  // [c] user proxy  [/tmp/x509up_u<uid>]
   char  *valid;  // [c] proxy validity  [12:00]
   int    deplen; // [c] depth of signature path for proxies [0] 
   int    bits;   // [c] bits in PKI for proxies [512] 
   char  *gridmap;// [s] gridmap file [/etc/grid-security/gridmap]
   int    gmapto; // [s] validity in secs of grid-map cache entries [600 s]
   char  *gmapfun;// [s] file with the function to map DN to usernames [0]
   char  *gmapfunparms;// [s] parameters for the function to map DN to usernames [0]
   char  *authzfun;// [s] file with the function to fill entities [0]
   char  *authzfunparms;// [s] parameters for the function to fill entities [0]
   int    authzto; // [s] validity in secs of authz cache entries [-1 => unlimited]
   int    ogmap;  // [s] gridmap file checking option 
   int    dlgpxy; // [c] explicitely ask the creation of a delegated proxy 
                  // [s] ask client for proxies
   int    sigpxy; // [c] accept delegated proxy requests 
   char  *srvnames;// [c] '|' separated list of allowed server names
   char  *exppxy; // [s] template for the exported file with proxies (dlgpxy == 3)
   int    authzpxy; // [s] if 1 make proxy available in exported form in the 'endorsement'
                    //     field of the XrdSecEntity object for use in XrdAcc
   int    vomsat; // [s] 0 do not look for; 1 extract if any
   char  *vomsfun;// [s] file with the function to fill VOMS [0]
   char  *vomsfunparms;// [s] parameters for the function to fill VOMS [0]
   int    moninfo; // [s] 0 do not look for; 1 use DN as default
   int    hashcomp; // [cs] 1 send hash names with both algorithms; 0 send only the default [1]

   gsiOptions() { debug = -1; mode = 's'; clist = 0; 
                  certdir = 0; crldir = 0; crlext = 0; cert = 0; key = 0;
                  cipher = 0; md = 0; ca = 1 ; crl = 1; crlrefresh = 86400;
                  proxy = 0; valid = 0; deplen = 0; bits = 512;
                  gridmap = 0; gmapto = 600;
                  gmapfun = 0; gmapfunparms = 0; authzfun = 0; authzfunparms = 0; authzto = -1;
                  ogmap = 1; dlgpxy = 0; sigpxy = 1; srvnames = 0;
                  exppxy = 0; authzpxy = 0;
                  vomsat = 1; vomsfun = 0; vomsfunparms = 0; moninfo = 0; hashcomp = 1; }
   virtual ~gsiOptions() { } // Cleanup inside XrdSecProtocolgsiInit
   void Print(XrdOucTrace *t); // Print summary of gsi option status
};

class XrdSecProtocolgsi;
class gsiHSVars;

// From a proxy query
typedef struct {
   X509Chain        *chain;
   XrdCryptoRSA     *ksig;
   XrdSutBucket     *cbck;
} ProxyOut_t;

// To query proxies
typedef struct {
   const char *cert;
   const char *key;
   const char *certdir;
   const char *out;
   const char *valid;
   int         deplen;
   int         bits;
} ProxyIn_t;


class GSICrlStack {
public:
   void Add(XrdCryptoX509Crl *crl) {
      char k[40]; snprintf(k, 40, "%p", crl); 
      mtx.Lock();
      if (!stack.Find(k)) stack.Add(k, crl, 0, Hash_count);
      stack.Add(k, crl, 0, Hash_count);
      mtx.UnLock();
   }
   void Del(XrdCryptoX509Crl *crl) {
      char k[40]; snprintf(k, 40, "%p", crl); 
      mtx.Lock();
      if (stack.Find(k)) stack.Del(k, Hash_count);
      mtx.UnLock();
   }
private:
   XrdSysMutex                  mtx;
   XrdOucHash<XrdCryptoX509Crl> stack;
};


/******************************************************************************/
/*              X r d S e c P r o t o c o l g s i   C l a s s                 */
/******************************************************************************/

class XrdSecProtocolgsi : public XrdSecProtocol
{
friend class gsiOptions;
friend class gsiHSVars;
public:
        int                Authenticate  (XrdSecCredentials *cred,
                                          XrdSecParameters **parms,
                                          XrdOucErrInfo     *einfo=0);

        XrdSecCredentials *getCredentials(XrdSecParameters  *parm=0,
                                          XrdOucErrInfo     *einfo=0);

        XrdSecProtocolgsi(int opts, const char *hname, XrdNetAddrInfo &endPoint,
                                    const char *parms = 0);
        virtual ~XrdSecProtocolgsi() {} // Delete() does it all

        // Initialization methods
        static char      *Init(gsiOptions o, XrdOucErrInfo *erp);

        void              Delete();

        // Encrypt / Decrypt methods
        int               Encrypt(const char *inbuf, int inlen,
                                  XrdSecBuffer **outbuf);
        int               Decrypt(const char *inbuf, int inlen,
                                  XrdSecBuffer **outbuf);
        // Sign / Verify methods
        int               Sign(const char *inbuf, int inlen,
                               XrdSecBuffer **outbuf);
        int               Verify(const char *inbuf, int inlen,
                                 const char *sigbuf, int siglen);

        // Export session key
        int               getKey(char *kbuf=0, int klen=0);
        // Import a key
        int               setKey(char *kbuf, int klen);

        // Enable tracing
        static XrdOucTrace *EnableTracing();

private:
          XrdNetAddrInfo   epAddr;

   // Static members initialized at startup
   static XrdSysMutex      gsiContext;
   static String           CAdir;
   static String           CRLdir;
   static String           DefCRLext;
   static String           SrvCert;
   static String           SrvKey;
   static String           UsrProxy;
   static String           UsrCert;
   static String           UsrKey;
   static String           PxyValid;
   static int              DepLength;
   static int              DefBits;
   static int              CACheck;
   static int              CRLCheck;
   static int              CRLDownload;
   static int              CRLRefresh;
   static String           DefCrypto;
   static String           DefCipher;
   static String           DefMD;
   static String           DefError;
   static String           GMAPFile;
   static int              GMAPOpt;
   static bool             GMAPuseDNname;
   static int              GMAPCacheTimeOut;
   static XrdSecgsiGMAP_t  GMAPFun;
   static XrdSecgsiAuthz_t AuthzFun; 
   static XrdSecgsiAuthzKey_t AuthzKey; 
   static int              AuthzCertFmt; 
   static int              AuthzCacheTimeOut;
   static int              PxyReqOpts;
   static int              AuthzPxyWhat;
   static int              AuthzPxyWhere;
   static String           SrvAllowedNames;
   static int              VOMSAttrOpt; 
   static XrdSecgsiVOMS_t  VOMSFun;
   static int              VOMSCertFmt; 
   static int              MonInfoOpt;
   static bool             HashCompatibility;
   //
   // Crypto related info
   static int              ncrypt;                  // Number of factories
   static XrdCryptoFactory *cryptF[XrdCryptoMax];   // their hooks 
   static int              cryptID[XrdCryptoMax];   // their IDs 
   static String           cryptName[XrdCryptoMax]; // their names 
   static XrdCryptoCipher *refcip[XrdCryptoMax];    // ref for session ciphers 
   //
   // Caches 
   static XrdSutCache      cacheCA;   // Info about trusted CA's
   static XrdSutCache      cacheCert; // Cache for available server certs
   static XrdSutCache      cachePxy;  // Cache for client proxies
   static XrdSutCache      cacheGMAP; // Cache for gridmap entries
   static XrdSutCache      cacheGMAPFun; // Cache for entries mapped by GMAPFun
   static XrdSutCache      cacheAuthzFun; // Cache for entities filled by AuthzFun
   //
   // Services
   static XrdOucGMap      *servGMap;  // Grid mapping service 
   //
   // CRL stack
   static GSICrlStack      stackCRL; // Stack of CRL in use
   //
   // GMAP control vars
   static time_t           lastGMAPCheck; // time of last check on GMAP
   static XrdSysMutex      mutexGMAP;     // mutex to control GMAP reloads
   //
   // Running options / settings
   static int              Debug;          // [CS] Debug level
   static bool             Server;         // [CS] If server mode 
   static int              TimeSkew;       // [CS] Allowed skew in secs for time stamps 
   //
   // for error logging and tracing
   static XrdSysLogger     Logger;
   static XrdSysError      eDest;
   static XrdOucTrace     *GSITrace;

   // Information local to this instance
   int              options;
   XrdCryptoFactory *sessionCF;    // Chosen crypto factory
   XrdCryptoCipher *sessionKey;    // Session Key (result of the handshake)
   XrdSutBucket    *bucketKey;     // Bucket with the key in export form
   XrdCryptoMsgDigest *sessionMD;  // Message Digest instance
   XrdCryptoRSA    *sessionKsig;   // RSA key to sign
   XrdCryptoRSA    *sessionKver;   // RSA key to verify
   X509Chain       *proxyChain;    // Chain with the delegated proxy on servers
   bool             srvMode;       // TRUE if server mode 

   // Temporary Handshake local info
   gsiHSVars     *hs;

   // Parsing received buffers: client
   int            ParseClientInput(XrdSutBuffer *br, XrdSutBuffer **bm,
                                   String &emsg);
   int            ClientDoInit(XrdSutBuffer *br, XrdSutBuffer **bm,
                               String &cmsg);
   int            ClientDoCert(XrdSutBuffer *br,  XrdSutBuffer **bm,
                               String &cmsg);
   int            ClientDoPxyreq(XrdSutBuffer *br,  XrdSutBuffer **bm,
                                 String &cmsg);

   // Parsing received buffers: server
   int            ParseServerInput(XrdSutBuffer *br, XrdSutBuffer **bm,
                                   String &cmsg);
   int            ServerDoCertreq(XrdSutBuffer *br, XrdSutBuffer **bm,
                                  String &cmsg);
   int            ServerDoCert(XrdSutBuffer *br,  XrdSutBuffer **bm,
                               String &cmsg);
   int            ServerDoSigpxy(XrdSutBuffer *br,  XrdSutBuffer **bm,
                                 String &cmsg);

   // Auxilliary functions
   int            ParseCrypto(String cryptlist);
   int            ParseCAlist(String calist);

   // Load CA certificates
   static int     GetCA(const char *cahash,
                        XrdCryptoFactory *cryptof, gsiHSVars *hs = 0);
   static String  GetCApath(const char *cahash);
   static bool    VerifyCA(int opt, X509Chain *cca, XrdCryptoFactory *cf);
   bool           ServerCertNameOK(const char *subject, String &e);
   static XrdSutPFEntry *GetSrvCertEnt(XrdSutCacheRef   &pfeRef,
                                       XrdCryptoFactory *cf,
                                       time_t timestamp, String &cal);

   // Load CRLs
   static XrdCryptoX509Crl *LoadCRL(XrdCryptoX509 *xca, const char *sjhash,
                                    XrdCryptoFactory *CF, int dwld);

   // Updating proxies
   static int     QueryProxy(bool checkcache, XrdSutCache *cache, const char *tag,
                             XrdCryptoFactory *cf, time_t timestamp,
                             ProxyIn_t *pi, ProxyOut_t *po);
   static int     InitProxy(ProxyIn_t *pi, XrdCryptoFactory *cf,
                            X509Chain *ch = 0, XrdCryptoRSA **key = 0);

   // Error functions
   static void    ErrF(XrdOucErrInfo *einfo, kXR_int32 ecode,
                       const char *msg1, const char *msg2 = 0,
                       const char *msg3 = 0);
   XrdSecCredentials *ErrC(XrdOucErrInfo *einfo, XrdSutBuffer *b1,
                           XrdSutBuffer *b2,XrdSutBuffer *b3,
                           kXR_int32 ecode, const char *msg1 = 0,
                           const char *msg2 = 0, const char *msg3 = 0);
   int            ErrS(String ID, XrdOucErrInfo *einfo, XrdSutBuffer *b1,
                       XrdSutBuffer *b2, XrdSutBuffer *b3,
                       kXR_int32 ecode, const char *msg1 = 0,
                       const char *msg2 = 0, const char *msg3 = 0);

   // Check Time stamp
   bool           CheckTimeStamp(XrdSutBuffer *b, int skew, String &emsg);

   // Check random challenge
   bool           CheckRtag(XrdSutBuffer *bm, String &emsg);

   // Auxilliary methods
   int            AddSerialized(char opt, kXR_int32 step, String ID, 
                                XrdSutBuffer *bls, XrdSutBuffer *buf,
                                kXR_int32 type, XrdCryptoCipher *cip);
   // Grid map cache handling
   static XrdSecgsiGMAP_t            // Load alternative function for mapping
                  LoadGMAPFun(const char *plugin, const char *parms);
   static XrdSecgsiAuthz_t           // Load alternative function to fill XrdSecEntity
                  LoadAuthzFun(const char *plugin, const char *parms, int &fmt);
   static XrdSecgsiVOMS_t           // Load alternative function to extract VOMS
                  LoadVOMSFun(const char *plugin, const char *parms, int &fmt);
   static void    QueryGMAP(XrdCryptoX509Chain* chain, int now, String &name); //Lookup info for DN
   
   // Entity handling
   void CopyEntity(XrdSecEntity *in, XrdSecEntity *out, int *lout = 0);
   void FreeEntity(XrdSecEntity *in);

   // VOMS parsing
   int ExtractVOMS(X509Chain *c, XrdSecEntity &ent);
};

class gsiHSVars {
public:
   int               Iter;          // iteration number
   time_t            TimeStamp;     // Time of last call
   String            CryptoMod;     // crypto module in use
   int               RemVers;       // Version run by remote counterpart
   XrdCryptoCipher  *Rcip;          // reference cipher
   XrdSutBucket     *Cbck;          // Bucket with the certificate in export form
   String            ID;            // Handshake ID (dummy for clients)
   XrdSutPFEntry    *Cref;          // Cache reference
   XrdSutPFEntry    *Pent;          // Pointer to relevant file entry 
   X509Chain        *Chain;         // Chain to be eventually verified 
   XrdCryptoX509Crl *Crl;           // Pointer to CRL, if required 
   X509Chain        *PxyChain;      // Proxy Chain on clients
   bool              RtagOK;        // Rndm tag checked / not checked
   bool              Tty;           // Terminal attached / not attached
   int               LastStep;      // Step required at previous iteration
   int               Options;       // Handshake options;
   int               HashAlg;       // Hash algorithm of peer hash name;
   XrdSutBuffer     *Parms;         // Buffer with server parms on first iteration 

   gsiHSVars() { Iter = 0; TimeStamp = -1; CryptoMod = "";
                 RemVers = -1; Rcip = 0;
                 Cbck = 0;
                 ID = ""; Cref = 0; Pent = 0; Chain = 0; Crl = 0; PxyChain = 0;
                 RtagOK = 0; Tty = 0; LastStep = 0; Options = 0; HashAlg = 0; Parms = 0;}

   ~gsiHSVars() { SafeDelete(Cref);
                  if (Options & kOptsDelChn) {
                     // Do not delete the CA certificate in the cached reference
                     if (Chain) Chain->Cleanup(1);
                     SafeDelete(Chain);
                  }
                  if (Crl) {
                     // This decreases the counter and actually deletes the object only
                     // when no instance is using it
                     XrdSecProtocolgsi::stackCRL.Del(Crl);
                     Crl = 0;
                  }
                  // The proxy chain is owned by the proxy cache; invalid proxies are
                  // detected (and eventually removed) by QueryProxy
                  PxyChain = 0;
                  SafeDelete(Parms); }
   void Dump(XrdSecProtocolgsi *p = 0);
};
