/******************************************************************************/
/* Copyright (C) 2026, XRootD Collaboration                                  */
/******************************************************************************/

#include "../XrdClHttpCommon/TransferTest.hh"

#include <XrdCl/XrdClFileSystem.hh>

#include <memory>

class CurlMoveFixture : public TransferFixture {};

TEST_F(CurlMoveFixture, RenameWithinEndpoint)
{
    const std::string source = "/test/move_source";
    const std::string destination = "/test/move_destination";
    WritePattern(GetOriginURL() + source, 8, 'a', 2);

    XrdCl::FileSystem filesystem(GetOriginURL());
    ASSERT_TRUE(filesystem.SetProperty(
        "XrdClHttpQueryParam", "authz=" + GetWriteToken()));
    auto status = filesystem.Mv(source, destination, 10);
    ASSERT_TRUE(status.IsOK()) << status.ToStr();

    std::unique_ptr<XrdCl::StatInfo> info;
    XrdCl::StatInfo *raw_info = nullptr;
    status = filesystem.Stat(source, raw_info, 10);
    info.reset(raw_info);
    EXPECT_FALSE(status.IsOK());
    EXPECT_EQ(status.errNo, kXR_NotFound);

    raw_info = nullptr;
    status = filesystem.Stat(destination, raw_info, 10);
    info.reset(raw_info);
    ASSERT_TRUE(status.IsOK()) << status.ToStr();
    EXPECT_EQ(info->GetSize(), 8);
}
