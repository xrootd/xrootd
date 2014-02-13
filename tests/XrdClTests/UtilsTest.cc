//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
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
#include "CppUnitXrdHelpers.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClAnyObject.hh"
#include "XrdCl/XrdClTaskManager.hh"
#include "XrdCl/XrdClSIDManager.hh"
#include "XrdCl/XrdClPropertyList.hh"

//------------------------------------------------------------------------------
// Declaration
//------------------------------------------------------------------------------
class UtilsTest: public CppUnit::TestCase
{
  public:
    CPPUNIT_TEST_SUITE( UtilsTest );
      CPPUNIT_TEST( URLTest );
      CPPUNIT_TEST( AnyTest );
      CPPUNIT_TEST( TaskManagerTest );
      CPPUNIT_TEST( SIDManagerTest );
      CPPUNIT_TEST( PropertyListTest );
    CPPUNIT_TEST_SUITE_END();
    void URLTest();
    void AnyTest();
    void TaskManagerTest();
    void SIDManagerTest();
    void PropertyListTest();
};

CPPUNIT_TEST_SUITE_REGISTRATION( UtilsTest );

//------------------------------------------------------------------------------
// URL test
//------------------------------------------------------------------------------
void UtilsTest::URLTest()
{
  XrdCl::URL url1( "root://user1:passwd1@host1:123//path?param1=val1&param2=val2" );
  XrdCl::URL url2( "root://user1@host1//path?param1=val1&param2=val2" );
  XrdCl::URL url3( "root://host1" );
  XrdCl::URL url4( "root://user1:passwd1@[::1]:123//path?param1=val1&param2=val2" );
  XrdCl::URL url5( "root://user1@192.168.1.1:123//path?param1=val1&param2=val2" );
  XrdCl::URL url6( "root://[::1]" );
  XrdCl::URL url7( "root://lxfsra02a08.cern.ch:1095//eos/dev/SMWZd3pdExample_NTUP_SMWZ.526666._000073.root.1?&cap.sym=sfdDqALWo3W3tWUJ2O5XwQ5GG8U=&cap.msg=eGj/mh+9TrecFBAZBNr/nLau4p0kjlEOjc1JC+9DVjLL1Tq+g311485W0baMBAsM#W8lNFdVQcKNAu8K5yVskIcLDOEi6oNpvoxDA1DN4oCxtHR6LkOWhO91MLn/ZosJ5#Dc7aeBCIz/kKs261mnL4dJeUu6r25acCn4vhyp8UKyL1cVmmnyBnjqe6tz28qFO2#0fQHrHf6Z9N0MNhw1fplYjpGeNwFH2jQSfSo24zSZKGa/PKClGYnXoXBWDGU1spm#kJsGGrErhBHYvLq3eS+jEBr8l+c1BhCQU7ZaLZiyaKOnspYnR/Tw7bMrooWMh7eL#&mgm.logid=766877e6-9874-11e1-a77f-003048cf8cd8&mgm.recdcdcdcdplicaindex=0&mgm.replicahead=0" );
  XrdCl::URL url8( "/etc/passwd" );
  XrdCl::URL url9( "localhost:1094//data/cb4aacf1-6f28-42f2-b68a-90a73460f424.dat" );
  XrdCl::URL url10( "localhost:1094/?test=123" );

  XrdCl::URL urlInvalid1( "root://user1:passwd1@host1:asd//path?param1=val1&param2=val2" );
  XrdCl::URL urlInvalid2( "root://user1:passwd1host1:123//path?param1=val1&param2=val2" );
  XrdCl::URL urlInvalid3( "root:////path?param1=val1&param2=val2" );
  XrdCl::URL urlInvalid4( "root://@//path?param1=val1&param2=val2" );
  XrdCl::URL urlInvalid5( "root://:@//path?param1=val1&param2=val2" );
  XrdCl::URL urlInvalid6( "root://" );
  XrdCl::URL urlInvalid7( "://asds" );
  XrdCl::URL urlInvalid8( "root://asd@://path?param1=val1&param2=val2" );

  //----------------------------------------------------------------------------
  // Full url
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT( url1.IsValid() == true );
  CPPUNIT_ASSERT( url1.GetProtocol() == "root" );
  CPPUNIT_ASSERT( url1.GetUserName() == "user1" );
  CPPUNIT_ASSERT( url1.GetPassword() == "passwd1" );
  CPPUNIT_ASSERT( url1.GetHostName() == "host1" );
  CPPUNIT_ASSERT( url1.GetPort() == 123 );
  CPPUNIT_ASSERT( url1.GetPathWithParams() == "/path?param1=val1&param2=val2" );
  CPPUNIT_ASSERT( url1.GetPath() == "/path" );
  CPPUNIT_ASSERT( url1.GetParams().size() == 2 );

  XrdCl::URL::ParamsMap::const_iterator it;
  it = url1.GetParams().find( "param1" );
  CPPUNIT_ASSERT( it != url1.GetParams().end() );
  CPPUNIT_ASSERT( it->second == "val1" );
  it = url1.GetParams().find( "param2" );
  CPPUNIT_ASSERT( it != url1.GetParams().end() );
  CPPUNIT_ASSERT( it->second == "val2" );
  it = url1.GetParams().find( "param3" );
  CPPUNIT_ASSERT( it == url1.GetParams().end() );

  //----------------------------------------------------------------------------
  // No password, no port
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT( url2.IsValid() == true );
  CPPUNIT_ASSERT( url2.GetProtocol() == "root" );
  CPPUNIT_ASSERT( url2.GetUserName() == "user1" );
  CPPUNIT_ASSERT( url2.GetPassword() == "" );
  CPPUNIT_ASSERT( url2.GetHostName() == "host1" );
  CPPUNIT_ASSERT( url2.GetPort() == 1094 );
  CPPUNIT_ASSERT( url2.GetPath() == "/path" );
  CPPUNIT_ASSERT( url2.GetPathWithParams() == "/path?param1=val1&param2=val2" );
  CPPUNIT_ASSERT( url1.GetParams().size() == 2 );

  it = url2.GetParams().find( "param1" );
  CPPUNIT_ASSERT( it != url2.GetParams().end() );
  CPPUNIT_ASSERT( it->second == "val1" );
  it = url2.GetParams().find( "param2" );
  CPPUNIT_ASSERT( it != url2.GetParams().end() );
  CPPUNIT_ASSERT( it->second == "val2" );
  it = url2.GetParams().find( "param3" );
  CPPUNIT_ASSERT( it == url2.GetParams().end() );

  //----------------------------------------------------------------------------
  // Just the host and the protocol
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT( url3.IsValid() == true );
  CPPUNIT_ASSERT( url3.GetProtocol() == "root" );
  CPPUNIT_ASSERT( url3.GetUserName() == "" );
  CPPUNIT_ASSERT( url3.GetPassword() == "" );
  CPPUNIT_ASSERT( url3.GetHostName() == "host1" );
  CPPUNIT_ASSERT( url3.GetPort() == 1094 );
  CPPUNIT_ASSERT( url3.GetPath() == "" );
  CPPUNIT_ASSERT( url3.GetPathWithParams() == "" );
  CPPUNIT_ASSERT( url3.GetParams().size() == 0 );

  //----------------------------------------------------------------------------
  // Full url - IPv6
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT( url4.IsValid() == true );
  CPPUNIT_ASSERT( url4.GetProtocol() == "root" );
  CPPUNIT_ASSERT( url4.GetUserName() == "user1" );
  CPPUNIT_ASSERT( url4.GetPassword() == "passwd1" );
  CPPUNIT_ASSERT( url4.GetHostName() == "[::1]" );
  CPPUNIT_ASSERT( url4.GetPort() == 123 );
  CPPUNIT_ASSERT( url4.GetPathWithParams() == "/path?param1=val1&param2=val2" );
  CPPUNIT_ASSERT( url4.GetPath() == "/path" );
  CPPUNIT_ASSERT( url4.GetParams().size() == 2 );

  it = url4.GetParams().find( "param1" );
  CPPUNIT_ASSERT( it != url4.GetParams().end() );
  CPPUNIT_ASSERT( it->second == "val1" );
  it = url4.GetParams().find( "param2" );
  CPPUNIT_ASSERT( it != url4.GetParams().end() );
  CPPUNIT_ASSERT( it->second == "val2" );
  it = url4.GetParams().find( "param3" );
  CPPUNIT_ASSERT( it == url4.GetParams().end() );

  //----------------------------------------------------------------------------
  // No password, no port
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT( url5.IsValid() == true );
  CPPUNIT_ASSERT( url5.GetProtocol() == "root" );
  CPPUNIT_ASSERT( url5.GetUserName() == "user1" );
  CPPUNIT_ASSERT( url5.GetPassword() == "" );
  CPPUNIT_ASSERT( url5.GetHostName() == "192.168.1.1" );
  CPPUNIT_ASSERT( url5.GetPort() == 123 );
  CPPUNIT_ASSERT( url5.GetPath() == "/path" );
  CPPUNIT_ASSERT( url5.GetPathWithParams() == "/path?param1=val1&param2=val2" );
  CPPUNIT_ASSERT( url5.GetParams().size() == 2 );

  it = url5.GetParams().find( "param1" );
  CPPUNIT_ASSERT( it != url5.GetParams().end() );
  CPPUNIT_ASSERT( it->second == "val1" );
  it = url5.GetParams().find( "param2" );
  CPPUNIT_ASSERT( it != url2.GetParams().end() );
  CPPUNIT_ASSERT( it->second == "val2" );
  it = url5.GetParams().find( "param3" );
  CPPUNIT_ASSERT( it == url5.GetParams().end() );

  //----------------------------------------------------------------------------
  // Just the host and the protocol
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT( url6.IsValid() == true );
  CPPUNIT_ASSERT( url6.GetProtocol() == "root" );
  CPPUNIT_ASSERT( url6.GetUserName() == "" );
  CPPUNIT_ASSERT( url6.GetPassword() == "" );
  CPPUNIT_ASSERT( url6.GetHostName() == "[::1]" );
  CPPUNIT_ASSERT( url6.GetPort() == 1094 );
  CPPUNIT_ASSERT( url6.GetPath() == "" );
  CPPUNIT_ASSERT( url6.GetPathWithParams() == "" );
  CPPUNIT_ASSERT( url6.GetParams().size() == 0 );

  //----------------------------------------------------------------------------
  // Local file
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT( url8.IsValid() == true );
  CPPUNIT_ASSERT( url8.GetProtocol() == "file" );
  CPPUNIT_ASSERT( url8.GetUserName() == "" );
  CPPUNIT_ASSERT( url8.GetPassword() == "" );
  CPPUNIT_ASSERT( url8.GetHostName() == "" );
  CPPUNIT_ASSERT( url8.GetPath() == "/etc/passwd" );

  CPPUNIT_ASSERT( url8.GetPathWithParams() == "/etc/passwd" );
  CPPUNIT_ASSERT( url8.GetParams().size() == 0 );

  //----------------------------------------------------------------------------
  // URL without a protocol spec
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT( url9.IsValid() == true );
  CPPUNIT_ASSERT( url9.GetProtocol() == "root" );
  CPPUNIT_ASSERT( url9.GetUserName() == "" );
  CPPUNIT_ASSERT( url9.GetPassword() == "" );
  CPPUNIT_ASSERT( url9.GetHostName() == "localhost" );
  CPPUNIT_ASSERT( url9.GetPort() == 1094 );
  CPPUNIT_ASSERT( url9.GetPath() == "/data/cb4aacf1-6f28-42f2-b68a-90a73460f424.dat" );

  CPPUNIT_ASSERT( url9.GetPathWithParams() == "/data/cb4aacf1-6f28-42f2-b68a-90a73460f424.dat" );
  CPPUNIT_ASSERT( url9.GetParams().size() == 0 );

  //----------------------------------------------------------------------------
  // URL cgi without path
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT( url10.IsValid() == true );
  CPPUNIT_ASSERT( url10.GetProtocol() == "root" );
  CPPUNIT_ASSERT( url10.GetUserName() == "" );
  CPPUNIT_ASSERT( url10.GetPassword() == "" );
  CPPUNIT_ASSERT( url10.GetHostName() == "localhost" );
  CPPUNIT_ASSERT( url10.GetPort() == 1094 );
  CPPUNIT_ASSERT( url10.GetPath() == "" );

  CPPUNIT_ASSERT( url10.GetParams().size() == 1 );
  CPPUNIT_ASSERT( url10.GetParamsAsString() == "?test=123" );

  //----------------------------------------------------------------------------
  // Bunch od invalid ones
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT( urlInvalid1.IsValid() == false );
  CPPUNIT_ASSERT( urlInvalid2.IsValid() == false );
  CPPUNIT_ASSERT( urlInvalid3.IsValid() == false );
  CPPUNIT_ASSERT( urlInvalid4.IsValid() == false );
  CPPUNIT_ASSERT( urlInvalid5.IsValid() == false );
  CPPUNIT_ASSERT( urlInvalid6.IsValid() == false );
  CPPUNIT_ASSERT( urlInvalid7.IsValid() == false );
  CPPUNIT_ASSERT( urlInvalid8.IsValid() == false );
}

class A
{
  public:
    A( bool &st ): a(0.0), stat(st) {}
    ~A() { stat = true; }
    double  a;
    bool   &stat;
};

class B
{
  public:
    int b;
};

//------------------------------------------------------------------------------
// Any test
//------------------------------------------------------------------------------
void UtilsTest::AnyTest()
{
  bool destructorCalled1 = false;
  bool destructorCalled2 = false;
  bool destructorCalled3 = false;
  A *a1 = new A( destructorCalled1 );
  A *a2 = new A( destructorCalled2 );
  A *a3 = new A( destructorCalled3 );
  A *a4 = 0;
  B *b  = 0;

  XrdCl::AnyObject *any1 = new XrdCl::AnyObject();
  XrdCl::AnyObject *any2 = new XrdCl::AnyObject();
  XrdCl::AnyObject *any3 = new XrdCl::AnyObject();
  XrdCl::AnyObject *any4 = new XrdCl::AnyObject();

  any1->Set( a1 );
  any1->Get( b );
  any1->Get( a4 );
  CPPUNIT_ASSERT( !b );
  CPPUNIT_ASSERT( a4 );
  CPPUNIT_ASSERT( any1->HasOwnership() );

  delete any1;
  CPPUNIT_ASSERT( destructorCalled1 );

  any2->Set( a2 );
  any2->Set( (int*)0 );
  delete any2;
  CPPUNIT_ASSERT( !destructorCalled2 );
  delete a2;

  any3->Set( a3, false );
  CPPUNIT_ASSERT( !any3->HasOwnership() );
  delete any3;
  CPPUNIT_ASSERT( !destructorCalled3 );
  delete a3;

  // test destruction of an empty object
  delete any4;
}

//------------------------------------------------------------------------------
// Some tasks that do something
//------------------------------------------------------------------------------
class TestTask1: public XrdCl::Task
{
  public:
    TestTask1( std::vector<time_t> &runs ): pRuns( runs )
    {
      SetName( "TestTask1" );
    }
    virtual time_t Run( time_t now )
    {
      pRuns.push_back( now );
      return 0;
    }
  private:
    std::vector<time_t> &pRuns;
};

class TestTask2: public XrdCl::Task
{
  public:
    TestTask2( std::vector<time_t> &runs ): pRuns( runs )
    {
      SetName( "TestTask2" );
    }

    virtual time_t Run( time_t now )
    {
      pRuns.push_back( now );
      if( pRuns.size() >= 5 )
        return 0;
      return now+2;
    }
  private:
    std::vector<time_t> &pRuns;
};

//------------------------------------------------------------------------------
// Task Manager test
//------------------------------------------------------------------------------
void UtilsTest::TaskManagerTest()
{
  using namespace XrdCl;

  std::vector<time_t> runs1, runs2;
  Task *tsk1 = new TestTask1( runs1 );
  Task *tsk2 = new TestTask2( runs2 );

  TaskManager taskMan;
  CPPUNIT_ASSERT( taskMan.Start() );

  time_t now = ::time(0);
  taskMan.RegisterTask( tsk1, now+2 );
  taskMan.RegisterTask( tsk2, now+1 );

  ::sleep( 6 );
  taskMan.UnregisterTask( tsk2 );

  ::sleep( 2 );

  CPPUNIT_ASSERT( runs1.size() == 1 );
  CPPUNIT_ASSERT( runs2.size() == 3 );
  CPPUNIT_ASSERT( taskMan.Stop() );
}

//------------------------------------------------------------------------------
// SID Manager test
//------------------------------------------------------------------------------
void UtilsTest::SIDManagerTest()
{
  using namespace XrdCl;
  SIDManager manager;

  uint8_t sid1[2];
  uint8_t sid2[2];
  uint8_t sid3[2];
  uint8_t sid4[2];
  uint8_t sid5[2];

  CPPUNIT_ASSERT_XRDST( manager.AllocateSID( sid1 ) );
  CPPUNIT_ASSERT_XRDST( manager.AllocateSID( sid2 ) );
  manager.ReleaseSID( sid2 );
  CPPUNIT_ASSERT_XRDST( manager.AllocateSID( sid3 ) );
  CPPUNIT_ASSERT_XRDST( manager.AllocateSID( sid4 ) );
  CPPUNIT_ASSERT_XRDST( manager.AllocateSID( sid5 ) );

  CPPUNIT_ASSERT( (sid1[0] != sid2[0]) || (sid1[1] != sid2[1]) );
  CPPUNIT_ASSERT( manager.NumberOfTimedOutSIDs() == 0 );
  manager.TimeOutSID( sid4 );
  manager.TimeOutSID( sid5 );
  CPPUNIT_ASSERT( manager.NumberOfTimedOutSIDs() == 2 );
  CPPUNIT_ASSERT( manager.IsTimedOut( sid3 ) == false );
  CPPUNIT_ASSERT( manager.IsTimedOut( sid1 ) == false );
  CPPUNIT_ASSERT( manager.IsTimedOut( sid4 ) == true );
  CPPUNIT_ASSERT( manager.IsTimedOut( sid5 ) == true );
  manager.ReleaseTimedOut( sid5 );
  CPPUNIT_ASSERT( manager.IsTimedOut( sid5 ) == false );
  manager.ReleaseAllTimedOut();
  CPPUNIT_ASSERT( manager.NumberOfTimedOutSIDs() == 0 );
}

//------------------------------------------------------------------------------
// SID Manager test
//------------------------------------------------------------------------------
void UtilsTest::PropertyListTest()
{
  using namespace XrdCl;
  PropertyList l;
  l.Set( "s1", "test string 1" );
  l.Set( "i1", 123456789123ULL );

  uint64_t i1;
  std::string s1;

  CPPUNIT_ASSERT( l.Get( "s1", s1 ) );
  CPPUNIT_ASSERT( s1 == "test string 1" );
  CPPUNIT_ASSERT( l.Get( "i1", i1 ) );
  CPPUNIT_ASSERT( i1 == 123456789123ULL );
  CPPUNIT_ASSERT( l.HasProperty( "s1" ) );
  CPPUNIT_ASSERT( !l.HasProperty( "s2" ) );
  CPPUNIT_ASSERT( l.HasProperty( "i1" ) );

  for( int i = 0; i < 1000; ++i )
    l.Set( "vect_int", i, i+1000 );

  int i;
  int num;
  for( i = 0; l.HasProperty( "vect_int", i ); ++i )
  {
    CPPUNIT_ASSERT( l.Get( "vect_int", i, num ) );
    CPPUNIT_ASSERT( num = i+1000 );
  }
  CPPUNIT_ASSERT( i == 1000 );

  XRootDStatus st1, st2;
  st1.SetErrorMessage( "test error message" );
  l.Set( "status", st1 );
  CPPUNIT_ASSERT( l.Get( "status", st2 ) );
  CPPUNIT_ASSERT( st2.status == st1.status );
  CPPUNIT_ASSERT( st2.code   == st1.code );
  CPPUNIT_ASSERT( st2.errNo  == st1.errNo );
  CPPUNIT_ASSERT( st2.GetErrorMessage() == st1.GetErrorMessage() );

  std::vector<std::string> v1, v2;
  v1.push_back( "test string 1" );
  v1.push_back( "test string 2" );
  v1.push_back( "test string 3" );
  l.Set( "vector", v1 );
  CPPUNIT_ASSERT( l.Get( "vector", v2 ) );
  for( size_t i = 0; i < v1.size(); ++i )
    CPPUNIT_ASSERT( v1[i] == v2[i] );
}
