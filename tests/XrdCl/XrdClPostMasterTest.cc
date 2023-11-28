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

#include <XrdCl/XrdClPostMaster.hh>
#include <XrdCl/XrdClMessage.hh>
#include <XProtocol/XProtocol.hh>
#include <XrdCl/XrdClXRootDTransport.hh>
#include <XrdCl/XrdClDefaultEnv.hh>
#include <XrdCl/XrdClSIDManager.hh>

#include <pthread.h>

#include "TestEnv.hh"
#include "GTestXrdHelpers.hh"

using namespace XrdClTests;

//------------------------------------------------------------------------------
// Declaration
//------------------------------------------------------------------------------
class PostMasterTest: public ::testing::Test
{
  public:
    void FunctionalTest();
    void ThreadingTest();
    void PingIPv6();
    void MultiIPConnectionTest();
};

//------------------------------------------------------------------------------
// Tear down the post master
//------------------------------------------------------------------------------
namespace
{
  class PostMasterFetch
  {
    public:
      PostMasterFetch() { }
      ~PostMasterFetch()  { }
      XrdCl::PostMaster *Get() {
        return XrdCl::DefaultEnv::GetPostMaster();
      }
      XrdCl::PostMaster *Reset() {
        XrdCl::PostMaster *pm = Get();
        pm->Stop();
        pm->Finalize();
        EXPECT_NE( pm->Initialize(), 0 );
        EXPECT_NE( pm->Start(), 0 );
        return pm;
      }
  };
}

//------------------------------------------------------------------------------
// Message filter
//------------------------------------------------------------------------------
class XrdFilter
{
  friend class SyncMsgHandler;

  public:
    XrdFilter( unsigned char id0 = 0, unsigned char id1 = 0 )
    {
      streamId[0] = id0;
      streamId[1] = id1;
    }

    virtual bool Filter( const XrdCl::Message *msg )
    {
      ServerResponse *resp = (ServerResponse *)msg->GetBuffer();
      if( resp->hdr.streamid[0] == streamId[0] &&
          resp->hdr.streamid[1] == streamId[1] )
        return true;
      return false;
    }

    virtual uint16_t GetSid() const
    {
      return (((uint16_t)streamId[1] << 8) | (uint16_t)streamId[0]);
    }

    unsigned char streamId[2];
};

//------------------------------------------------------------------------------
// Synchronous Message Handler
//------------------------------------------------------------------------------
class SyncMsgHandler : public XrdCl::MsgHandler
{
  public:
    SyncMsgHandler() :
      sem( 0 ), request( nullptr ), response( nullptr ), expiration( 0 )
    {
    }

  private:

    XrdFilter                        filter;
    XrdSysSemaphore                  sem;
    XrdCl::XRootDStatus              status;
    const XrdCl::Message            *request;
    std::shared_ptr<XrdCl::Message>  response;
    time_t                           expiration;

  public:

    //------------------------------------------------------------------------
    // Examine an incoming message, and decide on the action to be taken
    //------------------------------------------------------------------------
    virtual uint16_t Examine( std::shared_ptr<XrdCl::Message> &msg )
    {
      if( filter.Filter( msg.get() ) )
      {
        response = msg;
        return RemoveHandler;
      }
      return Ignore;
    }

    //------------------------------------------------------------------------
    // Reexamine the incoming message, and decide on the action to be taken
    //------------------------------------------------------------------------
    virtual uint16_t InspectStatusRsp()
    {
      return XrdCl::MsgHandler::Action::None;
    }

    //------------------------------------------------------------------------
    // Get handler sid
    //------------------------------------------------------------------------
    virtual uint16_t GetSid() const
    {
      return filter.GetSid();
    }

    //------------------------------------------------------------------------
    // Process the message if it was "taken" by the examine action
    //------------------------------------------------------------------------
    virtual void Process()
    {
      sem.Post();
    };

    //------------------------------------------------------------------------
    // Handle an event other that a message arrival
    //------------------------------------------------------------------------
    virtual uint8_t OnStreamEvent( StreamEvent          event,
                                   XrdCl::XRootDStatus  status )
    {
      if( event == Ready )
        return 0;
      this->status = status;
      sem.Post();
      return RemoveHandler;
    };

    //------------------------------------------------------------------------
    // The requested action has been performed and the status is available
    //------------------------------------------------------------------------
    virtual void OnStatusReady( const XrdCl::Message *message,
                                XrdCl::XRootDStatus   status )
    {
      request = message;
      this->status = status;
      if( !status.IsOK() )
        sem.Post();
    }

    //------------------------------------------------------------------------
    // Get a timestamp after which we give up
    //------------------------------------------------------------------------
    virtual time_t GetExpiration()
    {
      return expiration;
    }

    void SetExpiration( time_t e )
    {
      expiration = e;
    }

    XrdCl::XRootDStatus WaitFor( XrdCl::Message &rsp )
    {
      sem.Wait();
      if( response )
        rsp = std::move( *response );
      return status;
    }

    void SetFilter( unsigned char id0 = 0, unsigned char id1 = 0 )
    {
      filter.streamId[0] = id0;
      filter.streamId[1] = id1;
    }
};

//------------------------------------------------------------------------------
// Thread argument passing helper
//------------------------------------------------------------------------------
struct ArgHelper
{
  XrdCl::PostMaster *pm;
  int                index;
};

//------------------------------------------------------------------------------
// Post master test thread
//------------------------------------------------------------------------------
void *TestThreadFunc( void *arg )
{
  using namespace XrdCl;

  std::string address;
  Env *testEnv = TestEnv::GetEnv();
  EXPECT_TRUE( testEnv->GetString( "MainServerURL", address ) );

  ArgHelper *a = (ArgHelper*)arg;
  URL        host( address );

  //----------------------------------------------------------------------------
  // Send the ping messages
  //----------------------------------------------------------------------------
  SyncMsgHandler msgHandlers[100];
  Message        msgs[100];
  time_t expires = time(0)+1200;
  for( int i = 0; i < 100; ++i )
  {
    msgs[i].Allocate( sizeof( ClientPingRequest ) );
    msgs[i].Zero();
    msgs[i].SetDescription( "kXR_ping ()" );
    ClientPingRequest *request = (ClientPingRequest *)msgs[i].GetBuffer();
    request->streamid[0] = a->index;
    request->requestid   = kXR_ping;
    request->dlen        = 0;
    XRootDTransport::MarshallRequest( &msgs[i] );
    request->streamid[1] = i;
    msgHandlers[i].SetFilter( a->index, i );
    msgHandlers[i].SetExpiration( expires );
    GTEST_ASSERT_XRDST( a->pm->Send( host, &msgs[i], &msgHandlers[i], false, expires ) );
  }

  //----------------------------------------------------------------------------
  // Receive the answers
  //----------------------------------------------------------------------------
  for( int i = 0; i < 100; ++i )
  {
    XrdCl::Message msg;
    GTEST_ASSERT_XRDST( msgHandlers[i].WaitFor( msg ) );
    ServerResponse *resp = (ServerResponse *)msg.GetBuffer();
    EXPECT_TRUE( resp );
    EXPECT_EQ( resp->hdr.status, kXR_ok );
    EXPECT_EQ( msg.GetSize(), 8 );
  }
  return 0;
}

//------------------------------------------------------------------------------
// Threading test
//------------------------------------------------------------------------------
TEST(PostMasterTest, ThreadingTest)
{
  using namespace XrdCl;
  PostMasterFetch pmfetch;
  PostMaster *postMaster = pmfetch.Get();

  pthread_t thread[100];
  ArgHelper helper[100];

  for( int i = 0; i < 100; ++i )
  {
    helper[i].pm    = postMaster;
    helper[i].index = i;
    pthread_create( &thread[i], 0, TestThreadFunc, &helper[i] );
  }

  for( int i = 0; i < 100; ++i )
    pthread_join( thread[i], 0 );
}

//------------------------------------------------------------------------------
// Test the functionality of a poller
//------------------------------------------------------------------------------
TEST(PostMasterTest, FunctionalTest)
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize the stuff
  //----------------------------------------------------------------------------
  Env *env = DefaultEnv::GetEnv();
  Env *testEnv = TestEnv::GetEnv();
  env->PutInt( "TimeoutResolution", 1 );
  env->PutInt( "ConnectionWindow",  5 );

  PostMasterFetch pmfetch;
  PostMaster *postMaster = pmfetch.Get();

  std::string address;
  EXPECT_TRUE( testEnv->GetString( "MainServerURL", address ) );

  //----------------------------------------------------------------------------
  // Send a message and wait for the answer
  //----------------------------------------------------------------------------
  time_t    expires = ::time(0)+60;
  Message   m1, m2;
  URL       host( address );

  SyncMsgHandler msgHandler1;
  msgHandler1.SetFilter( 1, 2 );
  msgHandler1.SetExpiration( expires );

  m1.Allocate( sizeof( ClientPingRequest ) );
  m1.Zero();

  ClientPingRequest *request = (ClientPingRequest *)m1.GetBuffer();
  request->streamid[0] = 1;
  request->streamid[1] = 2;
  request->requestid   = kXR_ping;
  request->dlen        = 0;
  XRootDTransport::MarshallRequest( &m1 );

  GTEST_ASSERT_XRDST( postMaster->Send( host, &m1, &msgHandler1, false, expires ) );

  GTEST_ASSERT_XRDST( msgHandler1.WaitFor( m2 ) );
  ServerResponse *resp = (ServerResponse *)m2.GetBuffer();
  EXPECT_TRUE( resp );
  EXPECT_EQ( resp->hdr.status, kXR_ok );
  EXPECT_EQ( m2.GetSize(), 8 );

  //----------------------------------------------------------------------------
  // Send out some stuff to a location where nothing listens
  //----------------------------------------------------------------------------
  env->PutInt( "ConnectionWindow", 5 );
  env->PutInt( "ConnectionRetry", 3 );
  URL localhost1( "root://localhost:10101" );

  SyncMsgHandler msgHandler2;
  msgHandler2.SetFilter( 1, 2 );
  time_t shortexp = ::time(0) + 1;
  msgHandler2.SetExpiration( shortexp );
  GTEST_ASSERT_XRDST( postMaster->Send( localhost1, &m1, &msgHandler2, false,
                        shortexp ) );
  GTEST_ASSERT_XRDST_NOTOK( msgHandler2.WaitFor( m2 ), errOperationExpired );

  SyncMsgHandler msgHandler3;
  msgHandler3.SetFilter( 1, 2 );
  msgHandler3.SetExpiration( expires );
  GTEST_ASSERT_XRDST( postMaster->Send( localhost1, &m1, &msgHandler3, false,
                                          expires ) );
  GTEST_ASSERT_XRDST_NOTOK( msgHandler3.WaitFor( m2 ), errConnectionError );

  //----------------------------------------------------------------------------
  // Test the transport queries
  //----------------------------------------------------------------------------
  AnyObject nameObj, sidMgrObj;
  Status st1, st2;
  const char *name   = 0;

  GTEST_ASSERT_XRDST( postMaster->QueryTransport( host,
                                                   TransportQuery::Name,
                                                   nameObj ) );
  nameObj.Get( name );

  EXPECT_TRUE( name );
  EXPECT_TRUE( !::strcmp( name, "XRootD" ) );

  //----------------------------------------------------------------------------
  // Reinitialize and try to do something
  //----------------------------------------------------------------------------
  env->PutInt( "LoadBalancerTTL", 5 );
  postMaster = pmfetch.Reset();

  m2.Free();
  m1.Zero();

  request = (ClientPingRequest *)m1.GetBuffer();
  request->streamid[0] = 1;
  request->streamid[1] = 2;
  request->requestid   = kXR_ping;
  request->dlen        = 0;
  XRootDTransport::MarshallRequest( &m1 );

  SyncMsgHandler msgHandler4;
  msgHandler4.SetFilter( 1, 2 );
  msgHandler4.SetExpiration( expires );
  GTEST_ASSERT_XRDST( postMaster->Send( host, &m1, &msgHandler4, false, expires ) );

  GTEST_ASSERT_XRDST( msgHandler4.WaitFor( m2 ) );
  resp = (ServerResponse *)m2.GetBuffer();
  EXPECT_TRUE( resp );
  EXPECT_EQ( resp->hdr.status, kXR_ok );
  EXPECT_EQ( m2.GetSize(), 8 );

  //----------------------------------------------------------------------------
  // Sleep 10 secs waiting for iddle connection to be closed and see
  // whether we can reconnect
  //----------------------------------------------------------------------------
  sleep( 10 );
  SyncMsgHandler msgHandler5;
  msgHandler5.SetFilter( 1, 2 );
  msgHandler5.SetExpiration( expires );
  GTEST_ASSERT_XRDST( postMaster->Send( host, &m1, &msgHandler5, false, expires ) );

  GTEST_ASSERT_XRDST( msgHandler5.WaitFor( m2 ) );
  resp = (ServerResponse *)m2.GetBuffer();
  EXPECT_TRUE( resp );
  EXPECT_EQ( resp->hdr.status, kXR_ok );
  EXPECT_EQ( m2.GetSize(), 8 );
}


//------------------------------------------------------------------------------
// Test the functionality of a poller
//------------------------------------------------------------------------------
TEST(PostMasterTest, PingIPv6)
{
  using namespace XrdCl;
#if 0
  //----------------------------------------------------------------------------
  // Initialize the stuff
  //----------------------------------------------------------------------------
  PostMasterFetch pmfetch;
  PostMaster *postMaster = pmfetch.Get();

  //----------------------------------------------------------------------------
  // Build the message
  //----------------------------------------------------------------------------
  Message   m1, *m2 = 0;
  XrdFilter f1( 1, 2 );
  URL       localhost1( "root://[::1]" );
  URL       localhost2( "root://[::127.0.0.1]" );

  m1.Allocate( sizeof( ClientPingRequest ) );
  m1.Zero();

  ClientPingRequest *request = (ClientPingRequest *)m1.GetBuffer();
  request->streamid[0] = 1;
  request->streamid[1] = 2;
  request->requestid   = kXR_ping;
  request->dlen        = 0;
  XRootDTransport::MarshallRequest( &m1 );

  Status sc;

  //----------------------------------------------------------------------------
  // Send the message - localhost1
  //----------------------------------------------------------------------------
  sc = postMaster->Send( localhost1, &m1, false, 1200 );
  EXPECT_TRUE( sc.IsOK() );

  sc = postMaster->Receive( localhost1, m2, &f1, false, 1200 );
  EXPECT_TRUE( sc.IsOK() );
  ServerResponse *resp = (ServerResponse *)m2->GetBuffer();
  EXPECT_TRUE( resp );
  EXPECT_EQ( resp->hdr.status, kXR_ok );
  EXPECT_EQ( m2->GetSize(), 8 );

  //----------------------------------------------------------------------------
  // Send the message - localhost2
  //----------------------------------------------------------------------------
  sc = postMaster->Send( localhost2, &m1, false, 1200 );
  EXPECT_TRUE( sc.IsOK() );

  sc = postMaster->Receive( localhost2, m2, &f1, 1200 );
  EXPECT_TRUE( sc.IsOK() );
  resp = (ServerResponse *)m2->GetBuffer();
  EXPECT_TRUE( resp );
  EXPECT_EQ( resp->hdr.status, kXR_ok );
  EXPECT_EQ( m2->GetSize(), 8 );
#endif
}

namespace
{
  //----------------------------------------------------------------------------
  // Create a ping message
  //----------------------------------------------------------------------------
  // XrdCl::Message *CreatePing( char streamID1, char streamID2 )
  // {
  //   using namespace XrdCl;
  //   Message *m = new Message();
  //   m->Allocate( sizeof( ClientPingRequest ) );
  //   m->Zero();

  //   ClientPingRequest *request = (ClientPingRequest *)m->GetBuffer();
  //   request->streamid[0] = streamID1;
  //   request->streamid[1] = streamID2;
  //   request->requestid   = kXR_ping;
  //   XRootDTransport::MarshallRequest( m );
  //   return m;
  // }
}


//------------------------------------------------------------------------------
// Connection test
//------------------------------------------------------------------------------

#if 0
TEST(PostMasterTest, MultiIPConnectionTest)
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize the stuff
  //----------------------------------------------------------------------------
  Env *env = DefaultEnv::GetEnv();
  Env *testEnv = TestEnv::GetEnv();
  env->PutInt( "TimeoutResolution", 1 );
  env->PutInt( "ConnectionWindow",  5 );

  PostMasterFetch pmfetch;
  PostMaster *postMaster = pmfetch.Get();

  std::string address;
  EXPECT_TRUE( testEnv->GetString( "MultiIPServerURL", address ) );

  time_t expires = ::time(0)+1200;
  URL url1( "nenexistent" );
  URL url2( address );
  URL url3( address );
  url2.SetPort( 1111 );
  url3.SetPort( 1099 );

  //----------------------------------------------------------------------------
  // Sent ping to a nonexistent host
  //----------------------------------------------------------------------------
  SyncMsgHandler msgHandler1;
  msgHandler1.SetFilter( 1, 2 );
  msgHandler1.SetExpiration( expires );
  Message *m = CreatePing( 1, 2 );
  GTEST_ASSERT_XRDST_NOTOK( postMaster->Send( url1, m, &msgHandler1, false, expires ),
                              errInvalidAddr );

  //----------------------------------------------------------------------------
  // Try on the wrong port
  //----------------------------------------------------------------------------
  SyncMsgHandler msgHandler2;
  msgHandler2.SetFilter( 1, 2 );
  msgHandler2.SetExpiration( expires );
  Message m2;

  GTEST_ASSERT_XRDST( postMaster->Send( url2, m, &msgHandler2, false, expires ) );
  GTEST_ASSERT_XRDST_NOTOK( msgHandler2.WaitFor( m2 ), errConnectionError );

  //----------------------------------------------------------------------------
  // Try on a good one
  //----------------------------------------------------------------------------
  SyncMsgHandler msgHandler3;
  msgHandler3.SetFilter( 1, 2 );
  msgHandler3.SetExpiration( expires );

  GTEST_ASSERT_XRDST( postMaster->Send( url3, m, &msgHandler3, false, expires ) );
  GTEST_ASSERT_XRDST( msgHandler3.WaitFor( m2 ) );
  ServerResponse *resp = (ServerResponse *)m2.GetBuffer();
  EXPECT_TRUE( resp );
  EXPECT_EQ( resp->hdr.status, kXR_ok );
  EXPECT_EQ( m2.GetSize(), 8 );
}
#endif
