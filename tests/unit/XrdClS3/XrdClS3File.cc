/******************************************************************************/
/*                                                                            */
/*                        X r d C l S 3 F i l e . c c                         */
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

#include "XrdClS3/XrdClS3Factory.hh"
#include "XrdClS3/XrdClS3File.hh"

#include <XrdCl/XrdClDefaultEnv.hh>
#include <XProtocol/XProtocol.hh>

#include <gtest/gtest.h>

using namespace XrdClS3;

TEST(FileTest, RejectsWriteToDirectoryKey) {
    Factory::SetEndpoint("s3.amazonaws.com");
    Factory::SetUrlStyle("path");
    XrdClS3::File file(XrdCl::DefaultEnv::GetLog());

    auto directory = file.Open("s3://s3.amazonaws.com/bucket/foo/",
                               XrdCl::OpenFlags::Update | XrdCl::OpenFlags::Delete,
                               XrdCl::Access::None, nullptr, 0);
    EXPECT_EQ(directory.status, XrdCl::stError);
    EXPECT_EQ(directory.code, XrdCl::errErrorResponse);
    EXPECT_EQ(directory.errNo, kXR_isDirectory);

    auto empty = file.Open("s3://s3.amazonaws.com/bucket/",
                           XrdCl::OpenFlags::Write,
                           XrdCl::Access::None, nullptr, 0);
    EXPECT_EQ(empty.errNo, kXR_isDirectory);

    auto queried = file.Open("s3://s3.amazonaws.com/bucket/foo/?versionId=1",
                             XrdCl::OpenFlags::New,
                             XrdCl::Access::None, nullptr, 0);
    EXPECT_EQ(queried.errNo, kXR_isDirectory);
}
