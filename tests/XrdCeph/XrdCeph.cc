#undef NDEBUG

#include <XrdCeph/XrdCephPosix.hh>
#include <XrdOuc/XrdOucEnv.hh>

#include <gtest/gtest.h>

#define MB 1024 * 1024

struct CephFile {
  std::string name;
  std::string pool;
  std::string userId;
  unsigned int nbStripes;
  unsigned long long stripeUnit;
  unsigned long long objectSize;
};

void fillCephFile(const char *path, XrdOucEnv *env, CephFile &file);
void fillCephFileParams(const std::string &params, XrdOucEnv *env,
                        CephFile &file);

using namespace testing;

class ParsingTest : public ::testing::Test {};

static CephFile parseParam(std::string param, XrdOucEnv *env = NULL)
{
  CephFile cf;
  fillCephFileParams(param, env, cf);
  return cf;
}

static CephFile parseFile(std::string param, XrdOucEnv *env = NULL)
{
  CephFile cf;
  fillCephFile(param.c_str(), env, cf);
  return cf;
}

static void checkResult(CephFile a, CephFile b)
{
  EXPECT_STREQ(a.name.c_str(), b.name.c_str());
  EXPECT_STREQ(a.pool.c_str(), b.pool.c_str());
  EXPECT_STREQ(a.userId.c_str(), b.userId.c_str());
  EXPECT_EQ(a.nbStripes, b.nbStripes);
  EXPECT_EQ(a.stripeUnit, b.stripeUnit);
  EXPECT_EQ(a.objectSize, b.objectSize);
}

TEST(ParsingTest, Parameters)
{
  std::map<std::string, CephFile> inputs;

  inputs[""] = (CephFile){"", "default", "admin", 1, 4 * MB, 4 * MB};
  inputs["pool"] = (CephFile){"", "pool", "admin", 1, 4 * MB, 4 * MB};
  inputs["@"] = (CephFile){"", "default", "", 1, 4 * MB, 4 * MB};
  inputs["@pool"] = (CephFile){"", "pool", "", 1, 4 * MB, 4 * MB};
  inputs["user@"] = (CephFile){"", "default", "user", 1, 4 * MB, 4 * MB};
  inputs["user@pool"] = (CephFile){"", "pool", "user", 1, 4 * MB, 4 * MB};
  inputs["pool,1"] = (CephFile){"", "pool", "admin", 1, 4 * MB, 4 * MB};
  inputs["user@pool,1"] = (CephFile){"", "pool", "user", 1, 4 * MB, 4 * MB};
  inputs["pool,5"] = (CephFile){"", "pool", "admin", 5, 4 * MB, 4 * MB};
  inputs["user@pool,5"] = (CephFile){"", "pool", "user", 5, 4 * MB, 4 * MB};
  inputs["pool,5,200"] = (CephFile){"", "pool", "admin", 5, 200, 4 * MB};
  inputs["user@pool,5,200"] = (CephFile){"", "pool", "user", 5, 200, 4 * MB};
  inputs["pool,5,200,800"] = (CephFile){"", "pool", "admin", 5, 200, 800};
  inputs["user@pool,5,200,800"] = (CephFile){"", "pool", "user", 5, 200, 800};

  for (auto it = inputs.begin(); it != inputs.end(); it++)
    checkResult(parseParam(it->first), it->second);
}

TEST(ParsingTest, File)
{
  std::vector<std::string> filenames;
  std::map<std::string, CephFile> inputs;

  filenames.push_back("");
  filenames.push_back("foo");
  filenames.push_back("/foo/bar");
  filenames.push_back("foo@bar");
  filenames.push_back("foo@bar,1");
  filenames.push_back("foo@bar,1,2");
  filenames.push_back("foo@bar,1,2,3");
  filenames.push_back("foo:bar");
  filenames.push_back(":foo");

  for (auto it = filenames.begin(); it != filenames.end(); it++) {
    if (std::string::npos == it->find(':'))
      inputs[*it] = (CephFile){*it, "default", "admin", 1, 4 * MB, 4 * MB};

    inputs[":" + *it] = (CephFile){*it, "default", "admin", 1, 4 * MB, 4 * MB};
    inputs["pool:" + *it] = (CephFile){*it, "pool", "admin", 1, 4 * MB, 4 * MB};
    inputs["@:" + *it] = (CephFile){*it, "default", "", 1, 4 * MB, 4 * MB};
    inputs["@pool:" + *it] = (CephFile){*it, "pool", "", 1, 4 * MB, 4 * MB};
    inputs["user@:" + *it] = (CephFile){*it, "default", "user", 1, 4 * MB, 4 * MB};
    inputs["user@pool:" + *it] = (CephFile){*it, "pool", "user", 1, 4 * MB, 4 * MB};
    inputs["pool,1:" + *it] = (CephFile){*it, "pool", "admin", 1, 4 * MB, 4 * MB};
    inputs["user@pool,1:" + *it] = (CephFile){*it, "pool", "user", 1, 4 * MB, 4 * MB};
    inputs["pool,5:" + *it] = (CephFile){*it, "pool", "admin", 5, 4 * MB, 4 * MB};
    inputs["user@pool,5:" + *it] = (CephFile){*it, "pool", "user", 5, 4 * MB, 4 * MB};
    inputs["pool,5,200:" + *it] = (CephFile){*it, "pool", "admin", 5, 200, 4 * MB};
    inputs["user@pool,5,200:" + *it] = (CephFile){*it, "pool", "user", 5, 200, 4 * MB};
    inputs["pool,5,200,800:" + *it] = (CephFile){*it, "pool", "admin", 5, 200, 800};
    inputs["user@pool,5,200,800:" + *it] = (CephFile){*it, "pool", "user", 5, 200, 800};
  }

  for (auto it = inputs.begin(); it != inputs.end(); it++)
    checkResult(parseFile(it->first), it->second);
}
