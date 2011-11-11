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

//------------------------------------------------------------------------------
// Declaration
//------------------------------------------------------------------------------
class PostMasterTest: public CppUnit::TestCase
{
  public:
    CPPUNIT_TEST_SUITE( PostMasterTest );
      CPPUNIT_TEST( FunctionalTest );
    CPPUNIT_TEST_SUITE_END();
    void FunctionalTest();
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
      pStreamId[0] = id0;
      pStreamId[1] = id1;
    }

    virtual bool Filter( const XrdClient::Message *msg )
    {
      ServerResponse *resp = (ServerResponse *)msg->GetBuffer();
      if( resp->hdr.streamid[0] == pStreamId[0] &&
          resp->hdr.streamid[1] == pStreamId[1] )
        return true;
      return false;
    }

    char pStreamId[2];
};

//------------------------------------------------------------------------------
// Test the functionality of a poller
//------------------------------------------------------------------------------
void PostMasterTest::FunctionalTest()
{
  using namespace XrdClient;
  PostMaster postMaster;
  postMaster.Initialize();
  postMaster.Start();

  Message   m1, *m2 = 0;
  XrdFilter f1( 1, 2 );
  URL       localhost( "root://localhost" );

  m1.Allocate( sizeof( ClientPingRequest ) );

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

  postMaster.Stop();
  postMaster.Finalize();
}
