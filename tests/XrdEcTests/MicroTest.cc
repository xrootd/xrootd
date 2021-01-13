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

#include <unistd.h>
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
    void Verify();
    void CleanUp();

    void AlignedReadVerify();

  private:

    void copy_rawdata( char *buffer, size_t size )
    {
      const char *begin = buffer;
      const char *end   = begin + size;
      std::copy( begin, end, std::back_inserter( rawdata ) );
    }

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
  std::string datadir = cwd + "/data";
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

void MicroTest::AlignedReadVerify()
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
  uint32_t  rdsize = objcfg->chunksize;
  char     *rdbuff = new char[objcfg->chunksize];

  for( size_t i = 0; i < nbiters; ++i )
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
    std::string result( reinterpret_cast<char*>( ch->buffer ), ch->length );
    // get the expected result
    size_t rawoff = rdoff;
    size_t rawsz  = rdsize;
    if( rawoff + rawsz > rawdata.size() ) rawsz = rawdata.size() - rawoff;
    std::string expected( rawdata.data() + rdoff, rdsize );
    // make sure the expected and actual results are the same
    CPPUNIT_ASSERT( result == expected );
    delete status;
    delete rsp;
    rdoff += chsize;
  }

  // close the data object
  XrdCl::SyncResponseHandler handler2;
  reader.Close( &handler2 );
  handler2.WaitForResponse();
  status = handler2.GetStatus();
  CPPUNIT_ASSERT_XRDST( *status );
  delete status;
}

void MicroTest::Verify()
{
  AlignedReadVerify();
}

void MicroTest::CleanUp()
{
}

void MicroTest::AlignedWriteTest()
{
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

  Verify();
  CleanUp();
}
