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

class CurlDeleteFixture : public TransferFixture {};

TEST_F(CurlDeleteFixture, Test)
{
    std::string fname = "/test/delete_file";
    auto url = GetOriginURL() + fname;
    WritePattern(url, 8, 'a', 2);
    XrdCl::FileSystem fs(GetOriginURL());

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