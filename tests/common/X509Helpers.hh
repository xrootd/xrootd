#ifndef XRDTESTS_X509HELPERS_HH
#define XRDTESTS_X509HELPERS_HH

//------------------------------------------------------------------------------
// Helpers to make keys and X.509 certificates in memory for the unit tests.
// The helpers use only the OpenSSL API which is common to OpenSSL 1.1.1 and
// later versions.
//------------------------------------------------------------------------------

#include <openssl/conf.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace XrdTest {

// One extension of a certificate, with the name and the value in the syntax
// of the configuration file of OpenSSL, for example {"keyUsage",
// "critical,digitalSignature"}. A value "DER:<hex>" gives the raw content.
using Extension = std::pair<std::string, std::string>;

// Fixed RSA keys of 2048 bits, only for the tests. A new key takes a long
// time under Valgrind, thus the tests do not make their own keys.
const char *const kTestKeys[] = {
   "-----BEGIN PRIVATE KEY-----\n"
   "MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCgmAcQFEMJvcZ4\n"
   "CLu2IbkOo0lvmKDhYyqFtXclyRlM1W20uoazYS2FPtuJ6nDRORr7XuZy/M0Ts/xW\n"
   "IAHiQb20qxUA2K5f92eWe1G1Bqlg7rG1jE+TlmTp7T6pSqlEYUuAxsQak9t5apug\n"
   "FsOMuFB0ejpCWSFHp4LoEpG9us/jy5MGx//p2iLBj3kmKIcdRXw0kuln7lmCnp/n\n"
   "lMCmWusY4AU/Myzu3pHTobKa3ViZoZYebwl+qd9n6wSYziebNgl4g5/dQlP5rxm1\n"
   "be/CRmDOmJdfpqM4ToTagSL8wGA8bD7kgGPLPT2ztgE5PvtTADJzlBy40KHkL3u8\n"
   "RO4gQGOXAgMBAAECggEAArvDtwLZR0OrKqTfaRC24ShnWhZrepqhASifEA4Vt6iU\n"
   "VvSr0UbQqwlwXC8Y337M1kV9v6xM4053d5HQ5mR/EaGt2FdX1HtWEIol0GJ+JI+7\n"
   "h58NRbJUImUu4VvLeMX9LoJzmQ42OKFXZ8iraGxEtk7USgfRWLJhuIiEYYnZ+wMX\n"
   "nWDZjZbdYUFdfuYb1DAKErmALtzeGMuFGXy7lm1NyU1pBC87ybOImrKxUM/9KOID\n"
   "1JF/D35lHIoyan7Ub0o5Cotuckjn3kjQv0GnbI3U8emirSkVlvt+qzu27t4CHzdn\n"
   "aFOACRP1wZt1wNH4UIJrCUdFk5Wswpko2A+i0a/RqQKBgQDeiJ/YTswZydwT/Cfl\n"
   "SZGBgfpRyFgRe45Qp/HV1SPNwI++kEqdyVbY+hKI5evXIAirHTD4hTD/0b1ZPWPk\n"
   "XYRp0fH29Pn2saRnWaUbitG08UGCEc1MeuLmk6a9BZbnUdtHdeZudp4RrJPDubK4\n"
   "A4RxnUMOORHF1NcD84TJqWj8aQKBgQC4vsR0nV5dZMTHcQG6MzPcsCtf40aoCIdT\n"
   "fhjcF884STyUzdUsXVaKUwIFchdDiooudPJrKvAvlIBMErlW1c/XPc+9Zbwb1QX8\n"
   "H8ttPbVGJJRWaY7PjY00UCyiMFHAPLgRFR7Ah3SjlBud75qwqzo47pJ+3nf7Urho\n"
   "0QRUWrJf/wKBgFzM2OvZoF+4wGBNLU3+6gD59SuZwrA3uFeGUeF8atvoy4kKnzNB\n"
   "vUfqsUQJrdoeb778A2oAETj9fFr0sO2Q2iFO/fqgmoT3+7lltubPczhjy4+0YMre\n"
   "DDVfqY71/8IjbrBUSG4UZUK1l0Ft9Uurm9XNYSNTRUgcxDXPnbaDQVbRAoGANENj\n"
   "eEn8pBcZxAIoJvDNPyVnX9z/5BFlly0ZA0qJCLdPOBlysPZGwSLyc2/vPOA4byQO\n"
   "YFrpT8yG7v5tCC+8eHu4BTB+orEE3GpyNhk66vj2Lev2xqiYxa/YsJlSxztmJw7+\n"
   "gCDR0mTX2zopXMlT0z9KmjF9Z8/i82QOAX8W7ykCgYEAgwgadfxwMF2guHamVPFI\n"
   "8ZPczTILGZQVoEBaCBtuDMhm5m8dz2dW6ORqvTwHCCWkpX8ij7p7VKVHnkvM7ZEk\n"
   "mLu3P/lvOcSaIRgKARtYOyn0SPC9cU1xmeeX/fAMlAarzPzCdbzBXJ1VDULaiTnl\n"
   "mSSfB01DrYmH9xInyfZtwDI=\n"
   "-----END PRIVATE KEY-----\n",
   "-----BEGIN PRIVATE KEY-----\n"
   "MIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQCGJuiNsNIWesc4\n"
   "PmDNgxqdTwwNbVxiafJDc9H6gCbUHjZbRHKz5pZ9fNhc6YFyIiYVRe0jpZmk0sES\n"
   "f82utDSwjo1AW0feSe9Cj4MgJgaTQ90t3w1y2mldNXYZnYtwArz+6I+LH8uHRLXb\n"
   "nKtw0Waqus+dkVVJR/bET4NdUKEnNDrZYGIUrYJ9MwWl78lQgVSGlXMONjZndeqe\n"
   "q6fMxqqBycT0PXrkcXPA3IoLT+fAUsP4ryOiRHqCN+cPmGTNrJwDqyredqZhU2ES\n"
   "MPca3fW3M/U9bPbRjQx9k+8/jQ09QjSt/diT+Tj8fSIbX5ahnP468Qdd5beE6nrb\n"
   "YAlwnamfAgMBAAECggEAJoakAJUIMIaAEgnv+PUuvoCGAJEol+a/OVnYsqz8+avX\n"
   "hmxo2HtfOpiQ9lXk4+cksrVLPhJ/xlL0OaDUa9i6zP2TWSkdwZvQB3h5etEQ0FgL\n"
   "+/FzxkktOyRwTzjsY3WV20rkdlriM22rIG+k2Jh6eIt/tkuXh50QJB6FplXSghRW\n"
   "ZPpdcQz9BmkCUtQshAOiDrK0JNESHz7Qht6ZHnfNd9Jx2BPp+6rD60UtD72bq9PC\n"
   "hAxr9cAFyu/4zs+F9vQ9ll7y2XSr1/3U65FBBQojB3ybSbZBXCLmYs73I0jsjmEI\n"
   "UszCG+aqL61jLg3+uE50HEprKqyYtBDQq1qKR4IVqQKBgQC6CDyDkoNkxWSXSi6g\n"
   "A33dTdr80pLzCtRBLwwVShuDxfMSQ84xr1QzSC4X8FRvcMtLRGPDNO+HDO1ZtqWf\n"
   "H1eSZe9dg+eZBx3kZa1L3DOcH7/AG9Q7xIPZAiTZptTdNhDNOZOOAkh3vY03QQuQ\n"
   "mGXDRMT3dh+Dx0X8sGH4/oDWswKBgQC4m3vy600A9VO0ljxSOeakeGpV6+odqrhv\n"
   "Nf2dkVwOu0u4FP8OdAoskkrZP8sYpKFE/iO9ptO2cN+zG+0P/rBhTH8GtQf0aUVE\n"
   "Lhi5LsNEgBPNBL1ffd+IFulhDlxgn5D2BI16/wni7tQFoPsEKx9r5A+WWDXMywwB\n"
   "oJdLN/W3ZQKBgB8QwQo3rTlmvSWGfcK60oniNm+BuohbhpZcBOiJN48Jv02NhSFG\n"
   "5giqRQg6Wd9CNrCa+w3eCSHERt50YbPwx+7qls5G+KXVSmiZkhbY7sVjh+HMw+Q3\n"
   "LM1QTlcKRQGrxaJ/uuQ7/V4BiVNwyT2cNWImWb35O8oFvOo1iibKWTmBAoGAKRp1\n"
   "fqDlhFEbUhohVC2QLQ6inn7oobJhXmGnFm5HEQCi21nRcGZl3Ya1I6oQjmwZfEMZ\n"
   "siBzAyweosKVBfqvSvq9TcEBBGpsAcJBvvmqgJYD0qI7iAd+vc5cKEAKUXk1zZE1\n"
   "7yzaBq0miBpPyZ3QYTVmQxNg5zhOdA5NoXHURAECgYBRhV7ev1aOkH9sr0M+IghI\n"
   "RytZs+SKhQzZLR+wtcftA6Lge1F1rd929qWwQl5eHF8R1sK/855tI5RCjEmXv+ey\n"
   "EhSL9RjV/WrOPX9wK49aDNIYMlzUqiIbKN2D5PZ9vG5IIv/l6u9Ob/cZujE+hzoB\n"
   "nMTnSTT6NMuUqqveV0GGaA==\n"
   "-----END PRIVATE KEY-----\n",
};

// Give a new copy of the fixed key 'index' (0 or 1). The caller owns the key.
inline EVP_PKEY *TestKey(int index)
{
   BIO *bio = BIO_new_mem_buf(kTestKeys[index], -1);
   EVP_PKEY *key = bio ? PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr) : nullptr;
   BIO_free(bio);
   if (!key)
      throw std::runtime_error("cannot read the test key");
   return key;
}

// Encode one DER element with a short length (less than 128 bytes).
inline std::string Tlv(unsigned char tag, const std::string &content)
{
   if (content.size() > 127)
      throw std::length_error("Tlv() supports only the short form of the length");
   return std::string(1, static_cast<char>(tag)) +
          std::string(1, static_cast<char>(content.size())) + content;
}

// Encode an object identifier, for example "1.2.3.4", as a DER element.
inline std::string Oid(const char *oid)
{
   ASN1_OBJECT *obj = OBJ_txt2obj(oid, 1);
   if (!obj)
      throw std::invalid_argument(oid);
   std::string der(static_cast<size_t>(i2d_ASN1_OBJECT(obj, nullptr)), '\0');
   unsigned char *p = reinterpret_cast<unsigned char *>(&der[0]);
   i2d_ASN1_OBJECT(obj, &p);
   ASN1_OBJECT_free(obj);
   return der;
}

// Give the value "DER:<hex>" of an extension with the content 'der'.
inline std::string DerValue(const std::string &der)
{
   std::string value = "DER:";
   char hex[4];
   for (size_t i = 0; i < der.size(); i++) {
      snprintf(hex, sizeof(hex), i ? ":%02X" : "%02X",
               static_cast<unsigned char>(der[i]));
      value += hex;
   }
   return value;
}

// Make a certificate for 'key', with the common name 'cn' and the extensions
// 'exts'. The common name can have an embedded NUL character. The issuer
// signs the certificate with 'issuerKey'. Without an issuer, the certificate
// is self-signed, and its subject is "/O=XRootD Tests/CN=<cn>". XrdCrypto
// takes a certificate with one CN, signed by itself, for a proxy. The
// certificate is valid for one hour. The caller owns the certificate.
inline X509 *MakeCert(EVP_PKEY *key, const std::string &cn,
                      const std::vector<Extension> &exts = {},
                      X509 *issuer = nullptr, EVP_PKEY *issuerKey = nullptr,
                      long serial = 1)
{
   X509 *cert = X509_new();
   if (!cert)
      throw std::runtime_error("cannot make a certificate");

   X509_set_version(cert, 2);
   ASN1_INTEGER_set(X509_get_serialNumber(cert), serial);
   X509_gmtime_adj(X509_getm_notBefore(cert), -60);
   X509_gmtime_adj(X509_getm_notAfter(cert), 3600);
   X509_set_pubkey(cert, key);

   // A proxy certificate has the subject of the issuer with one more CN
   X509_NAME *name = nullptr;
   if (issuer) {
      name = X509_NAME_dup(const_cast<X509_NAME *>(X509_get_subject_name(issuer)));
   } else {
      name = X509_NAME_new();
      X509_NAME_add_entry_by_txt(name, "O", MBSTRING_UTF8,
         reinterpret_cast<const unsigned char *>("XRootD Tests"), -1, -1, 0);
   }
   const unsigned char *bytes = reinterpret_cast<const unsigned char *>(cn.data());
   X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8, bytes,
                              static_cast<int>(cn.size()), -1, 0);
   X509_set_subject_name(cert, name);
   X509_NAME_free(name);

   X509 *signer = issuer ? issuer : cert;
   X509_set_issuer_name(cert, const_cast<X509_NAME *>(X509_get_subject_name(signer)));

   // Some extensions, for example proxyCertInfo, need a configuration
   // database, also when the value does not refer to a section.
   CONF *conf = NCONF_new(nullptr);
   X509V3_CTX ctx;
   X509V3_set_ctx(&ctx, signer, cert, nullptr, nullptr, 0);
   X509V3_set_nconf(&ctx, conf);
   for (const auto &ext : exts) {
      X509_EXTENSION *x = X509V3_EXT_nconf(conf, &ctx, ext.first.c_str(),
                                           ext.second.c_str());
      if (!x || !X509_add_ext(cert, x, -1)) {
         X509_EXTENSION_free(x);
         X509_free(cert);
         NCONF_free(conf);
         throw std::runtime_error("cannot add the extension " + ext.first);
      }
      X509_EXTENSION_free(x);
   }
   NCONF_free(conf);

   if (!X509_sign(cert, issuerKey ? issuerKey : key, EVP_sha256())) {
      X509_free(cert);
      throw std::runtime_error("cannot sign the certificate");
   }

   // Encode the certificate, and decode it again. Then the strings in the
   // certificate come from the decoder of OpenSSL, as for a certificate of
   // a peer or of a file. OpenSSL 4.1 marks the byte after the data of such
   // strings as not accessible for Valgrind and the sanitizers.
   unsigned char *der = nullptr;
   int len = i2d_X509(cert, &der);
   X509_free(cert);
   const unsigned char *p = der;
   X509 *decoded = len > 0 ? d2i_X509(nullptr, &p, len) : nullptr;
   OPENSSL_free(der);
   if (!decoded)
      throw std::runtime_error("cannot encode and decode the certificate");
   return decoded;
}

} // namespace XrdTest

#endif
