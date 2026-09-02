//------------------------------------------------------------------------------
// Copyright (c) 2026 by European Organization for Nuclear Research (CERN)
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClFSRemove.hh"

#include "XProtocol/XProtocol.hh"

#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
  XrdCl::XRootDStatus DirectoryStatus()
  {
    return XrdCl::XRootDStatus( XrdCl::stError,
                                XrdCl::errErrorResponse,
                                kXR_isDirectory );
  }

  XrdCl::XRootDStatus ErrorStatus( unsigned int error )
  {
    return XrdCl::XRootDStatus( XrdCl::stError,
                                XrdCl::errErrorResponse, error );
  }

  XrdCl::RecursiveRemoveOperations NeverCalledOperations( int &calls )
  {
    XrdCl::RecursiveRemoveOperations operations;
    operations.remove = [&calls]( const std::string & )
    {
      ++calls;
      return XrdCl::XRootDStatus();
    };
    operations.list = [&calls]( const std::string &,
                                std::vector<std::string> & )
    {
      ++calls;
      return XrdCl::XRootDStatus();
    };
    operations.removeDirectory = [&calls]( const std::string & )
    {
      ++calls;
      return XrdCl::XRootDStatus();
    };
    return operations;
  }
}

TEST( XrdClFSRemove, ParsesRecursiveOptionsAndOperands )
{
  const std::vector<std::vector<std::string>> cases = {
    {"rm", "-r", "/one"},
    {"rm", "-R", "/one"},
    {"rm", "--recursive", "/one"},
    {"rm", "/one", "-r", "/two"},
    {"rm", "-r", "-R", "--recursive", "/one"}
  };

  for( const std::vector<std::string> &arguments : cases )
  {
    XrdCl::RemoveCommand command;
    std::string error;
    ASSERT_TRUE( XrdCl::ParseRemoveCommand( arguments, command, error ) )
      << error;
    EXPECT_TRUE( command.recursive );
    if( arguments == cases[3] )
      EXPECT_EQ( command.paths,
                 (std::vector<std::string>{"/one", "/two"}) );
  }

  XrdCl::RemoveCommand command;
  std::string error;
  ASSERT_TRUE( XrdCl::ParseRemoveCommand(
    {"rm", "/one", "/two"}, command, error ) );
  EXPECT_FALSE( command.recursive );
  EXPECT_FALSE( command.dryRun );
  EXPECT_EQ( command.paths,
             (std::vector<std::string>{"/one", "/two"}) );
}

TEST( XrdClFSRemove, ParsesDryRunWithRecursiveOptionsAndDelimiter )
{
  XrdCl::RemoveCommand command;
  std::string error;
  ASSERT_TRUE( XrdCl::ParseRemoveCommand(
    {"rm", "--dry-run", "/one", "-R", "--", "--dry-run", "-r"},
    command, error ) ) << error;
  EXPECT_TRUE( command.dryRun );
  EXPECT_TRUE( command.recursive );
  EXPECT_EQ( command.paths,
             (std::vector<std::string>{"/one", "--dry-run", "-r"}) );
}

TEST( XrdClFSRemove, OptionDelimiterPreservesSpecialPathNames )
{
  XrdCl::RemoveCommand command;
  std::string error;
  ASSERT_TRUE( XrdCl::ParseRemoveCommand(
    {"rm", "-r", "--", "-R", "-directory", ""}, command, error ) );
  EXPECT_TRUE( command.recursive );
  EXPECT_EQ( command.paths,
             (std::vector<std::string>{"-R", "-directory", ""}) );
}

TEST( XrdClFSRemove, ParsesForceAndGroupedOptions )
{
  const std::vector<std::vector<std::string>> cases = {
    {"rm", "-f", "/one"},
    {"rm", "--force", "/one"},
    {"rm", "-rf", "/one"},
    {"rm", "-fr", "/one"},
    {"rm", "-rR", "/one"},
    {"rm", "-rRf", "/one"},
    {"rm", "/one", "-f", "-r", "/two"}
  };

  for( const std::vector<std::string> &arguments : cases )
  {
    XrdCl::RemoveCommand command;
    std::string error;
    ASSERT_TRUE( XrdCl::ParseRemoveCommand( arguments, command, error ) )
      << error;
    if( arguments[1] == "-f" || arguments[1] == "--force" )
    {
      EXPECT_TRUE( command.force );
      EXPECT_FALSE( command.recursive );
    }
    else if( arguments[1] == "-rR" )
    {
      EXPECT_FALSE( command.force );
      EXPECT_TRUE( command.recursive );
    }
    else
    {
      EXPECT_TRUE( command.force );
      EXPECT_TRUE( command.recursive );
    }
  }
}

TEST( XrdClFSRemove, RejectsUnknownOptionsAndMissingOperands )
{
  for( const std::vector<std::string> &arguments : {
         std::vector<std::string>{"rm"},
         std::vector<std::string>{"rm", "-r"},
         std::vector<std::string>{"rm", "--"},
         std::vector<std::string>{"rm", "-x", "/path"},
         std::vector<std::string>{"rm", "--unknown", "/path"} } )
  {
    XrdCl::RemoveCommand command;
    std::string error;
    EXPECT_FALSE( XrdCl::ParseRemoveCommand( arguments, command, error ) );
    EXPECT_FALSE( error.empty() );
  }
}

TEST( XrdClFSRemove, GuardsRootEmptyAndTraversalPaths )
{
  const char *unsafePaths[] = {
    "", "/", "//", "///?token=value", ".", "..", "a/../b",
    "/a/./b", "/a/%2e%2e/b", "/a/%2E/b", "%2f", "/%00"
  };
  for( const char *path : unsafePaths )
  {
    std::string error;
    EXPECT_FALSE( XrdCl::ValidateRecursiveRemovePath( path, error ) ) << path;
    EXPECT_FALSE( error.empty() ) << path;
  }

  for( const char *path : {
         "/safe", "relative/path", "/safe?token=/../value",
         "/safe/%25literal" } )
  {
    std::string error;
    EXPECT_TRUE( XrdCl::ValidateRecursiveRemovePath( path, error ) )
      << path << ": " << error;
  }
}

TEST( XrdClFSRemove, PreservesQueryParametersWhenJoiningChildren )
{
  EXPECT_EQ( XrdCl::RecursiveRemovalChildPath(
               "/tree?auth=one&x=two", "child" ),
             "/tree/child?auth=one&x=two" );
  EXPECT_EQ( XrdCl::RecursiveRemovalChildPath(
               "/tree///?auth=one", "child" ),
             "/tree/child?auth=one" );
  EXPECT_EQ( XrdCl::RecursiveRemovalChildPath( "/tree", "child name" ),
             "/tree/child name" );
}

TEST( XrdClFSRemove, DescendsOnlyOnAnExplicitDirectoryResponse )
{
  EXPECT_FALSE( XrdCl::IsRecursiveRemovalDirectoryStatus(
    DirectoryStatus(), false ) );
  EXPECT_TRUE( XrdCl::IsRecursiveRemovalDirectoryStatus(
    DirectoryStatus(), true ) );
  EXPECT_FALSE( XrdCl::IsRecursiveRemovalDirectoryStatus(
    ErrorStatus( kXR_ItExists ), false ) );
  EXPECT_TRUE( XrdCl::IsRecursiveRemovalDirectoryStatus(
    ErrorStatus( kXR_ItExists ), true ) );
  EXPECT_FALSE( XrdCl::IsRecursiveRemovalDirectoryStatus(
    ErrorStatus( kXR_ServerError ), true ) );
  EXPECT_FALSE( XrdCl::IsRecursiveRemovalDirectoryStatus(
    XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errInvalidResponse,
                         kXR_isDirectory ), true ) );
  EXPECT_FALSE( XrdCl::IsRecursiveRemovalDirectoryStatus(
    XrdCl::XRootDStatus(), true ) );
}

TEST( XrdClFSRemove, SanitizesRemovalStatusWithoutChangingItsClassification )
{
  const std::string secret =
    "https://example.test/file?authz=return-secret&signature=signed";
  const XrdCl::XRootDStatus original(
    XrdCl::stError, XrdCl::errErrorResponse, kXR_NotAuthorized,
    "metadata request failed for " + secret );

  const XrdCl::XRootDStatus sanitized =
    XrdCl::SanitizeRemovalStatus( original );

  EXPECT_EQ( sanitized.status, original.status );
  EXPECT_EQ( sanitized.code, original.code );
  EXPECT_EQ( sanitized.errNo, original.errNo );
  EXPECT_EQ( sanitized.GetErrorMessage().find( "return-secret" ),
             std::string::npos );
  EXPECT_NE( sanitized.GetErrorMessage().find( "authz=REDACTED" ),
             std::string::npos );
  EXPECT_NE( original.GetErrorMessage().find( "return-secret" ),
             std::string::npos );
}

TEST( XrdClFSRemove, PlansNonrecursiveFilesAndContinuesAfterADirectory )
{
  std::vector<std::string> calls;
  std::vector<std::string> reports;
  XrdCl::DryRunRemoveOperations operations;
  operations.stat = [&]( const std::string &path, bool &isDirectory )
  {
    calls.push_back( "stat " + path );
    isDirectory = path == "/directory";
    return XrdCl::XRootDStatus();
  };
  operations.list = [&]( const std::string &path,
                          std::vector<std::string> & )
  {
    calls.push_back( "ls " + path );
    return XrdCl::XRootDStatus();
  };
  operations.report = [&]( const std::string &path, bool isDirectory )
  {
    reports.push_back( std::string( isDirectory ? "dir " : "file " ) + path );
  };
  operations.reportFailure = [&]( const std::string &path,
                                   const XrdCl::XRootDStatus & )
  {
    reports.push_back( "failed " + path );
  };

  std::string failedPath;
  const XrdCl::XRootDStatus status = XrdCl::PlanRemoval(
    {"/first", "/directory", "/later"}, false, operations, failedPath );
  EXPECT_FALSE( status.IsOK() );
  EXPECT_EQ( status.errNo, static_cast<unsigned int>( kXR_isDirectory ) );
  EXPECT_EQ( failedPath, "/directory" );
  EXPECT_EQ( calls, (std::vector<std::string>{
    "stat /first", "stat /directory", "stat /later"
  }) );
  EXPECT_EQ( reports, (std::vector<std::string>{
    "file /first", "failed /directory", "file /later"
  }) );
}

TEST( XrdClFSRemove, PlansRecursiveTreesInPostorderAndPreservesQueries )
{
  const std::string root = "/tree?authz=top-secret&signature=signed";
  const std::map<std::string, std::vector<std::string>> directories = {
    {root, {"file", "nested"}},
    {"/tree/nested?authz=top-secret&signature=signed", {"leaf"}}
  };
  std::vector<std::string> calls;
  std::vector<std::string> reports;
  XrdCl::DryRunRemoveOperations operations;
  operations.stat = [&]( const std::string &path, bool &isDirectory )
  {
    calls.push_back( "stat " + path );
    isDirectory = directories.find( path ) != directories.end();
    return XrdCl::XRootDStatus();
  };
  operations.list = [&]( const std::string &path,
                          std::vector<std::string> &children )
  {
    calls.push_back( "ls " + path );
    children = directories.at( path );
    return XrdCl::XRootDStatus();
  };
  operations.report = [&]( const std::string &path, bool isDirectory )
  {
    reports.push_back( XrdCl::RemovalDisplayPath( path ) +
                       (isDirectory ? "\tSKIP DIR" : "\tSKIP") );
  };
  operations.reportFailure = []( const std::string &,
                                  const XrdCl::XRootDStatus & ) {};

  std::string failedPath;
  EXPECT_TRUE( XrdCl::PlanRemoval(
    {root}, true, operations, failedPath ).IsOK() );
  EXPECT_TRUE( failedPath.empty() );
  EXPECT_EQ( calls, (std::vector<std::string>{
    "stat " + root,
    "ls " + root,
    "stat /tree/file?authz=top-secret&signature=signed",
    "stat /tree/nested?authz=top-secret&signature=signed",
    "ls /tree/nested?authz=top-secret&signature=signed",
    "stat /tree/nested/leaf?authz=top-secret&signature=signed"
  }) );
  EXPECT_EQ( reports, (std::vector<std::string>{
    "/tree/file?authz=REDACTED&signature=signed\tSKIP",
    "/tree/nested/leaf?authz=REDACTED&signature=signed\tSKIP",
    "/tree/nested?authz=REDACTED&signature=signed\tSKIP DIR",
    "/tree?authz=REDACTED&signature=signed\tSKIP DIR"
  }) );
  for( const std::string &report : reports )
    EXPECT_EQ( report.find( "top-secret" ), std::string::npos );
}

TEST( XrdClFSRemove, BoundsDryRunTraversalOnCyclicListings )
{
  int statCalls = 0;
  int listCalls = 0;
  int failureCalls = 0;
  XrdCl::DryRunRemoveOperations operations;
  operations.stat = [&]( const std::string &, bool &isDirectory )
  {
    ++statCalls;
    isDirectory = true;
    return XrdCl::XRootDStatus();
  };
  operations.list = [&]( const std::string &,
                          std::vector<std::string> &children )
  {
    ++listCalls;
    children = {"d"};
    return XrdCl::XRootDStatus();
  };
  operations.report = []( const std::string &, bool ) {};
  operations.reportFailure = [&]( const std::string &,
                                   const XrdCl::XRootDStatus & )
  {
    ++failureCalls;
  };

  std::string failedPath;
  const XrdCl::XRootDStatus status = XrdCl::PlanRemoval(
    {"/tree"}, true, operations, failedPath );

  EXPECT_FALSE( status.IsOK() );
  EXPECT_EQ( status.code, XrdCl::errInvalidResponse );
  EXPECT_NE( status.GetErrorMessage().find( "maximum directory depth" ),
             std::string::npos );
  EXPECT_FALSE( failedPath.empty() );
  EXPECT_EQ( statCalls, 4097 );
  EXPECT_EQ( listCalls, 4097 );
  EXPECT_EQ( failureCalls, 1 );
}

TEST( XrdClFSRemove, RedactsMissingFailureAndPlansLaterRoots )
{
  const std::string missing = "/missing?authz=missing-secret&scope=read";
  const std::string later = "/later?authz=later-secret&scope=read";
  std::vector<std::string> reports;
  XrdCl::DryRunRemoveOperations operations;
  operations.stat = [&]( const std::string &path, bool &isDirectory )
  {
    isDirectory = false;
    return path == missing ?
      XrdCl::XRootDStatus(
        XrdCl::stError, XrdCl::errErrorResponse, kXR_NotFound,
        "metadata request failed for " + missing ) :
      XrdCl::XRootDStatus();
  };
  operations.list = []( const std::string &,
                         std::vector<std::string> & )
  {
    return XrdCl::XRootDStatus();
  };
  operations.report = [&]( const std::string &path, bool )
  {
    reports.push_back( XrdCl::RemovalDisplayPath( path ) + "\tSKIP" );
  };
  operations.reportFailure = [&]( const std::string &path,
                                   const XrdCl::XRootDStatus &status )
  {
    reports.push_back( XrdCl::RemovalDisplayPath( path ) +
                       (status.errNo == kXR_NotFound ? "\tMISSING" :
                                                       "\tFAILED") );
  };

  std::string failedPath;
  const XrdCl::XRootDStatus status = XrdCl::PlanRemoval(
    {missing, later}, false, operations, failedPath );
  EXPECT_FALSE( status.IsOK() );
  EXPECT_EQ( status.status, XrdCl::stError );
  EXPECT_EQ( status.code, XrdCl::errErrorResponse );
  EXPECT_EQ( status.errNo, static_cast<unsigned int>( kXR_NotFound ) );
  EXPECT_EQ( status.GetErrorMessage().find( "missing-secret" ),
             std::string::npos );
  EXPECT_NE( status.GetErrorMessage().find( "authz=REDACTED" ),
             std::string::npos );
  EXPECT_EQ( failedPath, missing );
  EXPECT_EQ( reports, (std::vector<std::string>{
    "/missing?authz=REDACTED&scope=read\tMISSING",
    "/later?authz=REDACTED&scope=read\tSKIP"
  }) );
  for( const std::string &report : reports )
  {
    EXPECT_EQ( report.find( "missing-secret" ), std::string::npos );
    EXPECT_EQ( report.find( "later-secret" ), std::string::npos );
  }
}

TEST( XrdClFSRemove, DryRunRejectsIncompleteMetadataWithoutListing )
{
  int listCalls = 0;
  XrdCl::DryRunRemoveOperations operations;
  operations.stat = []( const std::string &, bool &isDirectory )
  {
    isDirectory = true;
    return XrdCl::XRootDStatus( XrdCl::stOK, XrdCl::suPartial );
  };
  operations.list = [&]( const std::string &,
                          std::vector<std::string> & )
  {
    ++listCalls;
    return XrdCl::XRootDStatus();
  };
  operations.report = []( const std::string &, bool ) {};
  operations.reportFailure = []( const std::string &,
                                  const XrdCl::XRootDStatus & ) {};

  std::string failedPath;
  const XrdCl::XRootDStatus status = XrdCl::PlanRemoval(
    {"/tree"}, true, operations, failedPath );
  EXPECT_FALSE( status.IsOK() );
  EXPECT_EQ( status.code, XrdCl::errInvalidResponse );
  EXPECT_EQ( failedPath, "/tree" );
  EXPECT_EQ( listCalls, 0 );
}

TEST( XrdClFSRemove, DryRunPrevalidatesEveryRecursiveRootBeforeMetadata )
{
  int metadataCalls = 0;
  XrdCl::DryRunRemoveOperations operations;
  operations.stat = [&]( const std::string &, bool & )
  {
    ++metadataCalls;
    return XrdCl::XRootDStatus();
  };
  operations.list = [&]( const std::string &,
                          std::vector<std::string> & )
  {
    ++metadataCalls;
    return XrdCl::XRootDStatus();
  };
  operations.report = []( const std::string &, bool ) {};
  operations.reportFailure = []( const std::string &,
                                  const XrdCl::XRootDStatus & ) {};

  std::string failedPath;
  const XrdCl::XRootDStatus status = XrdCl::PlanRemoval(
    {"/safe", "/"}, true, operations, failedPath );
  EXPECT_FALSE( status.IsOK() );
  EXPECT_EQ( status.code, XrdCl::errInvalidArgs );
  EXPECT_EQ( failedPath, "/" );
  EXPECT_EQ( metadataCalls, 0 );
}

TEST( XrdClFSRemove, RemovesNestedTreesInIterativePostorder )
{
  const std::map<std::string, std::vector<std::string>> directories = {
    {"/tree", {"file", "empty", "nested"}},
    {"/tree/empty", {}},
    {"/tree/nested", {"leaf"}}
  };
  const std::set<std::string> files = {
    "/tree/file", "/tree/nested/leaf"
  };
  std::vector<std::string> calls;

  XrdCl::RecursiveRemoveOperations operations;
  operations.nativeXRootD = true;
  operations.remove = [&]( const std::string &path )
  {
    calls.push_back( "rm " + path );
    const auto directory = directories.find( path );
    if( directory == directories.end() ) return XrdCl::XRootDStatus();
    return directory->second.empty() ? DirectoryStatus() :
                                       ErrorStatus( kXR_ItExists );
  };
  operations.list = [&]( const std::string &path,
                         std::vector<std::string> &children )
  {
    calls.push_back( "ls " + path );
    children = directories.at( path );
    return XrdCl::XRootDStatus();
  };
  operations.removeDirectory = [&]( const std::string &path )
  {
    calls.push_back( "rmdir " + path );
    return XrdCl::XRootDStatus();
  };

  std::string failedPath;
  EXPECT_TRUE( XrdCl::RemoveRecursively(
    {"/tree"}, operations, failedPath ).IsOK() );
  EXPECT_TRUE( failedPath.empty() );
  EXPECT_EQ( calls, (std::vector<std::string>{
    "rm /tree", "ls /tree",
    "rm /tree/file",
    "rm /tree/empty", "rmdir /tree/empty",
    "rm /tree/nested", "ls /tree/nested",
    "rm /tree/nested/leaf", "rmdir /tree/nested",
    "rmdir /tree"
  }) );
  EXPECT_EQ( files.size(), 2U );
}

TEST( XrdClFSRemove, RmSuccessDoesNotFollowDirectorySymlinksOrWebDAVCollections )
{
  std::vector<std::string> calls;
  XrdCl::RecursiveRemoveOperations operations;
  operations.remove = [&]( const std::string &path )
  {
    calls.push_back( "rm " + path );
    return XrdCl::XRootDStatus();
  };
  operations.list = [&]( const std::string &path,
                         std::vector<std::string> & )
  {
    calls.push_back( "ls " + path );
    return XrdCl::XRootDStatus();
  };
  operations.removeDirectory = [&]( const std::string &path )
  {
    calls.push_back( "rmdir " + path );
    return XrdCl::XRootDStatus();
  };

  std::string failedPath;
  EXPECT_TRUE( XrdCl::RemoveRecursively(
    {"/link", "/webdav-collection"}, operations, failedPath ).IsOK() );
  EXPECT_EQ( calls, (std::vector<std::string>{
    "rm /link", "rm /webdav-collection"
  }) );
}

TEST( XrdClFSRemove, RejectsIncompleteOperationResponses )
{
  for( const int incompleteOperation : {0, 1, 2} )
  {
    XrdCl::RecursiveRemoveOperations operations;
    operations.nativeXRootD = true;
    operations.remove = [incompleteOperation]( const std::string & )
    {
      if( incompleteOperation == 0 )
        return XrdCl::XRootDStatus( XrdCl::stOK, XrdCl::suPartial );
      return ErrorStatus( kXR_ItExists );
    };
    operations.list = [incompleteOperation](
      const std::string &, std::vector<std::string> & )
    {
      if( incompleteOperation == 1 )
        return XrdCl::XRootDStatus( XrdCl::stOK, XrdCl::suPartial );
      return XrdCl::XRootDStatus();
    };
    operations.removeDirectory = [incompleteOperation]( const std::string & )
    {
      if( incompleteOperation == 2 )
        return XrdCl::XRootDStatus( XrdCl::stOK, XrdCl::suPartial );
      return XrdCl::XRootDStatus();
    };

    std::string failedPath;
    const XrdCl::XRootDStatus status = XrdCl::RemoveRecursively(
      {"/tree"}, operations, failedPath );
    EXPECT_FALSE( status.IsOK() ) << incompleteOperation;
    EXPECT_EQ( status.code, XrdCl::errInvalidResponse ) << incompleteOperation;
    EXPECT_EQ( failedPath, "/tree" ) << incompleteOperation;
  }
}

TEST( XrdClFSRemove, TreatsMultiStatusAndAmbiguousErrorsAsTerminal )
{
  for( const unsigned int error : {kXR_ServerError, kXR_ItExists,
                                    kXR_Conflict} )
  {
    int listCalls = 0;
    XrdCl::RecursiveRemoveOperations operations;
    operations.remove = [error]( const std::string & )
    {
      return ErrorStatus( error );
    };
    operations.list = [&listCalls]( const std::string &,
                                    std::vector<std::string> & )
    {
      ++listCalls;
      return XrdCl::XRootDStatus();
    };
    operations.removeDirectory = []( const std::string & )
    {
      return XrdCl::XRootDStatus();
    };

    std::string failedPath;
    const XrdCl::XRootDStatus status = XrdCl::RemoveRecursively(
      {"/tree"}, operations, failedPath );
    EXPECT_FALSE( status.IsOK() ) << error;
    EXPECT_EQ( status.errNo, error );
    EXPECT_EQ( listCalls, 0 );
  }
}

TEST( XrdClFSRemove, NativeNonemptyDirectoryResponseTriggersTraversal )
{
  std::vector<std::string> calls;
  XrdCl::RecursiveRemoveOperations operations;
  operations.nativeXRootD = true;
  operations.remove = [&]( const std::string &path )
  {
    calls.push_back( "rm " + path );
    return path == "/tree" ? ErrorStatus( kXR_ItExists ) :
                              XrdCl::XRootDStatus();
  };
  operations.list = [&]( const std::string &path,
                         std::vector<std::string> &children )
  {
    calls.push_back( "ls " + path );
    children = {"child"};
    return XrdCl::XRootDStatus();
  };
  operations.removeDirectory = [&]( const std::string &path )
  {
    calls.push_back( "rmdir " + path );
    return XrdCl::XRootDStatus();
  };

  std::string failedPath;
  EXPECT_TRUE( XrdCl::RemoveRecursively(
    {"/tree"}, operations, failedPath ).IsOK() );
  EXPECT_EQ( calls, (std::vector<std::string>{
    "rm /tree", "ls /tree", "rm /tree/child", "rmdir /tree"
  }) );
}

TEST( XrdClFSRemove, NativeDirectoryProbeRemovesAnEmptyRealDirectory )
{
  std::vector<std::string> calls;
  XrdCl::RecursiveRemoveOperations operations;
  operations.nativeXRootD = true;
  operations.remove = [&]( const std::string &path )
  {
    calls.push_back( "rm " + path );
    return DirectoryStatus();
  };
  operations.list = [&]( const std::string &path,
                         std::vector<std::string> & )
  {
    calls.push_back( "ls " + path );
    return XrdCl::XRootDStatus();
  };
  operations.removeDirectory = [&]( const std::string &path )
  {
    calls.push_back( "rmdir " + path );
    return XrdCl::XRootDStatus();
  };

  std::string failedPath;
  EXPECT_TRUE( XrdCl::RemoveRecursively(
    {"/empty"}, operations, failedPath ).IsOK() );
  EXPECT_EQ( calls, (std::vector<std::string>{
    "rm /empty", "rmdir /empty"
  }) );
}

TEST( XrdClFSRemove, NativeDirectoryProbeConfirmsNonemptyBeforeListing )
{
  std::vector<std::string> calls;
  int directoryRemovals = 0;
  XrdCl::RecursiveRemoveOperations operations;
  operations.nativeXRootD = true;
  operations.remove = [&]( const std::string &path )
  {
    calls.push_back( "rm " + path );
    return path == "/tree" ? DirectoryStatus() : XrdCl::XRootDStatus();
  };
  operations.list = [&]( const std::string &path,
                         std::vector<std::string> &children )
  {
    calls.push_back( "ls " + path );
    children = {"child"};
    return XrdCl::XRootDStatus();
  };
  operations.removeDirectory = [&]( const std::string &path )
  {
    calls.push_back( "rmdir " + path );
    ++directoryRemovals;
    return directoryRemovals == 1 ? ErrorStatus( kXR_ItExists ) :
                                    XrdCl::XRootDStatus();
  };

  std::string failedPath;
  EXPECT_TRUE( XrdCl::RemoveRecursively(
    {"/tree"}, operations, failedPath ).IsOK() );
  EXPECT_EQ( calls, (std::vector<std::string>{
    "rm /tree", "rmdir /tree", "ls /tree", "rm /tree/child",
    "rmdir /tree"
  }) );
}

TEST( XrdClFSRemove, NativeDirectoryProbeNeverFollowsALinuxDirectorySymlink )
{
  std::vector<std::string> calls;
  XrdCl::RecursiveRemoveOperations operations;
  operations.nativeXRootD = true;
  operations.remove = [&]( const std::string &path )
  {
    calls.push_back( "rm " + path );
    // Linux XrdOss BreakLink may return EISDIR for an absolute symlink whose
    // target is a directory.
    return DirectoryStatus();
  };
  operations.list = [&]( const std::string &path,
                         std::vector<std::string> & )
  {
    calls.push_back( "ls " + path );
    return XrdCl::XRootDStatus();
  };
  operations.removeDirectory = [&]( const std::string &path )
  {
    calls.push_back( "rmdir " + path );
    return ErrorStatus( kXR_NotFile );
  };

  std::string failedPath;
  const XrdCl::XRootDStatus status = XrdCl::RemoveRecursively(
    {"/directory-link"}, operations, failedPath );
  EXPECT_FALSE( status.IsOK() );
  EXPECT_EQ( status.errNo, static_cast<unsigned int>( kXR_NotFile ) );
  EXPECT_EQ( calls, (std::vector<std::string>{
    "rm /directory-link", "rmdir /directory-link"
  }) );
}

TEST( XrdClFSRemove, SkipsPseudoEntriesAndRejectsUnsafeChildrenBeforeMutation )
{
  for( const std::string unsafe : {"", "../escape", "encoded%2fslash",
                                    "%2e", "%2E%2E", "query?name"} )
  {
    std::vector<std::string> removed;
    XrdCl::RecursiveRemoveOperations operations;
    operations.nativeXRootD = true;
    operations.remove = [&]( const std::string &path )
    {
      removed.push_back( path );
      return path == "/tree" ? ErrorStatus( kXR_ItExists ) :
                                XrdCl::XRootDStatus();
    };
    operations.list = [&]( const std::string &,
                           std::vector<std::string> &children )
    {
      children = {".", "..", "safe", unsafe};
      return XrdCl::XRootDStatus();
    };
    operations.removeDirectory = []( const std::string & )
    {
      return XrdCl::XRootDStatus();
    };

    std::string failedPath;
    const XrdCl::XRootDStatus status = XrdCl::RemoveRecursively(
      {"/tree"}, operations, failedPath );
    EXPECT_FALSE( status.IsOK() ) << unsafe;
    EXPECT_EQ( status.code, XrdCl::errInvalidResponse ) << unsafe;
    EXPECT_EQ( removed, (std::vector<std::string>{"/tree"}) ) << unsafe;
  }
}

TEST( XrdClFSRemove, ValidatesEveryRootBeforeMutation )
{
  int calls = 0;
  XrdCl::RecursiveRemoveOperations operations = NeverCalledOperations( calls );
  std::string failedPath;
  const XrdCl::XRootDStatus status = XrdCl::RemoveRecursively(
    {"/safe", "/"}, operations, failedPath );
  EXPECT_FALSE( status.IsOK() );
  EXPECT_EQ( status.code, XrdCl::errInvalidArgs );
  EXPECT_EQ( failedPath, "/" );
  EXPECT_EQ( calls, 0 );
}

TEST( XrdClFSRemove, ContinuesWithLaterRootsAfterOneTreeFails )
{
  std::vector<std::string> removed;
  XrdCl::RecursiveRemoveOperations operations;
  operations.nativeXRootD = true;
  operations.remove = [&]( const std::string &path )
  {
    removed.push_back( path );
    return path == "/missing" ? ErrorStatus( kXR_NotFound ) :
                                 XrdCl::XRootDStatus();
  };
  operations.list = []( const std::string &,
                        std::vector<std::string> & )
  {
    return XrdCl::XRootDStatus();
  };
  operations.removeDirectory = []( const std::string & )
  {
    return XrdCl::XRootDStatus();
  };

  std::string failedPath;
  const XrdCl::XRootDStatus status = XrdCl::RemoveRecursively(
    {"/missing", "/valid-second"}, operations, failedPath );
  EXPECT_FALSE( status.IsOK() );
  EXPECT_EQ( status.errNo, static_cast<unsigned int>( kXR_NotFound ) );
  EXPECT_EQ( failedPath, "/missing" );
  EXPECT_EQ( removed, (std::vector<std::string>{
    "/missing", "/valid-second"
  }) );
}

TEST( XrdClFSRemove, StopsTheFailedTreeBeforeContinuingTheNextRoot )
{
  std::vector<std::string> removed;
  XrdCl::RecursiveRemoveOperations operations;
  operations.nativeXRootD = true;
  operations.remove = [&]( const std::string &path )
  {
    removed.push_back( path );
    if( path == "/tree" ) return ErrorStatus( kXR_ItExists );
    if( path == "/tree/failing" ) return ErrorStatus( kXR_NotAuthorized );
    return XrdCl::XRootDStatus();
  };
  operations.list = []( const std::string &,
                        std::vector<std::string> &children )
  {
    children = {"failing", "untouched-sibling"};
    return XrdCl::XRootDStatus();
  };
  operations.removeDirectory = []( const std::string & )
  {
    return XrdCl::XRootDStatus();
  };

  std::string failedPath;
  const XrdCl::XRootDStatus status = XrdCl::RemoveRecursively(
    {"/tree", "/later-root"}, operations, failedPath );
  EXPECT_FALSE( status.IsOK() );
  EXPECT_EQ( failedPath, "/tree/failing" );
  EXPECT_EQ( removed, (std::vector<std::string>{
    "/tree", "/tree/failing", "/later-root"
  }) );
}

TEST( XrdClFSRemove, HandlesDeepTreesWithoutCallStackRecursion )
{
  const int depth = 2048;
  int removeCalls = 0;
  int listCalls = 0;
  int removeDirectoryCalls = 0;

  XrdCl::RecursiveRemoveOperations operations;
  operations.nativeXRootD = true;
  operations.remove = [&]( const std::string &path )
  {
    ++removeCalls;
    const int currentDepth =
      static_cast<int>( std::count( path.begin(), path.end(), '/' ) ) - 1;
    return currentDepth < depth ? ErrorStatus( kXR_ItExists ) :
                                  XrdCl::XRootDStatus();
  };
  operations.list = [&]( const std::string &,
                         std::vector<std::string> &children )
  {
    ++listCalls;
    children.push_back( "d" );
    return XrdCl::XRootDStatus();
  };
  operations.removeDirectory = [&]( const std::string & )
  {
    ++removeDirectoryCalls;
    return XrdCl::XRootDStatus();
  };

  std::string failedPath;
  EXPECT_TRUE( XrdCl::RemoveRecursively(
    {"/deep"}, operations, failedPath ).IsOK() );
  EXPECT_EQ( removeCalls, depth + 1 );
  EXPECT_EQ( listCalls, depth );
  EXPECT_EQ( removeDirectoryCalls, depth );
}

TEST( XrdClFSRemove, ForceOptionToleratesMissingPathsInRecursiveRemoval )
{
  std::vector<std::string> calls;
  XrdCl::RecursiveRemoveOperations operations;
  operations.nativeXRootD = true;
  operations.force = true;
  operations.remove = [&]( const std::string &path )
  {
    calls.push_back( "rm " + path );
    return path == "/missing" ? ErrorStatus( kXR_NotFound ) :
                                XrdCl::XRootDStatus();
  };
  operations.list = [&]( const std::string &path,
                         std::vector<std::string> & )
  {
    calls.push_back( "ls " + path );
    return XrdCl::XRootDStatus();
  };
  operations.removeDirectory = [&]( const std::string &path )
  {
    calls.push_back( "rmdir " + path );
    return XrdCl::XRootDStatus();
  };

  std::string failedPath;
  const XrdCl::XRootDStatus status = XrdCl::RemoveRecursively(
    {"/missing", "/existing"}, operations, failedPath );
  EXPECT_TRUE( status.IsOK() );
  EXPECT_EQ( calls, (std::vector<std::string>{
    "rm /missing", "rm /existing"
  }) );
}

TEST( XrdClFSRemove, ForceOptionToleratesMissingPathsInDryRunPlan )
{
  std::vector<std::string> reported;
  std::vector<std::string> failures;

  XrdCl::DryRunRemoveOperations operations;
  operations.force = true;
  operations.stat = [&]( const std::string &path, bool &isDirectory )
  {
    if( path == "/missing" )
      return ErrorStatus( kXR_NotFound );
    isDirectory = false;
    return XrdCl::XRootDStatus();
  };
  operations.list = [&]( const std::string &,
                         std::vector<std::string> & )
  {
    return XrdCl::XRootDStatus();
  };
  operations.report = [&]( const std::string &path, bool )
  {
    reported.push_back( path );
  };
  operations.reportFailure = [&]( const std::string &path,
                                 const XrdCl::XRootDStatus & )
  {
    failures.push_back( path );
  };

  std::string failedPath;
  const XrdCl::XRootDStatus status = XrdCl::PlanRemoval(
    {"/missing", "/existing"}, false, operations, failedPath );
  EXPECT_TRUE( status.IsOK() );
  EXPECT_EQ( failures, (std::vector<std::string>{"/missing"}) );
  EXPECT_EQ( reported, (std::vector<std::string>{"/existing"}) );
}
