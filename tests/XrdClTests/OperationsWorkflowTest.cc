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
#include "XrdCl/XrdClOperations.hh"

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
    CPPUNIT_TEST_SUITE_END();
    void ReadingWorkflowTest();
    void WritingWorkflowTest();
    void MissingParameterTest();
    void OperationFailureTest();
    void DoubleRunningTest();
    std::string GetFileUrl(std::string fileName);
};

CPPUNIT_TEST_SUITE_REGISTRATION( WorkflowTest );


namespace {
    using namespace XrdCl;

    class TestingForwardingHandler: public ForwardingHandler {
        public:
            TestingForwardingHandler(){
                executed = false;
            }

            void HandleResponseWithHosts(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response, XrdCl::HostList *hostList) {
                CPPUNIT_ASSERT_XRDST(*status);
                if(status){ delete status; }
                if(response){ delete response; }
                if(hostList){ delete hostList; }
                executed = true;
            } 

            void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) {
                CPPUNIT_ASSERT_XRDST(*status);
                if(status){ delete status; }
                if(response){ delete response; }
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
                container->SetPtrParam("buffer", buffer);
                container->SetParam("size", size);

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
                vec = NULL;
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

                container->SetPtrParam("iov", vec);
                container->SetParam("iovcnt", 3);

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


std::string WorkflowTest::GetFileUrl(std::string fileName){
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


    Workflow workflow = Workflow(pipe);
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

    //----------------------------------------------------------------------------
    // Create handlers
    //----------------------------------------------------------------------------
    auto openHandler = new WriteVOpenHandler(texts, 3);
    auto writeHandler = new TestingForwardingHandler(); 
    auto syncHandler = new TestingForwardingHandler();
    auto statHandler = new StatHandler(false);
    auto readHandler = new ReadHandler(texts[0] + texts[1] + texts[2]);
    auto closeHandler = new TestingForwardingHandler();

    //----------------------------------------------------------------------------
    // Create and execute workflow
    //----------------------------------------------------------------------------

    uint64_t offset = 0;

    auto &pipe = Open(f)(fileUrl, flags) >> openHandler
        | WriteV(f)(offset, notdef, notdef) >> writeHandler
        | Sync(f)() >> syncHandler
        | Stat(f)(true) >> statHandler
        | Read(f)(offset, notdef, notdef) >> readHandler
        | Close(f)() >> closeHandler;

    Workflow workflow = Workflow(pipe);         
    workflow.Run().Wait();

    CPPUNIT_ASSERT(workflow.GetStatus().IsOK());

    CPPUNIT_ASSERT(openHandler->Executed());
    CPPUNIT_ASSERT(writeHandler->Executed());
    CPPUNIT_ASSERT(syncHandler->Executed());
    CPPUNIT_ASSERT(statHandler->Executed());
    CPPUNIT_ASSERT(readHandler->Executed());
    CPPUNIT_ASSERT(closeHandler->Executed());

    //----------------------------------------------------------------------------
    // Release memory
    //----------------------------------------------------------------------------
    delete f;

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


    Workflow workflow = Workflow(pipe);
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


    Workflow workflow = Workflow(pipe);
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


    Workflow workflow = Workflow(pipe);

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