/******************************************************************************/
/*                                                                            */
/*              X r d C l H t t p H e a d e r P a r s e r T e s t           */
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

#include "XrdClHttp/XrdClHttpUtil.hh"

#include <gtest/gtest.h>

TEST(HeaderParser, SkipsInvalidResponseFieldNames)
{
  XrdClHttp::HeaderParser parser;
  EXPECT_TRUE(parser.Parse("HTTP/1.1 200 OK\r\n"));
  EXPECT_TRUE(parser.Parse("application/type: json\r\n"));
  EXPECT_TRUE(parser.Parse("Content-Type: application/json\r\n"));
  EXPECT_TRUE(parser.Parse("Content-Length: 2\r\n"));
  EXPECT_TRUE(parser.Parse("\r\n"));

  EXPECT_TRUE(parser.HeadersDone());
  EXPECT_EQ(parser.GetStatusCode(), 200);
  EXPECT_EQ(parser.GetContentLength(), 2);

  auto headers = parser.MoveHeaders();
  EXPECT_EQ(headers.count("application/type"), 0u);
  const auto contentType = headers.find("Content-Type");
  ASSERT_NE(contentType, headers.end());
  ASSERT_EQ(contentType->second.size(), 1u);
  EXPECT_EQ(contentType->second.front(), "application/json");
}
