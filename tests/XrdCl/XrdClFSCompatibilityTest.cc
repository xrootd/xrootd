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

TEST( XrdClFSCompatibility, MapsGFALDiskAndTapeStatus )
{
  EXPECT_STREQ( XrdCl::GetGFALFileStatus( false, false ), "ONLINE" );
  EXPECT_STREQ( XrdCl::GetGFALFileStatus( true, true ), "NEARLINE" );
  EXPECT_STREQ( XrdCl::GetGFALFileStatus( false, true ),
                "ONLINE_AND_NEARLINE" );
  EXPECT_STREQ( XrdCl::GetGFALFileStatus( true, false ), "UNKNOWN" );
}
