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
#include "XrdCl/XrdClFileOperations.hh"
#include "XrdCl/XrdClFileSystemOperations.hh"

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
    CPPUNIT_TEST_SUITE_END();
    void ReadingWorkflowTest();
    void WritingWorkflowTest();
    void MissingParameterTest();
    void OperationFailureTest();
    void DoubleRunningTest();
    void ParallelTest();
    void FileSystemWorkflowTest();
    void MixedWorkflowTest();
};

CPPUNIT_TEST_SUITE_REGISTRATION( WorkflowTest );


namespace {
    using namespace XrdCl;

    void PrintStatus(Workflow &workflow){
        std::cout<<workflow.GetStatus().ToStr()<<std::endl;
    }

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


    class TestingForwardingHandler: public ForwardingHandler {
        public:
            TestingForwardingHandler(){
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


    class StatHandler: public TestingForwardingHandler {
        public:
            StatHandler(bool sizeCheck, uint32_t expectedSize = 0){
                _expectedSize = expectedSize;
                _sizeCheck = sizeCheck;
                buffer = NULL;
            }

            void HandleResponseWithHosts(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response, XrdCl::HostList *hostList) {
                StatInfo *stat = 0;
                response->Get(stat);

                CPPUNIT_ASSERT(stat);
                CPPUNIT_ASSERT(stat->TestFlags(StatInfo::IsReadable));

                uint32_t size = stat->GetSize();
                if(_sizeCheck){
                    CPPUNIT_ASSERT_EQUAL(size, _expectedSize);
                }

                buffer = new char[size]();
                ForwardParam<Read::BufferArg>(buffer);
                ForwardParam<Read::SizeArg>(size);

                TestingForwardingHandler::HandleResponseWithHosts(status, response, hostList);
            }

            ~StatHandler(){
                if(buffer){
                    delete[] buffer;
                }
            }

        private:
            char* buffer;
            uint32_t _expectedSize;
            bool _sizeCheck;
    };


    char* createBuf(const char* content, uint32_t length){
        char* buf = new char[length + 1]();
        strncpy(buf, content, length);
        return buf;
    }

    class WriteVOpenHandler: public TestingForwardingHandler {
        public:
            WriteVOpenHandler(std::string *textsArr, int textsNumber){
                vec = nullptr;
                firstBuf = nullptr;
                secondBuf = nullptr;
                contents = textsArr;
                amount = textsNumber;
            }

            void HandleResponseWithHosts(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response, XrdCl::HostList *hostList) {
                firstBuf = createBuf(contents[0].c_str(), contents[0].length());
                secondBuf = createBuf(contents[1].c_str(), contents[1].length());
                thirdBuf = createBuf(contents[2].c_str(), contents[2].length());

                iovec v1 = {firstBuf, contents[0].length()};
                iovec v2 = {secondBuf, contents[1].length()};
                iovec v3 = {thirdBuf, contents[2].length()};

                vec = new iovec[3];
                vec[0] = v1;
                vec[1] = v2;
                vec[2] = v3;

                ForwardParam<WriteV::IovArg>(vec);
                ForwardParam<WriteV::IovcntArg>(3);

                TestingForwardingHandler::HandleResponseWithHosts(status, response, hostList);
            }

            ~WriteVOpenHandler(){
                if(vec){
                    delete[] firstBuf;
                    delete[] secondBuf;
                    delete[] thirdBuf;
                    delete[] vec;
                }

            }

        protected:
            std::string *contents;
            int amount;
            struct iovec* vec;
            char* firstBuf;
            char* secondBuf;
            char* thirdBuf;
    };

    class ReadHandler: public TestingForwardingHandler {
        public:

            ReadHandler(std::string expectedContent){
                _expectedContent = expectedContent;
            }

            void HandleResponseWithHosts(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response, XrdCl::HostList *hostList){
                ChunkInfo * info = 0;
                response->Get( info );
                std::string content = std::string((char*)info->buffer, info->length);
                CPPUNIT_ASSERT_EQUAL(content, _expectedContent);

                TestingForwardingHandler::HandleResponseWithHosts(status, response, hostList);

            }

        protected:
            std::string _expectedContent;
    };

}


void WorkflowTest::ReadingWorkflowTest(){
    using namespace XrdCl;

    //----------------------------------------------------------------------------
    // Initialize
    //----------------------------------------------------------------------------
    std::string fileUrl = GetFileUrl("cb4aacf1-6f28-42f2-b68a-90a73460f424.dat");
    auto f = new File();

    //----------------------------------------------------------------------------
    // Create handlers
    //----------------------------------------------------------------------------
    auto openHandler = new TestingForwardingHandler();
    auto statHandler = new StatHandler(true, 1048576000);
    auto readHandler = new TestingForwardingHandler();
    auto closeHandler = new TestingForwardingHandler();

    //----------------------------------------------------------------------------
    // Create and execute workflow
    //----------------------------------------------------------------------------

    const OpenFlags::Flags flags = OpenFlags::Read;
    uint64_t offset = 0;

    auto &pipe = Open(f)(fileUrl, flags) >> openHandler
        | Stat(f)(true) >> statHandler
        | Read(f)(offset, notdef, notdef) >> readHandler
        | Close(f)() >> closeHandler;


    Workflow workflow(pipe);
    workflow.Run().Wait();

    CPPUNIT_ASSERT(workflow.GetStatus().IsOK());

    CPPUNIT_ASSERT(openHandler->Executed());
    CPPUNIT_ASSERT(statHandler->Executed());
    CPPUNIT_ASSERT(readHandler->Executed());
    CPPUNIT_ASSERT(closeHandler->Executed());

    //----------------------------------------------------------------------------
    // Release memory
    //----------------------------------------------------------------------------
    delete f;
}


void WorkflowTest::WritingWorkflowTest(){
    using namespace XrdCl;

    //----------------------------------------------------------------------------
    // Initialize
    //----------------------------------------------------------------------------
    std::string fileUrl = GetFileUrl("testFile.dat");
    auto flags = OpenFlags::Write | OpenFlags::Delete | OpenFlags::Update;
    std::string texts[3] = {"First line\n", "Second line\n", "Third line\n"};
    auto *f = new File();
    
    auto url = GetAddress();
    auto fs = new FileSystem(url);
    auto relativePath = GetPath("testFile.dat");

    auto createdFileSize = texts[0].size() + texts[1].size() + texts[2].size();

    //----------------------------------------------------------------------------
    // Create handlers
    //----------------------------------------------------------------------------
    auto openHandler = new WriteVOpenHandler(texts, 3);
    auto writeHandler = new TestingForwardingHandler();
    auto syncHandler = new TestingForwardingHandler();
    auto statHandler = new StatHandler(true, createdFileSize);
    auto readHandler = new ReadHandler(texts[0] + texts[1] + texts[2]);
    auto closeHandler = new TestingForwardingHandler();
    auto removeHandler = new TestingForwardingHandler();



    //----------------------------------------------------------------------------
    // Create and execute workflow
    //----------------------------------------------------------------------------

    uint64_t offset = 0;

    auto &pipe = Open(f)(fileUrl, flags) >> openHandler
        | WriteV(f)(offset, notdef, notdef) >> writeHandler
        | Sync(f)() >> syncHandler
        | Stat(f)(true) >> statHandler
        | Read(f)(offset, notdef, notdef) >> readHandler
        | Close(f)() >> closeHandler
        | Rm(fs)(relativePath) >> removeHandler;

    Workflow workflow(pipe);
    workflow.Run().Wait();

    CPPUNIT_ASSERT(workflow.GetStatus().IsOK());

    CPPUNIT_ASSERT(openHandler->Executed());
    CPPUNIT_ASSERT(writeHandler->Executed());
    CPPUNIT_ASSERT(syncHandler->Executed());
    CPPUNIT_ASSERT(statHandler->Executed());
    CPPUNIT_ASSERT(readHandler->Executed());
    CPPUNIT_ASSERT(closeHandler->Executed());
    CPPUNIT_ASSERT(removeHandler->Executed());

    //----------------------------------------------------------------------------
    // Release memory
    //----------------------------------------------------------------------------
    delete f;
    delete fs;

}


void WorkflowTest::MissingParameterTest(){
    using namespace XrdCl;

    //----------------------------------------------------------------------------
    // Initialize
    //----------------------------------------------------------------------------
    std::string fileUrl = GetFileUrl("cb4aacf1-6f28-42f2-b68a-90a73460f424.dat");
    auto f = new File();

    //----------------------------------------------------------------------------
    // Create handlers
    //----------------------------------------------------------------------------
    auto openHandler = new TestingForwardingHandler();
    auto statHandler = new TestingForwardingHandler();
    auto readHandler = new TestingForwardingHandler();
    auto closeHandler = new TestingForwardingHandler();


    //----------------------------------------------------------------------------
    // Create and execute workflow
    //----------------------------------------------------------------------------

    const OpenFlags::Flags flags = OpenFlags::Read;
    uint64_t offset = 0;

    auto &pipe = Open(f)(fileUrl, flags) >> openHandler
        | Stat(f)(true) >> statHandler
        | Read(f)(offset, notdef, notdef) >> readHandler
        | Close(f)() >> closeHandler;


    Workflow workflow(pipe);
    workflow.Run().Wait();

    CPPUNIT_ASSERT(workflow.GetStatus().IsError());

    CPPUNIT_ASSERT(openHandler->Executed());
    CPPUNIT_ASSERT(statHandler->Executed());
    //----------------------------------------------------------------------------
    // If there is an error, last handlers should not be executed
    //----------------------------------------------------------------------------
    CPPUNIT_ASSERT(!readHandler->Executed());
    CPPUNIT_ASSERT(!closeHandler->Executed());

    //----------------------------------------------------------------------------
    // Release memory
    //----------------------------------------------------------------------------
    delete f;
}



void WorkflowTest::OperationFailureTest(){
    using namespace XrdCl;

    //----------------------------------------------------------------------------
    // Initialize
    //----------------------------------------------------------------------------
    std::string fileUrl = GetFileUrl("noexisting.dat");
    auto f = new File();

    //----------------------------------------------------------------------------
    // Create handlers
    //----------------------------------------------------------------------------
    auto openHandler = new TestingForwardingHandler();
    auto statHandler = new TestingForwardingHandler();
    auto readHandler = new TestingForwardingHandler();
    auto closeHandler = new TestingForwardingHandler();

    //----------------------------------------------------------------------------
    // Create and execute workflow
    //----------------------------------------------------------------------------

    const OpenFlags::Flags flags = OpenFlags::Read;
    uint64_t offset = 0;

    auto &pipe = Open(f)(fileUrl, flags) >> openHandler
        | Stat(f)(true) >> statHandler
        | Read(f)(offset, notdef, notdef) >> readHandler
        | Close(f)() >> closeHandler;


    Workflow workflow(pipe);
    workflow.Run().Wait();

    CPPUNIT_ASSERT(workflow.GetStatus().IsError());

    //----------------------------------------------------------------------------
    // If there is an error, handlers should not be executed
    //----------------------------------------------------------------------------
    CPPUNIT_ASSERT(!openHandler->Executed());
    CPPUNIT_ASSERT(!statHandler->Executed());
    CPPUNIT_ASSERT(!readHandler->Executed());
    CPPUNIT_ASSERT(!closeHandler->Executed());

    //----------------------------------------------------------------------------
    // Release memory
    //----------------------------------------------------------------------------
    delete f;
}


void WorkflowTest::DoubleRunningTest(){
    using namespace XrdCl;

    //----------------------------------------------------------------------------
    // Initialize
    //----------------------------------------------------------------------------
    std::string fileUrl = GetFileUrl("cb4aacf1-6f28-42f2-b68a-90a73460f424.dat");
    auto f = new File();

    //----------------------------------------------------------------------------
    // Create handlers
    //----------------------------------------------------------------------------
    auto openHandler = new TestingForwardingHandler();
    auto closeHandler = new TestingForwardingHandler();

    //----------------------------------------------------------------------------
    // Create and execute workflow
    //----------------------------------------------------------------------------

    const OpenFlags::Flags flags = OpenFlags::Read;

    auto &pipe = Open(f)(fileUrl, flags) >> openHandler | Close(f)() >> closeHandler;


    Workflow workflow(pipe);

    workflow.Run();

    //----------------------------------------------------------------------------
    // Running workflow again should fail
    //----------------------------------------------------------------------------
    try {
        workflow.Run();
        CPPUNIT_ASSERT(false);
    } catch(std::logic_error err){}

    workflow.Wait();

    //----------------------------------------------------------------------------
    // Running workflow again should fail
    //----------------------------------------------------------------------------
    try {
        workflow.Run();
        CPPUNIT_ASSERT(false);
    } catch(std::logic_error err){}

    CPPUNIT_ASSERT(workflow.GetStatus().IsOK());

    CPPUNIT_ASSERT(openHandler->Executed());
    CPPUNIT_ASSERT(closeHandler->Executed());

    //----------------------------------------------------------------------------
    // Release memory
    //----------------------------------------------------------------------------
    delete f;
}


void WorkflowTest::ParallelTest(){
    using namespace XrdCl;

    //----------------------------------------------------------------------------
    // Initialize
    //----------------------------------------------------------------------------
    auto lockFile = new File();
    auto firstFile = new File();
    auto secondFile = new File();

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

    auto parallelOperationHandler = new TestingForwardingHandler();
    auto first = new TestingForwardingHandler();
    auto second = new TestingForwardingHandler();

    auto &creatingPipe = Parallel{
        &(Open(f)(lockUrl, createFlags) >> first | Close(f)() >> second),
        &(Open(dataF)(secondFileUrl, createFlags) | Close(dataF)())
    } >> parallelOperationHandler;

    Workflow w(creatingPipe);
    w.Run().Wait();

    delete f;
    delete dataF;


    //----------------------------------------------------------------------------
    // Create and execute workflow
    //----------------------------------------------------------------------------
    uint64_t offset = 0;
    uint32_t size = 50  ;
    char* firstBuffer = new char[size]();
    char* secondBuffer = new char[size]();

    bool lockHandlerExecuted = false;
    auto lockOpenHandler = [&firstFileUrl, &secondFileUrl, &lockHandlerExecuted](XRootDStatus &st, OperationContext& params) -> void {
        params.ForwardParam<Open::UrlArg>(firstFileUrl, 1);
        params.ForwardParam<Open::UrlArg>(secondFileUrl, 2);
        lockHandlerExecuted = true;
    };
    auto lockCloseHandler = new TestingForwardingHandler();
    auto firstOpenHandler = new TestingForwardingHandler();
    auto firstReadHandler = new TestingForwardingHandler();
    auto firstCloseHandler = new TestingForwardingHandler();
    auto secondOpenHandler = new TestingForwardingHandler();
    auto secondReadHandler = new TestingForwardingHandler();
    auto secondCloseHandler = new TestingForwardingHandler();
    auto multiWorkflowHandler = new TestingForwardingHandler();

    auto &firstPipe = Open(firstFile)(notdef, readFlags) >> firstOpenHandler
    | Read(firstFile)(offset, size, firstBuffer) >> firstReadHandler
    | Close(firstFile)() >> firstCloseHandler;

    auto &secondPipe = Open(secondFile)(notdef, readFlags) >> secondOpenHandler
    | Read(secondFile)(offset, size, secondBuffer) >> secondReadHandler
    | Close(secondFile)() >> secondCloseHandler;

    auto &pipe = Open(lockFile)(lockUrl, readFlags) >> lockOpenHandler
    | Parallel{&firstPipe, &secondPipe} >> multiWorkflowHandler
    | Close(lockFile)() >> lockCloseHandler;

    Workflow workflow(pipe);
    workflow.Run().Wait();


    CPPUNIT_ASSERT(workflow.GetStatus().IsOK());

    CPPUNIT_ASSERT(lockHandlerExecuted);
    CPPUNIT_ASSERT(lockCloseHandler->Executed());
    CPPUNIT_ASSERT(firstOpenHandler->Executed());
    CPPUNIT_ASSERT(firstReadHandler->Executed());
    CPPUNIT_ASSERT(firstCloseHandler->Executed());
    CPPUNIT_ASSERT(secondOpenHandler->Executed());
    CPPUNIT_ASSERT(secondReadHandler->Executed());
    CPPUNIT_ASSERT(secondCloseHandler->Executed());
    CPPUNIT_ASSERT(multiWorkflowHandler->Executed());

    delete[] firstBuffer;
    delete[] secondBuffer;
    delete firstFile;
    delete secondFile;
    delete lockFile;


    //----------------------------------------------------------------------------
    // Remove lock file and data file
    //----------------------------------------------------------------------------
    f = new File();
    dataF = new File();

    auto url = GetAddress();
    auto fs = new FileSystem(url);

    auto lockRelativePath = GetPath(lockFileName);
    auto dataRelativePath = GetPath(dataFileName);

    auto lockRemovingHandler = new TestingForwardingHandler();
    auto dataFileRemovingHandler = new TestingForwardingHandler();

    Workflow deletingWorkflow(Parallel{
        &(Rm(fs)(lockRelativePath) >> lockRemovingHandler),
        &(Rm(fs)(dataRelativePath) >> dataFileRemovingHandler)
    });
    deletingWorkflow.Run().Wait();

    CPPUNIT_ASSERT(lockRemovingHandler->Executed());
    CPPUNIT_ASSERT(dataFileRemovingHandler->Executed());

    delete f;
    delete dataF;
    delete fs;
}


void WorkflowTest::FileSystemWorkflowTest(){
    using namespace XrdCl;

    auto mkDirHandler = new TestingForwardingHandler();
    auto locateHandler = new TestingForwardingHandler();
    auto moveHandler = new TestingForwardingHandler();
    auto secondLocateHandler = new TestingForwardingHandler();
    auto removeHandler = new TestingForwardingHandler();

    auto url = GetAddress();
    auto fs = new FileSystem(url);

    std::string newDirUrl = GetPath("sourceDirectory");
    std::string destDirUrl = GetPath("destDirectory");

    auto noneFlags = OpenFlags::None;

    auto &fsPipe = MkDir(fs)(newDirUrl, MkDirFlags::None, Access::None) >> mkDirHandler
        | Locate(fs)(newDirUrl, noneFlags) >> locateHandler
        | Mv(fs)(newDirUrl, destDirUrl) >> moveHandler
        | Locate(fs)(destDirUrl, OpenFlags::Refresh) >> secondLocateHandler
        | RmDir(fs)(destDirUrl) >> removeHandler;

    Workflow workflow(fsPipe);
    workflow.Run().Wait();

    CPPUNIT_ASSERT(workflow.GetStatus().IsOK());

    CPPUNIT_ASSERT(mkDirHandler->Executed());
    CPPUNIT_ASSERT(locateHandler->Executed());
    CPPUNIT_ASSERT(moveHandler->Executed());
    CPPUNIT_ASSERT(secondLocateHandler->Executed());
    CPPUNIT_ASSERT(removeHandler->Executed());

    delete fs;
}


void WorkflowTest::MixedWorkflowTest(){
    using namespace XrdCl;

    auto url = GetAddress();
    auto fs = new FileSystem(url);
    auto f1 = new File();
    auto f2 = new File();

    auto flags = OpenFlags::Write | OpenFlags::Delete | OpenFlags::Update;
    auto noneAccess = Access::None;

    std::string dirName = "tempDir";
    auto dirPath = GetPath(dirName);

    std::string firstFileName = dirName + "/firstFile";
    std::string secondFileName = dirName + "/secondFile";
    auto firstFileUrl = GetFileUrl(firstFileName);
    auto secondFileUrl = GetFileUrl(secondFileName);
    auto firstFilePath = GetPath(firstFileName);
    auto secondFilePath = GetPath(secondFileName);

    std::string firstContent = "First file content";
    std::string secondContent = "Second file content";
    char* firstText = const_cast<char*>(firstContent.c_str());
    char* secondText = const_cast<char*>(secondContent.c_str());
    auto firstContentLength = firstContent.size();
    auto secondContentLength = secondContent.size();

    uint64_t offset = 0;

    auto firstReadHandler = new ReadHandler(firstContent);
    auto firstStatHandler = new StatHandler(true, firstContentLength);
    auto secondReadHandler = new ReadHandler(secondContent);
    auto secondStatHandler = new StatHandler(true, secondContentLength);
    
    bool cleaningHandlerExecuted = false;
    
    auto cleaningHandler = [&dirPath, &cleaningHandlerExecuted](XRootDStatus &st, LocationInfo& info){
        LocationInfo::Iterator it;
        for( it = info.Begin(); it != info.End(); ++it )
        {
            auto url = URL(it->GetAddress());
            auto fs = new FileSystem(url);
            auto st = fs->RmDir(dirPath);
            CPPUNIT_ASSERT(st.IsOK());

            delete fs;
        }
        cleaningHandlerExecuted = true;
    };

    auto &firstFileOperations = Open(f1)(firstFileUrl, flags, noneAccess)
        | Write(f1)(offset, firstContentLength, firstText)
        | Sync(f1)()
        | Stat(f1)(true) >> firstStatHandler
        | Read(f1)(offset, notdef, notdef) >> firstReadHandler
        | Close(f1)();
    
    auto &secondFileOperations = Open(f2)(secondFileUrl, flags, noneAccess) 
        | Write(f2)(offset, secondContentLength, secondText)
        | Sync(f2)()
        | Stat(f2)(true) >> secondStatHandler
        | Read(f2)(offset, notdef, notdef) >> secondReadHandler
        | Close(f2)();

    std::vector<Operation<Handled>*> fileWorkflows{&firstFileOperations, &secondFileOperations};

    auto &pipe = MkDir(fs)(dirPath, MkDirFlags::None, noneAccess)
        | Parallel {fileWorkflows}
        | Rm(fs)(firstFilePath)  
        | Rm(fs)(secondFilePath)
        | DeepLocate(fs)(dirPath, OpenFlags::Refresh) >> cleaningHandler;

    Workflow workflow(pipe);
    workflow.Run().Wait();

    CPPUNIT_ASSERT(workflow.GetStatus().IsOK());

    CPPUNIT_ASSERT(firstStatHandler->Executed());
    CPPUNIT_ASSERT(firstReadHandler->Executed());
    CPPUNIT_ASSERT(secondStatHandler->Executed());
    CPPUNIT_ASSERT(secondReadHandler->Executed());
    CPPUNIT_ASSERT(cleaningHandlerExecuted);

    delete f1;
    delete f2;
    delete fs;
}
