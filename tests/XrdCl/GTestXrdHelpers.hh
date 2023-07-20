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

/** @brief Equivalent of CPPUNIT_ASSERT_XRDST
 *
 * Shows the code that we are asserting and its value
 * in the final evaluation.
 */
#define GTEST_ASSERT_XRDST( x )                                                                  \
{                                                                                                \
  XrdCl::XRootDStatus _st = x;                                                                   \
  EXPECT_TRUE(_st.IsOK()) << "[" << #x << "]: " << _st.ToStr() << std::endl;                     \
}

/** @brief Equivalent of CPPUNIT_ASSERT_XRDST_NOTOK
 *
 * Shows the code that we are asserting and asserts that its
 * execution is throwing an error.
 */
#define GTEST_ASSERT_XRDST_NOTOK( x, err )                                                       \
{                                                                                                \
  XrdCl::XRootDStatus _st = x;                                                                   \
  EXPECT_TRUE(!_st.IsOK() && _st.code == err) << "[" << #x << "]: " << _st.ToStr() << std::endl; \
}

/** @brief Equivalent of CPPUNIT_ASSERT_ERRNO
 *
 * Shows the code that we are asserting and its error
 * number.
 */
#define GTEST_ASSERT_ERRNO( x )                                                                  \
{                                                                                                \
  EXPECT_TRUE(x) << "[" << #x << "]: " << strerror(errno) << std::endl;                          \
}

/** @brief Equivalent of GTEST_ASSERT_PTHREAD
 *
 * Shows the code that we are asserting and its error
 * number, in a thread-safe manner.
 */
#define GTEST_ASSERT_PTHREAD( x )                                                                \
{                                                                                                \
  errno = x;                                                                                     \
  EXPECT_TRUE(errno == 0) << "[" << #x << "]: " << strerror(errno) << std::endl;                 \
}

#endif // __GTEST_XRD_HELPERS_HH__
