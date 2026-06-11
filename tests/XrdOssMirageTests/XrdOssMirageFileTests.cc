#include "XrdOssMirageFixture.hh"

#include <gtest/gtest.h>

#include <fcntl.h>

class XrdOssMirageFileFixture : public XrdOssMirageFixture
{
};

TEST_F(XrdOssMirageFileFixture, StatReturnsOk)
{
    file.Open("/dummy", {}, O_RDONLY, env);

    struct stat buff;
    ASSERT_EQ(XrdOssOK, file.Fstat(&buff));
}

TEST_F(XrdOssMirageFileFixture, StatReturnsCorrectSize)
{
    file.Open("/dummy", {}, O_RDONLY, env);

    struct stat buff;
    file.Fstat(&buff);

    ASSERT_EQ(9999, buff.st_size);
}

TEST_F(XrdOssMirageFileFixture, TruncateReturnsOk)
{
    file.Open("/dummy", O_WRONLY, {}, env);

    ASSERT_EQ(XrdOssOK, file.Ftruncate(1000));
}

TEST_F(XrdOssMirageFileFixture, TruncateChangesItsSize)
{
    file.Open("/dummy", O_WRONLY, {}, env);
    file.Ftruncate(1000);
    file.Close();

    ASSERT_EQ(1000, oss.get_entry_read("/dummy").value().size);
}

TEST_F(XrdOssMirageFileFixture, OpenInWriteModeReturnsOk)
{
    ASSERT_EQ(XrdOssOK, file.Open("/dummy", O_WRONLY, {}, env));
}

TEST_F(XrdOssMirageFileFixture, OpenInWriteModeFileThatDoesNotExistReturnsNOENTError)
{
    ASSERT_EQ(-ENOENT, file.Open("/inexistent", O_WRONLY, {}, env));
}

TEST_F(XrdOssMirageFileFixture, OpenInWriteModeFileThatIsBeWrittenReturnsNOENTError)
{
    auto entry = oss.get_entry_write("/dummy");

    ASSERT_EQ(-ENOENT, file.Open("/dummy", O_WRONLY, {}, env));
}

TEST_F(XrdOssMirageFileFixture, OpenInReadOnlyModeReturnsOk)
{
    ASSERT_EQ(XrdOssOK, file.Open("/dummy", O_RDONLY, {}, env));
}

TEST_F(XrdOssMirageFileFixture, OpenInReadOnlyModeFileThatDoesNotExistReturnsNOENTError)
{
    ASSERT_EQ(-ENOENT, file.Open("/inexistent", O_RDONLY, {}, env));
}

TEST_F(XrdOssMirageFileFixture, OpenInReadOnlyModeFileThatIsBeingWrittenReturnsNOENTError)
{
    auto entry = oss.get_entry_write("/dummy");

    ASSERT_EQ(-ENOENT, file.Open("/dummy", O_RDONLY, {}, env));
}

TEST_F(XrdOssMirageFileFixture, OpenWithExtendedAttributeOpenReturnCodeReturnsSpecifiedErrorCode)
{
    oss.get_entry_write("/dummy").value()->open.return_code = 1111;

    ASSERT_EQ(-1111, file.Open("/dummy", {}, {}, env));
}

TEST_F(XrdOssMirageFileFixture, ReadFromFileWhereRemainingSizeIsBiggerThanBufferReturnsBufferSizeAsReadBytes)
{
    file.Open("/dummy", O_RDONLY, {}, env);

    ASSERT_EQ(1000, file.Read(nullptr, 0, 1000));
}

TEST_F(XrdOssMirageFileFixture, ReadWithOffsetFromFileWhereRemainingSizeIsBiggerThanBufferReturnsBufferSizeAsReadBytes)
{
    file.Open("/dummy", O_RDONLY, {}, env);

    ASSERT_EQ(1000, file.Read(nullptr, 1000, 1000));
}

TEST_F(XrdOssMirageFileFixture, ReadWithOffsetFromFileWhereRemainingSizeIsSmallerThanBufferReturnsRemainingSizeAsReadBytes)
{
    file.Open("/dummy", O_RDONLY, {}, env);

    ASSERT_EQ(499,  file.Read(nullptr, 9500, 1000));
}

TEST_F(XrdOssMirageFileFixture, ReadWithExtendedAttributeReadReturnCodeReturnsSpecifiedErrorCode)
{
    {
        auto entry = oss.get_entry_write("/dummy").value();
        entry->read.return_code = 1111;
    }

    file.Open("/dummy", O_RDONLY, {}, env);

    ASSERT_EQ(-1111, file.Read(nullptr, 0, 1000));
}

TEST_F(XrdOssMirageFileFixture, ReadWithExtendedAttributeReadReturnCodeAndPositionReturnsReadBytesNumberAtUnspecifiedPosition)
{
    {
        auto entry = oss.get_entry_write("/dummy").value();
        entry->read.return_code = 1111;
        entry->read.return_position = 5500;
    }

    file.Open("/dummy", O_RDONLY, {}, env);

    ASSERT_EQ(1000,  file.Read(nullptr, 0, 1000));
}

TEST_F(XrdOssMirageFileFixture, ReadWithExtendedAttributeReadReturnCodeAndPositionReturnsSpecifiedErrorCodeAtSpecifiedPosition)
{
    {
        auto entry = oss.get_entry_write("/dummy").value();
        entry->read.return_code = 1111;
        entry->read.return_position = 5500;
    }

    file.Open("/dummy", O_RDONLY, {}, env);

    ASSERT_EQ(-1111, file.Read(nullptr, 5000, 1000));
}

TEST_F(XrdOssMirageFileFixture, ReadWithExtendedAttributeCharPatternReturnsContentFullOfSpecifiedChar)
{
    oss.get_entry_write("/dummy").value()->pattern = "a";

    file.Open("/dummy", O_RDONLY, {}, env);

    char buffer[10]{};
    file.Read(&buffer, 0, sizeof(buffer));

    ASSERT_EQ(0, std::memcmp(buffer, "aaaaaaaaaa", sizeof(buffer)));
}

TEST_F(XrdOssMirageFileFixture, ReadWithExtendedAttributeStringPatternReturnsContentFullOfSpecifiedString)
{
    oss.get_entry_write("/dummy").value()->pattern = "abc";

    file.Open("/dummy", O_RDONLY, {}, env);

    char buffer[10]{};
    file.Read(&buffer, 0, sizeof(buffer));

    ASSERT_EQ(0, std::memcmp(buffer, "abcabcabca", sizeof(buffer)));
}

TEST_F(XrdOssMirageFileFixture, WriteReturnsTheAmountOfWrittenBytes)
{
    file.Open("/dummy", O_WRONLY, {}, env);

    ASSERT_EQ(1000, file.Write(nullptr, 0, 1000));
}

TEST_F(XrdOssMirageFileFixture, WriteUpdatesItsSize)
{
    file.Open("/dummy", O_WRONLY, {}, env);
    file.Write(nullptr, 0, 1000);
    file.Close();

    ASSERT_EQ(10999, oss.get_entry_read("/dummy").value().size);
}

TEST_F(XrdOssMirageFileFixture, WriteWithExtendedAttributeWriteReturnCodeReturnsSpecifiedErrorCode)
{
    {
        auto entry = oss.get_entry_write("/dummy").value();
        entry->write.return_code = 1111;
    }

    file.Open("/dummy", O_WRONLY, {}, env);

    ASSERT_EQ(-1111, file.Write(nullptr, 0, 1000));
}

TEST_F(XrdOssMirageFileFixture, WriteWithExtendedAttributeWriteReturnCodeAndPositionReturnsWrittenBytesNumberAtUnspecifiedPosition)
{
    {
        auto entry = oss.get_entry_write("/dummy").value();
        entry->write.return_code = 1111;
        entry->write.return_position = 5500;
    }

    file.Open("/dummy", O_WRONLY, {}, env);

    ASSERT_EQ(1000,  file.Write(nullptr, 0, 1000));
}


TEST_F(XrdOssMirageFileFixture, WriteWithExtendedAttributeWriteReturnCodeAndPositionReturnsSpecifiedErrorCodeAtSpecifiedPosition)
{
    {
        auto entry = oss.get_entry_write("/dummy").value();
        entry->write.return_code = 1111;
        entry->write.return_position = 5500;
    }

    file.Open("/dummy", O_WRONLY, {}, env);

    ASSERT_EQ(-1111, file.Write(nullptr, 5000, 1000));
}

TEST_F(XrdOssMirageFileFixture, CloseReleasesFileLock)
{
    file.Open("/dummy", O_WRONLY, {}, env);
    file.Close();

    ASSERT_TRUE(oss.get_entry_write("/dummy"));
}
