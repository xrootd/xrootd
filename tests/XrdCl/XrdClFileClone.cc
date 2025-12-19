//------------------------------------------------------------------------------
// Copyright (c) 2025 by European Organization for Nuclear Research (CERN)
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
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClPlugInManager.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XrdCl/XrdClSIDManager.hh"
#include "XrdCl/XrdClPostMaster.hh"
#include "XrdCl/XrdClXRootDTransport.hh"
#include "XrdCl/XrdClMessageUtils.hh"
#include "XrdCl/XrdClXRootDMsgHandler.hh"
#include "XrdCl/XrdClCopyProcess.hh"
#include "XrdCl/XrdClZipArchive.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClZipOperations.hh"
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>

using namespace XrdClTests;

//------------------------------------------------------------------------------
// Class declaration
//------------------------------------------------------------------------------
class FileClone: public ::testing::Test
{
  public:
    void FileCloneTest();
};

//------------------------------------------------------------------------------
// Tests declaration
//------------------------------------------------------------------------------
TEST_F(FileClone, FileCloneTest)
{
  //----------------------------------------------------------------------------
  // Initialize
  //----------------------------------------------------------------------------
  XrdCl::Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;
  std::string localDataPath;

  EXPECT_TRUE( testEnv->GetString( "MainServerURL", address ) );
  EXPECT_TRUE( testEnv->GetString( "DataPath", dataPath ) );
  EXPECT_TRUE( testEnv->GetString( "LocalDataPath", localDataPath ) );

  XrdCl::URL url( address );
  EXPECT_TRUE( url.IsValid() );

  std::string fileCloneSrc1 = address + "/" + dataPath + "/clonesrc1.dat";
  std::string fileCloneSrc2 = address + "/" + dataPath + "/clonesrc2.dat";
  std::string fileCloneSrc3 = address + "/" + dataPath + "/clonesrc3.dat";

  std::string fileCloneDst1 = address + "/" + dataPath + "/wholecloned.dat";
  std::string fileCloneDst2 = address + "/" + dataPath + "/rangecloned.dat";

  XrdCl::FileSystem fs( address );
  if (fs.Rm( "/" + dataPath + "/clonesrc1.dat" ).IsOK()) {}
  if (fs.Rm( "/" + dataPath + "/clonesrc2.dat" ).IsOK()) {}
  if (fs.Rm( "/" + dataPath + "/clonesrc3.dat" ).IsOK()) {}
  if (fs.Rm( "/" + dataPath + "/wholecloned.dat" ).IsOK()) {}
  if (fs.Rm( "/" + dataPath + "/rangecloned.dat" ).IsOK()) {}

  XrdCl::File file1,file2,file3;
  EXPECT_XRDST_OK( file1.Open( fileCloneSrc1, XrdCl::OpenFlags::New ) );
  EXPECT_XRDST_OK( file2.OpenUsingTemplate( file1, fileCloneSrc2, XrdCl::OpenFlags::New|XrdCl::OpenFlags::Samefs ) );
  EXPECT_XRDST_OK( file3.OpenUsingTemplate( file1, fileCloneSrc3, XrdCl::OpenFlags::New|XrdCl::OpenFlags::Samefs ) );

  std::string buffer1(8192, 'A');
  std::string buffer2(8192, 'B');
  std::string buffer3(8192, 'C');
  EXPECT_XRDST_OK( file1.Write( 0, 8192, buffer1.data() ) );
  EXPECT_XRDST_OK( file1.Write( 8192, 8192, buffer2.data() ) );
  EXPECT_XRDST_OK( file1.Write( 16384, 8192, buffer3.data() ) );

  EXPECT_XRDST_OK( file2.Write( 0, 8192, buffer2.data() ) );
  EXPECT_XRDST_OK( file2.Write( 8192, 8192, buffer3.data() ) );
  EXPECT_XRDST_OK( file2.Write( 16384, 8192, buffer1.data() ) );

  EXPECT_XRDST_OK( file3.Write( 0, 8192, buffer3.data() ) );
  EXPECT_XRDST_OK( file3.Write( 8192, 8192, buffer1.data() ) );
  EXPECT_XRDST_OK( file3.Write( 16384, 8192, buffer2.data() ) );

  // the files should be on the same fs
  // (we just check they are at the same server)
  XrdCl::LocationInfo *l1=0,*l2=0,*l3=0;
  EXPECT_XRDST_OK( fs.Locate( "/" + dataPath + "/clonesrc1.dat", XrdCl::OpenFlags::Read, l1, 0) );
  EXPECT_XRDST_OK( fs.Locate( "/" + dataPath + "/clonesrc2.dat", XrdCl::OpenFlags::Read, l2, 0) );
  EXPECT_XRDST_OK( fs.Locate( "/" + dataPath + "/clonesrc3.dat", XrdCl::OpenFlags::Read, l3, 0) );

  EXPECT_TRUE( l1 && l1->GetSize() );
  EXPECT_TRUE( l2 && l2->GetSize() );
  EXPECT_TRUE( l3 && l3->GetSize() );

  std::string loc = l1->At(0).GetAddress();
  EXPECT_TRUE(l2->At(0).GetAddress() == loc);
  EXPECT_TRUE(l3->At(0).GetAddress() == loc);

  delete l1; l1=0;
  delete l2; l2=0;
  delete l3; l3=0;

  XrdCl::File newf1,newf2;
  auto st1 = newf1.OpenUsingTemplate( file1, fileCloneDst1, XrdCl::OpenFlags::New|XrdCl::OpenFlags::Update|XrdCl::OpenFlags::Dup );
  EXPECT_TRUE( st1.IsOK() || ( st1.IsError() && st1.code == XrdCl::errErrorResponse && st1.errNo == kXR_Unsupported ) );
  EXPECT_XRDST_OK( newf2.OpenUsingTemplate( file1, fileCloneDst2, XrdCl::OpenFlags::New|XrdCl::OpenFlags::Update|XrdCl::OpenFlags::Samefs ) );

  XrdCl::CloneLocations locs;
  //  Add(const File &file, off_t dstOffs, off_t srcOffs, off_t srcLen)
  locs.Add( file1, 0, 4096, 8192); // offset:0  AAAABBBB
  locs.Add( file2, 16384, 4096, 8192); // offset:16384 BBBBCCCC
  locs.Add( file2, 8192, 12288, 8192); // offset:8192 CCCCAAAA
  locs.Add( file3, 24576, 4096, 8192); // offset:24576 CCCCAAAA

  auto st2 = newf2.Clone( locs );
  EXPECT_TRUE( st2.IsOK() || ( st2.IsError() && st2.code == XrdCl::errErrorResponse && st2.errNo == kXR_Unsupported ) );

  if (!st2.IsOK()) {
    EXPECT_TRUE(!st1.IsOK());
    std::cout << "Whole file clone and range Clone() "
              << "both indicate unsupported. Not testing further." << std::endl;
    return;
  }

  EXPECT_XRDST_OK( newf2.Close() );
  EXPECT_XRDST_OK( newf1.Close() );
  EXPECT_XRDST_OK( file3.Close() );
  EXPECT_XRDST_OK( file2.Close() );
  EXPECT_XRDST_OK( file1.Close() );

  XrdCl::File rf1, rf2;
  EXPECT_XRDST_OK( rf1.Open( fileCloneDst1, XrdCl::OpenFlags::Read ) );
  EXPECT_XRDST_OK( rf2.Open( fileCloneDst2, XrdCl::OpenFlags::Read ) );

  std::array<char, 24576> rbuf1;
  std::array<char, 32768> rbuf2;

  uint32_t readBytes1 = 0, readBytes2 = 0;
  EXPECT_XRDST_OK( rf1.Read( 0, 24576, rbuf1.data(), readBytes1 ) );
  EXPECT_XRDST_OK( rf2.Read( 0, 32768, rbuf2.data(), readBytes2 ) );

  EXPECT_XRDST_OK( rf2.Close() );
  EXPECT_XRDST_OK( rf1.Close() );

  EXPECT_EQ( readBytes1, 24576 );
  EXPECT_EQ( readBytes2, 32768 );

  std::string smatch1 = buffer1 + buffer2 + buffer3;
  EXPECT_TRUE(! memcmp( rbuf1.data(), smatch1.data(), 24576 ) );

  std::string smatch2 = buffer1.substr(4096) + buffer2.substr(0,4096) +
                        buffer3.substr(4096) + buffer1.substr(0,4096) +
                        buffer2.substr(4096) + buffer3.substr(0,4096) +
                        buffer3.substr(4096) + buffer1.substr(0,4096);

  EXPECT_TRUE(! memcmp( rbuf2.data(), smatch2.data(), 32768 ) );
}
