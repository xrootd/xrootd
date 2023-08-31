//------------------------------------------------------------------------------
// Copyright (c) 2023 by European Organization for Nuclear Research (CERN)
// Author: Angelo Galavotti <agalavottib@gmail.com>
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

#include "TestEnv.hh"
#include "Utils.hh"
#include "IdentityPlugIn.hh"

#include "GTestXrdHelpers.hh"
#include "XrdZip/XrdZipUtils.hh"
#include "XrdCl/XrdClZipArchive.hh"
#include "XrdCl/XrdClZipListHandler.hh"
#include "XrdCl/XrdClZipOperations.hh"

using namespace XrdClTests;
using namespace XrdCl;

//------------------------------------------------------------------------------
// Class declaration
//------------------------------------------------------------------------------
class ZipTest: public ::testing::Test {
  public:
    void Init();
    std::string archiveUrl;
    std::string testFileUrl;
    ZipArchive zip_file;
};

void ZipTest::Init(){
  using namespace XrdCl;
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;

  EXPECT_TRUE( testEnv->GetString( "MainServerURL", address ) );
  EXPECT_TRUE( testEnv->GetString( "DataPath", dataPath ) );

  std::string path = dataPath + "/data.zip";
  archiveUrl = address + "/" + path;
  std::string testFilePath = dataPath + "/san_martino.txt";
  testFileUrl = address + "/" + testFilePath;
}

TEST_F(ZipTest, ExtractTest) {
  Init();
  uint16_t timeout = 2;
  GTEST_ASSERT_XRDST(zip_file.OpenArchive(archiveUrl, OpenFlags::Read, NULL,  timeout));
  GTEST_ASSERT_XRDST(zip_file.CloseArchive(NULL, timeout));
}

TEST_F(ZipTest, OpenFileTest){
  Init();
  GTEST_ASSERT_XRDST(WaitFor(OpenArchive(zip_file, archiveUrl, OpenFlags::Read)))
  GTEST_ASSERT_XRDST(zip_file.OpenFile("paper.txt", OpenFlags::Read));
  // get stat info for the given file
  StatInfo* info_out;
  GTEST_ASSERT_XRDST(zip_file.Stat("paper.txt", info_out));
  GTEST_ASSERT_XRDST(zip_file.CloseFile());
  GTEST_ASSERT_XRDST_NOTOK(zip_file.OpenFile("gibberish.txt", OpenFlags::Read), errNotFound);
  GTEST_ASSERT_XRDST(WaitFor(CloseArchive(zip_file)));
}

TEST_F(ZipTest, ListFileTest) {
  Init();
  GTEST_ASSERT_XRDST(WaitFor(OpenArchive(zip_file, archiveUrl, OpenFlags::Read)));
  DirectoryList* dummy_list;
  GTEST_ASSERT_XRDST(zip_file.List(dummy_list));
  EXPECT_TRUE(dummy_list != NULL);
  GTEST_ASSERT_XRDST(WaitFor(CloseArchive(zip_file)));
}

TEST_F(ZipTest, GetterTests) {
  Init();

  // Get file
  GTEST_ASSERT_XRDST(WaitFor(OpenArchive(zip_file, archiveUrl, OpenFlags::Read)));
  File* file = NULL;
  file = &(zip_file.GetFile());
  EXPECT_TRUE(file != NULL);

  // Get checksum
  uint32_t cksum;
  GTEST_ASSERT_XRDST(zip_file.GetCRC32("paper.txt", cksum));

  // Get offset (i.e. byte position in the archive)
  uint64_t offset;
  GTEST_ASSERT_XRDST(zip_file.GetOffset("paper.txt", offset));
}