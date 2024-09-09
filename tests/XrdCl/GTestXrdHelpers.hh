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

/** Shows the code that we are asserting and its value in the final evaluation. */
#define ASSERT_XRDST_OK( x )                                                                     \
{                                                                                                \
  XrdCl::XRootDStatus _st = x;                                                                   \
  ASSERT_TRUE(_st.IsOK()) << "[" << #x << "]: " << _st.ToStr() << std::endl;                     \
}

/** Shows the code that we are asserting and asserts that its execution is throwing an error. */
#define ASSERT_XRDST_NOTOK( x, err )                                                             \
{                                                                                                \
  XrdCl::XRootDStatus _st = x;                                                                   \
  ASSERT_TRUE(!_st.IsOK() && _st.code == err) << "[" << #x << "]: " << _st.ToStr() << std::endl; \
}

/** Shows the code that we are asserting and its value in the final evaluation. */
#define EXPECT_XRDST_OK( x )                                                                     \
{                                                                                                \
  XrdCl::XRootDStatus _st = x;                                                                   \
  EXPECT_TRUE(_st.IsOK()) << "[" << #x << "]: " << _st.ToStr() << std::endl;                     \
}

/** Shows the code that we are asserting and asserts that its execution is throwing an error. */
#define EXPECT_XRDST_NOTOK( x, err )                                                             \
{                                                                                                \
  XrdCl::XRootDStatus _st = x;                                                                   \
  EXPECT_TRUE(!_st.IsOK() && _st.code == err) << "[" << #x << "]: " << _st.ToStr() << std::endl; \
}

/** Shows the code that we are asserting and its error number. */
#define EXPECT_ERRNO_OK( x )                                                                     \
{                                                                                                \
  EXPECT_TRUE(x) << "[" << #x << "]: " << strerror(errno) << std::endl;                          \
}

/** Shows the code that we are asserting and its error number, in a thread-safe manner. */
#define EXPECT_PTHREAD_OK( x )                                                                   \
{                                                                                                \
  errno = x;                                                                                     \
  EXPECT_TRUE(errno == 0) << "[" << #x << "]: " << strerror(errno) << std::endl;                 \
}

#endif // __GTEST_XRD_HELPERS_HH__
