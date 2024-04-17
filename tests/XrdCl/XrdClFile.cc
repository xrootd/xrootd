#undef NDEBUG

#include <XrdCl/XrdClDefaultEnv.hh>
#include <XrdCl/XrdClFile.hh>

#include <gtest/gtest.h>

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

  auto st = f.Open("root://localhost//test.txt", XrdCl::OpenFlags::Read);

  EXPECT_TRUE(st.IsOK()) << "Open not OK:" << st.ToString() << std::endl;

  sleep(3); // wait for timeout

  st = f.Read(0, 5, buf, BytesRead, 0);

  EXPECT_TRUE(st.IsOK()) << "Read not OK:" << st.ToString() << std::endl;

  st = f.Close();

  EXPECT_TRUE(st.IsOK()) << "Close not OK:" << st.ToString() << std::endl;
}
