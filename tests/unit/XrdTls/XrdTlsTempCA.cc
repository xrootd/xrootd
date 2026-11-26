//------------------------------------------------------------------------------
// The destructor of XrdTlsTempCA must return. It used to block when the
// monitor thread was never started, and it waits for that thread when the
// thread is running.
//------------------------------------------------------------------------------

#include <gtest/gtest.h>

#include <openssl/pem.h>
#include <openssl/x509.h>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unistd.h>

#include "X509Helpers.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdTls/XrdTlsTempCA.hh"

namespace {

class TempCATest : public ::testing::Test {
protected:
   void SetUp() override
   {
      const char *cur = getenv("XRDADMINPATH");
      if (cur) {
         saved_ = cur;
         had_ = true;
      }
   }

   void TearDown() override
   {
      alarm(0);
      if (had_)
         setenv("XRDADMINPATH", saved_.c_str(), 1);
      else
         unsetenv("XRDADMINPATH");
   }

   // Arm a short alarm so a blocked destructor fails this test instead of
   // waiting out the ctest timeout.
   static void Destroy(std::unique_ptr<XrdTlsTempCA> &ca)
   {
      alarm(5);
      ca.reset();
      alarm(0);
   }

   std::string saved_;
   bool had_ = false;
};

class TempDir {
public:
   explicit TempDir(const char *name)
   {
      path_ = (std::filesystem::temp_directory_path() / name).string();
      if (!mkdtemp(path_.data()))
         throw std::runtime_error("cannot create a temporary directory");
   }

   ~TempDir()
   {
      std::error_code ec;
      std::filesystem::remove_all(path_, ec);
   }

   const std::string &path() const { return path_; }

private:
   std::string path_;
};

struct EvpFree {
   void operator()(EVP_PKEY *key) const { EVP_PKEY_free(key); }
};

struct X509Free {
   void operator()(X509 *cert) const { X509_free(cert); }
};

void WritePem(const std::string &dir)
{
   std::unique_ptr<EVP_PKEY, EvpFree> key(XrdTest::TestKey(0));
   std::unique_ptr<X509, X509Free> cert(XrdTest::MakeCert(
      key.get(), "ca.example.org", {{"basicConstraints", "critical,CA:TRUE"}}));
   FILE *fp = fopen((dir + "/ca.pem").c_str(), "w");
   if (!fp)
      throw std::runtime_error("cannot create the certificate file");
   const bool ok = PEM_write_X509(fp, cert.get()) == 1;
   fclose(fp);
   if (!ok)
      throw std::runtime_error("cannot write the certificate");
}

} // namespace

TEST_F(TempCATest, DestructorReturnsWhenTheMonitorNeverStarts)
{
   unsetenv("XRDADMINPATH");
   XrdSysLogger logger;
   XrdSysError err(&logger, "tls");
   std::unique_ptr<XrdTlsTempCA> ca(new XrdTlsTempCA(&err, "/no/such/ca", false));
   EXPECT_FALSE(ca->IsValid());
   TempCATest::Destroy(ca);
}

TEST_F(TempCATest, DestructorReturnsWhenTheMonitorIsRunning)
{
   TempDir admin("xrdtls-admin-XXXXXX");
   TempDir cadir("xrdtls-ca-XXXXXX");
   WritePem(cadir.path());
   ASSERT_EQ(setenv("XRDADMINPATH", admin.path().c_str(), 1), 0);

   XrdSysLogger logger;
   XrdSysError err(&logger, "tls");
   std::unique_ptr<XrdTlsTempCA> ca(new XrdTlsTempCA(&err, cadir.path(), false));
   ASSERT_TRUE(ca->IsValid());
   TempCATest::Destroy(ca);
}
