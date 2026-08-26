// Copyright (c) 2026 by the XRootD Collaboration.
// Licensed under the GNU Lesser General Public License, version 3 or later.

#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClPlugInInterface.hh"
#include "XrdCl/XrdClPlugInManager.hh"

#include <gtest/gtest.h>
#include <deque>
#include <memory>

using namespace XrdCl;

namespace
{
struct ListingState
{
  int live = 0;
  std::string failPath;
  bool requireProperty = false;
  std::deque<std::pair<std::string, ResponseHandler *>> requests;
};

class ListingPlugIn : public FileSystemPlugIn
{
public:
  explicit ListingPlugIn( ListingState &state ) : state( state )
  { ++state.live; }
  ~ListingPlugIn() override { --state.live; }

  XRootDStatus DirList( const std::string &path, DirListFlags::Flags flags,
                       ResponseHandler *handler, time_t ) override
  {
    EXPECT_FALSE( flags & DirListFlags::Recursive );
    if( path == state.failPath || ( state.requireProperty && property != "token" ) )
      return XRootDStatus( stError, errOSError );
    state.requests.emplace_back( path, handler );
    return XRootDStatus();
  }

  bool SetProperty( const std::string &name, const std::string &value ) override
  {
    if( name != "TestCredential" ) return false;
    property = value;
    return true;
  }

private:
  ListingState &state;
  std::string property;
};

class ListingFactory : public PlugInFactory
{
public:
  explicit ListingFactory( ListingState &state ) : state( state ) {}
  FilePlugIn *CreateFile( const std::string & ) override { return nullptr; }
  FileSystemPlugIn *CreateFileSystem( const std::string & ) override
  { return new ListingPlugIn( state ); }
private:
  ListingState &state;
};

class ListingHandler : public ResponseHandler
{
public:
  void HandleResponse( XRootDStatus *st, AnyObject *obj ) override
  {
    ++calls;
    status.reset( st );
    response.reset( obj );
  }
  int calls = 0;
  std::unique_ptr<XRootDStatus> status;
  std::unique_ptr<AnyObject> response;
};

class DirListTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    ASSERT_TRUE( DefaultEnv::GetPlugInManager()->RegisterFactory(
        "http://dirlist.test", new ListingFactory( state ) ) );
    ASSERT_TRUE( DefaultEnv::GetPlugInManager()->RegisterFactory(
        "root://dirlist.test", new ListingFactory( state ) ) );
  }
  void TearDown() override
  {
    EXPECT_EQ( state.live, 0 );
    EXPECT_TRUE( state.requests.empty() );
    DefaultEnv::GetPlugInManager()->RegisterFactory(
        "http://dirlist.test", nullptr );
    DefaultEnv::GetPlugInManager()->RegisterFactory(
        "root://dirlist.test", nullptr );
  }

  void Respond( const std::string &name, bool directory, bool duplicate = false )
  {
    ASSERT_FALSE( state.requests.empty() );
    auto request = state.requests.front();
    state.requests.pop_front();
    auto list = new DirectoryList();
    // A plugin supplies the requested parent without adding a slash itself.
    list->SetParentName( request.first );
    auto flags = directory ? StatInfo::IsDir : StatInfo::IsReadable;
    list->Add( new DirectoryList::ListEntry(
        "dirlist.test", name, new StatInfo( "id", 1, flags, 0 ) ) );
    if( duplicate )
      list->Add( new DirectoryList::ListEntry(
          "dirlist.test", name, new StatInfo( "id", 1, flags, 0 ) ) );
    auto response = new AnyObject();
    response->Set( list );
    request.second->HandleResponse( new XRootDStatus(), response );
  }

  ListingState state;
};

TEST_F( DirListTest, PreserveQueryAndNormalizeParents )
{
  FileSystem fs( URL( "http://dirlist.test" ) );
  for( const std::string query : { "", "?authz=test/value&other=1" } )
  {
    for( const std::string slash : { "", "/" } )
    {
      ListingHandler handler;
      auto st = fs.DirList( "/dir" + slash + query,
          DirListFlags::Recursive | DirListFlags::Merge, &handler );
      ASSERT_TRUE( st.IsOK() );
      Respond( "nested", true );
      ASSERT_EQ( state.requests.size(), 1u );
      EXPECT_EQ( state.requests.front().first, "/dir/nested" + query );
      Respond( "file", false );
      ASSERT_EQ( handler.calls, 1 );
      ASSERT_TRUE( handler.status->IsOK() );
      DirectoryList *list = nullptr;
      handler.response->Get( list );
      ASSERT_NE( list, nullptr );
      EXPECT_EQ( list->GetParentName(), "/dir/" );
      ASSERT_EQ( list->GetSize(), 2u );
      EXPECT_EQ( list->At( 1 )->GetName(), "nested/file" );
      EXPECT_EQ( state.live, 1 );
    }
  }
}

TEST_F( DirListTest, SynchronousFailureReleasesRecursiveState )
{
  FileSystem fs( URL( "http://dirlist.test" ) );
  state.failPath = "/dir";
  for( auto flags : { DirListFlags::Recursive, DirListFlags::Merge,
                     DirListFlags::Recursive | DirListFlags::Merge } )
  {
    ListingHandler handler;
    auto st = fs.DirList( "/dir", flags, &handler );
    EXPECT_FALSE( st.IsOK() );
    EXPECT_EQ( handler.calls, 0 );
    EXPECT_EQ( state.live, 1 );
  }
}

TEST_F( DirListTest, MergePluginDuplicates )
{
  FileSystem fs( URL( "http://dirlist.test" ) );
  ListingHandler handler;
  auto st = fs.DirList( "/dir", DirListFlags::Merge, &handler );
  ASSERT_TRUE( st.IsOK() );
  Respond( "file", false, true );
  ASSERT_EQ( handler.calls, 1 );
  ASSERT_TRUE( handler.status->IsOK() );
  DirectoryList *list = nullptr;
  handler.response->Get( list );
  ASSERT_NE( list, nullptr );
  EXPECT_EQ( list->GetSize(), 1u );
}

TEST_F( DirListTest, SynchronousChildFailureCompletesRequest )
{
  FileSystem fs( URL( "http://dirlist.test" ) );
  ListingHandler handler;
  state.failPath = "/dir/nested?authz=test";
  auto st = fs.DirList( "/dir?authz=test",
      DirListFlags::Recursive | DirListFlags::Merge, &handler );
  ASSERT_TRUE( st.IsOK() );
  Respond( "nested", true );
  ASSERT_EQ( handler.calls, 1 );
  EXPECT_EQ( handler.status->code, suPartial );
  EXPECT_EQ( state.live, 1 );
}

TEST_F( DirListTest, ChildRequestsKeepInstancePropertiesAfterCallerDestruction )
{
  state.requireProperty = true;
  auto fs = std::make_unique<FileSystem>( URL( "http://dirlist.test" ) );
  ASSERT_TRUE( fs->SetProperty( "TestCredential", "token" ) );
  ListingHandler handler;
  auto st = fs->DirList( "/dir", DirListFlags::Recursive, &handler );
  ASSERT_TRUE( st.IsOK() );
  fs.reset();
  Respond( "nested", true );
  ASSERT_EQ( state.requests.size(), 1u );
  Respond( "file", false );
  ASSERT_EQ( handler.calls, 1 );
  EXPECT_TRUE( handler.status->IsOK() );
  EXPECT_EQ( handler.status->code, suDone );
}

TEST_F( DirListTest, RejectTraversalNamesBeforeRecursing )
{
  FileSystem fs( URL( "http://dirlist.test" ) );
  for( const char *name : { "..", "../escaped", "nested/escaped",
                           "\\\\escaped", "run#1.root", "" } )
  {
    ListingHandler handler;
    auto st = fs.DirList( "/dir", DirListFlags::Recursive, &handler );
    ASSERT_TRUE( st.IsOK() );
    Respond( name, true );
    ASSERT_EQ( handler.calls, 1 );
    EXPECT_EQ( handler.status->code, suPartial );
    EXPECT_TRUE( state.requests.empty() );
    DirectoryList *list = nullptr;
    handler.response->Get( list );
    ASSERT_NE( list, nullptr );
    EXPECT_EQ( list->GetSize(), 0u );
  }
}

TEST_F( DirListTest, PreserveNativeNamesWithHashAndBackslash )
{
  FileSystem fs( URL( "root://dirlist.test" ) );
  for( const char *name : { "run#1.root", "run\\1.root" } )
  {
    ListingHandler handler;
    auto st = fs.DirList( "/dir", DirListFlags::Recursive, &handler );
    ASSERT_TRUE( st.IsOK() );
    Respond( name, false );
    ASSERT_EQ( handler.calls, 1 );
    ASSERT_TRUE( handler.status->IsOK() );
    DirectoryList *list = nullptr;
    handler.response->Get( list );
    ASSERT_NE( list, nullptr );
    ASSERT_EQ( list->GetSize(), 1u );
    EXPECT_EQ( list->At( 0 )->GetName(), name );
    URL child( "root://dirlist.test" );
    child.SetPath( "/dir/" + std::string( name ) );
    EXPECT_EQ( URL( child.GetURL() ).GetPath(), child.GetPath() );
  }
}
}
