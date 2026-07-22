#include "XrdOssMirageWrapperFixture.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

using testing::_;
using testing::Return;
using testing::StrEq;

//
// Prefix configuration: the prefix comes from the osslib parameters and is
// immutable once the wrapper has been instantiated.
//

TEST(XrdOssMirageWrapperPrefix, ResolvesToConfiguredPrefix)
{
    ASSERT_EQ(WRAPPER_PREFIX, mirage_prefix());
}

TEST(XrdOssMirageWrapperPrefix, IsImmutableOnceSet)
{
    const std::string before = mirage_prefix();
    const std::string after  = mirage_prefix("/some/other/prefix/");

    ASSERT_EQ(before, after);
}

TEST(XrdOssMirageWrapperPrefix, PathsUnderPrefixUseMirage)
{
    ASSERT_TRUE(is_mirage_path("/mirage/file"));
    ASSERT_TRUE(is_mirage_path("/mirage/sub/dir/file"));
}

TEST(XrdOssMirageWrapperPrefix, PathsOutsidePrefixUseStacked)
{
    ASSERT_FALSE(is_mirage_path("/elsewhere/file"));
    ASSERT_FALSE(is_mirage_path("/"));
    ASSERT_FALSE(is_mirage_path("/mirag"));
}

//
// File-system level routing: path-bearing methods are served by XrdOssMirage
// when the path is under the prefix and forwarded to the stacked plugin
// otherwise.
//

class XrdOssMirageWrapperFileSystemFixture : public XrdOssMirageWrapperFixture
{
};

TEST_F(XrdOssMirageWrapperFileSystemFixture, CreateUnderPrefixIsHandledByMirage)
{
    EXPECT_CALL(mock_oss, Create).Times(0);

    ASSERT_EQ(XrdOssOK, wrapper.Create(nullptr, "/mirage/newfile", {}, env, XRDOSS_new));
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, CreateOutsidePrefixIsForwardedToStacked)
{
    EXPECT_CALL(mock_oss, Create(_, StrEq("/stacked/newfile"), _, _, XRDOSS_new))
        .WillOnce(Return(STACKED_RC));

    ASSERT_EQ(STACKED_RC, wrapper.Create(nullptr, "/stacked/newfile", {}, env, XRDOSS_new));
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, StatUnderPrefixForNonExistentReturnsNOENT)
{
    EXPECT_CALL(mock_oss, Stat).Times(0);

    struct stat buff{};
    ASSERT_EQ(-ENOENT, wrapper.Stat("/mirage/inexistent", &buff));
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, StatOutsidePrefixIsForwardedToStacked)
{
    EXPECT_CALL(mock_oss, Stat(StrEq("/stacked/file"), _, _, _)).WillOnce(Return(STACKED_RC));

    struct stat buff{};
    ASSERT_EQ(STACKED_RC, wrapper.Stat("/stacked/file", &buff));
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, CreateThenStatUnderPrefixReturnsMirageSize)
{
    EXPECT_CALL(mock_oss, Create).Times(0);
    EXPECT_CALL(mock_oss, Truncate).Times(0);
    EXPECT_CALL(mock_oss, Stat).Times(0);

    wrapper.Create(nullptr, "/mirage/file", {}, env, XRDOSS_new);
    wrapper.Truncate("/mirage/file", 1234);

    struct stat buff{};
    wrapper.Stat("/mirage/file", &buff);

    ASSERT_EQ(1234, buff.st_size);
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, TruncateUnderPrefixIsHandledByMirage)
{
    EXPECT_CALL(mock_oss, Truncate).Times(0);

    wrapper.Create(nullptr, "/mirage/file", {}, env, XRDOSS_new);

    ASSERT_EQ(XrdOssOK, wrapper.Truncate("/mirage/file", 1000));
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, TruncateOutsidePrefixIsForwardedToStacked)
{
    EXPECT_CALL(mock_oss, Truncate(StrEq("/stacked/file"), _, _)).WillOnce(Return(STACKED_RC));

    ASSERT_EQ(STACKED_RC, wrapper.Truncate("/stacked/file", 1000));
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, UnlinkUnderPrefixIsHandledByMirage)
{
    EXPECT_CALL(mock_oss, Unlink).Times(0);

    wrapper.Create(nullptr, "/mirage/file", {}, env, XRDOSS_new);

    ASSERT_EQ(XrdOssOK, wrapper.Unlink("/mirage/file"));
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, UnlinkOutsidePrefixIsForwardedToStacked)
{
    EXPECT_CALL(mock_oss, Unlink(StrEq("/stacked/file"), _, _)).WillOnce(Return(STACKED_RC));

    ASSERT_EQ(STACKED_RC, wrapper.Unlink("/stacked/file"));
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, RenameUnderPrefixIsHandledByMirage)
{
    EXPECT_CALL(mock_oss, Rename).Times(0);

    wrapper.Create(nullptr, "/mirage/from", {}, env, XRDOSS_new);

    ASSERT_EQ(XrdOssOK, wrapper.Rename("/mirage/from", "/mirage/to"));
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, RenameOutsidePrefixIsForwardedToStackedOnSourcePath)
{
    EXPECT_CALL(mock_oss, Rename(StrEq("/stacked/from"), StrEq("/stacked/to"), _, _))
        .WillOnce(Return(STACKED_RC));

    ASSERT_EQ(STACKED_RC, wrapper.Rename("/stacked/from", "/stacked/to"));
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, ChmodUnderPrefixIsHandledByMirage)
{
    EXPECT_CALL(mock_oss, Chmod).Times(0);

    ASSERT_EQ(-ENOTSUP, wrapper.Chmod("/mirage/file", {}));
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, ChmodOutsidePrefixIsForwardedToStacked)
{
    EXPECT_CALL(mock_oss, Chmod(StrEq("/stacked/file"), _, _)).WillOnce(Return(STACKED_RC));

    ASSERT_EQ(STACKED_RC, wrapper.Chmod("/stacked/file", {}));
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, MkdirUnderPrefixIsHandledByMirage)
{
    EXPECT_CALL(mock_oss, Mkdir).Times(0);

    ASSERT_EQ(-ENOTSUP, wrapper.Mkdir("/mirage/dir", {}));
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, MkdirOutsidePrefixIsForwardedToStacked)
{
    EXPECT_CALL(mock_oss, Mkdir(StrEq("/stacked/dir"), _, _, _)).WillOnce(Return(STACKED_RC));

    ASSERT_EQ(STACKED_RC, wrapper.Mkdir("/stacked/dir", {}));
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, RemdirUnderPrefixIsHandledByMirage)
{
    EXPECT_CALL(mock_oss, Remdir).Times(0);

    ASSERT_EQ(-ENOTSUP, wrapper.Remdir("/mirage/dir"));
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, RemdirOutsidePrefixIsForwardedToStacked)
{
    EXPECT_CALL(mock_oss, Remdir(StrEq("/stacked/dir"), _, _)).WillOnce(Return(STACKED_RC));

    ASSERT_EQ(STACKED_RC, wrapper.Remdir("/stacked/dir"));
}
