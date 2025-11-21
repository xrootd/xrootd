#undef NDEBUG

#include "TestEnv.hh"
#include <XrdCl/XrdClDefaultEnv.hh>
#include <XrdCl/XrdClFile.hh>

#include <gtest/gtest.h>
#include "GTestXrdHelpers.hh"

using namespace testing;

class FileTest : public ::testing::Test {};

static constexpr int NF = 20;

TEST(FileTest, TTLManyStreams)
{
  std::string address;
  std::string dataPath;

  XrdCl::Env *testEnv = XrdClTests::TestEnv::GetEnv();

  EXPECT_TRUE( testEnv->GetString( "MainServerURL", address ) );
  EXPECT_TRUE( testEnv->GetString( "DataPath", dataPath ) );

  XrdCl::Env *env  = XrdCl::DefaultEnv::GetEnv();

  env->PutInt("TimeoutResolution", 1);
  env->PutInt("SubStreamsPerChannel", 16);
  env->PutInt("DataServerTTL", 1);
  env->PutInt("LoadBalancerTTL", 1);
  env->PutInt("ConnectionRetry", 0);

  XrdCl::File f[NF];
  for(size_t i=0;i<NF;i++) {
    std::string fn = "root://u" + std::to_string(i) + "@" +
                      address + "/" + dataPath + "/nosuchfile";
    auto st = f[i].Open(fn.c_str(), XrdCl::OpenFlags::Read);
    EXPECT_TRUE(st.status == XrdCl::stError);
    EXPECT_TRUE(st.errNo == 3011);
  }

  sleep(4); // wait for ttl
}
