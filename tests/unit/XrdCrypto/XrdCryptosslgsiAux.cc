//------------------------------------------------------------------------------
// Tests for the GSI proxy helpers, see src/XrdCrypto/XrdCryptosslgsiAux.cc
//------------------------------------------------------------------------------

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "X509Helpers.hh"
#include "XrdCrypto/XrdCryptoX509Req.hh"
#include "XrdCrypto/XrdCryptosslAux.hh"
#include "XrdCrypto/XrdCryptosslRSA.hh"
#include "XrdCrypto/XrdCryptosslX509.hh"
#include "XrdCrypto/XrdCryptosslX509Req.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdSut/XrdSutBucket.hh"

namespace {

const XrdTest::Extension kKeyUsage{"keyUsage", "critical,digitalSignature,keyEncipherment"};

// The OID of the ProxyCertInfo extension of RFC 3820, and of the GSI 3 proxies
const char kRfcOid[] = "1.3.6.1.5.5.7.1.14";
const char kGsi3Oid[] = "1.3.6.1.4.1.3536.1.222";

// A ProxyCertInfo extension of RFC 3820, with the policy language inheritAll
XrdTest::Extension RfcProxyCertInfo(const std::string &pathlen)
{
   return {"proxyCertInfo", "critical,language:id-ppl-inheritAll" + pathlen};
}

// A ProxyCertInfo extension of the GSI 3 proxies, which has the path length
// after the policy, with the explicit tag [1]:
// SEQUENCE { SEQUENCE { OBJECT inheritAll }, [1] { INTEGER pathlen } }
XrdTest::Extension Gsi3ProxyCertInfo(int pathlen)
{
   const std::string policy = XrdTest::Tlv(0x30, XrdTest::Oid("1.3.6.1.5.5.7.21.1"));
   const std::string length =
      XrdTest::Tlv(0xa1, XrdTest::Tlv(V_ASN1_INTEGER, std::string(1, static_cast<char>(pathlen))));
   return {kGsi3Oid, "critical," + XrdTest::DerValue(XrdTest::Tlv(0x30, policy + length))};
}

// Give the path length constraint of the ProxyCertInfo extension with the OID
// 'oid' in 'exts', or -2 when the extension is not there or not valid.
int PathLength(const STACK_OF(X509_EXTENSION) *exts, const char *oid = kRfcOid)
{
   ASN1_OBJECT *obj = OBJ_txt2obj(oid, 1);
   int loc = X509v3_get_ext_by_OBJ(exts, obj, -1);
   ASN1_OBJECT_free(obj);
   int pathlen = -2;
   if (loc < 0 || !XrdCryptosslProxyCertInfo(X509v3_get_ext(exts, loc), pathlen))
      return -2;
   return pathlen;
}

int PathLength(XrdCryptoX509 *cert, const char *oid = kRfcOid)
{
   return PathLength(X509_get0_extensions(static_cast<X509 *>(cert->Opaque())), oid);
}

int PathLength(XrdCryptoX509Req *req)
{
   STACK_OF(X509_EXTENSION) *exts =
      X509_REQ_get_extensions(static_cast<X509_REQ *>(req->Opaque()));
   int pathlen = PathLength(exts);
   sk_X509_EXTENSION_pop_free(exts, X509_EXTENSION_free);
   return pathlen;
}

// An end-entity certificate (EEC) and a proxy certificate, which the EEC
// signs. The XrdCrypto objects own the OpenSSL objects.
class SslGsiAuxTest : public ::testing::Test {
protected:
   void SetUp() override
   {
      EVP_PKEY *eecKey = XrdTest::TestKey(0);
      eec = std::make_unique<XrdCryptosslX509>(XrdTest::MakeCert(eecKey, "Test User", {kKeyUsage}));
      eecRSA = std::make_unique<XrdCryptosslRSA>(eecKey);
   }

   // Make a proxy of the EEC with the ProxyCertInfo extension 'pci'
   void MakeProxy(const XrdTest::Extension &pci)
   {
      EVP_PKEY *proxyKey = XrdTest::TestKey(1);
      X509 *cert = XrdTest::MakeCert(proxyKey, "1234", {kKeyUsage, pci},
                                     static_cast<X509 *>(eec->Opaque()),
                                     static_cast<EVP_PKEY *>(eecRSA->Opaque()), 1234);
      proxy = std::make_unique<XrdCryptosslX509>(cert);
      proxyRSA = std::make_unique<XrdCryptosslRSA>(proxyKey);
   }

   // Make a proxy request for the proxy. The test owns the request and its key.
   void MakeRequest()
   {
      XrdCryptoX509Req *req = nullptr;
      XrdCryptoRSA *reqKey = nullptr;
      ASSERT_EQ(XrdCryptosslX509CreateProxyReq(proxy.get(), &req, &reqKey), 0);
      request.reset(req);
      requestKey.reset(reqKey);
   }

   std::unique_ptr<XrdCryptosslX509> eec, proxy;
   std::unique_ptr<XrdCryptosslRSA> eecRSA, proxyRSA;
   std::unique_ptr<XrdCryptoX509Req> request;
   std::unique_ptr<XrdCryptoRSA> requestKey;
};

} // namespace

TEST_F(SslGsiAuxTest, ProxyCertInfoReadsPathLength)
{
   MakeProxy(RfcProxyCertInfo(",pathlen:3"));
   EXPECT_EQ(PathLength(proxy.get()), 3);
}

TEST_F(SslGsiAuxTest, ProxyCertInfoGivesMinusOneWithoutPathLength)
{
   MakeProxy(RfcProxyCertInfo(""));
   EXPECT_EQ(PathLength(proxy.get()), -1);
}

TEST_F(SslGsiAuxTest, ProxyCertInfoReadsPathLengthOfGsi3Format)
{
   MakeProxy(Gsi3ProxyCertInfo(3));
   EXPECT_EQ(PathLength(proxy.get(), kGsi3Oid), 3);
}

TEST_F(SslGsiAuxTest, CreateProxyReqAsksForOneLevelLess)
{
   MakeProxy(RfcProxyCertInfo(",pathlen:3"));
   MakeRequest();
   EXPECT_EQ(PathLength(request.get()), 2);
}

TEST_F(SslGsiAuxTest, CreateProxyReqAsksForOneLevelLessOfGsi3Format)
{
   MakeProxy(Gsi3ProxyCertInfo(3));
   MakeRequest();
   EXPECT_EQ(PathLength(request.get()), 2);
}

TEST_F(SslGsiAuxTest, CreateProxyReqSignsRequest)
{
   MakeProxy(RfcProxyCertInfo(",pathlen:3"));
   MakeRequest();
   // Verify the request as the peer gets it. OpenSSL 4.1 does not verify the
   // request in memory, because CreateProxyReq() does not set its version.
   // The caller of Export() owns the bucket.
   std::unique_ptr<XrdSutBucket> bucket(request->Export());
   ASSERT_NE(bucket, nullptr);
   XrdCryptosslX509Req decoded(bucket.get());
   EXPECT_TRUE(decoded.Verify());
}

TEST_F(SslGsiAuxTest, CheckProxy3AcceptsSignedProxy)
{
   MakeProxy(RfcProxyCertInfo(",pathlen:3"));
   MakeRequest();
   XrdCryptoX509 *signedProxy = nullptr;
   ASSERT_EQ(XrdCryptosslX509SignProxyReq(proxy.get(), proxyRSA.get(), request.get(), &signedProxy), 0);
   std::unique_ptr<XrdCryptoX509> signedPtr(signedProxy);

   XrdOucString emsg;
   EXPECT_EQ(XrdCryptosslX509CheckProxy3(signedProxy, emsg), 0) << emsg.c_str();
}

TEST_F(SslGsiAuxTest, CheckProxy3AcceptsGsi3Format)
{
   MakeProxy(Gsi3ProxyCertInfo(3));
   XrdOucString emsg;
   EXPECT_EQ(XrdCryptosslX509CheckProxy3(proxy.get(), emsg), 0) << emsg.c_str();
}

TEST_F(SslGsiAuxTest, CheckProxy3RejectsCertificateWithoutProxyCertInfo)
{
   XrdOucString emsg;
   EXPECT_EQ(XrdCryptosslX509CheckProxy3(eec.get(), emsg), -2);
}

TEST_F(SslGsiAuxTest, GetVOMSAttrReadsAttributeOfExtension)
{
   // SEQUENCE { OBJECT voms-attribute-capability, OCTET STRING attribute }
   const std::string der = XrdTest::Tlv(0x30,
      XrdTest::Oid("1.3.6.1.4.1.8005.100.100.4") +
      XrdTest::Tlv(V_ASN1_OCTET_STRING, "/dteam/Role=NULL/Capability=NULL"));
   XrdCryptosslX509 cert(XrdTest::MakeCert(static_cast<EVP_PKEY *>(eecRSA->Opaque()),
      "Test User", {{"1.3.6.1.4.1.8005.100.100.5", XrdTest::DerValue(der)}}));

   XrdOucString vat;
   ASSERT_EQ(XrdCryptosslX509GetVOMSAttr(&cert, vat), 0);
   EXPECT_STREQ(vat.c_str(), "/dteam/Role=NULL/Capability=NULL");
}

TEST_F(SslGsiAuxTest, GetVOMSAttrGivesOneWithoutExtension)
{
   XrdOucString vat;
   EXPECT_EQ(XrdCryptosslX509GetVOMSAttr(eec.get(), vat), 1);
}
