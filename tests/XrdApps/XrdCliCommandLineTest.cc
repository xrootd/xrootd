/******************************************************************************/
/*                                                                            */
/*              X r d C l i C o m m a n d L i n e T e s t . c c             */
/*                                                                            */
/* (c) 2026 by the XRootD Collaboration                                       */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/******************************************************************************/

#include "XrdApps/XrdCliCommandLine.hh"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
  using XrdApps::CommandLineAction;
  using XrdApps::CommandLineResult;
  using XrdApps::ParseCommandLine;

  const std::string *EnvironmentValue( const CommandLineResult &result,
                                       const std::string &name )
  {
    for( const auto &entry : result.invocation.environment )
    {
      if( entry.first == name )
        return &entry.second;
    }
    return nullptr;
  }

  bool EnvironmentIsUnset( const CommandLineResult &result,
                           const std::string &name )
  {
    for( const std::string &entry : result.invocation.environmentToUnset )
    {
      if( entry == name )
        return true;
    }
    return false;
  }

  TEST( XrdCliCommandLine, ShowsTopLevelHelpWithoutArguments )
  {
    const CommandLineResult result = ParseCommandLine( {} );
    EXPECT_EQ( result.action, CommandLineAction::Help );
    EXPECT_TRUE( result.command.empty() );
  }

  TEST( XrdCliCommandLine, DelegatesStatToXrdfs )
  {
    const CommandLineResult result =
      ParseCommandLine( {"stat", "root://host//file"} );

    EXPECT_EQ( result.action, CommandLineAction::Execute );
    EXPECT_EQ( result.invocation.executable, "xrdfs" );
    EXPECT_EQ( result.invocation.arguments,
               (std::vector<std::string>{"stat", "root://host//file"}) );
  }

  TEST( XrdCliCommandLine, TranslatesCommonOptionsToEnvironment )
  {
    const CommandLineResult result = ParseCommandLine(
      {"stat", "-vv", "--timeout=+0030", "--cert", "/tmp/cert",
       "--key=/tmp/key", "-4", "--log-file", "/tmp/xrd.log",
       "root://host//file"} );

    ASSERT_EQ( result.action, CommandLineAction::Execute );
    ASSERT_NE( EnvironmentValue( result, "XRD_LOGLEVEL" ), nullptr );
    EXPECT_EQ( *EnvironmentValue( result, "XRD_LOGLEVEL" ), "Info" );
    EXPECT_EQ( *EnvironmentValue( result, "XRD_REQUESTTIMEOUT" ), "30" );
    EXPECT_EQ( *EnvironmentValue( result, "X509_USER_CERT" ), "/tmp/cert" );
    EXPECT_EQ( *EnvironmentValue( result, "XRD_HTTPCLIENTCERTFILE" ),
               "/tmp/cert" );
    EXPECT_EQ( *EnvironmentValue( result, "X509_USER_KEY" ), "/tmp/key" );
    EXPECT_EQ( *EnvironmentValue( result, "XRD_HTTPCLIENTKEYFILE" ),
               "/tmp/key" );
    EXPECT_EQ( *EnvironmentValue( result, "XRD_NETWORKSTACK" ), "IPv4" );
    EXPECT_EQ( *EnvironmentValue( result, "XRD_LOGFILE" ), "/tmp/xrd.log" );
    EXPECT_TRUE( EnvironmentIsUnset( result, "X509_USER_PROXY" ) );
    EXPECT_TRUE( EnvironmentIsUnset( result, "XrdSecCREDS" ) );
  }

  TEST( XrdCliCommandLine, DefaultsCredentialKeyToCertificate )
  {
    const CommandLineResult result = ParseCommandLine(
      {"stat", "--cert", "/tmp/proxy.pem", "root://host//file"} );

    ASSERT_EQ( result.action, CommandLineAction::Execute );
    EXPECT_EQ( *EnvironmentValue( result, "X509_USER_CERT" ),
               "/tmp/proxy.pem" );
    EXPECT_EQ( *EnvironmentValue( result, "X509_USER_KEY" ),
               "/tmp/proxy.pem" );
    EXPECT_EQ( *EnvironmentValue( result, "XRD_HTTPCLIENTCERTFILE" ),
               "/tmp/proxy.pem" );
    EXPECT_EQ( *EnvironmentValue( result, "XRD_HTTPCLIENTKEYFILE" ),
               "/tmp/proxy.pem" );
    EXPECT_EQ( *EnvironmentValue( result, "XrdSecGSIUSERCERT" ),
               "/tmp/proxy.pem" );
    EXPECT_EQ( *EnvironmentValue( result, "XrdSecGSIUSERKEY" ),
               "/tmp/proxy.pem" );
    EXPECT_EQ( *EnvironmentValue( result, "XrdSecGSIUSERPROXY" ),
               "/tmp/proxy.pem" );
    EXPECT_TRUE( EnvironmentIsUnset( result, "X509_USER_PROXY" ) );
    EXPECT_TRUE( EnvironmentIsUnset( result, "XrdSecCREDS" ) );
  }

  TEST( XrdCliCommandLine, AcceptsIgnoredCommonOptionsWithWarnings )
  {
    const CommandLineResult result = ParseCommandLine(
      {"stat", "-D", "X=Y", "--client-info", "workflow",
       "root://host//file"} );

    ASSERT_EQ( result.action, CommandLineAction::Execute );
    EXPECT_EQ( result.invocation.warnings.size(), 2U );
  }

  TEST( XrdCliCommandLine, DropsCatBytesAndPreservesMultipleUrls )
  {
    const CommandLineResult result = ParseCommandLine(
      {"cat", "-b", "root://host//one", "root://host//two"} );

    EXPECT_EQ( result.action, CommandLineAction::Execute );
    EXPECT_EQ( result.invocation.arguments,
               (std::vector<std::string>{"cat", "root://host//one",
                                         "root://host//two"}) );
  }

  TEST( XrdCliCommandLine, TranslatesCombinedLsOptions )
  {
    const CommandLineResult result =
      ParseCommandLine( {"ls", "-alH", "root://host//directory"} );

    EXPECT_EQ( result.action, CommandLineAction::Execute );
    EXPECT_EQ( result.invocation.arguments,
               (std::vector<std::string>{"ls", "-l", "-h",
                                         "root://host//directory"}) );
  }

  TEST( XrdCliCommandLine, DelegatesXAttrListAndGetToXrdfs )
  {
    const CommandLineResult list =
      ParseCommandLine( {"xattr", "root://host//file"} );
    const CommandLineResult get =
      ParseCommandLine( {"xattr", "root://host//file", "user.name"} );

    EXPECT_EQ( list.action, CommandLineAction::Execute );
    EXPECT_EQ( list.invocation.arguments,
               (std::vector<std::string>{"xattr", "root://host//file",
                                         "list"}) );
    EXPECT_EQ( get.action, CommandLineAction::Execute );
    EXPECT_EQ( get.invocation.arguments,
               (std::vector<std::string>{"xattr", "root://host//file",
                                         "get", "user.name"}) );
  }

  TEST( XrdCliCommandLine, RejectsXAttrMutation )
  {
    const CommandLineResult result =
      ParseCommandLine( {"xattr", "root://host//file", "user.name=value"} );

    EXPECT_EQ( result.action, CommandLineAction::Error );
    EXPECT_NE( result.error.find( "read-only" ), std::string::npos );
  }

  TEST( XrdCliCommandLine, AcceptsNonColoringLsModes )
  {
    const CommandLineResult result =
      ParseCommandLine( {"ls", "root://host//directory", "--color=auto"} );

    EXPECT_EQ( result.action, CommandLineAction::Execute );
    EXPECT_EQ( result.invocation.arguments,
               (std::vector<std::string>{"ls", "root://host//directory"}) );
  }

  TEST( XrdCliCommandLine, RejectsLsFormattingThatXrdfsCannotProvide )
  {
    const CommandLineResult result =
      ParseCommandLine( {"ls", "--time-style", "full-iso",
                         "root://host//directory"} );

    EXPECT_EQ( result.action, CommandLineAction::Error );
    EXPECT_NE( result.error.find( "native xrdfs output support" ),
               std::string::npos );
  }

  TEST( XrdCliCommandLine, TranslatesCopyOptions )
  {
    const CommandLineResult result = ParseCommandLine(
      {"copy", "-fpr", "-n04", "-T0030", "-K", "ADLER32:01234567",
       "root://host//file", "/tmp/file"} );

    EXPECT_EQ( result.action, CommandLineAction::Execute );
    EXPECT_EQ( result.invocation.executable, "xrdcp" );
    EXPECT_EQ( result.invocation.arguments,
               (std::vector<std::string>{"--force", "--path", "--recursive",
                 "--streams", "4", "--cksum", "adler32:01234567",
                 "root://host//file", "/tmp/file"}) );
    EXPECT_EQ( *EnvironmentValue( result, "XRD_CPTIMEOUT" ), "30" );
  }

  TEST( XrdCliCommandLine, DeduplicatesFlagsAndUsesLastOptionValue )
  {
    const CommandLineResult listing =
      ParseCommandLine( {"ls", "-llHH", "root://host//directory"} );
    const CommandLineResult copy = ParseCommandLine(
      {"copy", "-K", "MD5", "-K", "ADLER32", "--from-file", "one",
       "--from-file", "two", "/tmp/output"} );

    EXPECT_EQ( listing.invocation.arguments,
               (std::vector<std::string>{"ls", "-l", "-h",
                                         "root://host//directory"}) );
    EXPECT_EQ( copy.invocation.arguments,
               (std::vector<std::string>{"--cksum", "adler32", "--infiles",
                                         "two", "/tmp/output"}) );
  }

  TEST( XrdCliCommandLine, SupportsCpAlias )
  {
    const CommandLineResult result =
      ParseCommandLine( {"cp", "root://host//file", "/tmp/file"} );

    EXPECT_EQ( result.action, CommandLineAction::Execute );
    EXPECT_EQ( result.command, "copy" );
    EXPECT_EQ( result.invocation.executable, "xrdcp" );
  }

  TEST( XrdCliCommandLine, TranslatesCopyFromFile )
  {
    const CommandLineResult result =
      ParseCommandLine( {"copy", "--from-file=sources.txt", "/tmp/output"} );

    EXPECT_EQ( result.action, CommandLineAction::Execute );
    EXPECT_EQ( result.invocation.arguments,
               (std::vector<std::string>{"--infiles", "sources.txt",
                                         "/tmp/output"}) );
  }

  TEST( XrdCliCommandLine, RejectsChainedCopyDestinations )
  {
    const CommandLineResult result = ParseCommandLine(
      {"copy", "root://host//source", "/tmp/one", "/tmp/two"} );

    EXPECT_EQ( result.action, CommandLineAction::Error );
    EXPECT_NE( result.error.find( "chained destinations" ), std::string::npos );
  }

  TEST( XrdCliCommandLine, RejectsUnsupportedCopyOptions )
  {
    const CommandLineResult result = ParseCommandLine(
      {"copy", "--tcp-buffersize", "4096", "root://host//source",
       "/tmp/output"} );

    EXPECT_EQ( result.action, CommandLineAction::Error );
    EXPECT_NE( result.error.find( "no xrdcp equivalent" ), std::string::npos );
  }

  TEST( XrdCliCommandLine, RejectsInvalidTimeout )
  {
    const CommandLineResult result =
      ParseCommandLine( {"stat", "-t-1", "root://host//file"} );

    EXPECT_EQ( result.action, CommandLineAction::Error );
    EXPECT_NE( result.error.find( "non-negative integer" ), std::string::npos );
  }

  TEST( XrdCliCommandLine, AcceptsDisabledTimeouts )
  {
    const CommandLineResult common = ParseCommandLine(
      {"stat", "-t30", "-t0", "root://host//file"} );
    const CommandLineResult copy = ParseCommandLine(
      {"copy", "-n4", "-n0", "-T0", "root://host//file", "/tmp/file"} );

    EXPECT_EQ( common.action, CommandLineAction::Execute );
    EXPECT_EQ( EnvironmentValue( common, "XRD_REQUESTTIMEOUT" ), nullptr );
    ASSERT_NE( EnvironmentValue( copy, "XRD_CPTIMEOUT" ), nullptr );
    EXPECT_EQ( *EnvironmentValue( copy, "XRD_CPTIMEOUT" ), "0" );
    EXPECT_EQ( copy.invocation.arguments,
               (std::vector<std::string>{"root://host//file", "/tmp/file"}) );
  }

  TEST( XrdCliCommandLine, DoesNotConsumeOptionsAsMissingValues )
  {
    const CommandLineResult certificate = ParseCommandLine(
      {"stat", "--cert", "--help", "root://host//file"} );
    const CommandLineResult fromFile = ParseCommandLine(
      {"copy", "--from-file", "--force", "/tmp/output"} );

    EXPECT_EQ( certificate.action, CommandLineAction::Error );
    EXPECT_NE( certificate.error.find( "requires an argument" ),
               std::string::npos );
    EXPECT_EQ( fromFile.action, CommandLineAction::Error );
    EXPECT_NE( fromFile.error.find( "requires an argument" ),
               std::string::npos );
  }

  TEST( XrdCliCommandLine, PreservesDashPrefixedCopyOperandsAfterDelimiter )
  {
    const CommandLineResult result =
      ParseCommandLine( {"copy", "--", "-source", "-destination"} );

    EXPECT_EQ( result.action, CommandLineAction::Execute );
    EXPECT_EQ( result.invocation.arguments,
               (std::vector<std::string>{"./-source", "./-destination"}) );
  }

  TEST( XrdCliCommandLine, RejectsNonUrlFilesystemOperands )
  {
    const CommandLineResult stat = ParseCommandLine( {"stat", "/tmp/file"} );
    const CommandLineResult cat = ParseCommandLine(
      {"cat", "root://host//one", "relative-file"} );

    EXPECT_EQ( stat.action, CommandLineAction::Error );
    EXPECT_NE( stat.error.find( "complete remote URL" ), std::string::npos );
    EXPECT_EQ( cat.action, CommandLineAction::Error );
    EXPECT_NE( cat.error.find( "complete remote URLs" ), std::string::npos );
  }

  TEST( XrdCliCommandLine, RejectsConflictingNetworkStacks )
  {
    const CommandLineResult result =
      ParseCommandLine( {"cat", "-4", "-6", "root://host//file"} );

    EXPECT_EQ( result.action, CommandLineAction::Error );
    EXPECT_NE( result.error.find( "mutually exclusive" ), std::string::npos );
  }

  TEST( XrdCliCommandLine, ShowsPerCommandHelpWithoutOperands )
  {
    const CommandLineResult result = ParseCommandLine( {"ls", "--help"} );
    EXPECT_EQ( result.action, CommandLineAction::Help );
    EXPECT_EQ( result.command, "ls" );
  }

  TEST( XrdCliCommandLine, RejectsUnknownCommands )
  {
    const CommandLineResult result =
      ParseCommandLine( {"mkdir", "root://host//directory"} );

    EXPECT_EQ( result.action, CommandLineAction::Error );
    EXPECT_NE( result.error.find( "unknown command" ), std::string::npos );
  }
}
