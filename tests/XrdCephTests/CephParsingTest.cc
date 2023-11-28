//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Sebastien Ponce <sponce@cern.ch>
//------------------------------------------------------------------------------
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------

#include <cppunit/extensions/HelperMacros.h>
#include <XrdCeph/XrdCephPosix.hh>
#include <XrdOuc/XrdOucEnv.hh>

#define MB 1024*1024
struct CephFile {
  std::string name;
  std::string pool;
  std::string userId;
  unsigned int nbStripes;
  unsigned long long stripeUnit;
  unsigned long long objectSize;
};
void fillCephFileParams(const std::string &params, XrdOucEnv *env, CephFile &file);
void fillCephFile(const char *path, XrdOucEnv *env, CephFile &file);

//------------------------------------------------------------------------------
// Declaration
//------------------------------------------------------------------------------
class CephParsingTest: public CppUnit::TestCase
{
  public:
    CPPUNIT_TEST_SUITE( CephParsingTest );
      CPPUNIT_TEST( ParamTest );
      CPPUNIT_TEST( FileTest );
    CPPUNIT_TEST_SUITE_END();
    void ParamTest();
    void FileTest();
};

CPPUNIT_TEST_SUITE_REGISTRATION( CephParsingTest );

//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------
static CephFile parseParam(std::string param, XrdOucEnv *env= NULL) {
  CephFile cf;
  fillCephFileParams(param, env, cf);
  return cf;
}

static CephFile parseFile(std::string param, XrdOucEnv *env= NULL) {
  CephFile cf;
  fillCephFile(param.c_str(), env, cf);
  return cf;
}

void checkResult(CephFile a, CephFile b) {
  std::cout << a.name << " " << a.pool << " " << a.userId
            << " " << a.nbStripes << " " << a.stripeUnit << " " << a.objectSize
            << " / " << b.name << " " << b.pool << " " << b.userId
            << " " << b.nbStripes << " " << b.stripeUnit << " " << b.objectSize
            << std::endl;
  CPPUNIT_ASSERT_EQUAL(a.name, b.name);
  CPPUNIT_ASSERT_EQUAL(a.pool, b.pool);
  CPPUNIT_ASSERT_EQUAL(a.userId, b.userId);
  CPPUNIT_ASSERT_EQUAL(a.nbStripes, b.nbStripes);
  CPPUNIT_ASSERT_EQUAL(a.stripeUnit, b.stripeUnit);
  CPPUNIT_ASSERT_EQUAL(a.objectSize, b.objectSize);
}

//------------------------------------------------------------------------------
// Param test
//------------------------------------------------------------------------------
void CephParsingTest::ParamTest() {
  std::map<std::string, CephFile> inputs;
  inputs[""] = (CephFile){"", "default", "admin", 1, 4*MB, 4*MB};
  inputs["pool"] = (CephFile){"", "pool", "admin", 1, 4*MB, 4*MB};
  inputs["@"] = (CephFile){"", "default", "", 1, 4*MB, 4*MB};
  inputs["@pool"] = (CephFile){"", "pool", "", 1, 4*MB, 4*MB};
  inputs["user@"] = (CephFile){"", "default", "user", 1, 4*MB, 4*MB};
  inputs["user@pool"] = (CephFile){"", "pool", "user", 1, 4*MB, 4*MB};
  inputs["pool,1"] = (CephFile){"", "pool", "admin", 1, 4*MB, 4*MB};
  inputs["user@pool,1"] = (CephFile){"", "pool", "user", 1, 4*MB, 4*MB};
  inputs["pool,5"] = (CephFile){"", "pool", "admin", 5, 4*MB, 4*MB};
  inputs["user@pool,5"] = (CephFile){"", "pool", "user", 5, 4*MB, 4*MB};
  inputs["pool,5,200"] = (CephFile){"", "pool", "admin", 5, 200, 4*MB};
  inputs["user@pool,5,200"] = (CephFile){"", "pool", "user", 5, 200, 4*MB};
  inputs["pool,5,200,800"] = (CephFile){"", "pool", "admin", 5, 200, 800};
  inputs["user@pool,5,200,800"] = (CephFile){"", "pool", "user", 5, 200, 800};
  for (std::map<std::string, CephFile>::const_iterator it = inputs.begin();
       it != inputs.end();
       it++) {
    std::cout << it->first << std::endl;
    checkResult(parseParam(it->first), it->second);
  }  
}

//------------------------------------------------------------------------------
// File test
//------------------------------------------------------------------------------
void CephParsingTest::FileTest() {
  std::map<std::string, CephFile> inputs;
  std::vector<std::string> filenames;
  filenames.push_back("");
  filenames.push_back("foo");
  filenames.push_back("/foo/bar");
  filenames.push_back("foo@bar");
  filenames.push_back("foo@bar,1");
  filenames.push_back("foo@bar,1,2");
  filenames.push_back("foo@bar,1,2,3");
  filenames.push_back("foo:bar");
  filenames.push_back(":foo");
  for (std::vector<std::string>::const_iterator it = filenames.begin();
       it != filenames.end();
       it++) {
    if (std::string::npos == it->find(':')) {
      inputs[*it] = (CephFile){*it, "default", "admin", 1, 4*MB, 4*MB};
    }
    inputs[":" + *it] = (CephFile){*it, "default", "admin", 1, 4*MB, 4*MB};
    inputs["pool:" + *it] = (CephFile){*it, "pool", "admin", 1, 4*MB, 4*MB};
    inputs["@:" + *it] = (CephFile){*it, "default", "", 1, 4*MB, 4*MB};
    inputs["@pool:" + *it] = (CephFile){*it, "pool", "", 1, 4*MB, 4*MB};
    inputs["user@:" + *it] = (CephFile){*it, "default", "user", 1, 4*MB, 4*MB};
    inputs["user@pool:" + *it] = (CephFile){*it, "pool", "user", 1, 4*MB, 4*MB};
    inputs["pool,1:" + *it] = (CephFile){*it, "pool", "admin", 1, 4*MB, 4*MB};
    inputs["user@pool,1:" + *it] = (CephFile){*it, "pool", "user", 1, 4*MB, 4*MB};
    inputs["pool,5:" + *it] = (CephFile){*it, "pool", "admin", 5, 4*MB, 4*MB};
    inputs["user@pool,5:" + *it] = (CephFile){*it, "pool", "user", 5, 4*MB, 4*MB};
    inputs["pool,5,200:" + *it] = (CephFile){*it, "pool", "admin", 5, 200, 4*MB};
    inputs["user@pool,5,200:" + *it] = (CephFile){*it, "pool", "user", 5, 200, 4*MB};
    inputs["pool,5,200,800:" + *it] = (CephFile){*it, "pool", "admin", 5, 200, 800};
    inputs["user@pool,5,200,800:" + *it] = (CephFile){*it, "pool", "user", 5, 200, 800};
  }
  for (std::map<std::string, CephFile>::const_iterator it = inputs.begin();
       it != inputs.end();
       it++) {
    std::cout << it->first << std::endl;
    checkResult(parseFile(it->first), it->second);
  }  
}
