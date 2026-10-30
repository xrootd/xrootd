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

class XrdOssMirageWrapperPrefixFixture : public XrdOssMirageWrapperFixture
{
};

TEST_F(XrdOssMirageWrapperPrefixFixture, ResolvesToConfiguredPrefix)
{
    ASSERT_TRUE(wrapper.is_mirage_path(WRAPPER_PREFIX));
}

TEST_F(XrdOssMirageWrapperPrefixFixture, IsImmutableOnceSet)
{
    testing::NiceMock<MockOss> other_mock_oss;
    XrdOssMirageWrapper        other_wrapper(&other_mock_oss, nullptr, "/some/other/prefix/", nullptr);

    ASSERT_TRUE(wrapper.is_mirage_path(WRAPPER_PREFIX));
    ASSERT_FALSE(other_wrapper.is_mirage_path(WRAPPER_PREFIX));
}

TEST_F(XrdOssMirageWrapperPrefixFixture, PathsUnderPrefixUseMirage)
{
    ASSERT_TRUE(wrapper.is_mirage_path("/mirage/file"));
    ASSERT_TRUE(wrapper.is_mirage_path("/mirage/sub/dir/file"));
}

TEST_F(XrdOssMirageWrapperPrefixFixture, PathsOutsidePrefixUseStacked)
{
    ASSERT_FALSE(wrapper.is_mirage_path("/elsewhere/file"));
    ASSERT_FALSE(wrapper.is_mirage_path("/"));
    ASSERT_FALSE(wrapper.is_mirage_path("/mirag"));
}

//
// The wrapper must match one directory tree only, never a string pattern.
// A prefix given without a trailing '/' is the same as one given with it.
//

class XrdOssMirageWrapperDirectoryMatchFixture : public XrdOssMirageWrapperFixture
{
protected:
    XrdOssMirageWrapperDirectoryMatchFixture()
        : slashless_wrapper(&slashless_mock_oss, nullptr, "/mirage", nullptr)
    {
    }

    testing::NiceMock<MockOss> slashless_mock_oss;
    XrdOssMirageWrapper        slashless_wrapper;
};

TEST_F(XrdOssMirageWrapperDirectoryMatchFixture, PrefixWithoutTrailingSlashMatchesPathUnderDirectory)
{
    ASSERT_TRUE(slashless_wrapper.is_mirage_path("/mirage/under_mirage"));
}

TEST_F(XrdOssMirageWrapperDirectoryMatchFixture, PrefixWithoutTrailingSlashRejectsSiblingWithSameStart)
{
    ASSERT_FALSE(slashless_wrapper.is_mirage_path("/mirage2/file"));
}

TEST_F(XrdOssMirageWrapperDirectoryMatchFixture, PrefixWithoutTrailingSlashRejectsSiblingWithSuffix)
{
    ASSERT_FALSE(slashless_wrapper.is_mirage_path("/mirage_test/file"));
}

TEST_F(XrdOssMirageWrapperDirectoryMatchFixture, PrefixWithTrailingSlashRejectsSiblingWithSameStart)
{
    ASSERT_FALSE(wrapper.is_mirage_path("/mirage2/file"));
}

TEST_F(XrdOssMirageWrapperDirectoryMatchFixture, SiblingWithSameStartIsForwardedToStacked)
{
    EXPECT_CALL(slashless_mock_oss, Create(_, StrEq("/mirage2/newfile"), _, _, XRDOSS_new))
        .WillOnce(Return(STACKED_RC));

    ASSERT_EQ(STACKED_RC, slashless_wrapper.Create(nullptr, "/mirage2/newfile", {}, env, XRDOSS_new));
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

TEST_F(XrdOssMirageWrapperFileSystemFixture, RenameMirageToMiragePathCreatesDestinationFile)
{
    wrapper.Create(nullptr, "/mirage/from", {}, env, XRDOSS_new);
    wrapper.Rename("/mirage/from", "/mirage/to");

    struct stat buff;
    ASSERT_EQ(XrdOssOK, wrapper.Stat("/mirage/to", &buff));
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, RenameMirageToMiragePathRemovesSourceFile)
{
    wrapper.Create(nullptr, "/mirage/from", {}, env, XRDOSS_new);
    wrapper.Rename("/mirage/from", "/mirage/to");

    struct stat buff;
    ASSERT_EQ(-ENOENT, wrapper.Stat("/mirage/from", &buff));
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, RenameRealToRealPathReturnsStackedResult)
{
    EXPECT_CALL(mock_oss, Rename(StrEq("/stacked/from"), StrEq("/stacked/to"), _, _))
        .WillOnce(Return(XrdOssOK));

    ASSERT_EQ(XrdOssOK, wrapper.Rename("/stacked/from", "/stacked/to"));
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, RenameRealToRealPathDoesNotCreateMirageFile)
{
    wrapper.Rename("/stacked/from", "/stacked/to");

    struct stat buff;
    ASSERT_EQ(-ENOENT, wrapper.Stat("/mirage/to", &buff));
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, RenameMirageToRealPathFailsWithXDEV)
{
    wrapper.Create(nullptr, "/mirage/from", {}, env, XRDOSS_new);

    ASSERT_EQ(-EXDEV, wrapper.Rename("/mirage/from", "/stacked/to"));
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, RenameMirageToRealPathIsNotForwardedToStacked)
{
    EXPECT_CALL(mock_oss, Rename).Times(0);

    wrapper.Create(nullptr, "/mirage/from", {}, env, XRDOSS_new);
    wrapper.Rename("/mirage/from", "/stacked/to");
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, RenameMirageToRealPathKeepsMirageFile)
{
    wrapper.Create(nullptr, "/mirage/from", {}, env, XRDOSS_new);
    wrapper.Rename("/mirage/from", "/stacked/to");

    struct stat buff;
    ASSERT_EQ(XrdOssOK, wrapper.Stat("/mirage/from", &buff));
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, RenameRealToMiragePathFailsWithXDEV)
{
    ASSERT_EQ(-EXDEV, wrapper.Rename("/stacked/from", "/mirage/to"));
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, RenameRealToMiragePathIsNotForwardedToStacked)
{
    EXPECT_CALL(mock_oss, Rename).Times(0);

    wrapper.Rename("/stacked/from", "/mirage/to");
}

TEST_F(XrdOssMirageWrapperFileSystemFixture, RenameRealToMiragePathDoesNotCreateMirageFile)
{
    wrapper.Rename("/stacked/from", "/mirage/to");

    struct stat buff;
    ASSERT_EQ(-ENOENT, wrapper.Stat("/mirage/to", &buff));
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
