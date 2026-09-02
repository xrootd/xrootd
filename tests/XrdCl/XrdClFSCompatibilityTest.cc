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

#include "XrdCl/XrdClFSCompatibility.hh"

#include <gtest/gtest.h>

TEST( XrdClFSCompatibility, ParsesLegacySymbolicAccessModes )
{
  XrdCl::Access::Mode mode = XrdCl::Access::None;

  EXPECT_EQ( XrdCl::ParseAccessMode( mode, "rwxr-x---" ),
             XrdCl::AccessModeFormat::Symbolic );
  EXPECT_EQ( static_cast<unsigned int>( mode ), 0750U );

  EXPECT_EQ( XrdCl::ParseAccessMode( mode, "---------" ),
             XrdCl::AccessModeFormat::Symbolic );
  EXPECT_EQ( mode, XrdCl::Access::None );

  // Preserve the historical parser's permissive ordering within each
  // owner/group/other permission triplet.
  EXPECT_EQ( XrdCl::ParseAccessMode( mode, "xwrxwrxwr" ),
             XrdCl::AccessModeFormat::Symbolic );
  EXPECT_EQ( static_cast<unsigned int>( mode ), 0777U );
}

TEST( XrdClFSCompatibility, ParsesUnsignedOctalAccessModes )
{
  struct TestCase
  {
    const char *input;
    unsigned int expected;
  };
  const TestCase cases[] = {
    {"0", 0000U},
    {"7", 0007U},
    {"75", 0075U},
    {"755", 0755U},
    {"0755", 0755U},
    {"000000755", 0755U},
    {"777", 0777U}
  };

  for( const TestCase &testCase : cases )
  {
    XrdCl::Access::Mode mode = XrdCl::Access::None;
    EXPECT_EQ( XrdCl::ParseAccessMode( mode, testCase.input ),
               XrdCl::AccessModeFormat::Octal ) << testCase.input;
    EXPECT_EQ( static_cast<unsigned int>( mode ), testCase.expected )
      << testCase.input;
  }
}

TEST( XrdClFSCompatibility, RejectsInvalidAccessModes )
{
  const char *invalidModes[] = {
    "", "+755", "-755", "758", "08", "1000", "7777", "755x",
    " 755", "755 ", "rwxr-x--", "rwxr-x---x", "rwxr-z---"
  };

  for( const char *input : invalidModes )
  {
    XrdCl::Access::Mode mode = XrdCl::Access::UR;
    EXPECT_EQ( XrdCl::ParseAccessMode( mode, input ),
               XrdCl::AccessModeFormat::Invalid ) << input;
    EXPECT_EQ( mode, XrdCl::Access::None ) << input;
  }
}

TEST( XrdClFSCompatibility, IdentifiesWebDAVProtocols )
{
  for( const char *protocol : {"http", "https", "dav", "davs", "HTTPS"} )
    EXPECT_TRUE( XrdCl::IsWebDAVProtocol( protocol ) ) << protocol;

  for( const char *protocol : {"root", "roots", "file", "stdio", ""} )
    EXPECT_FALSE( XrdCl::IsWebDAVProtocol( protocol ) ) << protocol;
}

TEST( XrdClFSCompatibility, EvaluatesNonRecursiveRemovalSafety )
{
  using XrdCl::EvaluateNonRecursiveRemoval;
  using XrdCl::NonRecursiveRemoval;
  using XrdCl::NonRecursiveRemovalDecision;

  EXPECT_EQ( EvaluateNonRecursiveRemoval(
               NonRecursiveRemoval::File, false, 0 ),
             NonRecursiveRemovalDecision::Allow );
  EXPECT_EQ( EvaluateNonRecursiveRemoval(
               NonRecursiveRemoval::File, true, 0 ),
             NonRecursiveRemovalDecision::IsDirectory );
  EXPECT_EQ( EvaluateNonRecursiveRemoval(
               NonRecursiveRemoval::Directory, false, 0 ),
             NonRecursiveRemovalDecision::NotDirectory );
  EXPECT_EQ( EvaluateNonRecursiveRemoval(
               NonRecursiveRemoval::Directory, true, 0 ),
             NonRecursiveRemovalDecision::Allow );
  EXPECT_EQ( EvaluateNonRecursiveRemoval(
               NonRecursiveRemoval::Directory, true, 1 ),
             NonRecursiveRemovalDecision::NotEmpty );
}

TEST( XrdClFSCompatibility, RequiresCompleteMetadataBeforeRemoval )
{
  EXPECT_TRUE( XrdCl::IsCompleteSuccess( XrdCl::XRootDStatus() ) );
  EXPECT_FALSE( XrdCl::IsCompleteSuccess(
    XrdCl::XRootDStatus( XrdCl::stOK, XrdCl::suPartial ) ) );
  EXPECT_FALSE( XrdCl::IsCompleteSuccess(
    XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errInvalidResponse ) ) );
}

TEST( XrdClFSCompatibility, MapsGFALDiskAndTapeStatus )
{
  EXPECT_STREQ( XrdCl::GetGFALFileStatus( false, false ), "ONLINE" );
  EXPECT_STREQ( XrdCl::GetGFALFileStatus( true, true ), "NEARLINE" );
  EXPECT_STREQ( XrdCl::GetGFALFileStatus( false, true ),
                "ONLINE_AND_NEARLINE" );
  EXPECT_STREQ( XrdCl::GetGFALFileStatus( true, false ), "UNKNOWN" );
}
TEST( XrdClFSCompatibility, MapsGFALTapeRestLocality )
{
  EXPECT_STREQ( XrdCl::GetGFALTapeFileStatus( "DISK" ), "ONLINE" );
  EXPECT_STREQ( XrdCl::GetGFALTapeFileStatus( "TAPE" ), "NEARLINE" );
  EXPECT_STREQ( XrdCl::GetGFALTapeFileStatus( "DISK_AND_TAPE" ),
                "ONLINE_AND_NEARLINE" );
  EXPECT_EQ( XrdCl::GetGFALTapeFileStatus( "UNAVAILABLE" ), nullptr );
}

TEST( XrdClFSCompatibility, FormatsGFALXAttrFailuresWithoutServerDetails )
{
  const XrdCl::XRootDStatus status(
    XrdCl::stError, XrdCl::errErrorResponse, kXR_Unsupported,
    "Unable to query xattrs.\n/eos/pilot/private/path" );

  EXPECT_EQ( XrdCl::FormatGFALXAttrFailure( "xroot.xattr", status ),
             "Failed to get the xattr \"xroot.xattr\" "
             "(Operation not supported)" );
}

TEST( XrdClFSCompatibility, FormatsGFALXAttrFailuresWithoutErrno )
{
  const XrdCl::XRootDStatus status(
    XrdCl::stError, XrdCl::errNotSupported, 0,
    "Protocol-specific detail" );

  EXPECT_EQ( XrdCl::FormatGFALXAttrFailure( "xroot.xattr", status ),
             "Failed to get the xattr \"xroot.xattr\" "
             "(Operation not supported)" );
}

TEST( XrdClFSCompatibility, FormatsMissingXAttrsPortably )
{
  const XrdCl::XRootDStatus status(
    XrdCl::stError, XrdCl::errErrorResponse, kXR_AttrNotFound,
    "Attribute not found" );

  EXPECT_EQ( XrdCl::FormatGFALXAttrFailure( "user.missing", status ),
             "Failed to get the xattr \"user.missing\" "
             "(No data available)" );
}
