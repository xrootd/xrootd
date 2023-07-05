//------------------------------------------------------------------------------
// Copyright (c) 2023 by European Organization for Nuclear Research (CERN)
// Author: Angelo Galavotti <agalavottib@gmail.com>
//------------------------------------------------------------------------------
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
//------------------------------------------------------------------------------

#ifndef __GTEST_XRD_HELPERS_HH__
#define __GTEST_XRD_HELPERS_HH__

#include <gtest/gtest.h>
#include <XrdCl/XrdClXRootDResponses.hh>
#include <cerrno>
#include <cstring>

#define GTEST_ASSERT_XRDST( x )                    \
{                                                    \
  XrdCl::XRootDStatus _st = x;                       \
  EXPECT_TRUE(_st.IsOK()) << "[" << #x << "]: " << _st.ToStr() << std::endl; \
}
