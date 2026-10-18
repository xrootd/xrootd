#undef NDEBUG

#include <XrdCl/XrdClConstants.hh>
#include <XrdCl/XrdClDefaultEnv.hh>
#include <XrdCl/XrdClEnv.hh>
#include <gtest/gtest.h>

#include <cstdlib>

using XrdCl::Env;

class XrdClEnvTest : public ::testing::Test {
protected:
  void SetUp() override {
    XrdCl::DefaultEnv::SetLogLevel("Dump");

    unsetenv("XRD_TEST_INVALID");

    setenv("XRD_TEST_INT", "42", 1);
    setenv("XRD_TEST_STRING", "XRootD", 1);
  }

  Env e;
};

TEST_F(XrdClEnvTest, Int) {
  // get by default value
  int ParallelEvtLoop = 0;
  EXPECT_TRUE(e.GetDefaultIntValue("ParallelEvtLoop", ParallelEvtLoop));
  EXPECT_EQ(ParallelEvtLoop, XrdCl::DefaultParallelEvtLoop);

  // get/put by normalized name
  int test_int = 0;
  EXPECT_TRUE(e.PutInt("test_int", -1));
  EXPECT_TRUE(e.GetInt("test_int", test_int));
  EXPECT_EQ(test_int, -1);

  // get/put by environment name
  EXPECT_TRUE(e.PutInt("XRD_TEST_INT", 0));
  EXPECT_TRUE(e.GetInt("XRD_TEST_INT", test_int));
  EXPECT_EQ(test_int, 0);

  // imported value, should not be overriden
  EXPECT_TRUE(e.ImportInt("test_int", "XRD_TEST_INT"));
  EXPECT_FALSE(e.PutInt("XRD_TEST_INT", 1));
  EXPECT_TRUE(e.GetInt("XRD_TEST_INT", test_int));
  EXPECT_EQ(test_int, 42);

  // should return false if importing unset environment variable
  int test_invalid = 1729;
  EXPECT_FALSE(e.ImportInt("test_invalid", "XRD_TEST_INVALID"));
  EXPECT_FALSE(e.GetInt("XRD_TEST_INVALID", test_invalid));
  EXPECT_EQ(test_invalid, 1729); // unchanged

  // should return false if value imported is not an integer
  EXPECT_FALSE(e.ImportInt("test_string", "XRD_TEST_STRING"));
}

TEST_F(XrdClEnvTest, String) {
  std::string str;

  // get by default value
  EXPECT_FALSE(e.GetDefaultStringValue("DoesNotExist", str));
  EXPECT_TRUE(e.GetDefaultStringValue("XRD_CPRETRYPOLICY", str));
  EXPECT_EQ(str, XrdCl::DefaultCpRetryPolicy);

  // get/put by normalized name
  EXPECT_TRUE(e.PutString("test_string", "hello1"));
  EXPECT_TRUE(e.GetString("test_string", str));
  EXPECT_EQ(str, "hello1");

  EXPECT_TRUE(e.PutString("XRD_TEST_STRING", "hello2"));
  EXPECT_TRUE(e.GetString("test_string", str));
  EXPECT_EQ(str, "hello2");

  // get/put by environment name
  EXPECT_TRUE(e.PutString("XRD_TEST_STRING", "hello3"));
  EXPECT_TRUE(e.GetString("XRD_TEST_STRING", str));
  EXPECT_EQ(str, "hello3");

  EXPECT_TRUE(e.PutString("test_string", "hello4"));
  EXPECT_TRUE(e.GetString("XRD_TEST_STRING", str));
  EXPECT_EQ(str, "hello4");

  // imported value, should not be overriden
  EXPECT_TRUE(e.ImportString("test_string", "XRD_TEST_STRING"));
  EXPECT_FALSE(e.PutString("XRD_TEST_STRING", "hello5"));
  EXPECT_FALSE(e.PutString("test_string", "hello6"));
  EXPECT_TRUE(e.GetString("XRD_TEST_STRING", str));
  EXPECT_EQ(str, "XRootD");

  // should return false if importing unset environment variable
  str = "valid";
  EXPECT_FALSE(e.ImportString("test_invalid", "XRD_TEST_INVALID"));
  EXPECT_FALSE(e.GetString("XRD_TEST_INVALID", str));
  EXPECT_EQ(str, "valid");
}

TEST_F(XrdClEnvTest, Pointer) {
  void *ptr = nullptr;

  EXPECT_FALSE(e.GetPtr("XRD_TEST_INVALID", ptr));
  EXPECT_EQ(ptr, nullptr);

  EXPECT_TRUE(e.PutPtr("gStream", this));
  EXPECT_TRUE(e.GetPtr("gStream", ptr));
  EXPECT_EQ(ptr, this);
}
