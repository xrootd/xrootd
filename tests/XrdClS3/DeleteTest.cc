/******************************************************************************/
/*                                                                            */
/* (c) 2026 by the XRootD Collaboration                                       */
/*                                                                            */
/* This file is part of the XrdClS3 client plugin for XRootD.                 */
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

#include "../XrdClHttpCommon/TransferTest.hh"

#include <XrdCl/XrdClFileSystem.hh>

class S3DeleteFixture : public TransferFixture {};

TEST_F(S3DeleteFixture, Test)
{
    std::string fname = "/test-bucket/delete_file";
    auto url = GetCacheURL() + fname;
    WritePattern(url, 8, 'a', 2);
    XrdCl::FileSystem fs(GetCacheURL());

    XrdCl::StatInfo *response{nullptr};
    auto st = fs.Stat(fname + "?authz=" + GetReadToken(), response, 10);
    ASSERT_TRUE(st.IsOK()) << "Failed to stat new file: " << st.ToString();
    delete response;

    st = fs.Rm(fname + "?authz=" + GetWriteToken(), 10);
    ASSERT_TRUE(st.IsOK()) << "Failed to remove file: " << st.ToString();

    response = nullptr;
    st = fs.Stat(fname + "?authz=" + GetReadToken(), response, 10);
    ASSERT_FALSE(st.IsOK()) << "Stat of removed file should have failed";
    ASSERT_EQ(st.errNo, kXR_NotFound);
}