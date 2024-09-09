#undef NDEBUG

#include <XrdCl/XrdClDefaultEnv.hh>
#include <XrdCl/XrdClFile.hh>

#include <gtest/gtest.h>
#include "GTestXrdHelpers.hh"

using namespace testing;

class FileTest : public ::testing::Test {};

TEST(FileTest, StreamTimeout)
{
  XrdCl::Env *env  = XrdCl::DefaultEnv::GetEnv();

  env->PutInt("StreamTimeout", 1); //60 is default
  env->PutInt("TimeoutResolution", 0); //15 is default

  char buf[16];
  uint32_t BytesRead = 0;
  XrdCl::File f;

  f.SetProperty("ReadRecovery", "false");
  EXPECT_XRDST_OK(f.Open("root://localhost//test.txt",
                         XrdCl::OpenFlags::Read));
  sleep(3); // wait for timeout
  EXPECT_XRDST_OK(f.Read(0, 5, buf, BytesRead, 0));
  EXPECT_XRDST_OK(f.Close());
}
