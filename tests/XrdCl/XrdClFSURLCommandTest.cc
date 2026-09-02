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

#include "XrdCl/XrdClFSURLCommand.hh"

#include <gtest/gtest.h>

#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace
{
  struct NormalizedCommand
  {
    XrdCl::URLCommandResult result;
    std::vector<std::string> arguments;
    XrdCl::URL endpoint;
    std::string error;
  };

  NormalizedCommand Normalize( std::initializer_list<const char *> arguments )
  {
    NormalizedCommand command;
    for( const char *argument : arguments )
      command.arguments.emplace_back( argument );
    command.result = XrdCl::NormalizeFSURLCommand(
      command.arguments, command.endpoint, command.error );
    return command;
  }
}

TEST( XrdClFSURLCommand, IgnoresEmptyAndUnsupportedCommands )
{
  EXPECT_EQ( Normalize( {} ).result, XrdCl::NotURLCommand );

  NormalizedCommand command =
    Normalize( {"help", "root://root.example.org//store/file"} );
  EXPECT_EQ( command.result, XrdCl::NotURLCommand );
  EXPECT_TRUE( command.error.empty() );
}

TEST( XrdClFSURLCommand, NormalizesCommandFirstRootURL )
{
  NormalizedCommand command =
    Normalize( {"stat", "root://root.example.org//store/file"} );

  ASSERT_EQ( command.result, XrdCl::ValidURLCommand );
  EXPECT_EQ( command.endpoint.GetProtocol(), "root" );
  EXPECT_EQ( command.endpoint.GetHostName(), "root.example.org" );
  EXPECT_EQ( command.endpoint.GetPort(), 1094 );
  EXPECT_TRUE( command.endpoint.GetPath().empty() );
  EXPECT_TRUE( command.endpoint.GetParams().empty() );
  EXPECT_EQ( command.arguments,
             (std::vector<std::string>{"stat", "/store/file"}) );
}

TEST( XrdClFSURLCommand, PreservesURLParametersOnThePath )
{
  NormalizedCommand command = Normalize(
    {"stat", "root://root.example.org//store/file?oss.asize=1"} );

  ASSERT_EQ( command.result, XrdCl::ValidURLCommand );
  EXPECT_TRUE( command.endpoint.GetParams().empty() );
  EXPECT_EQ( command.arguments,
             (std::vector<std::string>{
               "stat", "/store/file?oss.asize=1"}) );
}

TEST( XrdClFSURLCommand, NormalizesSumPathButNotChecksumType )
{
  NormalizedCommand command = Normalize(
    {"sum", "root://root.example.org//store/file", "ADLER32"} );

  ASSERT_EQ( command.result, XrdCl::ValidURLCommand );
  EXPECT_EQ( command.arguments,
             (std::vector<std::string>{"sum", "/store/file", "ADLER32"}) );
}

TEST( XrdClFSURLCommand, AcceptsPluginBackedHTTPSURL )
{
  NormalizedCommand command =
    Normalize( {"stat", "https://https.example.org/store/file"} );

  ASSERT_EQ( command.result, XrdCl::ValidURLCommand );
  EXPECT_EQ( command.endpoint.GetProtocol(), "https" );
  EXPECT_EQ( command.endpoint.GetHostName(), "https.example.org" );
  EXPECT_EQ( command.endpoint.GetPort(), 443 );
  EXPECT_EQ( command.arguments,
             (std::vector<std::string>{"stat", "/store/file"}) );
}

TEST( XrdClFSURLCommand, ReducesURLsUsingTheSameEffectiveEndpoint )
{
  NormalizedCommand command = Normalize(
    {"cat", "ROOT://ROOT.example.org//store/one",
            "/store/two",
            "ROOT://root.example.org:1094//store/three"} );

  ASSERT_EQ( command.result, XrdCl::ValidURLCommand );
  EXPECT_EQ( command.endpoint.GetProtocol(), "root" );
  EXPECT_EQ( command.endpoint.GetHostName(), "root.example.org" );
  EXPECT_EQ( command.arguments,
             (std::vector<std::string>{
               "cat", "/store/one", "/store/two", "/store/three"}) );
}

TEST( XrdClFSURLCommand, RejectsMixedEndpointsWithoutChangingArguments )
{
  const std::vector<std::vector<std::string>> cases = {
    {"cat", "root://one.example.org//one",
            "root://two.example.org//two"},
    {"cat", "root://one.example.org:1094//one",
            "root://one.example.org:1095//two"},
    {"cat", "root://one.example.org//one",
            "https://one.example.org//two"},
    {"cat", "root://alice@one.example.org//one",
            "root://bob@one.example.org//two"}
  };

  for( const std::vector<std::string> &original : cases )
  {
    std::vector<std::string> arguments( original );
    XrdCl::URL endpoint;
    std::string error;
    EXPECT_EQ( XrdCl::NormalizeFSURLCommand( arguments, endpoint, error ),
               XrdCl::InvalidURLCommand );
    EXPECT_EQ( error, "all URL operands must use the same endpoint" );
    EXPECT_EQ( arguments, original );
  }
}

TEST( XrdClFSURLCommand, RejectsMalformedAndLocalURLs )
{
  const char *urls[] = {
    "1root://root.example.org//store/file",
    "root_://root.example.org//store/file",
    "root:///store/file",
    "file:///tmp/file",
    "FILE://localhost/tmp/file",
    "stdio://-/file"
  };

  for( const char *url : urls )
  {
    NormalizedCommand command = Normalize( {"stat", url} );
    EXPECT_EQ( command.result, XrdCl::InvalidURLCommand ) << url;
    EXPECT_EQ( command.error, "invalid remote URL operand" ) << url;
  }
}

TEST( XrdClFSURLCommand, LeavesURLValuedNonPathOperandsUntouched )
{
  using CommandPair = std::pair<std::vector<std::string>,
                                std::vector<std::string>>;
  const std::vector<CommandPair> cases = {
    {
      {"xattr", "root://root.example.org//store/file", "set",
                "user.url=https://value.example/value"},
      {"xattr", "/store/file", "set",
                "user.url=https://value.example/value"}
    },
    {
      {"prepare", "-a", "https://request.example/id",
                  "root://root.example.org//store/file"},
      {"prepare", "-a", "https://request.example/id", "/store/file"}
    },
    {
      {"prepare", "-p", "https://priority.example/value",
                  "root://root.example.org//store/file"},
      {"prepare", "-p", "https://priority.example/value", "/store/file"}
    },
    {
      {"stat", "-q", "https://query.example/value",
               "root://root.example.org//store/file"},
      {"stat", "-q", "https://query.example/value", "/store/file"}
    },
    {
      {"cat", "-o", "https://local-output.example/value",
              "root://root.example.org//store/file"},
      {"cat", "-o", "https://local-output.example/value", "/store/file"}
    },
    {
      {"ls", "--xattr", "https://attribute.example/value",
             "root://root.example.org//store/file"},
      {"ls", "--xattr", "https://attribute.example/value", "/store/file"}
    },
    {
      {"ls", "--color", "https://color.example/value",
             "root://root.example.org//store/file"},
      {"ls", "--color", "https://color.example/value", "/store/file"}
    },
    {
      {"tail", "-c", "https://offset.example/value",
               "root://root.example.org//store/file"},
      {"tail", "-c", "https://offset.example/value", "/store/file"}
    }
  };

  for( const CommandPair &testCase : cases )
  {
    std::vector<std::string> arguments( testCase.first );
    XrdCl::URL endpoint;
    std::string error;
    ASSERT_EQ( XrdCl::NormalizeFSURLCommand( arguments, endpoint, error ),
               XrdCl::ValidURLCommand );
    EXPECT_EQ( arguments, testCase.second );
    EXPECT_TRUE( error.empty() );
  }
}

TEST( XrdClFSURLCommand, UsesExactPathPositionsForFixedGrammars )
{
  NormalizedCommand cache = Normalize(
    {"cache", "evict", "root://root.example.org//store/file"} );
  ASSERT_EQ( cache.result, XrdCl::ValidURLCommand );
  EXPECT_EQ( cache.arguments,
             (std::vector<std::string>{"cache", "evict", "/store/file"}) );

  NormalizedCommand truncate = Normalize(
    {"truncate", "root://root.example.org//store/file",
                 "https://not-a-size.example/value"} );
  ASSERT_EQ( truncate.result, XrdCl::ValidURLCommand );
  EXPECT_EQ( truncate.arguments[2], "https://not-a-size.example/value" );
}

TEST( XrdClFSURLCommand, KeepsRawQueriesServerFirst )
{
  NormalizedCommand command = Normalize(
    {"query", "opaque", "root://root.example.org//store/file"} );

  EXPECT_EQ( command.result, XrdCl::InvalidURLCommand );
  EXPECT_EQ(
    command.error,
    "command-first full URLs are not supported for 'query'" );
}
