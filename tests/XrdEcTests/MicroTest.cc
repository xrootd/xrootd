//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <michal.simon@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
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
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#include <cppunit/extensions/HelperMacros.h>
#include "TestEnv.hh"
#include "CppUnitXrdHelpers.hh"

#include "XrdEc/XrdEcStrmWriter.hh"
#include "XrdEc/XrdEcReader.hh"
#include "XrdEc/XrdEcObjCfg.hh"

#include "XrdCl/XrdClMessageUtils.hh"

#include <string>
#include <memory>
#include <limits>

#include <unistd.h>
#include <stdio.h>
#include <ftw.h>
#include <sys/stat.h>

//------------------------------------------------------------------------------
// Declaration
//------------------------------------------------------------------------------
class MicroTest: public CppUnit::TestCase
{
  public:
    CPPUNIT_TEST_SUITE( MicroTest );
      CPPUNIT_TEST( AlignedWriteTest );
    CPPUNIT_TEST_SUITE_END();
    void Init();
    void AlignedWriteTest();
    void Verify()
    {
      AlignedReadVerify();
      PastEndReadVerify();
      SmallChunkReadVerify();
      BigChunkReadVerify();
    }
    void CleanUp();

    void ReadVerify( uint32_t rdsize, uint64_t maxrd = std::numeric_limits<uint64_t>::max() );

    inline void AlignedReadVerify()
    {
      ReadVerify( chsize, rawdata.size() );
    }

    inline void PastEndReadVerify()
    {
      ReadVerify( chsize );
    }

    inline void SmallChunkReadVerify()
    {
      ReadVerify( 5 );
    }

    inline void BigChunkReadVerify()
    {
      ReadVerify( 23 );
    }

  private:

    void copy_rawdata( char *buffer, size_t size )
    {
      const char *begin = buffer;
      const char *end   = begin + size;
      std::copy( begin, end, std::back_inserter( rawdata ) );
    }

    std::string datadir;
    std::unique_ptr<XrdEc::ObjCfg> objcfg;

    static const size_t nbdata   = 4;
    static const size_t nbparity = 2;
    static const size_t chsize   = 16;
    static const size_t nbiters  = 16;

    std::vector<char> rawdata;
};

CPPUNIT_TEST_SUITE_REGISTRATION( MicroTest );


void MicroTest::Init()
{
  objcfg.reset( new XrdEc::ObjCfg( "test.txt", "0", nbdata, nbparity, chsize ) );

  char cwdbuff[1024];
  char *cwdptr = getcwd( cwdbuff, sizeof( cwdbuff ) );
  CPPUNIT_ASSERT( cwdptr );
  std::string cwd = cwdptr;
  // create the data directory
  datadir = cwd + "/data";
  CPPUNIT_ASSERT( mkdir( datadir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH ) == 0 );
  // create a directory for each stripe
  size_t nbstrps = objcfg->nbdata + 2 * objcfg->nbparity;
  for( size_t i = 0; i < nbstrps; ++i )
  {
    std::stringstream ss;
    ss << std::setfill('0') << std::setw( 2 ) << i;
    std::string strp = datadir + '/' + ss.str() + '/';
    objcfg->plgr.emplace_back( strp );
    CPPUNIT_ASSERT( mkdir( strp.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH ) == 0 );
  }
}

void MicroTest::ReadVerify( uint32_t rdsize, uint64_t maxrd )
{
  XrdEc::Reader reader( *objcfg );
  // open the data object
  XrdCl::SyncResponseHandler handler1;
  reader.Open( &handler1 );
  handler1.WaitForResponse(); 
  XrdCl::XRootDStatus *status = handler1.GetStatus();
  CPPUNIT_ASSERT_XRDST( *status );
  delete status;
  
  uint64_t  rdoff  = 0;
  char     *rdbuff = new char[rdsize]; 
  uint32_t  bytesrd = 0;
  uint64_t  total_bytesrd = 0;
  do
  {
    XrdCl::SyncResponseHandler h;
    reader.Read( rdoff, rdsize, rdbuff, &h );
    h.WaitForResponse();
    status = h.GetStatus();
    CPPUNIT_ASSERT_XRDST( *status );
    // get the actual result
    auto rsp = h.GetResponse();
    XrdCl::ChunkInfo *ch = nullptr;
    rsp->Get( ch );
    bytesrd = ch->length;
    std::string result( reinterpret_cast<char*>( ch->buffer ), bytesrd );
    // get the expected result
    size_t rawoff = rdoff;
    size_t rawsz  = rdsize;
    if( rawoff + rawsz > rawdata.size() ) rawsz = rawdata.size() - rawoff;
    std::string expected( rawdata.data() + rawoff, rawsz );
    // make sure the expected and actual results are the same
    CPPUNIT_ASSERT( result == expected );
    delete status;
    delete rsp;
    rdoff += bytesrd;
    total_bytesrd += bytesrd;
  }
  while( bytesrd == rdsize && total_bytesrd < maxrd );
  delete[] rdbuff;
 
  // close the data object
  XrdCl::SyncResponseHandler handler2;
  reader.Close( &handler2 );
  handler2.WaitForResponse();
  status = handler2.GetStatus();
  CPPUNIT_ASSERT_XRDST( *status );
  delete status;
}

int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
  int rc = remove( fpath );
  CPPUNIT_ASSERT( rc == 0 );
  return rc;
}

void MicroTest::CleanUp()
{
  // delete the data directory
  nftw( datadir.c_str(), unlink_cb, 64, FTW_DEPTH | FTW_PHYS );
}

void MicroTest::AlignedWriteTest()
{
  // create the data and stripe directories
  Init();

  char buffer[objcfg->chunksize];
  XrdEc::StrmWriter writer( *objcfg );
  // open the data object
  XrdCl::SyncResponseHandler handler1;
  writer.Open( &handler1 );
  handler1.WaitForResponse();
  XrdCl::XRootDStatus *status = handler1.GetStatus();
  CPPUNIT_ASSERT_XRDST( *status ); 
  delete status;
  // write to the data object
  for( size_t i = 0; i < nbiters; ++i )
  {
    memset( buffer, 'A' + i, objcfg->chunksize );
    writer.Write( objcfg->chunksize, buffer, nullptr );
    copy_rawdata( buffer, sizeof( buffer ) );
  }
  XrdCl::SyncResponseHandler handler2;
  writer.Close( &handler2 );
  handler2.WaitForResponse();
  status = handler2.GetStatus();
  CPPUNIT_ASSERT_XRDST( *status );
  delete status;

  // verify that we wrote the data correctly
  Verify();
  // clean up the data directory
  CleanUp();
}
