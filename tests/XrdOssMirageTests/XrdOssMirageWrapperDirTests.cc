#include "XrdOssMirageWrapperFixture.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

using testing::_;
using testing::Return;
using testing::StrEq;

//
// Directory level routing: the back end is chosen on Opendir() from the path
// and every subsequent call is forwarded to it. XrdOssMirage does not support
// directories, so mirage-routed calls surface -ENOTSUP rather than being
// forwarded to the stacked plugin.
//
class XrdOssMirageWrapperDirFixture : public XrdOssMirageWrapperFixture
{
protected:
    void SetUp() override
    {
        dir.reset(wrapper.newDir(nullptr));
    }

    std::unique_ptr<XrdOssDF> dir;
};

TEST_F(XrdOssMirageWrapperDirFixture, UnopenedDirDefaultsToStacked)
{
    EXPECT_CALL(*mock_df, Readdir(_, _)).WillOnce(Return(STACKED_RC));

    char buff[256];
    ASSERT_EQ(STACKED_RC, dir->Readdir(buff, sizeof(buff)));
}

TEST_F(XrdOssMirageWrapperDirFixture, OpendirUnderPrefixRoutesToMirage)
{
    EXPECT_CALL(*mock_df, Opendir).Times(0);

    ASSERT_EQ(-ENOTSUP, dir->Opendir("/mirage/dir", env));
}

TEST_F(XrdOssMirageWrapperDirFixture, OpendirOutsidePrefixIsForwardedToStacked)
{
    EXPECT_CALL(*mock_df, Opendir(StrEq("/stacked/dir"), _)).WillOnce(Return(STACKED_RC));

    ASSERT_EQ(STACKED_RC, dir->Opendir("/stacked/dir", env));
}

TEST_F(XrdOssMirageWrapperDirFixture, ReaddirAfterMirageOpendirUsesMirage)
{
    EXPECT_CALL(*mock_df, Readdir).Times(0);

    dir->Opendir("/mirage/dir", env);

    char buff[256];
    ASSERT_EQ(-ENOTSUP, dir->Readdir(buff, sizeof(buff)));
}

TEST_F(XrdOssMirageWrapperDirFixture, ReaddirAfterStackedOpendirUsesStacked)
{
    EXPECT_CALL(*mock_df, Readdir(_, _)).WillOnce(Return(STACKED_RC));

    dir->Opendir("/stacked/dir", env);

    char buff[256];
    ASSERT_EQ(STACKED_RC, dir->Readdir(buff, sizeof(buff)));
}

TEST_F(XrdOssMirageWrapperDirFixture, CloseAfterMirageOpendirUsesMirage)
{
    EXPECT_CALL(*mock_df, Close).Times(0);

    dir->Opendir("/mirage/dir", env);

    ASSERT_EQ(-ENOTSUP, dir->Close());
}

TEST_F(XrdOssMirageWrapperDirFixture, CloseAfterStackedOpendirUsesStacked)
{
    EXPECT_CALL(*mock_df, Close(_)).WillOnce(Return(STACKED_RC));

    dir->Opendir("/stacked/dir", env);

    ASSERT_EQ(STACKED_RC, dir->Close());
}
