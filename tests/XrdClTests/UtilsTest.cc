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
      CPPUNIT_TEST( AnyTest );
      CPPUNIT_TEST( TaskManagerTest );
      CPPUNIT_TEST( SIDManagerTest );
      CPPUNIT_TEST( PropertyListTest );
    CPPUNIT_TEST_SUITE_END();
    void AnyTest();
    void TaskManagerTest();
    void SIDManagerTest();
    void PropertyListTest();
};

CPPUNIT_TEST_SUITE_REGISTRATION( UtilsTest );

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
  std::shared_ptr<SIDManager> manager = SIDMgrPool::Instance().GetSIDMgr( "root://fake:1094//dir/file" );

  uint8_t sid1[2];
  uint8_t sid2[2];
  uint8_t sid3[2];
  uint8_t sid4[2];
  uint8_t sid5[2];

  CPPUNIT_ASSERT_XRDST( manager->AllocateSID( sid1 ) );
  CPPUNIT_ASSERT_XRDST( manager->AllocateSID( sid2 ) );
  manager->ReleaseSID( sid2 );
  CPPUNIT_ASSERT_XRDST( manager->AllocateSID( sid3 ) );
  CPPUNIT_ASSERT_XRDST( manager->AllocateSID( sid4 ) );
  CPPUNIT_ASSERT_XRDST( manager->AllocateSID( sid5 ) );

  CPPUNIT_ASSERT( (sid1[0] != sid2[0]) || (sid1[1] != sid2[1]) );
  CPPUNIT_ASSERT( manager->NumberOfTimedOutSIDs() == 0 );
  manager->TimeOutSID( sid4 );
  manager->TimeOutSID( sid5 );
  CPPUNIT_ASSERT( manager->NumberOfTimedOutSIDs() == 2 );
  CPPUNIT_ASSERT( manager->IsTimedOut( sid3 ) == false );
  CPPUNIT_ASSERT( manager->IsTimedOut( sid1 ) == false );
  CPPUNIT_ASSERT( manager->IsTimedOut( sid4 ) == true );
  CPPUNIT_ASSERT( manager->IsTimedOut( sid5 ) == true );
  manager->ReleaseTimedOut( sid5 );
  CPPUNIT_ASSERT( manager->IsTimedOut( sid5 ) == false );
  manager->ReleaseAllTimedOut();
  CPPUNIT_ASSERT( manager->NumberOfTimedOutSIDs() == 0 );
}

//------------------------------------------------------------------------------
// Property List test
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
