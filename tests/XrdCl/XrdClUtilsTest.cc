//------------------------------------------------------------------------------
// Copyright (c) 2023 by European Organization for Nuclear Research (CERN)
// Author: Angelo Galavotti <agalavottib@gmail.com>
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

#include <gtest/gtest.h>
#include "XrdCl/XrdClAnyObject.hh"
#include "GTestXrdHelpers.hh"
#include "XrdCl/XrdClTaskManager.hh"
#include "XrdCl/XrdClSIDManager.hh"
#include "XrdCl/XrdClPropertyList.hh"

//------------------------------------------------------------------------------
// Declaration
//------------------------------------------------------------------------------

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
// UtilityTest class declaration
//------------------------------------------------------------------------------
class UtilsTest : public ::testing::Test {};

//------------------------------------------------------------------------------
// Any test
//------------------------------------------------------------------------------
TEST(UtilsTest, AnyTest)
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
  EXPECT_TRUE( !b );
  EXPECT_TRUE( a4 );
  EXPECT_TRUE( any1->HasOwnership() );

  delete any1;
  EXPECT_TRUE( destructorCalled1 );

  any2->Set( a2 );
  any2->Set( (int*)0 );
  delete any2;
  EXPECT_TRUE( !destructorCalled2 );
  delete a2;

  any3->Set( a3, false );
  EXPECT_TRUE( !any3->HasOwnership() );
  delete any3;
  EXPECT_TRUE( !destructorCalled3 );
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
TEST(UtilsTest, TaskManagerTest)
{
  using namespace XrdCl;

  std::vector<time_t> runs1, runs2;
  Task *tsk1 = new TestTask1( runs1 );
  Task *tsk2 = new TestTask2( runs2 );

  TaskManager taskMan;
  EXPECT_TRUE( taskMan.Start() );

  time_t now = ::time(0);
  taskMan.RegisterTask( tsk1, now+2 );
  taskMan.RegisterTask( tsk2, now+1 );

  ::sleep( 6 );
  taskMan.UnregisterTask( tsk2 );
  ::sleep( 1 );

  EXPECT_EQ( runs1.size(), 1 );
  EXPECT_EQ( runs2.size(), 3 );
  EXPECT_TRUE( taskMan.Stop() );
}

//------------------------------------------------------------------------------
// SID Manager test
//------------------------------------------------------------------------------
TEST(UtilsTest, SIDManagerTest)
{
  using namespace XrdCl;
  std::shared_ptr<SIDManager> manager = SIDMgrPool::Instance().GetSIDMgr( "root://fake:1094//dir/file" );

  uint8_t sid1[2];
  uint8_t sid2[2];
  uint8_t sid3[2];
  uint8_t sid4[2];
  uint8_t sid5[2];

  GTEST_ASSERT_XRDST( manager->AllocateSID( sid1 ) );
  GTEST_ASSERT_XRDST( manager->AllocateSID( sid2 ) );
  manager->ReleaseSID( sid2 );
  GTEST_ASSERT_XRDST( manager->AllocateSID( sid3 ) );
  GTEST_ASSERT_XRDST( manager->AllocateSID( sid4 ) );
  GTEST_ASSERT_XRDST( manager->AllocateSID( sid5 ) );

  EXPECT_TRUE( (sid1[0] != sid2[0]) || (sid1[1] != sid2[1]) );
  EXPECT_EQ( manager->NumberOfTimedOutSIDs(), 0 );
  manager->TimeOutSID( sid4 );
  manager->TimeOutSID( sid5 );
  EXPECT_EQ( manager->NumberOfTimedOutSIDs(), 2 );
  EXPECT_FALSE( manager->IsTimedOut( sid3 ) );
  EXPECT_FALSE( manager->IsTimedOut( sid1 ) );
  EXPECT_TRUE( manager->IsTimedOut( sid4 ) );
  EXPECT_TRUE( manager->IsTimedOut( sid5 ) );
  manager->ReleaseTimedOut( sid5 );
  EXPECT_FALSE( manager->IsTimedOut( sid5 ) );
  manager->ReleaseAllTimedOut();
  EXPECT_EQ( manager->NumberOfTimedOutSIDs(), 0 );
}

//------------------------------------------------------------------------------
// Property List test
//------------------------------------------------------------------------------
TEST(UtilsTest, PropertyListTest)
{
  using namespace XrdCl;
  PropertyList l;
  l.Set( "s1", "test string 1" );
  l.Set( "i1", 123456789123ULL );

  uint64_t i1;
  std::string s1;

  EXPECT_TRUE( l.Get( "s1", s1 ) );
  EXPECT_EQ( s1, "test string 1" );
  EXPECT_TRUE( l.Get( "i1", i1 ) );
  EXPECT_EQ( i1, 123456789123ULL );
  EXPECT_TRUE( l.HasProperty( "s1" ) );
  EXPECT_TRUE( !l.HasProperty( "s2" ) );
  EXPECT_TRUE( l.HasProperty( "i1" ) );

  for( int i = 0; i < 1000; ++i )
    l.Set( "vect_int", i, i+1000 );

  int i;
  int num;
  for( i = 0; l.HasProperty( "vect_int", i ); ++i )
  {
    EXPECT_TRUE( l.Get( "vect_int", i, num ) );
    EXPECT_TRUE( num = i+1000 );
  }
  EXPECT_EQ( i, 1000 );

  XRootDStatus st1, st2;
  st1.SetErrorMessage( "test error message" );
  l.Set( "status", st1 );
  EXPECT_TRUE( l.Get( "status", st2 ) );
  EXPECT_EQ( st2.status, st1.status );
  EXPECT_EQ( st2.code  , st1.code );
  EXPECT_EQ( st2.errNo , st1.errNo );
  EXPECT_EQ( st2.GetErrorMessage(), st1.GetErrorMessage() );

  std::vector<std::string> v1, v2;
  v1.push_back( "test string 1" );
  v1.push_back( "test string 2" );
  v1.push_back( "test string 3" );
  l.Set( "vector", v1 );
  EXPECT_TRUE( l.Get( "vector", v2 ) );
  for( size_t i = 0; i < v1.size(); ++i )
    EXPECT_EQ( v1[i], v2[i] );
}
