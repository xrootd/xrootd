//------------------------------------------------------------------------------
// Tests for the OpenSSL X.509 certificate, see src/XrdCrypto/XrdCryptosslX509.cc
//------------------------------------------------------------------------------

#include <gtest/gtest.h>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

#include "X509Helpers.hh"
#include "XrdCrypto/XrdCryptosslTrace.hh"
#include "XrdCrypto/XrdCryptosslX509.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"

namespace {

// Enable the trace of the OpenSSL crypto module, and keep what it writes
// to std::cerr, as long as the object exists.
class TraceCapture {
public:
   TraceCapture() : old(sslTrace), buf(std::cerr.rdbuf(out.rdbuf())) { sslTrace = &trace; }
   ~TraceCapture()
   {
      std::cerr.rdbuf(buf);
      sslTrace = old;
   }

   std::string Output() const { return out.str(); }

private:
   XrdSysLogger logger{STDERR_FILENO, 0};
   XrdSysError eDest{&logger, "test"};
   XrdOucTrace trace{&eDest};
   XrdOucTrace *old;
   std::ostringstream out;
   std::streambuf *buf;
};

class SslX509Test : public ::testing::Test {
protected:
   void SetUp() override { key = XrdTest::TestKey(0); }
   void TearDown() override { EVP_PKEY_free(key); }

   // XrdCryptosslX509 takes the ownership of the certificate
   XrdCryptosslX509 Cert(const std::vector<XrdTest::Extension> &exts)
   {
      return XrdCryptosslX509(XrdTest::MakeCert(key, "host.example.org", exts));
   }

   // Give the trace output of DumpExtensions(dump) for a certificate with an
   // unknown extension that has the content 'der'
   std::string DumpOf(const std::string &der, bool dump = true)
   {
      XrdCryptosslX509 cert = Cert({{"1.3.6.1.4.1.99999.1", XrdTest::DerValue(der)}});
      TraceCapture capture;
      cert.DumpExtensions(dump);
      return capture.Output();
   }

   EVP_PKEY *key = nullptr;
};

} // namespace

TEST_F(SslX509Test, MatchesSANFindsDnsName)
{
   XrdCryptosslX509 cert = Cert({{"subjectAltName", "DNS:other.example.org,DNS:host.example.org"}});
   bool hasSAN = false;
   EXPECT_TRUE(cert.MatchesSAN("host.example.org", hasSAN));
   EXPECT_TRUE(hasSAN);
}

TEST_F(SslX509Test, MatchesSANRejectsOtherHost)
{
   XrdCryptosslX509 cert = Cert({{"subjectAltName", "DNS:other.example.org"}});
   bool hasSAN = false;
   EXPECT_FALSE(cert.MatchesSAN("host.example.org", hasSAN));
   EXPECT_TRUE(hasSAN);
}

TEST_F(SslX509Test, MatchesSANSkipsNameWithEmbeddedNul)
{
   const std::string name("host.example.org\0.evil.org", 26);
   XrdCryptosslX509 cert = Cert({{"subjectAltName",
      XrdTest::DerValue(XrdTest::Tlv(0x30, XrdTest::Tlv(0x82, name)))}});
   bool hasSAN = false;
   EXPECT_FALSE(cert.MatchesSAN("host.example.org", hasSAN));
}

TEST_F(SslX509Test, MatchesSANFindsNameOf255Bytes)
{
   const std::string name = std::string(243, 'a') + ".example.org";
   ASSERT_EQ(name.size(), 255u);
   XrdCryptosslX509 cert = Cert({{"subjectAltName", "DNS:" + name}});
   bool hasSAN = false;
   EXPECT_TRUE(cert.MatchesSAN(name.c_str(), hasSAN));
}

TEST_F(SslX509Test, MatchesSANSkipsNameLongerThan255Bytes)
{
   // The longest name that MatchesSAN() compares has 255 bytes
   const std::string name = std::string(244, 'a') + ".example.org";
   ASSERT_EQ(name.size(), 256u);
   XrdCryptosslX509 cert = Cert({{"subjectAltName", "DNS:" + name}});
   bool hasSAN = false;
   EXPECT_FALSE(cert.MatchesSAN(name.c_str(), hasSAN));
}

TEST_F(SslX509Test, MatchesSANReportsMissingExtension)
{
   XrdCryptosslX509 cert = Cert({});
   bool hasSAN = true;
   EXPECT_FALSE(cert.MatchesSAN("host.example.org", hasSAN));
   EXPECT_FALSE(hasSAN);
}

TEST_F(SslX509Test, MatchesSANRejectsProxyCertificate)
{
   X509 *issuer = XrdTest::MakeCert(key, "Test User");
   XrdCryptosslX509 proxy(XrdTest::MakeCert(key, "1234",
      {{"subjectAltName", "DNS:host.example.org"},
       {"proxyCertInfo", "critical,language:id-ppl-inheritAll"}}, issuer, key));
   X509_free(issuer);
   bool hasSAN = true;
   EXPECT_FALSE(proxy.MatchesSAN("host.example.org", hasSAN));
   EXPECT_FALSE(hasSAN);
}

TEST_F(SslX509Test, BitStrengthGivesKeySize)
{
   XrdCryptosslX509 cert = Cert({});
   EXPECT_EQ(cert.BitStrength(), 2048);
}

TEST_F(SslX509Test, DumpExtensionsWithoutDumpPrintsNoContent)
{
   const std::string out = DumpOf(XrdTest::Tlv(V_ASN1_INTEGER, "\x01\x23\x45"), false);
   EXPECT_EQ(out.find("AINT:"), std::string::npos) << out;
}

TEST_F(SslX509Test, DumpExtensionsPrintsPrintableOctetString)
{
   const std::string out = DumpOf(XrdTest::Tlv(V_ASN1_OCTET_STRING, "hello"));
   EXPECT_NE(out.find("OBJS:hello (len: 5)"), std::string::npos) << out;
}

TEST_F(SslX509Test, DumpExtensionsPrintsBinaryOctetStringAsHexDump)
{
   const std::string out = DumpOf(XrdTest::Tlv(V_ASN1_OCTET_STRING, std::string("\x00\x01\xfe", 3)));
   EXPECT_NE(out.find("0000 - 00 01 fe"), std::string::npos) << out;
}

TEST_F(SslX509Test, DumpExtensionsPrintsIntegerInHex)
{
   const std::string out = DumpOf(XrdTest::Tlv(V_ASN1_INTEGER, "\x01\x23\x45"));
   EXPECT_NE(out.find("AINT:012345"), std::string::npos) << out;
}

TEST_F(SslX509Test, DumpExtensionsPrintsEnumeratedInHex)
{
   const std::string out = DumpOf(XrdTest::Tlv(V_ASN1_ENUMERATED, "\x05"));
   EXPECT_NE(out.find("AENU:05"), std::string::npos) << out;
}

TEST_F(SslX509Test, DumpExtensionsPrintsContextSpecificTag)
{
   const std::string out = DumpOf(XrdTest::Tlv(V_ASN1_CONTEXT_SPECIFIC | 3, "x"));
   EXPECT_NE(out.find("cont [ 3 ]"), std::string::npos) << out;
}

TEST_F(SslX509Test, DumpExtensionsPrintsPrivateTag)
{
   const std::string out = DumpOf(XrdTest::Tlv(V_ASN1_PRIVATE | 4, "x"));
   EXPECT_NE(out.find("priv [ 4 ]"), std::string::npos) << out;
}

TEST_F(SslX509Test, DumpExtensionsPrintsApplicationTag)
{
   const std::string out = DumpOf(XrdTest::Tlv(V_ASN1_APPLICATION | 5, "x"));
   EXPECT_NE(out.find("appl [ 5 ]"), std::string::npos) << out;
}
