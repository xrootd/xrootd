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

TEST( XrdClFSURLCommand, NormalizesOnlyTokenPathOperand )
{
  NormalizedCommand command = Normalize(
    {"token", "--write", "--validity", "15", "--issuer",
              "https://issuer.example/token",
              "https://storage.example/eos/file?opaque=value",
              "DOWNLOAD", "https://activity.example/not-a-path"} );

  ASSERT_EQ( command.result, XrdCl::ValidURLCommand );
  EXPECT_EQ( command.endpoint.GetProtocol(), "https" );
  EXPECT_EQ( command.endpoint.GetHostName(), "storage.example" );
  EXPECT_EQ( command.arguments,
             (std::vector<std::string>{
               "token", "--write", "--validity", "15", "--issuer",
               "https://issuer.example/token", "/eos/file?opaque=value",
               "DOWNLOAD", "https://activity.example/not-a-path"}) );
}

TEST( XrdClFSURLCommand, TokenDoubleDashStillSeparatesPathFromActivities )
{
  NormalizedCommand command = Normalize(
    {"token", "--validity=5", "--",
              "davs://storage.example/eos/file",
              "https://activity.example/not-a-path"} );

  ASSERT_EQ( command.result, XrdCl::ValidURLCommand );
  EXPECT_EQ( command.endpoint.GetProtocol(), "davs" );
  EXPECT_EQ( command.endpoint.GetHostName(), "storage.example" );
  EXPECT_EQ( command.arguments,
             (std::vector<std::string>{
               "token", "--validity=5", "--", "/eos/file",
               "https://activity.example/not-a-path"}) );
}

TEST( XrdClFSURLCommand, RoutesTokenOptionAbbreviationsToCommandParser )
{
  NormalizedCommand command = Normalize(
    {"token", "--val", "5", "https://storage.example/eos/file"} );

  ASSERT_EQ( command.result, XrdCl::ValidURLCommand );
  EXPECT_EQ( command.endpoint.GetProtocol(), "https" );
  EXPECT_EQ( command.arguments,
             (std::vector<std::string>{
               "token", "--val", "5", "/eos/file"}) );
}

TEST( XrdClFSURLCommand, PreservesEmptyTokenIssuerForCommandValidation )
{
  NormalizedCommand command = Normalize(
    {"token", "--issuer=", "https://storage.example/eos/file"} );

  ASSERT_EQ( command.result, XrdCl::ValidURLCommand );
  EXPECT_EQ( command.endpoint.GetProtocol(), "https" );
  EXPECT_EQ( command.arguments,
             (std::vector<std::string>{
               "token", "--issuer=", "/eos/file"}) );
}

TEST( XrdClFSURLCommand, NormalizesEveryMkdirPathAndSkipsOptionValues )
{
  NormalizedCommand command = Normalize(
    {"mkdir", "-p", "--parents",
              "-m", "https://short-mode.example/value",
              "--mode", "https://long-mode.example/value",
              "-mrwxr-x---", "--mode=rwx------",
              "root://root.example.org//store/one",
              "root://root.example.org//store/two"} );

  ASSERT_EQ( command.result, XrdCl::ValidURLCommand );
  EXPECT_EQ( command.endpoint.GetHostName(), "root.example.org" );
  EXPECT_EQ( command.arguments,
             (std::vector<std::string>{
               "mkdir", "-p", "--parents",
               "-m", "https://short-mode.example/value",
               "--mode", "https://long-mode.example/value",
               "-mrwxr-x---", "--mode=rwx------",
               "/store/one", "/store/two"}) );
}

TEST( XrdClFSURLCommand, MkdirDoubleDashEndsOptionParsing )
{
  NormalizedCommand command = Normalize(
    {"mkdir", "--parents", "--",
              "root://root.example.org//store/one",
              "root://root.example.org//store/two"} );

  ASSERT_EQ( command.result, XrdCl::ValidURLCommand );
  EXPECT_EQ( command.arguments,
             (std::vector<std::string>{
               "mkdir", "--parents", "--", "/store/one", "/store/two"}) );
}

TEST( XrdClFSURLCommand, NormalizesLegacyAndGfalChmodLayouts )
{
  NormalizedCommand legacy = Normalize(
    {"chmod", "root://root.example.org//store/legacy", "rwxr-x---"} );
  ASSERT_EQ( legacy.result, XrdCl::ValidURLCommand );
  EXPECT_EQ( legacy.arguments,
             (std::vector<std::string>{
               "chmod", "/store/legacy", "rwxr-x---"}) );

  NormalizedCommand legacyURLMode = Normalize(
    {"chmod", "root://root.example.org//store/legacy",
              "https://mode.example/value"} );
  ASSERT_EQ( legacyURLMode.result, XrdCl::ValidURLCommand );
  EXPECT_EQ( legacyURLMode.arguments,
             (std::vector<std::string>{
               "chmod", "/store/legacy", "https://mode.example/value"}) );

  NormalizedCommand gfal = Normalize(
    {"chmod", "0755", "root://root.example.org//store/one"} );
  ASSERT_EQ( gfal.result, XrdCl::ValidURLCommand );
  EXPECT_EQ( gfal.arguments,
             (std::vector<std::string>{
               "chmod", "0755", "/store/one"}) );

  NormalizedCommand invalidMode = Normalize(
    {"chmod", "not-octal", "root://root.example.org//store/file"} );
  ASSERT_EQ( invalidMode.result, XrdCl::ValidURLCommand );
  EXPECT_EQ( invalidMode.arguments,
             (std::vector<std::string>{
               "chmod", "not-octal", "/store/file"}) );
}

TEST( XrdClFSURLCommand, NormalizesEveryMvAndRmPath )
{
  NormalizedCommand mv = Normalize(
    {"mv", "root://root.example.org//store/source",
           "root://root.example.org//store/destination"} );
  ASSERT_EQ( mv.result, XrdCl::ValidURLCommand );
  EXPECT_EQ( mv.arguments,
             (std::vector<std::string>{
               "mv", "/store/source", "/store/destination"}) );

  NormalizedCommand rm = Normalize(
    {"rm", "root://root.example.org//store/one", "/store/two",
           "root://root.example.org//store/three"} );
  ASSERT_EQ( rm.result, XrdCl::ValidURLCommand );
  EXPECT_EQ( rm.arguments,
             (std::vector<std::string>{
               "rm", "/store/one", "/store/two", "/store/three"}) );
}

TEST( XrdClFSURLCommand, KeepsRecursiveRmOptionsOutsideURLNormalization )
{
  for( const char *option : {"-r", "-R", "--recursive"} )
  {
    NormalizedCommand command = Normalize(
      {"rm", option, "root://root.example.org//store/tree?auth=value"} );
    ASSERT_EQ( command.result, XrdCl::ValidURLCommand ) << option;
    EXPECT_EQ( command.endpoint.GetProtocol(), "root" ) << option;
    EXPECT_EQ( command.arguments,
               (std::vector<std::string>{
                 "rm", option, "/store/tree?auth=value"}) ) << option;
  }
}

TEST( XrdClFSURLCommand, KeepsDryRunOptionsOutsideURLNormalization )
{
  NormalizedCommand command = Normalize(
    {"rm", "--dry-run", "-r",
     "root://root.example.org//store/tree?authz=value&signature=signed"} );
  ASSERT_EQ( command.result, XrdCl::ValidURLCommand );
  EXPECT_EQ( command.endpoint.GetProtocol(), "root" );
  EXPECT_EQ( command.arguments,
             (std::vector<std::string>{
               "rm", "--dry-run", "-r",
               "/store/tree?authz=value&signature=signed"}) );

  NormalizedCommand delimited = Normalize(
    {"rm", "--dry-run", "--",
     "root://root.example.org//store/-tree"} );
  ASSERT_EQ( delimited.result, XrdCl::ValidURLCommand );
  EXPECT_EQ( delimited.arguments,
             (std::vector<std::string>{
               "rm", "--dry-run", "--", "/store/-tree"}) );
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

TEST( XrdClFSURLCommand, TreatsXRootProtocolsAsRootAliases )
{
  const struct
  {
    const char *source;
    const char *destination;
    const char *expectedProtocol;
  } cases[] = {
    {"root://root.example.org//store/source",
     "xroot://root.example.org//store/destination", "root"},
    {"xroot://root.example.org//store/source",
     "root://root.example.org//store/destination", "root"},
    {"roots://root.example.org//store/source",
     "xroots://root.example.org//store/destination", "roots"},
    {"xroots://root.example.org//store/source",
     "roots://root.example.org//store/destination", "roots"}
  };

  for( const auto &testCase : cases )
  {
    NormalizedCommand command = Normalize(
      {"mv", testCase.source, testCase.destination} );

    ASSERT_EQ( command.result, XrdCl::ValidURLCommand );
    EXPECT_EQ( command.endpoint.GetProtocol(), testCase.expectedProtocol );
    EXPECT_EQ( command.arguments,
               (std::vector<std::string>{
                 "mv", "/store/source", "/store/destination"}) );
  }
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

TEST( XrdClFSURLCommand, RejectsMixedNamespaceEndpointsBeforeExecution )
{
  const std::vector<std::vector<std::string>> cases = {
    {"mkdir", "--parents", "-m", "0755",
              "root://one.example.org//one",
              "root://two.example.org//two"},
    {"mv", "root://one.example.org//one",
           "root://two.example.org//two"},
    {"rm", "root://one.example.org//one",
           "root://two.example.org//two"}
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

TEST( XrdClFSURLCommand, RejectsInvalidNamespaceURLOperands )
{
  const std::vector<std::vector<std::string>> cases = {
    {"mkdir", "-m", "0755", "root_://root.example.org//one"},
    {"mkdir", "--", "file:///tmp/one"},
    {"chmod", "0755", "root_://root.example.org//one"},
    {"chmod", "root_://root.example.org//one", "0755"},
    {"chmod", "file:///tmp/one", "0755"}
  };

  for( const std::vector<std::string> &original : cases )
  {
    std::vector<std::string> arguments( original );
    XrdCl::URL endpoint;
    std::string error;
    EXPECT_EQ( XrdCl::NormalizeFSURLCommand( arguments, endpoint, error ),
               XrdCl::InvalidURLCommand );
    EXPECT_EQ( error, "invalid remote URL operand" );
    EXPECT_EQ( arguments, original );
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
