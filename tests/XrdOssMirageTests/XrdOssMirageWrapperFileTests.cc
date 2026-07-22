#include "XrdOssMirageWrapperFixture.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fcntl.h>

#include <memory>

using testing::_;
using testing::Return;
using testing::StrEq;

//
// File level routing: the back end is chosen on Open() from the path and every
// subsequent call is forwarded to it. A mirage file is created once the entry
// exists in the wrapper's own XrdOssMirage; the mocked stacked file is used
// otherwise.
//
class XrdOssMirageWrapperFileFixture : public XrdOssMirageWrapperFixture
{
protected:
    void SetUp() override
    {
        wrapper.Create(nullptr, "/mirage/dummy", {}, env, XRDOSS_new);
        wrapper.Truncate("/mirage/dummy", 9999);

        file.reset(wrapper.newFile(nullptr));
    }

    std::unique_ptr<XrdOssDF> file;
};

TEST_F(XrdOssMirageWrapperFileFixture, UnopenedFileDefaultsToStacked)
{
    EXPECT_CALL(*mock_df, Fstat(_)).WillOnce(Return(STACKED_RC));

    struct stat buff{};
    ASSERT_EQ(STACKED_RC, file->Fstat(&buff));
}

TEST_F(XrdOssMirageWrapperFileFixture, OpenUnderPrefixRoutesToMirage)
{
    EXPECT_CALL(*mock_df, Open).Times(0);

    ASSERT_EQ(XrdOssOK, file->Open("/mirage/dummy", O_RDONLY, {}, env));
}

TEST_F(XrdOssMirageWrapperFileFixture, OpenUnderPrefixForNonExistentReturnsNOENT)
{
    EXPECT_CALL(*mock_df, Open).Times(0);

    ASSERT_EQ(-ENOENT, file->Open("/mirage/inexistent", O_RDONLY, {}, env));
}

TEST_F(XrdOssMirageWrapperFileFixture, OpenOutsidePrefixIsForwardedToStacked)
{
    EXPECT_CALL(*mock_df, Open(StrEq("/stacked/file"), O_RDONLY, _, _)).WillOnce(Return(STACKED_RC));

    ASSERT_EQ(STACKED_RC, file->Open("/stacked/file", O_RDONLY, {}, env));
}

TEST_F(XrdOssMirageWrapperFileFixture, FstatAfterMirageOpenUsesMirage)
{
    EXPECT_CALL(*mock_df, Fstat).Times(0);

    file->Open("/mirage/dummy", O_RDONLY, {}, env);

    struct stat buff{};
    ASSERT_EQ(XrdOssOK, file->Fstat(&buff));
    ASSERT_EQ(9999, buff.st_size);
}

TEST_F(XrdOssMirageWrapperFileFixture, FstatAfterStackedOpenUsesStacked)
{
    EXPECT_CALL(*mock_df, Fstat(_)).WillOnce(Return(STACKED_RC));

    file->Open("/stacked/file", O_RDONLY, {}, env);

    struct stat buff{};
    ASSERT_EQ(STACKED_RC, file->Fstat(&buff));
}

TEST_F(XrdOssMirageWrapperFileFixture, ReadAfterMirageOpenReadsFromMirage)
{
    EXPECT_CALL(*mock_df, Read).Times(0);

    file->Open("/mirage/dummy", O_RDONLY, {}, env);

    ASSERT_EQ(1000, file->Read(nullptr, 0, 1000));
}

TEST_F(XrdOssMirageWrapperFileFixture, ReadAfterStackedOpenReadsFromStacked)
{
    EXPECT_CALL(*mock_df, Read(_, 0, 1000)).WillOnce(Return(STACKED_RC));

    file->Open("/stacked/file", O_RDONLY, {}, env);

    ASSERT_EQ(STACKED_RC, file->Read(nullptr, 0, 1000));
}

TEST_F(XrdOssMirageWrapperFileFixture, WriteAfterMirageOpenWritesToMirage)
{
    EXPECT_CALL(*mock_df, Write).Times(0);

    file->Open("/mirage/dummy", O_WRONLY, {}, env);

    ASSERT_EQ(1000, file->Write(nullptr, 0, 1000));
}

TEST_F(XrdOssMirageWrapperFileFixture, WriteUnderPrefixUpdatesMirageSize)
{
    EXPECT_CALL(*mock_df, Write).Times(0);

    file->Open("/mirage/dummy", O_WRONLY, {}, env);
    file->Write(nullptr, 0, 1000);
    file->Close();

    struct stat buff{};
    wrapper.Stat("/mirage/dummy", &buff);
    ASSERT_EQ(10999, buff.st_size);
}

TEST_F(XrdOssMirageWrapperFileFixture, WriteAfterStackedOpenWritesToStacked)
{
    EXPECT_CALL(*mock_df, Write(_, 0, 1000)).WillOnce(Return(STACKED_RC));

    file->Open("/stacked/file", O_WRONLY, {}, env);

    ASSERT_EQ(STACKED_RC, file->Write(nullptr, 0, 1000));
}

TEST_F(XrdOssMirageWrapperFileFixture, FtruncateAfterMirageOpenUsesMirage)
{
    EXPECT_CALL(*mock_df, Ftruncate).Times(0);

    file->Open("/mirage/dummy", O_WRONLY, {}, env);
    file->Ftruncate(1000);
    file->Close();

    struct stat buff{};
    wrapper.Stat("/mirage/dummy", &buff);
    ASSERT_EQ(1000, buff.st_size);
}

TEST_F(XrdOssMirageWrapperFileFixture, FtruncateAfterStackedOpenUsesStacked)
{
    EXPECT_CALL(*mock_df, Ftruncate(1000)).WillOnce(Return(STACKED_RC));

    file->Open("/stacked/file", O_WRONLY, {}, env);

    ASSERT_EQ(STACKED_RC, file->Ftruncate(1000));
}

TEST_F(XrdOssMirageWrapperFileFixture, CloseAfterMirageOpenReleasesMirageLock)
{
    EXPECT_CALL(*mock_df, Close).Times(0);

    file->Open("/mirage/dummy", O_WRONLY, {}, env);
    ASSERT_EQ(-EBUSY, wrapper.Create(nullptr, "/mirage/dummy", {}, env));

    file->Close();
    ASSERT_EQ(XrdOssOK, wrapper.Create(nullptr, "/mirage/dummy", {}, env));
}

TEST_F(XrdOssMirageWrapperFileFixture, GetFDAfterStackedOpenUsesStacked)
{
    EXPECT_CALL(*mock_df, getFD()).WillOnce(Return(STACKED_RC));

    file->Open("/stacked/file", O_RDONLY, {}, env);

    ASSERT_EQ(STACKED_RC, file->getFD());
}
