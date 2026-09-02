//------------------------------------------------------------------------------
// Copyright (c) 2026 by European Organization for Nuclear Research (CERN)
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
// GNU Lesser General Public License for more details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClXRootDResponses.hh"

#include <gtest/gtest.h>

#include <cstdint>
#include <sstream>

TEST( StatInfoTest, CopyPreservesChecksum )
{
  using namespace XrdCl;

  const std::uint32_t flags = StatInfo::IsReadable | StatInfo::IsWritable;
  std::ostringstream response;
  response << "42 5 " << flags
           << " 1700000000 1700000001 1700000002"
           << " 0750 alice analysis [ adler32:01234567 ]";

  StatInfo info;
  ASSERT_TRUE( info.ParseServerResponse( response.str().c_str() ) );
  ASSERT_TRUE( info.HasChecksum() );

  const StatInfo copiedInfo( info );
  EXPECT_TRUE( copiedInfo.HasChecksum() );
  EXPECT_EQ( copiedInfo.GetChecksum(), "adler32:01234567" );
}
