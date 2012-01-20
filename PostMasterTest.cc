//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include <cppunit/extensions/HelperMacros.h>
#include <XrdCl/XrdClPostMaster.hh>
#include <XrdCl/XrdClMessage.hh>
#include <XProtocol/XProtocol.hh>
#include <XrdCl/XrdClXRootDTransport.hh>
#include <XrdCl/XrdClDefaultEnv.hh>

#include <pthread.h>

//------------------------------------------------------------------------------
// Declaration
//------------------------------------------------------------------------------
class PostMasterTest: public CppUnit::TestCase
{
  public:
    CPPUNIT_TEST_SUITE( PostMasterTest );
      CPPUNIT_TEST( FunctionalTest );
      CPPUNIT_TEST( ThreadingTest );
    CPPUNIT_TEST_SUITE_END();
    void FunctionalTest();
    void ThreadingTest();
};

CPPUNIT_TEST_SUITE_REGISTRATION( PostMasterTest );

//------------------------------------------------------------------------------
// Message filter
//------------------------------------------------------------------------------
class XrdFilter: public XrdClient::MessageFilter
{
  public:
    XrdFilter( char id0 = 0, char id1 = 0 )
    {
      streamId[0] = id0;
      streamId[1] = id1;
    }

    virtual bool Filter( const XrdClient::Message *msg )
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
  XrdClient::PostMaster *pm;
  int                    index;
};

//------------------------------------------------------------------------------
// Post master test thread
//------------------------------------------------------------------------------
void *TestThreadFunc( void *arg )
{
  using namespace XrdClient;

  ArgHelper *a = (ArgHelper*)arg;
  URL        localhost( "root://localhost" );
  XrdFilter  f( a->index, 0 );

  //----------------------------------------------------------------------------
  // Send the ping messages
  //----------------------------------------------------------------------------
  Message m;
  m.Allocate( sizeof( ClientPingRequest ) );
  ClientPingRequest *request = (ClientPingRequest *)m.GetBuffer();
  request->streamid[0] = a->index;
  request->requestid   = kXR_ping;
  request->dlen        = 0;
  XRootDTransport::Marshall( &m );
  Status sc;

  for( int i = 0; i < 100; ++i )
  {
    request->streamid[1] = i;
    sc = a->pm->Send( localhost, &m, 1200 );
    CPPUNIT_ASSERT( sc.IsOK() );
  }

  //----------------------------------------------------------------------------
  // Receive the answers
  //----------------------------------------------------------------------------
  for( int i = 0; i < 100; ++i )
  {
    Message *m;
    f.streamId[1] = i;
    sc = a->pm->Receive( localhost, m, &f, 1200 );
    CPPUNIT_ASSERT( sc.IsOK() );
    ServerResponse *resp = (ServerResponse *)m->GetBuffer();
    CPPUNIT_ASSERT( resp != 0 );
    CPPUNIT_ASSERT( resp->hdr.status == kXR_ok );
    CPPUNIT_ASSERT( m->GetSize() == 8 );
  }
}

//------------------------------------------------------------------------------
// Threading test
//------------------------------------------------------------------------------
void PostMasterTest::ThreadingTest()
{
  using namespace XrdClient;
  PostMaster postMaster;
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

  postMaster.Stop();
  postMaster.Finalize();
}

//------------------------------------------------------------------------------
// Test the functionality of a poller
//------------------------------------------------------------------------------
void PostMasterTest::FunctionalTest()
{
  using namespace XrdClient;

  //----------------------------------------------------------------------------
  // Initialize the stuff
  //----------------------------------------------------------------------------
  Env *env = DefaultEnv::GetEnv();
  env->PutInt( "DataServerTTL", 2 );
  env->PutInt( "ManagerTTL", 2 );
  env->PutInt( "TimeoutResolution", 1 );
  env->PutInt( "ConnectionWindow", 15 );

  PostMaster postMaster;
  postMaster.Initialize();
  postMaster.Start();

  //----------------------------------------------------------------------------
  // Send a message and wait for the answer
  //----------------------------------------------------------------------------
  Message   m1, *m2 = 0;
  XrdFilter f1( 1, 2 );
  URL       localhost( "root://localhost" );

  m1.Allocate( sizeof( ClientPingRequest ) );
  m1.Zero();

  ClientPingRequest *request = (ClientPingRequest *)m1.GetBuffer();
  request->streamid[0] = 1;
  request->streamid[1] = 2;
  request->requestid   = kXR_ping;
  request->dlen        = 0;
  XRootDTransport::Marshall( &m1 );

  Status sc;

  sc = postMaster.Send( localhost, &m1, 1200 );
  CPPUNIT_ASSERT( sc.IsOK() );

  sc = postMaster.Receive( localhost, m2, &f1, 1200 );
  CPPUNIT_ASSERT( sc.IsOK() );
  ServerResponse *resp = (ServerResponse *)m2->GetBuffer();
  CPPUNIT_ASSERT( resp != 0 );
  CPPUNIT_ASSERT( resp->hdr.status == kXR_ok );
  CPPUNIT_ASSERT( m2->GetSize() == 8 );

  //----------------------------------------------------------------------------
  // Wait until the connection TTL expires and send echo again
  //----------------------------------------------------------------------------
  ::sleep( 4 );

  m2 = 0;
  sc = postMaster.Send( localhost, &m1, 1200 );
  CPPUNIT_ASSERT( sc.IsOK() );

  sc = postMaster.Receive( localhost, m2, &f1, 1200 );
  CPPUNIT_ASSERT( sc.IsOK() );
  resp = (ServerResponse *)m2->GetBuffer();
  CPPUNIT_ASSERT( resp != 0 );
  CPPUNIT_ASSERT( resp->hdr.status == kXR_ok );
  CPPUNIT_ASSERT( m2->GetSize() == 8 );

  //----------------------------------------------------------------------------
  // Wait for an answer to a message that has not been sent - test the
  // reception timeout
  //----------------------------------------------------------------------------
  env->PutInt( "DataServerTTL", 5 );
  env->PutInt( "ManagerTTL", 5 );
  sc = postMaster.Receive( localhost, m2, &f1, 2 );
  CPPUNIT_ASSERT( !sc.IsOK() );
  CPPUNIT_ASSERT( sc.code == errSocketTimeout );

  sc = postMaster.Receive( localhost, m2, &f1, 20 );
  CPPUNIT_ASSERT( !sc.IsOK() );
  CPPUNIT_ASSERT( sc.code == errStreamDisconnect );

  //----------------------------------------------------------------------------
  // Send out some stuff to a location where nothing listens
  //----------------------------------------------------------------------------
  env->PutInt( "ConnectionWindow", 5 );
  env->PutInt( "ConnectionRetry", 3 );
  URL localhost1( "root://localhost:10101" );
  sc = postMaster.Send( localhost1, &m1, 3 );
  CPPUNIT_ASSERT( !sc.IsOK() );
  CPPUNIT_ASSERT( sc.code == errSocketTimeout );

  sc = postMaster.Send( localhost1, &m1, 1200 );
  CPPUNIT_ASSERT( !sc.IsOK() );
  CPPUNIT_ASSERT( sc.code == errConnectionError );

  postMaster.Stop();
  postMaster.Finalize();
}
