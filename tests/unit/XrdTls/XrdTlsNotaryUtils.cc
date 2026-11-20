//------------------------------------------------------------------------------
// Tests for the hostname validation, see src/XrdTls/XrdTlsNotaryUtils.icc
//------------------------------------------------------------------------------

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "X509Helpers.hh"
#include "XrdTls/XrdTlsNotaryUtils.hh"

namespace {

using X509Ptr = std::unique_ptr<X509, decltype(&X509_free)>;

class ValidateHostnameTest : public ::testing::Test {
protected:
   void SetUp() override { key = XrdTest::TestKey(0); }
   void TearDown() override { EVP_PKEY_free(key); }

   X509Ptr Cert(const std::string &cn, const std::vector<XrdTest::Extension> &exts = {})
   {
      return X509Ptr(XrdTest::MakeCert(key, cn, exts), &X509_free);
   }

   // A subjectAltName extension with one DNS name, which can have a NUL
   static XrdTest::Extension DnsName(const std::string &name)
   {
      return {"subjectAltName", XrdTest::DerValue(XrdTest::Tlv(0x30, XrdTest::Tlv(0x82, name)))};
   }

   EVP_PKEY *key = nullptr;
};

} // namespace

TEST_F(ValidateHostnameTest, MatchesDnsNameOfSubjectAltName)
{
   auto cert = Cert("other.example.org", {{"subjectAltName", "DNS:host.example.org"}});
   EXPECT_EQ(validate_hostname("host.example.org", cert.get()), MatchFound);
}

TEST_F(ValidateHostnameTest, RejectsHostNotInSubjectAltName)
{
   auto cert = Cert("host.example.org", {{"subjectAltName", "DNS:other.example.org"}});
   EXPECT_EQ(validate_hostname("host.example.org", cert.get()), MatchNotFound);
}

TEST_F(ValidateHostnameTest, RejectsSubjectAltNameWithEmbeddedNul)
{
   auto cert = Cert("host.example.org",
                    {DnsName(std::string("host.example.org\0.evil.org", 26))});
   EXPECT_EQ(validate_hostname("host.example.org", cert.get()), MalformedCertificate);
}

TEST_F(ValidateHostnameTest, MatchesCommonNameWithoutSubjectAltName)
{
   auto cert = Cert("host.example.org");
   EXPECT_EQ(validate_hostname("host.example.org", cert.get()), MatchFound);
}

TEST_F(ValidateHostnameTest, RejectsCommonNameWithEmbeddedNul)
{
   auto cert = Cert(std::string("host.example.org\0.evil.org", 26));
   EXPECT_EQ(validate_hostname("host.example.org", cert.get()), MalformedCertificate);
}
