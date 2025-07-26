/******************************************************************************/
/* Copyright (C) 2025, Pelican Project, Morgridge Institute for Research      */
/*                                                                            */
/* This file is part of the XrdClCurl client plugin for XRootD.               */
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
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#include "../XrdClCurlCommon/TransferTest.hh"

#include <XrdCl/XrdClFileSystem.hh>

#include <memory>

class CurlMkcolFixture : public TransferFixture {};

TEST_F(CurlMkcolFixture, Test)
{
    std::string fname = "/test/mkcol_directory";
    XrdCl::FileSystem fs(GetOriginURL());

    std::unique_ptr<XrdCl::StatInfo> response_ptr;
    XrdCl::StatInfo *response{nullptr};
    auto st = fs.Stat(fname + "?authz=" + GetReadToken(), response, 10);
    response_ptr.reset(response);
    ASSERT_FALSE(st.IsOK()) << "New directory should not already exist: " << st.ToString();
    ASSERT_EQ(st.errNo, kXR_NotFound);

    st = fs.MkDir(fname + "?authz=" + GetWriteToken(), XrdCl::MkDirFlags::None, XrdCl::Access::None, 10);
    ASSERT_TRUE(st.IsOK()) << "Failed to create directory: " << st.ToString();

    response = nullptr;
    st = fs.Stat(fname + "?authz=" + GetReadToken(), response, 10);
    response_ptr.reset(response);
    ASSERT_TRUE(st.IsOK()) << "Stat of created directory failed: " << st.ToString();
}

TEST_F(CurlMkcolFixture, MkpathTest)
{
    std::string fname = "/test/mkpath_directory/subdir1/subdir2";
    XrdCl::FileSystem fs(GetOriginURL());

    std::unique_ptr<XrdCl::StatInfo> response_ptr;
    XrdCl::StatInfo *response{nullptr};
    auto st = fs.Stat(fname + "?authz=" + GetReadToken(), response, 10);
    response_ptr.reset(response);
    ASSERT_FALSE(st.IsOK()) << "New directory should not already exist: " << st.ToString();
    ASSERT_EQ(st.errNo, kXR_NotFound);

    st = fs.MkDir(fname + "?authz=" + GetWriteToken(), XrdCl::MkDirFlags::MakePath, XrdCl::Access::None, 10);
    ASSERT_TRUE(st.IsOK()) << "Failed to create directory: " << st.ToString();

    response = nullptr;
    st = fs.Stat(fname + "?authz=" + GetReadToken(), response, 10);
    response_ptr.reset(response);
    ASSERT_TRUE(st.IsOK()) << "Stat of created directory failed: " << st.ToString();
}
