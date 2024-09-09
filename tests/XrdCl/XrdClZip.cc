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
    void SetUp() override;
    void TearDown() override;
    std::string archiveUrl;
    std::string testFileUrl;
    ZipArchive zip_file;
};

void ZipTest::SetUp(){
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;

  EXPECT_TRUE( testEnv->GetString( "MainServerURL", address ) );
  EXPECT_TRUE( testEnv->GetString( "DataPath", dataPath ) );

  std::string path = dataPath + "/data.zip";
  archiveUrl = address + "/" + path;
  std::string testFilePath = dataPath + "/san_martino.txt";
  testFileUrl = address + "/" + testFilePath;

  EXPECT_XRDST_OK(WaitFor(OpenArchive(zip_file, archiveUrl, OpenFlags::Read)));
}

void ZipTest::TearDown()
{
  EXPECT_XRDST_OK(WaitFor(CloseArchive(zip_file)));
}

TEST_F(ZipTest, ExtractTest) {
  /* intentionally empty, just let SetUp() and TearDown() do the work */
}

TEST_F(ZipTest, OpenFileTest){
  EXPECT_XRDST_OK(zip_file.OpenFile("paper.txt", OpenFlags::Read));
  // get stat info for the given file
  StatInfo* info_out;
  EXPECT_XRDST_OK(zip_file.Stat("paper.txt", info_out));
  EXPECT_XRDST_OK(zip_file.CloseFile());
  EXPECT_XRDST_NOTOK(zip_file.OpenFile("gibberish.txt", OpenFlags::Read), errNotFound);
}

TEST_F(ZipTest, ListFileTest) {
  DirectoryList* dummy_list;
  EXPECT_XRDST_OK(zip_file.List(dummy_list));
  ASSERT_TRUE(dummy_list);
}

TEST_F(ZipTest, GetterTests) {
  // Get file
  File* file = NULL;
  file = &(zip_file.GetFile());
  ASSERT_TRUE(file);

  // Get checksum
  uint32_t cksum;
  EXPECT_XRDST_OK(zip_file.GetCRC32("paper.txt", cksum));

  // Get offset (i.e. byte position in the archive)
  uint64_t offset;
  EXPECT_XRDST_OK(zip_file.GetOffset("paper.txt", offset));
}
