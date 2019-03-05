//------------------------------------------------------------------------------
// Copyright (c) 2011-2017 by European Organization for Nuclear Research (CERN)
// Author: Krzysztof Jamrog <krzysztof.piotr.jamrog@cern.ch>
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
#include "IdentityPlugIn.hh"
#include "CppUnitXrdHelpers.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClOperations.hh"
#include "XrdCl/XrdClParallelOperation.hh"
#include "XrdCl/XrdClFileOperations.hh"
#include "XrdCl/XrdClFileSystemOperations.hh"
#include "XrdCl/XrdClFwd.hh"

using namespace XrdClTests;

//------------------------------------------------------------------------------
// Declaration
//------------------------------------------------------------------------------
class WorkflowTest: public CppUnit::TestCase
{
  public:
    CPPUNIT_TEST_SUITE( WorkflowTest );
      CPPUNIT_TEST( ReadingWorkflowTest );
      CPPUNIT_TEST( WritingWorkflowTest );
      CPPUNIT_TEST( MissingParameterTest );
      CPPUNIT_TEST( OperationFailureTest );
      CPPUNIT_TEST( DoubleRunningTest );
      CPPUNIT_TEST( ParallelTest );
      CPPUNIT_TEST( FileSystemWorkflowTest );
      CPPUNIT_TEST( MixedWorkflowTest );
      CPPUNIT_TEST( WorkflowWithFutureTest );
    CPPUNIT_TEST_SUITE_END();
    void ReadingWorkflowTest();
    void WritingWorkflowTest();
    void MissingParameterTest();
    void OperationFailureTest();
    void DoubleRunningTest();
    void ParallelTest();
    void FileSystemWorkflowTest();
    void MixedWorkflowTest();
    void WorkflowWithFutureTest();
};

CPPUNIT_TEST_SUITE_REGISTRATION( WorkflowTest );


namespace {
    using namespace XrdCl;

    XrdCl::URL GetAddress(){
        Env *testEnv = TestEnv::GetEnv();
        std::string address;
        CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
        return XrdCl::URL(address);
    }

    std::string GetPath(const std::string &fileName){
        Env *testEnv = TestEnv::GetEnv();

        std::string dataPath;
        CPPUNIT_ASSERT( testEnv->GetString( "DataPath", dataPath ) );
        
        return dataPath + "/" + fileName;
    }


    std::string GetFileUrl(const std::string &fileName){
        Env *testEnv = TestEnv::GetEnv();

        std::string address;
        std::string dataPath;

        CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
        CPPUNIT_ASSERT( testEnv->GetString( "DataPath", dataPath ) );

        URL url( address );
        CPPUNIT_ASSERT( url.IsValid() );

        std::string path = dataPath + "/" + fileName;
        std::string fileUrl = address + "/" + path;

        return fileUrl;
    }

    class TestingHandler: public ResponseHandler {
        public:
            TestingHandler(){
                executed = false;
            }

            void HandleResponseWithHosts(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response, XrdCl::HostList *hostList) {
                delete hostList;
                HandleResponse(status, response);
            }

            void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) {
                CPPUNIT_ASSERT_XRDST(*status);
                delete status;
                delete response;
                executed = true;
            }

            bool Executed(){
                return executed;
            }

        protected:
            bool executed;
    };

    class ExpectErrorHandler: public ResponseHandler
    {
      public:
          ExpectErrorHandler(){
              executed = false;
          }

          void HandleResponseWithHosts(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response, XrdCl::HostList *hostList) {
              delete hostList;
              HandleResponse(status, response);
          }

          void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) {
              CPPUNIT_ASSERT( !status->IsOK() );
              delete status;
              delete response;
              executed = true;
          }

          bool Executed(){
              return executed;
          }

      protected:
          bool executed;
    };



}


void WorkflowTest::ReadingWorkflowTest(){
    using namespace XrdCl;

    //----------------------------------------------------------------------------
    // Initialize
    //----------------------------------------------------------------------------
    std::string fileUrl = GetFileUrl("cb4aacf1-6f28-42f2-b68a-90a73460f424.dat");
    File f;

    //----------------------------------------------------------------------------
    // Create handlers
    //----------------------------------------------------------------------------
    TestingHandler openHandler;
    TestingHandler readHandler;
    TestingHandler closeHandler;

    //----------------------------------------------------------------------------
    // Forward parameters between operations
    //----------------------------------------------------------------------------
    Fwd<uint32_t> size;
    Fwd<void*>    buffer;

    //----------------------------------------------------------------------------
    // Create and execute workflow
    //----------------------------------------------------------------------------

    const OpenFlags::Flags flags = OpenFlags::Read;
    uint64_t offset = 0;

    auto &&pipe = Open( f, fileUrl, flags ) >> openHandler // by reference
                | Stat( f, true) >> [size, buffer]( XRootDStatus &status, StatInfo &stat )
                    {
                      CPPUNIT_ASSERT_XRDST( status );
                      CPPUNIT_ASSERT( stat.GetSize() == 1048576000 );
                      size = stat.GetSize();
                      buffer = new char[stat.GetSize()];
                    }
                | Read( f, offset, size, buffer ) >> &readHandler // by pointer
                | Close( f ) >> closeHandler; // by reference

    XRootDStatus status = WaitFor( pipe );
    CPPUNIT_ASSERT_XRDST( status );

    CPPUNIT_ASSERT( openHandler.Executed() );
    CPPUNIT_ASSERT( readHandler.Executed() );
    CPPUNIT_ASSERT( closeHandler.Executed() );

    delete[] reinterpret_cast<char*>( *buffer );
}


void WorkflowTest::WritingWorkflowTest(){
    using namespace XrdCl;

    //----------------------------------------------------------------------------
    // Initialize
    //----------------------------------------------------------------------------
    std::string fileUrl = GetFileUrl("testFile.dat");
    auto flags = OpenFlags::Write | OpenFlags::Delete | OpenFlags::Update;
    std::string texts[3] = {"First line\n", "Second line\n", "Third line\n"};
    File f;
    
    auto url = GetAddress();
    FileSystem fs(url);
    auto relativePath = GetPath("testFile.dat");

    auto createdFileSize = texts[0].size() + texts[1].size() + texts[2].size();

    //----------------------------------------------------------------------------
    // Create handlers
    //----------------------------------------------------------------------------
    std::packaged_task<std::string(XRootDStatus&, ChunkInfo&)> parser {
      []( XRootDStatus& status, ChunkInfo &chunk )
        {
          CPPUNIT_ASSERT_XRDST( status );
          char* buffer = reinterpret_cast<char*>( chunk.buffer );
          std::string ret( buffer, chunk.length );
          delete[] buffer;
          return ret;
        }
    };
    std::future<std::string> rdresp = parser.get_future();

    //----------------------------------------------------------------------------
    // Forward parameters between operations
    //----------------------------------------------------------------------------
    Fwd<iovec*>   iov;
    Fwd<int>      iovcnt;
    Fwd<uint32_t> size;
    Fwd<void*>    buffer;

    //----------------------------------------------------------------------------
    // Create and execute workflow
    //----------------------------------------------------------------------------
    Pipeline pipe = Open( f, fileUrl, flags ) >> [iov, iovcnt, texts]( XRootDStatus &status )
                      {
                        CPPUNIT_ASSERT_XRDST( status );
                        iovec *vec = new iovec[3];
                        vec[0].iov_base = strdup( texts[0].c_str() );
                        vec[0].iov_len  = texts[0].size();
                        vec[1].iov_base = strdup( texts[1].c_str() );
                        vec[1].iov_len  = texts[1].size();
                        vec[2].iov_base = strdup( texts[2].c_str() );
                        vec[2].iov_len  = texts[2].size();
                        iov = vec;
                        iovcnt = 3;
                      }
                  | WriteV( f, 0, iov, iovcnt )
                  | Sync( f )
                  | Stat( f, true ) >> [size, buffer, createdFileSize]( XRootDStatus &status, StatInfo &info )
                      {
                        CPPUNIT_ASSERT_XRDST( status );
                        CPPUNIT_ASSERT( createdFileSize == info.GetSize() );
                        size   = info.GetSize();
                        buffer = new char[info.GetSize()];
                      }
                  | Read( f, 0, size, buffer ) >> parser
                  | Close( f )
                  | Rm( fs, relativePath );

    XRootDStatus status = WaitFor( std::move( pipe ) );
    CPPUNIT_ASSERT_XRDST( status );
    CPPUNIT_ASSERT( rdresp.get() == texts[0] + texts[1] + texts[2] );

    iovec *vec = *iov;
    free( vec[0].iov_base );
    free( vec[1].iov_base );
    free( vec[2].iov_base );
    delete[] vec;
}


void WorkflowTest::MissingParameterTest(){
    using namespace XrdCl;

    //----------------------------------------------------------------------------
    // Initialize
    //----------------------------------------------------------------------------
    std::string fileUrl = GetFileUrl("cb4aacf1-6f28-42f2-b68a-90a73460f424.dat");
    File f;

    //----------------------------------------------------------------------------
    // Create handlers
    //----------------------------------------------------------------------------
    ExpectErrorHandler readHandler;

    //----------------------------------------------------------------------------
    // Bad forwards
    //----------------------------------------------------------------------------
    Fwd<uint32_t> size;
    Fwd<void*>    buffer;

    //----------------------------------------------------------------------------
    // Create and execute workflow
    //----------------------------------------------------------------------------

    bool closed = false;
    const OpenFlags::Flags flags = OpenFlags::Read;
    uint64_t offset = 0;

    Pipeline pipe = Open( f, fileUrl, flags )
                  | Stat( f, true )
                  | Read( f, offset, size, buffer ) >> readHandler // by reference
                  | Close( f ) >> [&]( XRootDStatus& st ){ closed = true; };

    XRootDStatus status = WaitFor( std::move( pipe ) );
    CPPUNIT_ASSERT( status.IsError() );
    //----------------------------------------------------------------------------
    // If there is an error, last handlers should not be executed
    //----------------------------------------------------------------------------
    CPPUNIT_ASSERT( readHandler.Executed() );
    CPPUNIT_ASSERT( !closed );
}



void WorkflowTest::OperationFailureTest(){
    using namespace XrdCl;

    //----------------------------------------------------------------------------
    // Initialize
    //----------------------------------------------------------------------------
    std::string fileUrl = GetFileUrl("noexisting.dat");
    File f;

    //----------------------------------------------------------------------------
    // Create handlers
    //----------------------------------------------------------------------------
    ExpectErrorHandler openHandler;
    std::future<StatInfo> statresp;
    std::future<ChunkInfo> readresp;
    std::future<void> closeresp;

    //----------------------------------------------------------------------------
    // Create and execute workflow
    //----------------------------------------------------------------------------

    const OpenFlags::Flags flags = OpenFlags::Read;
    auto &&pipe = Open( f, fileUrl, flags ) >> &openHandler // by pointer
                | Stat( f, true ) >> statresp
                | Read( f, 0, 0, nullptr ) >> readresp
                | Close( f ) >> closeresp;

    XRootDStatus status = WaitFor( pipe ); // by obscure operation type
    CPPUNIT_ASSERT( status.IsError() );

    //----------------------------------------------------------------------------
    // If there is an error, handlers should not be executed
    //----------------------------------------------------------------------------
    CPPUNIT_ASSERT(openHandler.Executed());

    try
    {
      statresp.get();
    }
    catch( PipelineException &ex )
    {
      CPPUNIT_ASSERT_XRDST_NOTOK( ex.GetError(), errPipelineFailed );
    }

    try
    {
      readresp.get();
    }
    catch( PipelineException &ex )
    {
      CPPUNIT_ASSERT_XRDST_NOTOK( ex.GetError(), errPipelineFailed );
    }

    try
    {
      closeresp.get();
    }
    catch( PipelineException &ex )
    {
      CPPUNIT_ASSERT_XRDST_NOTOK( ex.GetError(), errPipelineFailed );
    }
}


void WorkflowTest::DoubleRunningTest(){
    using namespace XrdCl;

    //----------------------------------------------------------------------------
    // Initialize
    //----------------------------------------------------------------------------
    std::string fileUrl = GetFileUrl("cb4aacf1-6f28-42f2-b68a-90a73460f424.dat");
    File f;

    //----------------------------------------------------------------------------
    // Create and execute workflow
    //----------------------------------------------------------------------------

    const OpenFlags::Flags flags = OpenFlags::Read;
    bool opened = false, closed = false;

    auto &&pipe = Open( f, fileUrl, flags ) >> [&]( XRootDStatus &status ){ opened = status.IsOK(); }
                | Close( f ) >> [&]( XRootDStatus &status ){ closed = status.IsOK(); };

    std::future<XRootDStatus> ftr = Async( pipe );

    //----------------------------------------------------------------------------
    // Running workflow again should fail
    //----------------------------------------------------------------------------
    try
    {
        Async( pipe );
        CPPUNIT_ASSERT( false );
    }
    catch( std::logic_error &err )
    {

    }


    XRootDStatus status = ftr.get();

    //----------------------------------------------------------------------------
    // Running workflow again should fail
    //----------------------------------------------------------------------------
    try
    {
        Async( pipe );
        CPPUNIT_ASSERT( false );
    }
    catch( std::logic_error &err )
    {

    }

    CPPUNIT_ASSERT( status.IsOK() );

    CPPUNIT_ASSERT( opened );
    CPPUNIT_ASSERT( closed );
}


void WorkflowTest::ParallelTest(){
    using namespace XrdCl;

    //----------------------------------------------------------------------------
    // Initialize
    //----------------------------------------------------------------------------
    File lockFile;
    File firstFile;
    File secondFile;

    std::string lockFileName = "lockfile.lock";
    std::string dataFileName = "testFile.dat";

    std::string lockUrl = GetFileUrl(lockFileName);
    std::string firstFileUrl = GetFileUrl("cb4aacf1-6f28-42f2-b68a-90a73460f424.dat");
    std::string secondFileUrl = GetFileUrl(dataFileName);

    const auto readFlags = OpenFlags::Read;
    const auto createFlags = OpenFlags::Delete;

    // ----------------------------------------------------------------------------
    // Create lock file and new data file
    // ----------------------------------------------------------------------------
    auto f = new File();
    auto dataF = new File();

    std::vector<Pipeline> pipes; pipes.reserve( 2 );
    pipes.emplace_back( Open( f, lockUrl, createFlags ) | Close( f ) );
    pipes.emplace_back( Open( dataF, secondFileUrl, createFlags ) | Close( dataF ) );
    CPPUNIT_ASSERT_XRDST( WaitFor( Parallel( pipes ) >> []( XRootDStatus &status ){ CPPUNIT_ASSERT_XRDST( status ); } ) );
    CPPUNIT_ASSERT( pipes.empty() );

    delete f;
    delete dataF;

    //----------------------------------------------------------------------------
    // Create and execute workflow
    //----------------------------------------------------------------------------
    uint64_t offset = 0;
    uint32_t size = 50  ;
    char* firstBuffer  = new char[size]();
    char* secondBuffer = new char[size]();

    Fwd<std::string> url1, url2;

    bool lockHandlerExecuted = false;
    auto lockOpenHandler = [&,url1, url2]( XRootDStatus &status )
        {
          url1 = GetFileUrl( "cb4aacf1-6f28-42f2-b68a-90a73460f424.dat" );
          url2 = GetFileUrl( dataFileName );
          lockHandlerExecuted = true;
        };

    std::future<void> parallelresp, closeresp;

    Pipeline firstPipe = Open( firstFile, url1, readFlags )
                       | Read( firstFile, offset, size, firstBuffer )
                       | Close( firstFile );

    Pipeline secondPipe = Open( secondFile, url2, readFlags )
                        | Read( secondFile, offset, size, secondBuffer )
                        | Close( secondFile );

    Pipeline pipe = Open( lockFile, lockUrl, readFlags ) >> lockOpenHandler
                  | Parallel( firstPipe, secondPipe ) >> parallelresp
                  | Close( lockFile ) >> closeresp;

    XRootDStatus status = WaitFor( std::move( pipe ) );
    CPPUNIT_ASSERT(status.IsOK());

    CPPUNIT_ASSERT(lockHandlerExecuted);

    try
    {
      parallelresp.get();
      closeresp.get();
    }
    catch( std::exception &ex )
    {
      CPPUNIT_ASSERT( false );
    }

    delete[] firstBuffer;
    delete[] secondBuffer;

    //----------------------------------------------------------------------------
    // Remove lock file and data file
    //----------------------------------------------------------------------------
    f = new File();
    dataF = new File();

    auto url = GetAddress();
    FileSystem fs( url );

    auto lockRelativePath = GetPath(lockFileName);
    auto dataRelativePath = GetPath(dataFileName);

    bool exec1 = false, exec2 = false;
    Pipeline deletingPipe( Parallel( Rm( fs, lockRelativePath ) >> [&]( XRootDStatus &status ){ CPPUNIT_ASSERT_XRDST( status ); exec1 = true; },
                                     Rm( fs, dataRelativePath ) >> [&]( XRootDStatus &status ){ CPPUNIT_ASSERT_XRDST( status ); exec2 = true; } ) );
    CPPUNIT_ASSERT_XRDST( WaitFor( std::move( deletingPipe ) ) );

    CPPUNIT_ASSERT( exec1 );
    CPPUNIT_ASSERT( exec2 );

    delete f;
    delete dataF;
}


void WorkflowTest::FileSystemWorkflowTest(){
    using namespace XrdCl;

    TestingHandler mkDirHandler;
    TestingHandler locateHandler;
    TestingHandler moveHandler;
    TestingHandler secondLocateHandler;
    TestingHandler removeHandler;

    auto url = GetAddress();
    FileSystem fs( url );

    std::string newDirUrl = GetPath("sourceDirectory");
    std::string destDirUrl = GetPath("destDirectory");

    auto noneFlags = OpenFlags::None;

    Pipeline fsPipe = MkDir( fs, newDirUrl, MkDirFlags::None, Access::None ) >> mkDirHandler
                    | Locate( fs, newDirUrl, noneFlags ) >> locateHandler
                    | Mv( fs, newDirUrl, destDirUrl ) >> moveHandler
                    | Locate( fs, destDirUrl, OpenFlags::Refresh ) >> secondLocateHandler
                    | RmDir( fs, destDirUrl ) >> removeHandler;

    Pipeline pipe( std::move( fsPipe) );

    XRootDStatus status = WaitFor( std::move( pipe ) );
    CPPUNIT_ASSERT(status.IsOK());

    CPPUNIT_ASSERT(mkDirHandler.Executed());
    CPPUNIT_ASSERT(locateHandler.Executed());
    CPPUNIT_ASSERT(moveHandler.Executed());
    CPPUNIT_ASSERT(secondLocateHandler.Executed());
    CPPUNIT_ASSERT(removeHandler.Executed());
}


void WorkflowTest::MixedWorkflowTest(){
    using namespace XrdCl;

    const size_t nbFiles = 2;

    FileSystem fs( GetAddress() );
    File file[nbFiles];

    auto flags = OpenFlags::Write | OpenFlags::Delete | OpenFlags::Update;

    std::string dirName = "tempDir";
    std::string dirPath = GetPath( dirName );

    std::string firstFileName = dirName + "/firstFile";
    std::string secondFileName = dirName + "/secondFile";
    std::string url[nbFiles] = { GetFileUrl(firstFileName), GetFileUrl(secondFileName) };

    std::string path[nbFiles] = { GetPath(firstFileName), GetPath(secondFileName) };

    std::string content[nbFiles] = { "First file content",  "Second file content" };
    char* text[nbFiles] = { const_cast<char*>(content[0].c_str()), const_cast<char*>(content[1].c_str()) };
    size_t length[nbFiles] = { content[0].size(), content[1].size() };


    Fwd<uint32_t> size[nbFiles];
    Fwd<void*> buffer[nbFiles];

    std::future<ChunkInfo> ftr[nbFiles];

    bool cleaningHandlerExecuted = false;
    auto cleaningHandler = [&](XRootDStatus &status, LocationInfo& info)
      {
        LocationInfo::Iterator it;
        for( it = info.Begin(); it != info.End(); ++it )
        {
            auto url = URL(it->GetAddress());
            FileSystem fs(url);
            auto st = fs.RmDir(dirPath);
            CPPUNIT_ASSERT(st.IsOK());
        }
        cleaningHandlerExecuted = true;
      };

    std::vector<Pipeline> fileWorkflows;
    for( size_t i = 0; i < nbFiles; ++i )
    {
      auto &&operation = Open( file[i], url[i], flags )
                       | Write( file[i], 0, length[i], text[i] )
                       | Sync( file[i] )
                       | Stat( file[i], true ) >> [size, buffer, i]( XRootDStatus &status, StatInfo &info )
                           {
                             CPPUNIT_ASSERT_XRDST( status );
                             size[i] = info.GetSize();
                             buffer[i] = new char[*size[i]];
                           }
                       | Read( file[i], 0, size[i], buffer[i] ) >> ftr[i]
                       | Close( file[i] );
      fileWorkflows.emplace_back( operation );
    }

    Pipeline pipe = MkDir( fs, dirPath, MkDirFlags::None, Access::None ) >> []( XRootDStatus &status ){ CPPUNIT_ASSERT_XRDST( status ); }
                  | Parallel( fileWorkflows )
                  | Rm( fs, path[0] )
                  | Rm( fs, path[1] )
                  | DeepLocate( fs, dirPath, OpenFlags::Refresh ) >> cleaningHandler;

    XRootDStatus status = WaitFor( std::move( pipe ) );
    CPPUNIT_ASSERT_XRDST( status );

    for( size_t i = 0; i < nbFiles; ++i )
    {
      ChunkInfo chunk = ftr[i].get();
      char *buffer = reinterpret_cast<char*>( chunk.buffer );
      std::string result( buffer, chunk.length );
      delete[] buffer;
      CPPUNIT_ASSERT( result == content[i] );
    }

    CPPUNIT_ASSERT(cleaningHandlerExecuted);
}


void WorkflowTest::WorkflowWithFutureTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  std::string filePath = dataPath + "/cb4aacf1-6f28-42f2-b68a-90a73460f424.dat";
  std::string fileUrl = address + "/";
  fileUrl += filePath;

  //----------------------------------------------------------------------------
  // Fetch some data and checksum
  //----------------------------------------------------------------------------
  const uint32_t MB = 1024*1024;
  char *expected = new char[40*MB];
  char *buffer   = new char[40*MB];
  uint32_t bytesRead = 0;
  File f;

  //----------------------------------------------------------------------------
  // Open and Read and Close in standard way
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT_XRDST( f.Open( fileUrl, OpenFlags::Read ) );
  CPPUNIT_ASSERT_XRDST( f.Read( 10*MB, 40*MB, expected, bytesRead ) );
  CPPUNIT_ASSERT( bytesRead == 40*MB );
  CPPUNIT_ASSERT_XRDST( f.Close() );

  //----------------------------------------------------------------------------
  // Now do the test
  //----------------------------------------------------------------------------
  File file;
  std::future<ChunkInfo> ftr;
  Pipeline pipeline = Open( file, fileUrl, OpenFlags::Read ) | Read( file, 10*MB, 40*MB, buffer ) >> ftr | Close( file );
  std::future<XRootDStatus> status = Async( std::move( pipeline ) );

  try
  {
    ChunkInfo result = ftr.get();
    CPPUNIT_ASSERT( result.length = bytesRead );
    CPPUNIT_ASSERT( strncmp( expected, (char*)result.buffer, bytesRead ) == 0 );
  }
  catch( PipelineException &ex )
  {
    CPPUNIT_ASSERT( false );
  }

  CPPUNIT_ASSERT_XRDST( status.get() )

  delete[] expected;
  delete[] buffer;
}
