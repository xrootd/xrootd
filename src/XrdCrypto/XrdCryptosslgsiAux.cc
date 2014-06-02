/******************************************************************************/
/*                                                                            */
/*                  X r d C r y p t o s s l g s i A u x . h h                 */
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

/* ************************************************************************** */
/*                                                                            */
/* GSI utility functions                                                      */
/*                                                                            */
/* ************************************************************************** */
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <openssl/asn1.h>
#include <openssl/asn1_mac.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509v3.h>

#include "XrdSut/XrdSutRndm.hh"
#include "XrdCrypto/XrdCryptosslgsiAux.hh"
#include "XrdCrypto/XrdCryptoTrace.hh"
#include "XrdCrypto/XrdCryptosslAux.hh"
#include "XrdCrypto/XrdCryptosslRSA.hh"
#include "XrdCrypto/XrdCryptosslX509.hh"
#include "XrdCrypto/XrdCryptosslX509Req.hh"

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//                                                                           //
// Extensions OID relevant for proxies                                       //
//                                                                           //
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//

// X509v3 Key Usage: critical
#define KEY_USAGE_OID  "2.5.29.15"
// X509v3 Subject Alternative Name: must be absent
#define SUBJ_ALT_NAME_OID  "2.5.29.17"

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//                                                                           //
// VOMS relevant stuff                                                       //
//                                                                           //
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//

#define XRDGSI_VOMS_ATCAP_OID  "1.3.6.1.4.1.8005.100.100.4"
#define XRDGSI_VOMS_ACSEQ_OID  "1.3.6.1.4.1.8005.100.100.5"

#define BIO_PRINT(b,c) \
   BUF_MEM *bptr; \
   BIO_get_mem_ptr(b, &bptr); \
   if (bptr) { \
      char *s = new char[bptr->length+1]; \
      memcpy(s, bptr->data, bptr->length); \
      s[bptr->length] = '\0'; \
      PRINT(c << s); \
      delete [] s; \
   } else { \
      PRINT("ERROR: "<<c<<" BIO internal buffer undefined!"); \
   } \
   if (b) BIO_free(b);

#define BIO_GET_STRING(b,str) \
   BUF_MEM *bptr; \
   BIO_get_mem_ptr(b, &bptr); \
   if (bptr) { \
      char *s = new char[bptr->length+1]; \
      memcpy(s, bptr->data, bptr->length); \
      s[bptr->length] = '\0'; \
      str = s; \
      delete [] s; \
   } else { \
      PRINT("ERROR: GET_STRING: BIO internal buffer undefined!"); \
   } \
   if (b) BIO_free(b);

#if OPENSSL_VERSION_NUMBER >= 0x0090800f
#  define XRDGSI_CONST const
#else
#  define XRDGSI_CONST
#endif

int XrdSslgsiX509Asn1PrintInfo(int tag, int xclass, int constructed, int indent);
int XrdSslgsiX509FillUnknownExt(XRDGSI_CONST unsigned char **pp, long length);
int XrdSslgsiX509FillVOMS(XRDGSI_CONST unsigned char **pp,
                          long length, bool &getvat, XrdOucString &vat);

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//                                                                           //
// Handlers of the ProxyCertInfo extension following RFC3820                 //
//                                                                           //
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
 
//
// Types describing the ProxyCertInfo extension
typedef struct {
   ASN1_OBJECT       *policyLanguage;
   ASN1_OCTET_STRING *policy;
} gsiProxyPolicy_t;
//
typedef struct {
   ASN1_INTEGER      *proxyCertPathLengthConstraint;
   gsiProxyPolicy_t  *proxyPolicy;
} gsiProxyCertInfo_t;
//
// Some function ID codes as in asn1.h: the ASN1 macros require something
// though not sure we really need them.
// (not yet used above 299: keep some margin)
#define ASN1_F_GSIPROXYCERTINFO_NEW     500
#define ASN1_F_D2I_GSIPROXYCERTINFO     501
#define ASN1_F_GSIPROXYPOLICY_NEW       510
#define ASN1_F_D2I_GSIPROXYPOLICY       511

// -------------------------------------------------------------------------
//
// Version of OBJ_txt2obj with a bug fix introduced starting
// with some 0.9.6 versions
static ASN1_OBJECT *OBJ_txt2obj_fix(const char *s, int no_name)
{
   int nid = NID_undef;
   ASN1_OBJECT *op=NULL;
   unsigned char *buf,*p;
   int i, j;

   if (!no_name) {
      if( ((nid = OBJ_sn2nid(s)) != NID_undef) ||
          ((nid = OBJ_ln2nid(s)) != NID_undef) ) 
         return OBJ_nid2obj(nid);
   }

   // Work out size of content octets
   i = a2d_ASN1_OBJECT(NULL,0,s,-1);
   if (i <= 0) {
      // Clear the error
      ERR_get_error();
      return NULL;
   }
   // Work out total size
   j = ASN1_object_size(0,i,V_ASN1_OBJECT);

   if ((buf=(unsigned char *)OPENSSL_malloc(j)) == NULL) return NULL;

   p = buf;
   // Write out tag+length
   ASN1_put_object(&p,0,i,V_ASN1_OBJECT,V_ASN1_UNIVERSAL);
   // Write out contents
   a2d_ASN1_OBJECT(p,i,s,-1);

   p = buf;
#if OPENSSL_VERSION_NUMBER >= 0x0090800f
   // not op=d2i_ASN1_OBJECT(0, &p, i) (C.H. Christensen, Oct 12, 2005)
   op = d2i_ASN1_OBJECT(0, (XRDGSI_CONST unsigned char**)(&p), j);
#else
   op = d2i_ASN1_OBJECT(0, &p, i);
#endif
   OPENSSL_free(buf);
   return op;
}
// -------------------------------------------------------------------------

//
// Functions to create and destroy a gsiProxyPolicy_t
// (NB: the names of the internal variables a fixed by the ASN1 macros)
//
//___________________________________________________________________________
gsiProxyPolicy_t *gsiProxyPolicy_new()
{
   // Create a new gsiProxyPolicy_t object
   ASN1_CTX          c;
   gsiProxyPolicy_t *ret;

   // Init object
   ret = 0;
   M_ASN1_New_Malloc(ret, gsiProxyPolicy_t);
   // Fill default policy
   ret->policyLanguage = OBJ_txt2obj_fix("1.3.6.1.5.5.7.21.1", 1);
   ret->policy = 0;
   // Return ok
   return (ret);
   // Error: flag it 
   M_ASN1_New_Error(ASN1_F_GSIPROXYPOLICY_NEW);
}

//___________________________________________________________________________
void gsiProxyPolicy_free(gsiProxyPolicy_t *pol)
{
   // Free a gsiProxyPolicy_t object

   // Make sure there is something to free
   if (!pol)
      return;
   //
   // Free language object
   if (pol->policyLanguage)
      ASN1_OBJECT_free(pol->policyLanguage);
   //
   // Free policy octet string
   if (pol->policy)
      M_ASN1_OCTET_STRING_free(pol->policy);
   //
   // Free the container
   OPENSSL_free(pol);
}

//
// This function allows to convert the internal representation to a 
// gsiProxyPolicy_t object. We need this for correct parsing of a
// ProxyCertInfo object, even if we are not presently interested
// in the policy.
//___________________________________________________________________________
gsiProxyPolicy_t *d2i_gsiProxyPolicy(gsiProxyPolicy_t **pol,
                                     XRDGSI_CONST unsigned char **pp, long length)
{
   // Get the policy object from buffer at pp, of length bytes.

   // Define vars
   M_ASN1_D2I_vars(pol, gsiProxyPolicy_t *, gsiProxyPolicy_new);
   //
   // Init sequence
   M_ASN1_D2I_Init();
   M_ASN1_D2I_start_sequence();
   //
   // Retrieve language
   M_ASN1_D2I_get(ret->policyLanguage, d2i_ASN1_OBJECT);
   //
   // Retrieve content
   M_ASN1_D2I_get_IMP_opt(ret->policy, d2i_ASN1_OCTET_STRING,
                                     0, V_ASN1_OCTET_STRING);
   //
   // Finalize
   M_ASN1_D2I_Finish(pol, gsiProxyPolicy_free, ASN1_F_D2I_GSIPROXYPOLICY);
}

//
// This function allows to convert a gsiProxyPolicy_t object to 
// internal representation. We need this for correct updating of
// the path length in a ProxyCertInfo object, even if we are not
// presently interested in the policy.
//___________________________________________________________________________
int i2d_gsiProxyPolicy(gsiProxyPolicy_t *pol, unsigned char **pp)
{
   // Set the policy object from pol to buffer at pp.
   // Return number of meningful bytes 

   // Define vars
   M_ASN1_I2D_vars(pol);
   //
   // Set language length
   M_ASN1_I2D_len(pol->policyLanguage, i2d_ASN1_OBJECT);
   //
   // Set content length
   if (pol->policy) {
      M_ASN1_I2D_len(pol->policy, i2d_ASN1_OCTET_STRING);
   }
   //
   // Sequence
   M_ASN1_I2D_seq_total();
   //
   // Set language
   M_ASN1_I2D_put(pol->policyLanguage, i2d_ASN1_OBJECT);
   //
   // Set content
   if (pol->policy) {
      M_ASN1_I2D_put(pol->policy, i2d_ASN1_OCTET_STRING);
   }
   //
   // Finalize
   M_ASN1_I2D_finish();
}
//
// Functions to create and destroy a gsiProxyCertInfo_t
//
//___________________________________________________________________________
gsiProxyCertInfo_t *gsiProxyCertInfo_new()
{
   // Create a new gsiProxyCertInfo_t object
   ASN1_CTX            c;
   gsiProxyCertInfo_t *ret;
   //
   // Init object
   ret = 0;
   M_ASN1_New_Malloc(ret, gsiProxyCertInfo_t);
   memset(ret, 0, sizeof(gsiProxyCertInfo_t));
   //
   // Default values
   ret->proxyCertPathLengthConstraint = 0;
   ret->proxyPolicy = gsiProxyPolicy_new();
   //
   // Return OK
   return (ret);
   //
   // Error: flag it
   M_ASN1_New_Error(ASN1_F_GSIPROXYCERTINFO_NEW);
}

//___________________________________________________________________________
void gsiProxyCertInfo_free(gsiProxyCertInfo_t *pci)
{
   // Free a gsiProxyPolicy_t object

   // Make sure there is something to free
   if (!pci)
      return;
   // Free path len constraint object
   if (pci->proxyCertPathLengthConstraint)
      ASN1_INTEGER_free(pci->proxyCertPathLengthConstraint);
   // Free the container
   OPENSSL_free(pci);
}

//
// This function allow to convert the internal representation to a 
// gsiProxyCertInfo_t object.
//___________________________________________________________________________
gsiProxyCertInfo_t *d2i_gsiProxyCertInfo(gsiProxyCertInfo_t **pci,
                                         XRDGSI_CONST unsigned char **pp, long length)
{
   // Get the proxy certificate info object from length bytes at pp.

   // Define vars
   M_ASN1_D2I_vars(pci, gsiProxyCertInfo_t *, gsiProxyCertInfo_new);
   //
   // Init sequence
   M_ASN1_D2I_Init();
   M_ASN1_D2I_start_sequence();
   //
   // Retrieve the policy (wee need to do this to avoid screwing
   // up the sequence pointers)
   M_ASN1_D2I_get(ret->proxyPolicy, d2i_gsiProxyPolicy);
   //
   // Retrieve the path length constraint
   M_ASN1_D2I_get_EXP_opt(ret->proxyCertPathLengthConstraint, d2i_ASN1_INTEGER, 1);
   M_ASN1_D2I_get_opt(ret->proxyCertPathLengthConstraint, d2i_ASN1_INTEGER,
                                                          V_ASN1_INTEGER);
   //
   // Finalize
   M_ASN1_D2I_Finish(pci, gsiProxyCertInfo_free, ASN1_F_D2I_GSIPROXYCERTINFO);
}
//
// This function allows to convert a gsiProxyCertInfo_t object to 
// internal representation.
//___________________________________________________________________________
int i2d_gsiProxyCertInfo(gsiProxyCertInfo_t *pci, unsigned char **pp)
{
   // Set the proxy certificate info object from pol to buffer at pp.
   // Return number of meningful bytes 
   int v1 = 0;

   // Define vars
   M_ASN1_I2D_vars(pci);
   v1 = 0;
   //
   // Set length of proxyPolicy
   M_ASN1_I2D_len(pci->proxyPolicy, i2d_gsiProxyPolicy);
   //
   // Set len of the path length constraint field
   if (pci->proxyCertPathLengthConstraint) {
      M_ASN1_I2D_len_EXP_opt(pci->proxyCertPathLengthConstraint,      
                             i2d_ASN1_INTEGER, 1, v1);
   }
   //
   // Sequence
   M_ASN1_I2D_seq_total();
   //
   // Set policy
   M_ASN1_I2D_put(pci->proxyPolicy, i2d_gsiProxyPolicy);
   //
   // Set path length constraint
   if (pci->proxyCertPathLengthConstraint) {
      M_ASN1_I2D_put_EXP_opt(pci->proxyCertPathLengthConstraint, i2d_ASN1_INTEGER, 1, v1);
   }
   //
   // Finalize
   M_ASN1_I2D_finish();
}
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//

//___________________________________________________________________________
bool XrdSslgsiProxyCertInfo(const void *extdata, int &pathlen, bool *haspolicy)
{
   //
   // Check presence of a proxyCertInfo and retrieve the path length constraint.
   // Written following RFC3820, examples in openssl-<vers>/crypto source code.
   // in gridsite code and Globus proxycertinfo.h / .c.
   // if 'haspolicy' is defined, the existence of a policy field is checked;
   // the content ignored for the time being.

   // Make sure we got an extension
   if (!extdata) {
      return 0;
   }
   // Structure the buffer
   X509_EXTENSION *ext = (X509_EXTENSION *)extdata;

   // Check ProxyCertInfo OID
   char s[80] = {0};
   OBJ_obj2txt(s, sizeof(s), X509_EXTENSION_get_object(ext), 1);
   if (strcmp(s, gsiProxyCertInfo_OID)) {
      return 0;
   }

   // Now extract the path length constraint, if any
   unsigned char *p = ext->value->data;
   gsiProxyCertInfo_t *pci =
      d2i_gsiProxyCertInfo(0, (XRDGSI_CONST unsigned char **)(&p), ext->value->length);
   if (!pci) {
      return 0;
   }

   // Default length is -1, i.e. check disabled
   pathlen = -1;
   if (pci->proxyCertPathLengthConstraint) {
      pathlen = ASN1_INTEGER_get(pci->proxyCertPathLengthConstraint);
   }

   // If required, check te existence of a policy field
   if (haspolicy) {
      *haspolicy = (pci->proxyPolicy) ? 1 : 0;
   }

   // We are done
   return 1;
}

//___________________________________________________________________________
void XrdSslgsiSetPathLenConstraint(void *extdata, int pathlen)
{
   //
   // Set the patch length constraint valur in proxyCertInfo extension ext
   // to 'pathlen'.

   // Make sure we got an extension
   if (!extdata)
      return;
   // Structure the buffer
   X509_EXTENSION *ext = (X509_EXTENSION *)extdata;

   // Check ProxyCertInfo OID
   char s[80] = {0};
   OBJ_obj2txt(s, sizeof(s), X509_EXTENSION_get_object(ext), 1);
   if (strcmp(s, gsiProxyCertInfo_OID))
      return;

   // Now extract the path length constraint, if any
   unsigned char *p = ext->value->data;
   gsiProxyCertInfo_t *pci =
      d2i_gsiProxyCertInfo(0, (XRDGSI_CONST unsigned char **)(&p), ext->value->length);
   if (!pci)
      return;

   // Set the new length
   if (pci->proxyCertPathLengthConstraint) {
      ASN1_INTEGER_set(pci->proxyCertPathLengthConstraint, pathlen);
   }

   // We are done
   return;
}

//____________________________________________________________________________
int XrdSslgsiX509CreateProxy(const char *fnc, const char *fnk, 
                             XrdProxyOpt_t *pxopt,
                             XrdCryptosslgsiX509Chain *xp, XrdCryptoRSA **kp,
                             const char *fnp)
{
   // Create a proxy certificate following the GSI specification (RFC 3820)
   // for the EEC certificate in file 'fnc', private key in 'fnk'.
   // A chain containing the proxy certificate and the EEC is returned in 'xp'
   // and its full RSA key in 'kp'.
   // The structure pxopt can be used to change the default options about
   // number of bits for teh key, duration validity and max path signature depth.
   // If 'fpn' is defined, a PEM file is created with, in order, the proxy
   // certificate, the related private key and the EEC certificate (standard
   // GSI format).
   // Policy fields in the CertProxyExtension not yet included.
   // Return 0 in case of success, < 0 otherwise
   EPNAME("X509CreateProxy");

   // Make sure the files are specified
   if (!fnc || !fnk || !xp || !kp) {
      PRINT("invalid inputs ");
      return -1;
   }

   //
   // Init OpenSSL
   OpenSSL_add_all_ciphers();
   OpenSSL_add_all_digests();
   ERR_load_crypto_strings();

   // Use default options, if not specified
   int bits = (pxopt && pxopt->bits >= 512) ? pxopt->bits : 512;
   int valid = (pxopt) ? pxopt->valid : 43200;  // 12 hours
   int depthlen = (pxopt) ? pxopt->depthlen : -1; // unlimited

   //
   // Get EEC certificate from fnc
   X509 *xEEC = 0;
   FILE *fc = fopen(fnc, "r");
   if (fc) {
      // Read out the certificate
      if (PEM_read_X509(fc, &xEEC, 0, 0)) {
         DEBUG("EEC certificate loaded from file: "<<fnc);
      } else {
         PRINT("unable to load EEC certificate from file: "<<fnc);
         fclose(fc);
         return -kErrPX_BadEECfile;
      }
   } else {
      PRINT("EEC certificate cannot be opened (file: "<<fnc<<")"); 
      return -kErrPX_BadEECfile;
   }
   fclose(fc);
   // Make sure the certificate is not expired
   int now = (int)time(0);
   if (now > XrdCryptosslASN1toUTC(X509_get_notAfter(xEEC))) {
      PRINT("EEC certificate has expired"); 
      return -kErrPX_ExpiredEEC;
   }

   //
   // Get EEC private key from fnk
   EVP_PKEY *ekEEC = 0;
   FILE *fk = fopen(fnk, "r");
   if (fk) {
      // Read out the private key
      XrdOucString sbj;
      XrdCryptosslNameOneLine(X509_get_subject_name(xEEC), sbj);
      PRINT("Your identity: "<<sbj);
      if ((ekEEC = PEM_read_PrivateKey(fk, &ekEEC, 0, 0))) {
         DEBUG("EEC private key loaded from file: "<<fnk);
      } else {
         PRINT("unable to load EEC private key from file: "<<fnk);
         fclose(fk);
         return -kErrPX_BadEECfile;
      }
   } else {
      PRINT("EEC private key file cannot be opened (file: "<<fnk<<")"); 
      return -kErrPX_BadEECfile;
   }
   fclose(fk);
   // Check key consistency
   if ((RSA_check_key(ekEEC->pkey.rsa) == 0)) {
      PRINT("inconsistent key loaded");
      return -kErrPX_BadEECkey;
   }
   //
   // Create a new request
   X509_REQ *preq = X509_REQ_new();
   if (!preq) {
      PRINT("cannot to create cert request");
      return -kErrPX_NoResources;
   }
   //
   // Create the new PKI for the proxy (exponent 65537)
   RSA *kPX = RSA_generate_key(bits, 0x10001, 0, 0);
   if (!kPX) {
      PRINT("proxy key could not be generated - return"); 
      return -kErrPX_GenerateKey;
   }
   //
   // Set the key into the request
   EVP_PKEY *ekPX = EVP_PKEY_new();
   if (!ekPX) {
      PRINT("could not create a EVP_PKEY * instance - return"); 
      return -kErrPX_NoResources;
   }
   EVP_PKEY_set1_RSA(ekPX, kPX);
   X509_REQ_set_pubkey(preq, ekPX);
   // 
   // Generate a serial number. Specification says that this *should*
   // unique, so we just draw an unsigned random integer
   unsigned int serial = XrdSutRndm::GetUInt();
   //
   // The subject name is the certificate subject + /CN=<rand_uint>
   // with <rand_uint> is a random unsigned int used also as serial
   // number.
   // Duplicate user subject name
   X509_NAME *psubj = X509_NAME_dup(X509_get_subject_name(xEEC)); 
   // Create an entry with the common name
   unsigned char sn[20] = {0};
   sprintf((char *)sn, "%d", serial);
   if (!X509_NAME_add_entry_by_txt(psubj, (char *)"CN", MBSTRING_ASC,
                                   sn, -1, -1, 0)) {
      PRINT("could not add CN - (serial: "<<serial<<", sn: "<<sn<<")"); 
      return -kErrPX_SetAttribute;
   }
   //
   // Set the name
   if (X509_REQ_set_subject_name(preq, psubj) != 1) {
      PRINT("could not set subject name - return"); 
      return -kErrPX_SetAttribute;
   }
   //
   // Create the extension CertProxyInfo
   gsiProxyCertInfo_t *pci = gsiProxyCertInfo_new();
   if (!pci) {
      PRINT("could not create structure for extension - return"); 
      return -kErrPX_NoResources;
   }
   // Set the new length
   if (depthlen > -1) {
      if ((pci->proxyCertPathLengthConstraint = ASN1_INTEGER_new())) {
         ASN1_INTEGER_set(pci->proxyCertPathLengthConstraint, depthlen);
      } else {
         PRINT("could not set the path length contrain"); 
         return -kErrPX_SetPathDepth;
      }
   }

   //
   // create extension
   X509_EXTENSION *ext = X509_EXTENSION_new();
   if (!ext) {
      PRINT("could not create extension object"); 
      return -kErrPX_NoResources;
   }
   // Set extension name.
#if OPENSSL_VERSION_NUMBER >= 0x0090700f
   // We do not use directly OBJ_txt2obj because that is not working
   // with all OpenSSL 0.9.6 versions
   ASN1_OBJECT *obj = OBJ_nid2obj(OBJ_create(gsiProxyCertInfo_OID,
                            "gsiProxyCertInfo_OID","GSI ProxyCertInfo OID"));
#else
   // This version of OBJ_txt2obj fixes a bug affecting some 
   // OpenSSL 0.9.6 versions
   ASN1_OBJECT *obj = OBJ_txt2obj_fix(gsiProxyCertInfo_OID, 1);
#endif
   if (!obj || X509_EXTENSION_set_object(ext, obj) != 1) {
      PRINT("could not set extension name"); 
      return -kErrPX_SetAttribute;
   }
   // flag as critical
   if (X509_EXTENSION_set_critical(ext, 1) != 1) {
      PRINT("could not set extension critical flag"); 
      return -kErrPX_SetAttribute;
   }
   // Extract data in format for extension
   ext->value->length = i2d_gsiProxyCertInfo(pci, 0);
   if (!(ext->value->data = (unsigned char *)malloc(ext->value->length+1))) {
      PRINT("could not allocate data field for extension"); 
      return -kErrPX_NoResources;
   }
   unsigned char *pp = ext->value->data;
   if ((i2d_gsiProxyCertInfo(pci, &pp)) <= 0) {
      PRINT("problem converting data for extension"); 
      return -kErrPX_Error;
   }
   // Create a stack
   STACK_OF(X509_EXTENSION) *esk = sk_X509_EXTENSION_new_null();
   if (!esk) {
      PRINT("could not create stack for extensions"); 
      return -kErrPX_NoResources;
   }
   //
   // Now we add the new extension
   if (sk_X509_EXTENSION_push(esk, ext) == 0) {
      PRINT("could not push the extension in the stack"); 
      return -kErrPX_Error;
   }
   // Add extension
   if (!(X509_REQ_add_extensions(preq, esk))) {
      PRINT("problem adding extension"); 
      return -kErrPX_SetAttribute;
   }
   //
   // Sign the request
   if (!(X509_REQ_sign(preq, ekPX, EVP_md5()))) {
      PRINT("problems signing the request"); 
      return -kErrPX_Signing;
   }
   //
   // Create new proxy cert
   X509 *xPX = X509_new();
   if (!xPX) {
      PRINT("could not create certificate object for proxies"); 
      return -kErrPX_NoResources;
   }

   // Set version number
   if (X509_set_version(xPX, 2L) != 1) {
      PRINT("could not set version"); 
      return -kErrPX_SetAttribute;
   }

   // Set serial number
   if (ASN1_INTEGER_set(X509_get_serialNumber(xPX), serial) != 1) {
      PRINT("could not set serial number"); 
      return -kErrPX_SetAttribute;
   }

   // Set subject name
   if (X509_set_subject_name(xPX, psubj) != 1) {
      PRINT("could not set subject name"); 
      return -kErrPX_SetAttribute;
   }
   
   // Set issuer name
   if (X509_set_issuer_name(xPX, X509_get_subject_name(xEEC)) != 1) {
      PRINT("could not set issuer name"); 
      return -kErrPX_SetAttribute;
   }

   // Set public key
   if (X509_set_pubkey(xPX, ekPX) != 1) {
      PRINT("could not set issuer name"); 
      return -kErrPX_SetAttribute;
   }

   // Set proxy validity: notBefore now
   if (!X509_gmtime_adj(X509_get_notBefore(xPX), 0)) {
      PRINT("could not set notBefore"); 
      return -kErrPX_SetAttribute;
   }

   // Set proxy validity: notAfter expire_secs from now
   if (!X509_gmtime_adj(X509_get_notAfter(xPX), valid)) {
      PRINT("could not set notAfter"); 
      return -kErrPX_SetAttribute;
   }

   // First duplicate the extensions of the EE certificate
   X509_EXTENSION *xEECext = 0;
   int nEECext = X509_get_ext_count(xEEC);
   DEBUG("number of extensions found in the original certificate: "<< nEECext);
   int i = 0;
   bool haskeyusage = 0;
   for (i = 0; i< nEECext; i++) {
      xEECext = X509_get_ext(xEEC, i);
      char s[256];
      OBJ_obj2txt(s, sizeof(s), X509_EXTENSION_get_object(xEECext), 1);
      // Flag key usage extension
      if (!haskeyusage && !strcmp(s, KEY_USAGE_OID)) haskeyusage = 1;
      // Skip subject alternative name extension
      if (!strcmp(s, SUBJ_ALT_NAME_OID)) continue;
      // Duplicate and add to the stack
      X509_EXTENSION *xEECextdup = X509_EXTENSION_dup(xEECext);
      if (X509_add_ext(xPX, xEECextdup, -1) == 0) {
         PRINT("could not push the extension '"<<s<<"' in the stack"); 
         return -kErrPX_Error;
      }
      // Notify what we added
      int crit = X509_EXTENSION_get_critical(xEECextdup);
      DEBUG("added extension '"<<s<<"', critical: " << crit);
   }

   // Warn if the critical oen is missing
   if (!haskeyusage) {
      PRINT(">>> WARNING: critical extension 'Key Usage' not found in original certificate! ");
      PRINT(">>> WARNING: this proxy may not be accepted by some parsers. ");
   }

   // Add the extension
   if (X509_add_ext(xPX, ext, -1) != 1) {
      PRINT("could not add extension"); 
      return -kErrPX_SetAttribute;
   }

   //
   // Sign the certificate
   if (!(X509_sign(xPX, ekEEC, EVP_md5()))) {
      PRINT("problems signing the certificate"); 
      return -kErrPX_Signing;
   }

   // Fill outputs
   XrdCryptoX509 *xcPX = new XrdCryptosslX509(xPX);
   if (!xcPX) {
      PRINT("could not create container for proxy certificate"); 
      return -kErrPX_NoResources;
   }
   // We need the full key
   ((XrdCryptosslX509 *)xcPX)->SetPKI((XrdCryptoX509data)ekPX);
   xp->PushBack(xcPX);
   XrdCryptoX509 *xcEEC = new XrdCryptosslX509(xEEC);
   if (!xcEEC) {
      PRINT("could not create container for EEC certificate"); 
      return -kErrPX_NoResources;
   }
   xp->PushBack(xcEEC);
   *kp = new XrdCryptosslRSA(ekPX);
   if (!(*kp)) {
      PRINT("could not creatr out PKI"); 
      return -kErrPX_NoResources;
   }
   
   //
   // Write to a file if requested
   int rc = 0;
   if (fnp) {
      // Open the file in write mode
      FILE *fp = fopen(fnp,"w");
      if (!fp) {
         PRINT("cannot open file to save the proxy certificate (file: "<<fnp<<")"); 
         fclose(fp);
         rc = -kErrPX_ProxyFile;
      }
      int ifp = fileno(fp);
      if (ifp == -1) {
         PRINT("got invalid file descriptor for the proxy certificate (file: "<<
                fnp<<")"); 
         fclose(fp);
         rc = -kErrPX_ProxyFile;
      }
      // Set permissions to 0600
      if (fchmod(ifp, 0600) == -1) {
         PRINT("cannot set permissions on file: "<<fnp<<" (errno: "<<errno<<")"); 
         fclose(fp);
         rc = -kErrPX_ProxyFile;
      } 

      if (!rc && PEM_write_X509(fp, xPX) != 1) {
         PRINT("error while writing proxy certificate"); 
         fclose(fp);
         rc = -kErrPX_ProxyFile;
      }   
      if (!rc && PEM_write_RSAPrivateKey(fp, kPX, 0, 0, 0, 0, 0) != 1) {
         PRINT("error while writing proxy private key"); 
         fclose(fp);
         rc = -kErrPX_ProxyFile;
      }   
      if (!rc && PEM_write_X509(fp, xEEC) != 1) {
         PRINT("error while writing EEC certificate"); 
         fclose(fp);
         rc = -kErrPX_ProxyFile;
      }   
      fclose(fp);
      // Change
   }

   // Cleanup
   EVP_PKEY_free(ekEEC);
   X509_REQ_free(preq);
#if OPENSSL_VERSION_NUMBER >= 0x10000000L
   sk_X509_EXTENSION_free(esk);
#else /* OPENSSL */
   sk_free(esk);
#endif /* OPENSSL */

   // We are done
   return rc;
}

//____________________________________________________________________________
int XrdSslgsiX509CreateProxyReq(XrdCryptoX509 *xcpi,
                                XrdCryptoX509Req **xcro, XrdCryptoRSA **kcro)
{
   // Create a proxy certificate request following the GSI specification
   // (RFC 3820) for the proxy certificate 'xpi'.
   // The proxy certificate is returned in 'xpo' and its full RSA key in 'kpo'.
   // Policy fields in the CertProxyExtension not yet included.
   // Return 0 in case of success, < 0 otherwise
   EPNAME("X509CreateProxyReq");

   // Make sure we got an proxy certificate as input 
   if (!xcpi || !(xcpi->Opaque())) {
      PRINT("input proxy certificate not specified");
      return -1;
   }

   // Point to the cerificate
   X509 *xpi = (X509 *)(xcpi->Opaque());

   // Make sure the certificate is not expired
   if (!(xcpi->IsValid())) {
      PRINT("EEC certificate has expired"); 
      return -kErrPX_ExpiredEEC;
   }
   //
   // Create a new request
   X509_REQ *xro = X509_REQ_new();
   if (!xro) {
      PRINT("cannot to create cert request");
      return -kErrPX_NoResources;
   }
   //
   // Use same num of bits as the signing certificate, but
   // less than 512
   int bits = EVP_PKEY_bits(X509_get_pubkey(xpi));
   bits = (bits < 512) ? 512 : bits;
   //
   // Create the new PKI for the proxy (exponent 65537)
   RSA *kro = RSA_generate_key(bits, 0x10001, 0, 0);
   if (!kro) {
      PRINT("proxy key could not be generated - return"); 
      return -kErrPX_GenerateKey;
   }
   //
   // Set the key into the request
   EVP_PKEY *ekro = EVP_PKEY_new();
   if (!ekro) {
      PRINT("could not create a EVP_PKEY * instance - return"); 
      return -kErrPX_NoResources;
   }
   EVP_PKEY_set1_RSA(ekro, kro);
   X509_REQ_set_pubkey(xro, ekro);
   // 
   // Generate a serial number. Specification says that this *should*
   // unique, so we just draw an unsigned random integer
   unsigned int serial = XrdSutRndm::GetUInt();
   //
   // The subject name is the certificate subject + /CN=<rand_uint>
   // with <rand_uint> is a random unsigned int used also as serial
   // number.
   // Duplicate user subject name
   X509_NAME *psubj = X509_NAME_dup(X509_get_subject_name(xpi)); 
   if (xcro && *xcro && *((int *)(*xcro)) <= 10100) {
      // Delete existing proxy CN addition; for backward compatibility
#if OPENSSL_VERSION_NUMBER >= 0x10000000L
      int ne = sk_X509_NAME_ENTRY_num(psubj->entries);
#else /* OPENSSL */
      int ne = psubj->entries->num;
#endif /* OPENSSL */
      if (ne >= 0) {
         X509_NAME_ENTRY *cne = X509_NAME_delete_entry(psubj, ne-1);
         if (cne) {
            X509_NAME_ENTRY_free(cne);
         } else {
            DEBUG("problems modifying subject name");
         }
      }
      *xcro = 0;
   }
   // Create an entry with the common name
   unsigned char sn[20] = {0};
   sprintf((char *)sn, "%d", serial);
   if (!X509_NAME_add_entry_by_txt(psubj, (char *)"CN", MBSTRING_ASC,
                                   sn, -1, -1, 0)) {
      PRINT("could not add CN - (serial: "<<serial<<", sn: "<<sn<<")"); 
      return -kErrPX_SetAttribute;
   }
   //
   // Set the name
   if (X509_REQ_set_subject_name(xro, psubj) != 1) {
      PRINT("could not set subject name - return"); 
      return -kErrPX_SetAttribute;
   }
   //
   // Create the extension CertProxyInfo
   gsiProxyCertInfo_t *pci = gsiProxyCertInfo_new();
   if (!pci) {
      PRINT("could not create structure for extension - return"); 
      return -kErrPX_NoResources;
   }
   //
   // Create a stack
   STACK_OF(X509_EXTENSION) *esk = sk_X509_EXTENSION_new_null();
   if (!esk) {
      PRINT("could not create stack for extensions"); 
      return -kErrPX_NoResources;
   }
   //
   // Get signature path depth from present proxy
   X509_EXTENSION *xpiext = 0;
   int npiext = X509_get_ext_count(xpi);
   int i = 0;
   bool haskeyusage = 0;
   int indepthlen = -1;
   for (i = 0; i< npiext; i++) {
      xpiext = X509_get_ext(xpi, i);
      char s[256];
      OBJ_obj2txt(s, sizeof(s), X509_EXTENSION_get_object(xpiext), 1);
      // Flag key usage extension
      if (!haskeyusage && !strcmp(s, KEY_USAGE_OID)) haskeyusage = 1;
      // Skip subject alternative name extension
      if (!strcmp(s, SUBJ_ALT_NAME_OID)) continue;
      // Get signature path depth from present proxy
      if (!strcmp(s, gsiProxyCertInfo_OID)) {
         unsigned char *p = xpiext->value->data;
         gsiProxyCertInfo_t *inpci =
            d2i_gsiProxyCertInfo(0, (XRDGSI_CONST unsigned char **)(&p), xpiext->value->length);
         if (inpci && 
             inpci->proxyCertPathLengthConstraint)
            indepthlen = ASN1_INTEGER_get(inpci->proxyCertPathLengthConstraint);
         DEBUG("IN depth length: "<<indepthlen);
      } else {
         // Duplicate and add to the stack
         X509_EXTENSION *xpiextdup = X509_EXTENSION_dup(xpiext);
         if (sk_X509_EXTENSION_push(esk, xpiextdup) == 0) {
            PRINT("could not push the extension '"<<s<<"' in the stack"); 
            return -kErrPX_Error;
         }
         // Notify what we added
         int crit = X509_EXTENSION_get_critical(xpiextdup);
         DEBUG("added extension '"<<s<<"', critical: " << crit);
      }
      // Do not free the extension: its owned by the certificate
      xpiext = 0;
   }
   //
   // Warn if the critical oen is missing
   if (!haskeyusage) {
      PRINT(">>> WARNING: critical extension 'Key Usage' not found in original certificate! ");
      PRINT(">>> WARNING: this proxy may not be accepted by some parsers. ");
   }
   //
   // Set the new length
   if (indepthlen > -1) {
      if ((pci->proxyCertPathLengthConstraint = ASN1_INTEGER_new())) {
         int depthlen = (indepthlen > 0) ? (indepthlen-1) : 0;
         ASN1_INTEGER_set(pci->proxyCertPathLengthConstraint, depthlen);
      } else {
         PRINT("could not set the path length contrain"); 
         return -kErrPX_SetPathDepth;
      }
   }
   //
   // create extension
   X509_EXTENSION *ext = X509_EXTENSION_new();
   if (!ext) {
      PRINT("could not create extension object"); 
      return -kErrPX_NoResources;
   }
   // Extract data in format for extension
   ext->value->length = i2d_gsiProxyCertInfo(pci, 0);
   if (!(ext->value->data = (unsigned char *)malloc(ext->value->length+1))) {
      PRINT("could not allocate data field for extension"); 
      return -kErrPX_NoResources;
   }
   unsigned char *pp = ext->value->data;
   if ((i2d_gsiProxyCertInfo(pci, &pp)) <= 0) {
      PRINT("problem converting data for extension"); 
      return -kErrPX_Error;
   }
   // Set extension name.
#if OPENSSL_VERSION_NUMBER >= 0x0090700f
   // We do not use directly OBJ_txt2obj because that is not working
   // with all OpenSSL 0.9.6 versions
   ASN1_OBJECT *obj = OBJ_nid2obj(OBJ_create(gsiProxyCertInfo_OID,
                            "gsiProxyCertInfo_OID","GSI ProxyCertInfo OID"));
#else
   // This version of OBJ_txt2obj fixes a bug affecting some 
   // OpenSSL 0.9.6 versions
   ASN1_OBJECT *obj = OBJ_txt2obj_fix(gsiProxyCertInfo_OID, 1);
#endif
   if (!obj || X509_EXTENSION_set_object(ext, obj) != 1) {
      PRINT("could not set extension name"); 
      return -kErrPX_SetAttribute;
   }
   // flag as critical
   if (X509_EXTENSION_set_critical(ext, 1) != 1) {
      PRINT("could not set extension critical flag"); 
      return -kErrPX_SetAttribute;
   }
   if (sk_X509_EXTENSION_push(esk, ext) == 0) {
      PRINT("could not push the extension in the stack"); 
      return -kErrPX_Error;
   }
   // Add extensions
   if (!(X509_REQ_add_extensions(xro, esk))) {
      PRINT("problem adding extension"); 
      return -kErrPX_SetAttribute;
   }
   //
   // Sign the request
   if (!(X509_REQ_sign(xro, ekro, EVP_md5()))) {
      PRINT("problems signing the request"); 
      return -kErrPX_Signing;
   }

   // Prepare output
   *xcro = new XrdCryptosslX509Req(xro);
   *kcro = new XrdCryptosslRSA(ekro);

   // Cleanup
#if OPENSSL_VERSION_NUMBER >= 0x10000000L
   sk_X509_EXTENSION_free(esk);
#else /* OPENSSL */
   sk_free(esk);
#endif /* OPENSSL */

   // We are done
   return 0;
}


//____________________________________________________________________________
int XrdSslgsiX509SignProxyReq(XrdCryptoX509 *xcpi, XrdCryptoRSA *kcpi,
                              XrdCryptoX509Req *xcri, XrdCryptoX509 **xcpo)
{
   // Sign a proxy certificate request.
   // Return 0 in case of success, < 0 otherwise
   EPNAME("X509SignProxyReq");

   // Make sure we got the right inputs
   if (!xcpi || !kcpi || !xcri || !xcpo) {
      PRINT("invalid inputs");
      return -1;
   }

   // Make sure the certificate is not expired
   int timeleft = xcpi->NotAfter() - (int)time(0) + XrdCryptoTZCorr();
   if (timeleft < 0) {
      PRINT("EEC certificate has expired"); 
      return -kErrPX_ExpiredEEC;
   }
   // Point to the cerificate
   X509 *xpi = (X509 *)(xcpi->Opaque());

   // Check key consistency
   if (kcpi->status != XrdCryptoRSA::kComplete) {
      PRINT("inconsistent key loaded");
      return -kErrPX_BadEECkey;
   }
   // Point to the cerificate
   RSA *kpi = ((EVP_PKEY *)(kcpi->Opaque()))->pkey.rsa;
   //
   // Set the key into the request
   EVP_PKEY *ekpi = EVP_PKEY_new();
   if (!ekpi) {
      PRINT("could not create a EVP_PKEY * instance - return"); 
      return -kErrPX_NoResources;
   }
   EVP_PKEY_set1_RSA(ekpi, kpi);

   // Get request in raw form
   X509_REQ *xri = (X509_REQ *)(xcri->Opaque());

   // Extract subject names
   XrdOucString psbj, rsbj;
   XrdCryptosslNameOneLine(X509_get_subject_name(xpi), psbj);
   XrdCryptosslNameOneLine(X509_REQ_get_subject_name(xri), rsbj);
   if (psbj.length() <= 0 || rsbj.length() <= 0) {
      PRINT("names undefined");
      return -kErrPX_BadNames;
   }

   // Check the subject name: the new proxy one must be in the form
   // '<issuer subject> + /CN=<serial>'
   XrdOucString neecp(psbj);
   XrdOucString neecr(rsbj,0,rsbj.rfind("/CN=")-1);
   if (neecr.length() <= 0 || neecr.length() <= 0 || neecp != neecr) {
      if (xcri->Version() <= 10100) {
         // Support previous format
         neecp.erase(psbj.rfind("/CN="));
         if (neecr.length() <= 0 || neecr.length() <= 0 || neecp != neecr) {
            PRINT("Request subject not in the form '<EEC subject> + /CN=<serial>'");
            PRINT("   Versn: "<<xcri->Version());
            PRINT("   Proxy: "<<neecp);
            PRINT("   SubRq: "<<neecr);
            return -kErrPX_BadNames;
         }
      } else {
         PRINT("Request subject not in the form '<issuer subject> + /CN=<serial>'");
         PRINT("   Versn: "<<xcri->Version());
         PRINT("   Proxy: "<<neecp);
         PRINT("   SubRq: "<<neecr);
         return -kErrPX_BadNames;
      }
   }

   // Extract serial number
   XrdOucString sserial(rsbj,rsbj.rfind("/CN=")+4);
   unsigned int serial = (unsigned int)(strtol(sserial.c_str(), 0, 10));
   //
   // Create new proxy cert
   X509 *xpo = X509_new();
   if (!xpo) {
      PRINT("could not create certificate object for proxies"); 
      return -kErrPX_NoResources;
   }

   // Set version number
   if (X509_set_version(xpo, 2L) != 1) {
      PRINT("could not set version"); 
      return -kErrPX_SetAttribute;
   }

   // Set serial number
   if (ASN1_INTEGER_set(X509_get_serialNumber(xpo), serial) != 1) {
      PRINT("could not set serial number"); 
      return -kErrPX_SetAttribute;
   }

   // Set subject name
   if (X509_set_subject_name(xpo, X509_REQ_get_subject_name(xri)) != 1) {
      PRINT("could not set subject name"); 
      return -kErrPX_SetAttribute;
   }
   
   // Set issuer name
   if (X509_set_issuer_name(xpo, X509_get_subject_name(xpi)) != 1) {
      PRINT("could not set issuer name"); 
      return -kErrPX_SetAttribute;
   }

   // Set public key
   if (X509_set_pubkey(xpo, X509_REQ_get_pubkey(xri)) != 1) {
      PRINT("could not set public key"); 
      return -kErrPX_SetAttribute;
   }

   // Set proxy validity: notBefore now
   if (!X509_gmtime_adj(X509_get_notBefore(xpo), 0)) {
      PRINT("could not set notBefore"); 
      return -kErrPX_SetAttribute;
   }

   // Set proxy validity: notAfter timeleft from now
   if (!X509_gmtime_adj(X509_get_notAfter(xpo), timeleft)) {
      PRINT("could not set notAfter"); 
      return -kErrPX_SetAttribute;
   }

   //
   // Get signature path depth from input proxy
   X509_EXTENSION *xpiext = 0, *xriext = 0;
   int npiext = X509_get_ext_count(xpi);
   int i = 0;
   bool haskeyusage = 0;
   int indepthlen = -1;
   for (i = 0; i< npiext; i++) {
      xpiext = X509_get_ext(xpi, i);
      char s[256] = {0};
      ASN1_OBJECT *obj = X509_EXTENSION_get_object(xpiext);
      if (obj) 
         OBJ_obj2txt(s, sizeof(s), obj, 1);
      if (!strcmp(s, gsiProxyCertInfo_OID)) {
         unsigned char *p = xpiext->value->data;
         gsiProxyCertInfo_t *inpci =
            d2i_gsiProxyCertInfo(0, (XRDGSI_CONST unsigned char **)(&p), xpiext->value->length);
         if (inpci && 
             inpci->proxyCertPathLengthConstraint)
            indepthlen = ASN1_INTEGER_get(inpci->proxyCertPathLengthConstraint);
         DEBUG("IN depth length: "<<indepthlen);
      }
      // Flag key usage extension
      if (!haskeyusage && !strcmp(s, KEY_USAGE_OID)) haskeyusage = 1;
      // Fail if a subject alternative name extension is found
      if (!strcmp(s, SUBJ_ALT_NAME_OID)) {
         PRINT("subject alternative name extension not allowed! Skipping request"); 
         return -kErrPX_BadExtension;         
      }
      // Attach to ProxyCertInfo extension if any
      if (!strcmp(s, gsiProxyCertInfo_OID)) {
         if (xriext) {
            PRINT("more than one ProxyCertInfo extension! Skipping request"); 
            return -kErrPX_BadExtension;
         }
         xriext = xpiext;
      } else {
         // Duplicate and add to the stack
         X509_EXTENSION *xpiextdup = X509_EXTENSION_dup(xpiext);
         if (X509_add_ext(xpo, xpiextdup, -1) == 0) {
            PRINT("could not push the extension '"<<s<<"' in the stack"); 
            return -kErrPX_Error;
         }
         // Notify what we added
         int crit = X509_EXTENSION_get_critical(xpiextdup);
         DEBUG("added extension '"<<s<<"', critical: " << crit);
      }
      // Do not free the extension: its owned by the certificate
      xpiext = 0;
   }

   //
   // Get signature path depth from the request
   STACK_OF(X509_EXTENSION) *xrisk = X509_REQ_get_extensions(xri);
   //
   // There must be at most one extension
#if OPENSSL_VERSION_NUMBER >= 0x10000000L
   int nriext = sk_X509_EXTENSION_num(xrisk);
#else /* OPENSSL */
   int nriext = sk_num(xrisk);
#endif /* OPENSSL */
   if (nriext == 0 || !haskeyusage) {
      PRINT("wrong extensions in request: "<< nriext<<", "<<haskeyusage); 
      return -kErrPX_BadExtension;
   }
   //
   // Get the content
   int reqdepthlen = -1;
   unsigned char *p = xriext->value->data;
   gsiProxyCertInfo_t *reqpci =
      d2i_gsiProxyCertInfo(0, (XRDGSI_CONST unsigned char **)(&p), xriext->value->length);
   if (reqpci &&
       reqpci->proxyCertPathLengthConstraint)
      reqdepthlen = ASN1_INTEGER_get(reqpci->proxyCertPathLengthConstraint);
   DEBUG("REQ depth length: "<<reqdepthlen);
   
   // We allow max indepthlen-1
   int outdepthlen = (reqdepthlen < indepthlen) ? reqdepthlen :
                                                 (indepthlen - 1); 
   //
   // Create the extension CertProxyInfo
   gsiProxyCertInfo_t *pci = gsiProxyCertInfo_new();
   if (!pci) {
      PRINT("could not create structure for extension - return"); 
      return -kErrPX_NoResources;
   }

   // Set the new length
   if (outdepthlen > -1) {
      if ((pci->proxyCertPathLengthConstraint = ASN1_INTEGER_new())) {
         int depthlen = (outdepthlen > 0) ? (outdepthlen-1) : 0;
         ASN1_INTEGER_set(pci->proxyCertPathLengthConstraint, depthlen);
      } else {
         PRINT("could not set the path length contrain"); 
         return -kErrPX_SetPathDepth;
      }
   }
   // create extension
   X509_EXTENSION *ext = X509_EXTENSION_new();
   if (!ext) {
      PRINT("could not create extension object"); 
      return -kErrPX_NoResources;
   }
   // Extract data in format for extension
   ext->value->length = i2d_gsiProxyCertInfo(pci, 0);
   if (!(ext->value->data = (unsigned char *)malloc(ext->value->length+1))) {
      PRINT("could not allocate data field for extension"); 
      return -kErrPX_NoResources;
   }
   unsigned char *pp = ext->value->data;
   if ((i2d_gsiProxyCertInfo(pci, &pp)) <= 0) {
      PRINT("problem converting data for extension"); 
      return -kErrPX_Error;
   }
   // Set extension name.
#if OPENSSL_VERSION_NUMBER >= 0x0090700f
   // We do not use directly OBJ_txt2obj because that is not working
   // with all OpenSSL 0.9.6 versions
   ASN1_OBJECT *obj = OBJ_nid2obj(OBJ_create(gsiProxyCertInfo_OID,
                            "gsiProxyCertInfo_OID","GSI ProxyCertInfo OID"));
#else
   // This version of OBJ_txt2obj fixes a bug affecting some 
   // OpenSSL 0.9.6 versions
   ASN1_OBJECT *obj = OBJ_txt2obj_fix(gsiProxyCertInfo_OID, 1);
#endif
   if (!obj || X509_EXTENSION_set_object(ext, obj) != 1) {
      PRINT("could not set extension name"); 
      return -kErrPX_SetAttribute;
   }
   // flag as critical
   if (X509_EXTENSION_set_critical(ext, 1) != 1) {
      PRINT("could not set extension critical flag"); 
      return -kErrPX_SetAttribute;
   }

   // Add the extension
   if (X509_add_ext(xpo, ext, -1) == 0) {
      PRINT("could not add extension"); 
      return -kErrPX_SetAttribute;
   }

   //
   // Sign the certificate
   if (!(X509_sign(xpo, ekpi, EVP_md5()))) {
      PRINT("problems signing the certificate"); 
      return -kErrPX_Signing;
   }

   // Prepare outputs
   *xcpo = new XrdCryptosslX509(xpo);

   // Cleanup
#if OPENSSL_VERSION_NUMBER >= 0x10000000L
   sk_X509_EXTENSION_free(xrisk);
#else /* OPENSSL */
   sk_free(xrisk);
#endif /* OPENSSL */

   // We are done
   return 0;
}

//____________________________________________________________________________
int XrdSslgsiX509DumpExtensions(XrdCryptoX509 *xcpi)
{
   // Dump extensions of the certificate, if any
   // Returns -1 on failure, 0 on success 
   EPNAME("X509DumpExtensions");

   int rc = -1;
   // Make sure we got the right inputs
   if (!xcpi) {
      PRINT("invalid inputs");
      return rc;
   }

   // Point to the cerificate
   X509 *xpi = (X509 *)(xcpi->Opaque());

   rc = 1;
   // Go through the extensions
   X509_EXTENSION *xpiext = 0;
   int npiext = X509_get_ext_count(xpi);
   int i = 0;
   for (i = 0; i< npiext; i++) {
      xpiext = X509_get_ext(xpi, i);
      char s[256];
      OBJ_obj2txt(s, sizeof(s), X509_EXTENSION_get_object(xpiext), 1);
      int crit = X509_EXTENSION_get_critical(xpiext);
      // Notify what we found
      PRINT("found extension '"<<s<<"', critical: " << crit);
      // Dump its content
      rc = 0;
      XRDGSI_CONST unsigned char *pp = (XRDGSI_CONST unsigned char *) xpiext->value->data; 
      long length = xpiext->value->length;
      int ret = XrdSslgsiX509FillUnknownExt(&pp, length);
      PRINT("ret: " << ret);
   }

   // Done
   return rc;
}

//____________________________________________________________________________
int XrdSslgsiX509FillUnknownExt(XRDGSI_CONST unsigned char **pp, long length)
{
   // Do the actual filling of the bio; can be called recursevely
   EPNAME("X509FillUnknownExt");

   XRDGSI_CONST unsigned char *p,*ep,*tot,*op,*opp;
   long len;
   int tag, xclass, ret = 0;
   int nl,hl,j,r;
   ASN1_OBJECT *o = 0;
   ASN1_OCTET_STRING *os = 0;
   /* ASN1_BMPSTRING *bmp=NULL;*/
   int dump_indent = 6;
   int depth = 0;
   int indent = 0;

   p = *pp;
   tot = p + length;
   op = p - 1;
   while ((p < tot) && (op < p)) {
      op = p;
      j = ASN1_get_object(&p, &len, &tag, &xclass, length);
#ifdef LINT
      j = j;
#endif
      if (j & 0x80) {
         PRINT("ERROR: error in encoding");
         ret = 0;
         goto end;
      }
      hl = (p-op);
      length -= hl;
      /* if j == 0x21 it is a constructed indefinite length object */

      if (j != (V_ASN1_CONSTRUCTED | 1)) {
         PRINT("PRIM:  d="<<depth<<" hl="<<hl<<" l="<<len);
      } else {
         PRINT("CONST: d="<<depth<<" hl="<<hl<<" l=inf  ");
      }
      if (!XrdSslgsiX509Asn1PrintInfo(tag, xclass, j, (indent) ? depth : 0))
         goto end;
      if (j & V_ASN1_CONSTRUCTED) {
         ep = p + len;
         PRINT(" ");
         if (len > length) {
            PRINT("ERROR:CONST: length is greater than " <<length);
            ret=0;
            goto end;
         }
         if ((j == 0x21) && (len == 0)) {
            for (;;) {
               r = XrdSslgsiX509FillUnknownExt(&p, (long)(tot-p));
               if (r == 0) {
                  ret = 0;
                  goto end;
               }
               if ((r == 2) || (p >= tot))
                  break;
            }
         } else {
            while (p < ep) {
               r = XrdSslgsiX509FillUnknownExt(&p, (long)len);
               if (r == 0) {
                  ret = 0;
                  goto end;
               }
            }
         }
      } else if (xclass != 0) {
         p += len;
         PRINT(" ");
      } else {
         nl = 0;
         if ((tag == V_ASN1_PRINTABLESTRING) ||
             (tag == V_ASN1_T61STRING) ||
             (tag == V_ASN1_IA5STRING) ||
             (tag == V_ASN1_VISIBLESTRING) ||
             (tag == V_ASN1_NUMERICSTRING) ||
             (tag == V_ASN1_UTF8STRING) ||
             (tag == V_ASN1_UTCTIME) ||
             (tag == V_ASN1_GENERALIZEDTIME)) {
            if (len > 0) {
               char *s = new char[len + 1];
               memcpy(s, p, len);
               s[len] = 0;
               PRINT("GENERIC:" << s <<" (len: "<<(int)len<<")");
               delete [] s;
            } else {
               PRINT("GENERIC: (len: "<<(int)len<<")");
            }
         } else if (tag == V_ASN1_OBJECT) {
            opp = op;
            if (d2i_ASN1_OBJECT(&o, &opp, len+hl)) {
               BIO *mem = BIO_new(BIO_s_mem());
               i2a_ASN1_OBJECT(mem, o);
               XrdOucString objstr;
               BIO_PRINT(mem, "AOBJ:");
            } else {
               PRINT("ERROR:AOBJ: BAD OBJECT");
            }
         } else if (tag == V_ASN1_BOOLEAN) {
            opp = op;
            int ii = d2i_ASN1_BOOLEAN(NULL,&opp,len+hl);
            if (ii < 0) {
               PRINT("ERROR:BOOL: Bad boolean");
               goto end;
            }
            PRINT("BOOL:"<< ii);
         } else if (tag == V_ASN1_BMPSTRING) {
            /* do the BMP thang */
         } else if (tag == V_ASN1_OCTET_STRING) {
            int i, printable = 1;
            opp = op;
            os = d2i_ASN1_OCTET_STRING(0, &opp, len + hl);
            if (os && os->length > 0) {
               opp = os->data;
               /* testing whether the octet string is * printable */
               for (i=0; i<os->length; i++) {
                  if (( (opp[i] < ' ') && (opp[i] != '\n') &&
                        (opp[i] != '\r') && (opp[i] != '\t')) || (opp[i] > '~')) {
                     printable = 0;
                     break;
                  }
               }
               if (printable) {
                  /* printable string */
                  char *s = new char[os->length + 1];
                  memcpy(s, opp, os->length);
                  s[os->length] = 0;
                  PRINT("OBJS:" << s << " (len: "<<os->length<<")");
                  delete [] s;
               } else {
                  /* print the normal dump */
                  if (!nl) PRINT("OBJS:");
                  BIO *mem = BIO_new(BIO_s_mem());
                  if (BIO_dump_indent(mem, (const char *)opp, os->length, dump_indent) <= 0) {
                     PRINT("ERROR:OBJS: problems dumping to BIO");
                     BIO_free(mem);                  
                     goto end;
                  }
                  BIO_PRINT(mem, "OBJS:");
                  nl = 1;
               }
            }
            if (os) {
               M_ASN1_OCTET_STRING_free(os);
               os = 0;
            }
         } else if (tag == V_ASN1_INTEGER) {
            ASN1_INTEGER *bs;
            int i;

            opp = op;
            bs = d2i_ASN1_INTEGER(0, &opp, len+hl);
            if (bs) {
               PRINT("AINT:");
               if (bs->type == V_ASN1_NEG_INTEGER)
                  PRINT("-");
               BIO *mem = BIO_new(BIO_s_mem());
               for (i = 0; i < bs->length; i++) {
                  if (BIO_printf(mem, "%02X", bs->data[i]) <= 0) {
                     PRINT("ERROR:AINT: problems printf-ing to BIO");
                     BIO_free(mem); 
                     goto end;
                  }
               }
               BIO_PRINT(mem, "AINT:");
               if (bs->length == 0) PRINT("00");
            } else {
               PRINT("ERROR:AINT: BAD INTEGER");
            }
            M_ASN1_INTEGER_free(bs);
         } else if (tag == V_ASN1_ENUMERATED) {
            ASN1_ENUMERATED *bs;
            int i;

            opp = op;
            bs = d2i_ASN1_ENUMERATED(0, &opp, len+hl);
            if (bs) {
               PRINT("AENU:");
               if (bs->type == V_ASN1_NEG_ENUMERATED)
                  PRINT("-");
               BIO *mem = BIO_new(BIO_s_mem());
               for (i = 0; i < bs->length; i++) {
                  if (BIO_printf(mem, "%02X", bs->data[i]) <= 0) {
                     PRINT("ERROR:AENU: problems printf-ing to BIO");
                     BIO_free(mem); 
                     goto end;
                  }
               }
               BIO_PRINT(mem, "AENU:");
               if (bs->length == 0) PRINT("00");
            } else {
               PRINT("ERROR:AENU: BAD ENUMERATED");
            }
            M_ASN1_ENUMERATED_free(bs);
         }

         if (!nl) PRINT(" ");

         p += len;
         if ((tag == V_ASN1_EOC) && (xclass == 0)) {
            ret = 2; /* End of sequence */
            goto end;
         }
      }
      length -= len;
   }
   ret = 1;
end:
   if (o) ASN1_OBJECT_free(o);
   if (os) M_ASN1_OCTET_STRING_free(os);
   *pp = p;
   PRINT("ret: "<<ret);

   return ret;
}

//____________________________________________________________________________
int XrdSslgsiX509Asn1PrintInfo(int tag, int xclass, int constructed, int indent)
{
   // Print the BIO content
   EPNAME("X509Asn1PrintInfo");

   static const char fmt[]="%-18s";
   static const char fmt2[]="%2d %-15s";
   char str[128];
   const char *p, *p2 = 0;

   BIO *bp = BIO_new(BIO_s_mem());
   if (constructed & V_ASN1_CONSTRUCTED)
      p = "cons: ";
   else
      p = "prim: ";
   if (BIO_write(bp, p, 6) < 6)
      goto err;
   BIO_indent(bp, indent, 128);

   p = str;
   if ((xclass & V_ASN1_PRIVATE) == V_ASN1_PRIVATE)
      BIO_snprintf(str,sizeof str,"priv [ %d ] ",tag);
   else if ((xclass & V_ASN1_CONTEXT_SPECIFIC) == V_ASN1_CONTEXT_SPECIFIC)
      BIO_snprintf(str,sizeof str,"cont [ %d ]",tag);
   else if ((xclass & V_ASN1_APPLICATION) == V_ASN1_APPLICATION)
      BIO_snprintf(str,sizeof str,"appl [ %d ]",tag);
   else if (tag > 30)
      BIO_snprintf(str,sizeof str,"<ASN1 %d>",tag);
   else
      p = ASN1_tag2str(tag);

   if (p2) {
      if (BIO_printf(bp,fmt2,tag,p2) <= 0)
         goto err;
   } else {
      if (BIO_printf(bp, fmt, p) <= 0)
         goto err;
   }
   BIO_PRINT(bp, "A1PI:");
   return(1);
err:
   BIO_free(bp);
   return(0);
}

//____________________________________________________________________________
int XrdSslgsiX509GetVOMSAttr(XrdCryptoX509 *xcpi, XrdOucString &vat)
{
   // Get VOMS attributes from the certificate, if present
   // Return 0 in case of success, 1 if VOMS info is not available, < 0 if any
   // error occured
   EPNAME("X509GetVOMSAttr");

   int rc = -1;
   // Make sure we got the right inputs
   if (!xcpi) {
      PRINT("invalid inputs");
      return rc;
   }
   
   // Point to the cerificate
   X509 *xpi = (X509 *)(xcpi->Opaque());

   rc = 1;
   bool getvat = 0;
   // Go through the extensions
   X509_EXTENSION *xpiext = 0;
   int npiext = X509_get_ext_count(xpi);
   int i = 0;
   for (i = 0; i< npiext; i++) {
      xpiext = X509_get_ext(xpi, i);
      char s[256];
      OBJ_obj2txt(s, sizeof(s), X509_EXTENSION_get_object(xpiext), 1);
      // Notify what we found
      DEBUG("found extension '"<<s<<"'");
      if (strcmp(s, XRDGSI_VOMS_ACSEQ_OID)) continue;
      // This is the VOMS extension we are interested for
      rc = 0;
      XRDGSI_CONST unsigned char *pp = (XRDGSI_CONST unsigned char *) xpiext->value->data; 
      long length = xpiext->value->length;
      int ret = XrdSslgsiX509FillVOMS(&pp, length, getvat, vat);
      DEBUG("ret: " << ret << " - vat: " << vat);
   }

   // Done
   return rc;
}

//____________________________________________________________________________
int XrdSslgsiX509FillVOMS(XRDGSI_CONST unsigned char **pp,
                          long length, bool &getvat, XrdOucString &vat)
{
   // Look recursively for the VOMS attributes
   // Return 2 if found, 1 if to continue searching, 0 to stop    
   EPNAME("X509FillVOMS");

   XRDGSI_CONST unsigned char *p,*ep,*tot,*op,*opp;
   long len;
   int tag, xclass, ret = 0;
   int /*nl,*/ hl,j,r;
   ASN1_OBJECT *o = 0;
   ASN1_OCTET_STRING *os = 0;

   bool gotvat = 0;
   p = *pp;
   tot = p + length;
   op = p - 1;
   while ((p < tot) && (op < p)) {
      op = p;
      j = ASN1_get_object(&p, &len, &tag, &xclass, length);
#ifdef LINT
      j = j;
#endif
      if (j & 0x80) {
         PRINT("ERROR: error in encoding");
         ret = 0;
         goto end;
      }
      hl = (p-op);
      length -= hl;
      /* if j == 0x21 it is a constructed indefinite length object */

      if (j & V_ASN1_CONSTRUCTED) {
         ep = p + len;
         if (len > length) {
            PRINT("ERROR:CONST: length is greater than " <<length);
            ret=0;
            goto end;
         }
         if ((j == 0x21) && (len == 0)) {
            for (;;) {
               r = XrdSslgsiX509FillVOMS(&p, (long)(tot-p), getvat, vat);
               if (r == 0) {
                  ret = 0;
                  goto end;
               }
               if ((r == 2) || (p >= tot))
                  break;
            }
         } else {
            while (p < ep) {
               r = XrdSslgsiX509FillVOMS(&p, (long)len, getvat, vat);
               if (r == 0) {
                  ret = 0;
                  goto end;
               }
            }
         }
      } else {
         // nl = 0;
         if (tag == V_ASN1_OBJECT) {
            opp = op;
            if (d2i_ASN1_OBJECT(&o, &opp, len+hl)) {
               BIO *mem = BIO_new(BIO_s_mem());
               i2a_ASN1_OBJECT(mem, o);
               XrdOucString objstr;
               BIO_GET_STRING(mem, objstr);
               // Looking for the right extension ...
               if (objstr == XRDGSI_VOMS_ATCAP_OID || objstr == "idatcap") getvat = 1; 
               DEBUG("AOBJ:"<<objstr<< " (getvat: "<<getvat<<")");
            } else {
               PRINT("ERROR:AOBJ: BAD OBJECT");
            }
         } else if (tag == V_ASN1_OCTET_STRING) {
            int i, printable = 1;
            opp = op;
            os = d2i_ASN1_OCTET_STRING(0, &opp, len + hl);
            if (os && os->length > 0) {
               opp = os->data;
               // Testing whether the octet string is printable
               for (i=0; i<os->length; i++) {
                  if (( (opp[i] < ' ') && (opp[i] != '\n') &&
                        (opp[i] != '\r') && (opp[i] != '\t')) || (opp[i] > '~')) {
                     printable = 0;
                     break;
                  }
               }
               if (printable) {
                  // Printable string: it may be what we need
                  if (getvat) {
                     if (vat.length() > 0) vat += ",";
                     vat += (const char *)opp;
                     gotvat = 1;
                  }
                  DEBUG("OBJS:" << (const char *)opp << " (len: "<<os->length<<")");
               }
            }
            if (os) {
               M_ASN1_OCTET_STRING_free(os);
               os = 0;
            }
         }

         p += len;
         if ((tag == V_ASN1_EOC) && (xclass == 0)) {
            ret = 2; /* End of sequence */
            goto end;
         }
      }
      length -= len;
   }
   ret = 1;
   if (gotvat) {
      getvat = 0;
      ret = 2;
   }
end:
   if (o) ASN1_OBJECT_free(o);
   if (os) M_ASN1_OCTET_STRING_free(os);
   *pp = p;
   DEBUG("ret: "<<ret<<" - getvat: "<<getvat);

   return ret;
}
