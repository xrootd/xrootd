/****************************************************************
 *
 * Copyright (C) 2025, Pelican Project, Morgridge Institute for Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

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
