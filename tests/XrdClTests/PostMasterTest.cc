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
#include <XrdCl/XrdClPostMaster.hh>
#include <XrdCl/XrdClMessage.hh>
#include <XProtocol/XProtocol.hh>
#include <XrdCl/XrdClXRootDTransport.hh>
#include <XrdCl/XrdClDefaultEnv.hh>
#include <XrdCl/XrdClSIDManager.hh>

#include <pthread.h>

#include "TestEnv.hh"
#include "CppUnitXrdHelpers.hh"

using namespace XrdClTests;

//------------------------------------------------------------------------------
// Declaration
//------------------------------------------------------------------------------
class PostMasterTest: public CppUnit::TestCase
{
  public:
    CPPUNIT_TEST_SUITE( PostMasterTest );
      CPPUNIT_TEST( FunctionalTest );
      CPPUNIT_TEST( PingIPv6 );
      CPPUNIT_TEST( ThreadingTest );
      CPPUNIT_TEST( MultiIPConnectionTest );
    CPPUNIT_TEST_SUITE_END();
    void FunctionalTest();
    void ThreadingTest();
    void PingIPv6();
    void MultiIPConnectionTest();
};

CPPUNIT_TEST_SUITE_REGISTRATION( PostMasterTest );

//------------------------------------------------------------------------------
// Tear down the post master
//------------------------------------------------------------------------------
namespace
{
  class PostMasterFinalizer
  {
    public:
      PostMasterFinalizer( XrdCl::PostMaster *pm = 0 ): pPostMaster(pm) {}
      ~PostMasterFinalizer()
      {
        if( pPostMaster )
        {
          pPostMaster->Stop();
          pPostMaster->Finalize();
        }
      }
      void Set( XrdCl::PostMaster *pm ) { pPostMaster = pm; }
      XrdCl::PostMaster *Get() { return pPostMaster; }

    private:
      XrdCl::PostMaster *pPostMaster;
  };
}

//------------------------------------------------------------------------------
// Message filter
//------------------------------------------------------------------------------
class XrdFilter: public XrdCl::MessageFilter
{
  public:
    XrdFilter( char id0 = 0, char id1 = 0 )
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

    char streamId[2];
};

//------------------------------------------------------------------------------
// Thread argument passing helper
//------------------------------------------------------------------------------
struct ArgHelper
{
  XrdCl::PostMaster *pm;
  int                    index;
};

//------------------------------------------------------------------------------
// Post master test thread
//------------------------------------------------------------------------------
void *TestThreadFunc( void *arg )
{
  using namespace XrdCl;

  std::string address;
  Env *testEnv = TestEnv::GetEnv();
  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );

  ArgHelper *a = (ArgHelper*)arg;
  URL        host( address );
  XrdFilter  f( a->index, 0 );

  //----------------------------------------------------------------------------
  // Send the ping messages
  //----------------------------------------------------------------------------
  time_t expires = time(0)+1200;
  Message m;
  m.Allocate( sizeof( ClientPingRequest ) );
  m.Zero();
  m.SetDescription( "kXR_ping ()" );
  ClientPingRequest *request = (ClientPingRequest *)m.GetBuffer();
  request->streamid[0] = a->index;
  request->requestid   = kXR_ping;
  request->dlen        = 0;
  XRootDTransport::MarshallRequest( &m );

  for( int i = 0; i < 100; ++i )
  {
    request->streamid[1] = i;
    CPPUNIT_ASSERT_XRDST( a->pm->Send( host, &m, false, expires ) );
  }

  //----------------------------------------------------------------------------
  // Receive the answers
  //----------------------------------------------------------------------------
  for( int i = 0; i < 100; ++i )
  {
    Message *m = 0;
    f.streamId[1] = i;
    CPPUNIT_ASSERT_XRDST( a->pm->Receive( host, m, &f, expires ) );
    ServerResponse *resp = (ServerResponse *)m->GetBuffer();
    CPPUNIT_ASSERT( resp != 0 );
    CPPUNIT_ASSERT( resp->hdr.status == kXR_ok );
    CPPUNIT_ASSERT( m->GetSize() == 8 );
    delete m;
  }
  return 0;
}

//------------------------------------------------------------------------------
// Threading test
//------------------------------------------------------------------------------
void PostMasterTest::ThreadingTest()
{
  using namespace XrdCl;
  PostMaster postMaster;
  PostMasterFinalizer finalizer( &postMaster );
  postMaster.Initialize();
  postMaster.Start();

  pthread_t thread[100];
  ArgHelper helper[100];

  for( int i = 0; i < 100; ++i )
  {
    helper[i].pm    = &postMaster;
    helper[i].index = i;
    pthread_create( &thread[i], 0, TestThreadFunc, &helper[i] );
  }

  for( int i = 0; i < 100; ++i )
    pthread_join( thread[i], 0 );
}

//------------------------------------------------------------------------------
// Test the functionality of a poller
//------------------------------------------------------------------------------
void PostMasterTest::FunctionalTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize the stuff
  //----------------------------------------------------------------------------
  Env *env = DefaultEnv::GetEnv();
  Env *testEnv = TestEnv::GetEnv();
  env->PutInt( "TimeoutResolution", 1 );
  env->PutInt( "ConnectionWindow", 15 );

  PostMaster postMaster;
  PostMasterFinalizer finalizer( &postMaster );
  postMaster.Initialize();
  postMaster.Start();

  std::string address;
  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );

  //----------------------------------------------------------------------------
  // Send a message and wait for the answer
  //----------------------------------------------------------------------------
  time_t    expires = ::time(0)+1200;
  Message   m1, *m2 = 0;
  XrdFilter f1( 1, 2 );
  URL       host( address );

  m1.Allocate( sizeof( ClientPingRequest ) );
  m1.Zero();

  ClientPingRequest *request = (ClientPingRequest *)m1.GetBuffer();
  request->streamid[0] = 1;
  request->streamid[1] = 2;
  request->requestid   = kXR_ping;
  request->dlen        = 0;
  XRootDTransport::MarshallRequest( &m1 );

  CPPUNIT_ASSERT_XRDST( postMaster.Send( host, &m1, false, expires ) );

  CPPUNIT_ASSERT_XRDST( postMaster.Receive( host, m2, &f1, expires ) );
  ServerResponse *resp = (ServerResponse *)m2->GetBuffer();
  CPPUNIT_ASSERT( resp != 0 );
  CPPUNIT_ASSERT( resp->hdr.status == kXR_ok );
  CPPUNIT_ASSERT( m2->GetSize() == 8 );

  //----------------------------------------------------------------------------
  // Wait for an answer to a message that has not been sent - test the
  // reception timeout
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT_XRDST_NOTOK( postMaster.Receive( host, m2, &f1, 2 ),
                              errOperationExpired );

  //----------------------------------------------------------------------------
  // Send out some stuff to a location where nothing listens
  //----------------------------------------------------------------------------
  env->PutInt( "ConnectionWindow", 5 );
  env->PutInt( "ConnectionRetry", 3 );
  URL localhost1( "root://localhost:10101" );
  CPPUNIT_ASSERT_XRDST_NOTOK( postMaster.Send( localhost1, &m1, false,
                                               ::time(0)+3 ),
                              errOperationExpired );
  CPPUNIT_ASSERT_XRDST_NOTOK( postMaster.Send( localhost1, &m1, false,
                                               expires ),
                              errConnectionError );

  //----------------------------------------------------------------------------
  // Test the transport queries
  //----------------------------------------------------------------------------
  AnyObject nameObj, sidMgrObj;
  Status st1, st2;
  const char *name   = 0;
  SIDManager *sidMgr = 0;

  CPPUNIT_ASSERT_XRDST( postMaster.QueryTransport( host,
                                                   TransportQuery::Name,
                                                   nameObj ) );
  CPPUNIT_ASSERT_XRDST( postMaster.QueryTransport( host,
                                                   XRootDQuery::SIDManager,
                                                   sidMgrObj ) );

  nameObj.Get( name );
  sidMgrObj.Get( sidMgr );

  CPPUNIT_ASSERT( name );
  CPPUNIT_ASSERT( !::strcmp( name, "XRootD" ) );
  CPPUNIT_ASSERT( sidMgr );

  postMaster.Stop();
  postMaster.Finalize();

  //----------------------------------------------------------------------------
  // Reinitialize and try to do something
  //----------------------------------------------------------------------------
  env->PutInt( "LoadBalancerTTL", 5 );
  postMaster.Initialize();
  postMaster.Start();

  m2 = 0;
  m1.Zero();

  request = (ClientPingRequest *)m1.GetBuffer();
  request->streamid[0] = 1;
  request->streamid[1] = 2;
  request->requestid   = kXR_ping;
  request->dlen        = 0;
  XRootDTransport::MarshallRequest( &m1 );

  CPPUNIT_ASSERT_XRDST( postMaster.Send( host, &m1, false, expires ) );

  CPPUNIT_ASSERT_XRDST( postMaster.Receive( host, m2, &f1, expires ) );
  resp = (ServerResponse *)m2->GetBuffer();
  CPPUNIT_ASSERT( resp != 0 );
  CPPUNIT_ASSERT( resp->hdr.status == kXR_ok );
  CPPUNIT_ASSERT( m2->GetSize() == 8 );

  //----------------------------------------------------------------------------
  // Sleep 10 secs waiting for iddle connection to be closed and see
  // whether we can reconnect
  //----------------------------------------------------------------------------
  sleep( 10 );
  CPPUNIT_ASSERT_XRDST( postMaster.Send( host, &m1, false, expires ) );

  CPPUNIT_ASSERT_XRDST( postMaster.Receive( host, m2, &f1, expires ) );
  resp = (ServerResponse *)m2->GetBuffer();
  CPPUNIT_ASSERT( resp != 0 );
  CPPUNIT_ASSERT( resp->hdr.status == kXR_ok );
  CPPUNIT_ASSERT( m2->GetSize() == 8 );
}


//------------------------------------------------------------------------------
// Test the functionality of a poller
//------------------------------------------------------------------------------
void PostMasterTest::PingIPv6()
{
  using namespace XrdCl;
#if 0
  //----------------------------------------------------------------------------
  // Initialize the stuff
  //----------------------------------------------------------------------------
  PostMaster postMaster;
  postMaster.Initialize();
  postMaster.Start();

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
  sc = postMaster.Send( localhost1, &m1, false, 1200 );
  CPPUNIT_ASSERT( sc.IsOK() );

  sc = postMaster.Receive( localhost1, m2, &f1, false, 1200 );
  CPPUNIT_ASSERT( sc.IsOK() );
  ServerResponse *resp = (ServerResponse *)m2->GetBuffer();
  CPPUNIT_ASSERT( resp != 0 );
  CPPUNIT_ASSERT( resp->hdr.status == kXR_ok );
  CPPUNIT_ASSERT( m2->GetSize() == 8 );

  //----------------------------------------------------------------------------
  // Send the message - localhost2
  //----------------------------------------------------------------------------
  sc = postMaster.Send( localhost2, &m1, false, 1200 );
  CPPUNIT_ASSERT( sc.IsOK() );

  sc = postMaster.Receive( localhost2, m2, &f1, 1200 );
  CPPUNIT_ASSERT( sc.IsOK() );
  resp = (ServerResponse *)m2->GetBuffer();
  CPPUNIT_ASSERT( resp != 0 );
  CPPUNIT_ASSERT( resp->hdr.status == kXR_ok );
  CPPUNIT_ASSERT( m2->GetSize() == 8 );

  //----------------------------------------------------------------------------
  // Clean up
  //----------------------------------------------------------------------------
  postMaster.Stop();
  postMaster.Finalize();
#endif
}

namespace
{
  //----------------------------------------------------------------------------
  // Create a ping message
  //----------------------------------------------------------------------------
  XrdCl::Message *CreatePing( char streamID1, char streamID2 )
  {
    using namespace XrdCl;
    Message *m = new Message();
    m->Allocate( sizeof( ClientPingRequest ) );
    m->Zero();

    ClientPingRequest *request = (ClientPingRequest *)m->GetBuffer();
    request->streamid[0] = streamID1;
    request->streamid[1] = streamID2;
    request->requestid   = kXR_ping;
    XRootDTransport::MarshallRequest( m );
    return m;
  }
}


//------------------------------------------------------------------------------
// Connection test
//------------------------------------------------------------------------------
void PostMasterTest::MultiIPConnectionTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize the stuff
  //----------------------------------------------------------------------------
  Env *env = DefaultEnv::GetEnv();
  Env *testEnv = TestEnv::GetEnv();
  env->PutInt( "TimeoutResolution", 1 );
  env->PutInt( "ConnectionWindow",  5 );

  PostMaster postMaster;
  PostMasterFinalizer finalizer( &postMaster );
  postMaster.Initialize();
  postMaster.Start();

  std::string address;
  CPPUNIT_ASSERT( testEnv->GetString( "MultiIPServerURL", address ) );

  time_t expires = ::time(0)+1200;
  URL url1( "nenexistent" );
  URL url2( address );
  URL url3( address );
  url2.SetPort( 1111 );
  url3.SetPort( 1099 );

  //----------------------------------------------------------------------------
  // Sent ping to a nonexistent host
  //----------------------------------------------------------------------------
  Message *m = CreatePing( 1, 2 );
  CPPUNIT_ASSERT_XRDST_NOTOK( postMaster.Send( url1, m, false, expires ),
                              errInvalidAddr );

  //----------------------------------------------------------------------------
  // Try on the wrong port
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT_XRDST_NOTOK( postMaster.Send( url2, m, false, expires ),
                              errConnectionError );

  //----------------------------------------------------------------------------
  // Try on a good one
  //----------------------------------------------------------------------------
  Message   *m2 = 0;
  XrdFilter  f1( 1, 2 );

  CPPUNIT_ASSERT_XRDST( postMaster.Send( url3, m, false, expires ) );
  CPPUNIT_ASSERT_XRDST( postMaster.Receive( url3, m2, &f1, expires ) );
  ServerResponse *resp = (ServerResponse *)m2->GetBuffer();
  CPPUNIT_ASSERT( resp != 0 );
  CPPUNIT_ASSERT( resp->hdr.status == kXR_ok );
  CPPUNIT_ASSERT( m2->GetSize() == 8 );
}
