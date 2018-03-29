//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
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
#include "TestEnv.hh"
#include "Utils.hh"
#include "CppUnitXrdHelpers.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClUtils.hh"
#include <pthread.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/types.h>
#include <sys/wait.h>
#include "XrdCks/XrdCksData.hh"

//------------------------------------------------------------------------------
// Thread helper struct
//------------------------------------------------------------------------------
struct ThreadData
{
  ThreadData():
    file( 0 ), startOffset( 0 ), length( 0 ), checkSum( 0 ),
    firstBlockChecksum(0) {}
  XrdCl::File *file;
  uint64_t     startOffset;
  uint64_t     length;
  uint32_t     checkSum;
  uint32_t     firstBlockChecksum;
};

const uint32_t MB = 1024*1024;

//------------------------------------------------------------------------------
// Declaration
//------------------------------------------------------------------------------
class ThreadingTest: public CppUnit::TestCase
{
  public:
    typedef void (*TransferCallback)( ThreadData *data );
    CPPUNIT_TEST_SUITE( ThreadingTest );
      CPPUNIT_TEST( ReadTest );
      CPPUNIT_TEST( MultiStreamReadTest );
      CPPUNIT_TEST( ReadForkTest );
      CPPUNIT_TEST( MultiStreamReadForkTest );
      CPPUNIT_TEST( MultiStreamReadMonitorTest );
    CPPUNIT_TEST_SUITE_END();
    void ReadTestFunc( TransferCallback transferCallback );
    void ReadTest();
    void MultiStreamReadTest();
    void ReadForkTest();
    void MultiStreamReadForkTest();
    void MultiStreamReadMonitorTest();
};

CPPUNIT_TEST_SUITE_REGISTRATION( ThreadingTest );


//------------------------------------------------------------------------------
// Reader thread
//------------------------------------------------------------------------------
void *DataReader( void *arg )
{
  using namespace XrdClTests;

  ThreadData *td = (ThreadData*)arg;

  uint64_t  offset    = td->startOffset;
  uint64_t  dataLeft  = td->length;
  uint64_t  chunkSize = 0;
  uint32_t  bytesRead = 0;
  char     *buffer    = new char[4*MB];

  while( 1 )
  {
    chunkSize = 4*MB;
    if( chunkSize > dataLeft )
      chunkSize = dataLeft;

    if( chunkSize == 0 )
      break;

    CPPUNIT_ASSERT_XRDST( td->file->Read( offset, chunkSize, buffer,
                                          bytesRead ) );

    offset   += bytesRead;
    dataLeft -= bytesRead;
    td->checkSum = Utils::UpdateCRC32( td->checkSum, buffer, bytesRead );
  }

  delete [] buffer;

  return 0;
}

//------------------------------------------------------------------------------
// Read test
//------------------------------------------------------------------------------
void ThreadingTest::ReadTestFunc( TransferCallback transferCallback )
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize
  //----------------------------------------------------------------------------
  Env *testEnv = XrdClTests::TestEnv::GetEnv();

  std::string address;
  std::string dataPath;

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  std::string fileUrl[5];
  std::string path[5];
  path[0] = dataPath + "/1db882c8-8cd6-4df1-941f-ce669bad3458.dat";
  path[1] = dataPath + "/3c9a9dd8-bc75-422c-b12c-f00604486cc1.dat";
  path[2] = dataPath + "/7235b5d1-cede-4700-a8f9-596506b4cc38.dat";
  path[3] = dataPath + "/7e480547-fe1a-4eaf-a210-0f3927751a43.dat";
  path[4] = dataPath + "/89120cec-5244-444c-9313-703e4bee72de.dat";

  for( int i = 0; i < 5; ++i )
    fileUrl[i] = address + "/" + path[i];

  //----------------------------------------------------------------------------
  // Open and stat the files
  //----------------------------------------------------------------------------
  ThreadData threadData[20];

  for( int i = 0; i < 5; ++i )
  {
    File     *f = new File();
    StatInfo *si = 0;
    CPPUNIT_ASSERT_XRDST( f->Open( fileUrl[i], OpenFlags::Read ) );
    CPPUNIT_ASSERT_XRDST( f->Stat( false, si ) );
    CPPUNIT_ASSERT( si );
    CPPUNIT_ASSERT( si->TestFlags( StatInfo::IsReadable ) );

    uint64_t step = si->GetSize()/4;

    for( int j = 0; j < 4; ++j )
    {
      threadData[j*5+i].file        = f;
      threadData[j*5+i].startOffset = j*step;
      threadData[j*5+i].length      = step;
      threadData[j*5+i].checkSum    = XrdClTests::Utils::GetInitialCRC32();


      //------------------------------------------------------------------------
      // Get the checksum of the first 4MB block at the startOffser - this
      // will be verified by the forking test
      //------------------------------------------------------------------------
      uint64_t  offset = threadData[j*5+i].startOffset;
      char     *buffer    = new char[4*MB];
      uint32_t  bytesRead = 0;

      CPPUNIT_ASSERT_XRDST( f->Read( offset, 4*MB, buffer, bytesRead ) );
      CPPUNIT_ASSERT( bytesRead == 4*MB );
      threadData[j*5+i].firstBlockChecksum =
        XrdClTests::Utils::ComputeCRC32( buffer, 4*MB );
      delete [] buffer;
    }

    threadData[15+i].length = si->GetSize() - threadData[15+i].startOffset;
    delete si;
  }

  //----------------------------------------------------------------------------
  // Spawn the threads and wait for them to finish
  //----------------------------------------------------------------------------
  pthread_t thread[20];
  for( int i = 0; i < 20; ++i )
    CPPUNIT_ASSERT_PTHREAD( pthread_create( &(thread[i]), 0,
                            ::DataReader, &(threadData[i]) ) );

  if( transferCallback )
    (*transferCallback)( threadData );

  for( int i = 0; i < 20; ++i )
    CPPUNIT_ASSERT_PTHREAD( pthread_join( thread[i], 0 ) );

  //----------------------------------------------------------------------------
  // Glue up and compare the checksums
  //----------------------------------------------------------------------------
  uint32_t checkSums[5];
  for( int i = 0; i < 5; ++i )
  {
    //--------------------------------------------------------------------------
    // Calculate the local check sum
    //--------------------------------------------------------------------------
    checkSums[i] = threadData[i].checkSum;
    for( int j = 1; j < 4; ++j )
    {
      checkSums[i] = XrdClTests::Utils::CombineCRC32( checkSums[i],
                                          threadData[j*5+i].checkSum,
                                          threadData[j*5+i].length );
    }

    char crcBuff[9];
    XrdCksData crc; crc.Set( &checkSums[i], 4 ); crc.Get( crcBuff, 9 );
    std::string transferSum = "zcrc32:"; transferSum += crcBuff;

    //--------------------------------------------------------------------------
    // Get the checksum
    //--------------------------------------------------------------------------
    std::string remoteSum, dataServer;
    threadData[i].file->GetProperty( "DataServer", dataServer );
    CPPUNIT_ASSERT_XRDST( Utils::GetRemoteCheckSum(
                            remoteSum, "zcrc32", dataServer, path[i] ) );
    CPPUNIT_ASSERT( remoteSum == transferSum );
    CPPUNIT_ASSERT_MESSAGE( path[i], remoteSum == transferSum );
  }

  //----------------------------------------------------------------------------
  // Close the files
  //----------------------------------------------------------------------------
  for( int i = 0; i < 5; ++i )
  {
    CPPUNIT_ASSERT_XRDST( threadData[i].file->Close() );
    delete threadData[i].file;
  }
}


//------------------------------------------------------------------------------
// Read test
//------------------------------------------------------------------------------
void ThreadingTest::ReadTest()
{
  ReadTestFunc(0);
}

//------------------------------------------------------------------------------
// Multistream read test
//------------------------------------------------------------------------------
void ThreadingTest::MultiStreamReadTest()
{
  XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
  env->PutInt( "SubStreamsPerChannel", 4 );
  ReadTestFunc(0);
}

//------------------------------------------------------------------------------
// Child - read some data from each of the open files and close them
//------------------------------------------------------------------------------
int runChild( ThreadData *td )
{
  XrdCl::Log *log = XrdClTests::TestEnv::GetLog();
  log->Debug( 1, "Running the child" );

  for( int i = 0; i < 20; ++i )
  {
    uint64_t  offset    = td[i].startOffset;
    char     *buffer    = new char[4*MB];
    uint32_t  bytesRead = 0;

    CPPUNIT_ASSERT_XRDST( td[i].file->Read( offset, 4*MB, buffer, bytesRead ) );
    CPPUNIT_ASSERT( bytesRead == 4*MB );
    CPPUNIT_ASSERT( td[i].firstBlockChecksum ==
                    XrdClTests::Utils::ComputeCRC32( buffer, 4*MB ) );
    delete [] buffer;
  }

  for( int i = 0; i < 5; ++i )
  {
    CPPUNIT_ASSERT_XRDST( td[i].file->Close() );
    delete td[i].file;
  }

  return 0;
}

//------------------------------------------------------------------------------
// Forking function
//------------------------------------------------------------------------------
void forkAndRead( ThreadData *data )
{
  XrdCl::Log *log = XrdClTests::TestEnv::GetLog();
  for( int chld = 0; chld < 5; ++chld )
  {
    sleep(10);
    pid_t pid;
    log->Debug( 1, "About to fork" );
    CPPUNIT_ASSERT_ERRNO( (pid=fork()) != -1 );

    if( !pid )  _exit( runChild( data ) );

    log->Debug( 1, "Forked successfully, pid of the child: %d", pid );
    int status;
    log->Debug( 1, "Waiting for the child" );
    CPPUNIT_ASSERT_ERRNO( waitpid( pid, &status, 0 ) != -1 );
    log->Debug( 1, "Wait done, status: %d", status );
    CPPUNIT_ASSERT( WIFEXITED( status ) );
    CPPUNIT_ASSERT( WEXITSTATUS( status ) == 0 );
  }
}

//------------------------------------------------------------------------------
// Read fork test
//------------------------------------------------------------------------------
void ThreadingTest::ReadForkTest()
{
  XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
  env->PutInt( "RunForkHandler", 1 );
  ReadTestFunc(&forkAndRead);
}

//------------------------------------------------------------------------------
// Multistream read fork test
//------------------------------------------------------------------------------
void ThreadingTest::MultiStreamReadForkTest()
{
  XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
  env->PutInt( "SubStreamsPerChannel", 4 );
  env->PutInt( "RunForkHandler", 1 );
  ReadTestFunc(&forkAndRead);
}

//------------------------------------------------------------------------------
// Multistream read monitor
//------------------------------------------------------------------------------
void ThreadingTest::MultiStreamReadMonitorTest()
{
  XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
  env->PutString( "ClientMonitor", "./libXrdClTestMonitor.so" );
  env->PutString( "ClientMonitorParam", "TestParam" );
  env->PutInt( "SubStreamsPerChannel", 4 );
  ReadTestFunc(0);
}
