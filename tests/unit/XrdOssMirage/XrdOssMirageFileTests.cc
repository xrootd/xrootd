#include "XrdOssMirageFixture.hh"

#include <gtest/gtest.h>

#include <fcntl.h>

#include <limits>

class XrdOssMirageFileFixture : public XrdOssMirageFixture
{
};

TEST_F(XrdOssMirageFileFixture, StatShouldSucceed)
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

TEST_F(XrdOssMirageFileFixture, TruncateShouldSucceed)
{
    file.Open("/dummy", O_WRONLY, {}, env);

    ASSERT_EQ(XrdOssOK, file.Ftruncate(1000));
}

TEST_F(XrdOssMirageFileFixture, TruncateReducesFileSize)
{
    file.Open("/dummy", O_WRONLY, {}, env);
    file.Ftruncate(1000);
    file.Close();

    ASSERT_EQ(1000, oss.get_entry_read("/dummy").value().size);
}

TEST_F(XrdOssMirageFileFixture, TruncateIncreasesFileSize)
{
    file.Open("/dummy", O_WRONLY, {}, env);
    file.Ftruncate(1000000);
    file.Close();

    ASSERT_EQ(1000000, oss.get_entry_read("/dummy").value().size);
}

TEST_F(XrdOssMirageFileFixture, TruncateToMaximumOffsetShouldSucceed)
{
    file.Open("/dummy", O_WRONLY, {}, env);

    ASSERT_EQ(XrdOssOK, file.Ftruncate(9223372036854775807ULL));
}

TEST_F(XrdOssMirageFileFixture, TruncateToMaximumOffsetChangesFileSize)
{
    file.Open("/dummy", O_WRONLY, {}, env);
    file.Ftruncate(9223372036854775807ULL);
    file.Close();

    ASSERT_EQ(9223372036854775807ULL, oss.get_entry_read("/dummy").value().size);
}

TEST_F(XrdOssMirageFileFixture, TruncateBeyondMaximumOffsetFailsWithFBIG)
{
    file.Open("/dummy", O_WRONLY, {}, env);

    ASSERT_EQ(-EFBIG, file.Ftruncate(9223372036854775808ULL));
}

TEST_F(XrdOssMirageFileFixture, TruncateBeyondMaximumOffsetDoesNotChangeFileSize)
{
    file.Open("/dummy", O_WRONLY, {}, env);
    file.Ftruncate(9223372036854775808ULL);
    file.Close();

    ASSERT_EQ(9999, oss.get_entry_read("/dummy").value().size);
}

TEST_F(XrdOssMirageFileFixture, OpenInWriteModeShouldSucceed)
{
    ASSERT_EQ(XrdOssOK, file.Open("/dummy", O_WRONLY, {}, env));
}

TEST_F(XrdOssMirageFileFixture, OpenInWriteModeFileThatDoesNotExistFailsWithNOENT)
{
    ASSERT_EQ(-ENOENT, file.Open("/inexistent", O_WRONLY, {}, env));
}

TEST_F(XrdOssMirageFileFixture, OpenInWriteModeFileThatIsBeWrittenFailsWithNOENT)
{
    auto entry = oss.get_entry_write("/dummy");

    ASSERT_EQ(-ENOENT, file.Open("/dummy", O_WRONLY, {}, env));
}

TEST_F(XrdOssMirageFileFixture, OpenInReadOnlyModeShouldSucceed)
{
    ASSERT_EQ(XrdOssOK, file.Open("/dummy", O_RDONLY, {}, env));
}

TEST_F(XrdOssMirageFileFixture, OpenInReadOnlyModeFileThatDoesNotExistFailsWithNOENT)
{
    ASSERT_EQ(-ENOENT, file.Open("/inexistent", O_RDONLY, {}, env));
}

TEST_F(XrdOssMirageFileFixture, OpenInReadOnlyModeFileThatIsBeingWrittenFailsWithNOENT)
{
    auto entry = oss.get_entry_write("/dummy");

    ASSERT_EQ(-ENOENT, file.Open("/dummy", O_RDONLY, {}, env));
}

TEST_F(XrdOssMirageFileFixture, OpenWithExtendedAttributeOpenReturnCodeFailsWithSpecifiedCode)
{
    oss.get_entry_write("/dummy").value()->open.return_code = 1111;

    ASSERT_EQ(-1111, file.Open("/dummy", {}, {}, env));
}

TEST_F(XrdOssMirageFileFixture, ReadWhereRemainingIsBiggerThanBufferReturnsBufferSize)
{
    file.Open("/dummy", O_RDONLY, {}, env);

    ASSERT_EQ(1000, file.Read(nullptr, 0, 1000));
}

TEST_F(XrdOssMirageFileFixture, ReadWithOffsetWhereRemainingIsBiggerThanBufferReturnsBufferSize)
{
    file.Open("/dummy", O_RDONLY, {}, env);

    ASSERT_EQ(1000, file.Read(nullptr, 1000, 1000));
}

TEST_F(XrdOssMirageFileFixture, ReadWithOffsetWhereRemainingIsSmallerThanBufferReturnsRemainingSize)
{
    file.Open("/dummy", O_RDONLY, {}, env);

    ASSERT_EQ(499,  file.Read(nullptr, 9500, 1000));
}

TEST_F(XrdOssMirageFileFixture, ReadWithOffsetEqualToFileSizeReturnsZero)
{
    file.Open("/dummy", O_RDONLY, {}, env);

    ASSERT_EQ(0, file.Read(nullptr, 9999, 1000));
}

TEST_F(XrdOssMirageFileFixture, ReadWithOffsetGreaterThanFileSizeFailsWithEINVAL)
{
    file.Open("/dummy", O_RDONLY, {}, env);

    ASSERT_EQ(-EINVAL, file.Read(nullptr, 10000, 1000));
}

TEST_F(XrdOssMirageFileFixture, ReadWithMaximumOffsetFailsWithEINVAL)
{
    file.Open("/dummy", O_RDONLY, {}, env);

    ASSERT_EQ(-EINVAL, file.Read(nullptr, 9223372036854775807LL, 1000));
}

TEST_F(XrdOssMirageFileFixture, ReadWithNegativeOffsetFailsWithEINVAL)
{
    file.Open("/dummy", O_RDONLY, {}, env);

    ASSERT_EQ(-EINVAL, file.Read(nullptr, -1, 1000));
}

TEST_F(XrdOssMirageFileFixture, ReadWithMinimumOffsetFailsWithEINVAL)
{
    file.Open("/dummy", O_RDONLY, {}, env);

    ASSERT_EQ(-EINVAL, file.Read(nullptr, std::numeric_limits<off_t>::min(), 1000));
}

TEST_F(XrdOssMirageFileFixture, ReadWithNegativeOffsetIgnoresPropertyReturnCode)
{
    {
        auto entry = oss.get_entry_write("/dummy").value();
        entry->read.return_code = 1111;
        entry->read.return_position = 0;
    }

    file.Open("/dummy", O_RDONLY, {}, env);

    ASSERT_EQ(-EINVAL, file.Read(nullptr, -1, 1000));
}

TEST_F(XrdOssMirageFileFixture, ReadWithPropertyReturnCodeOnlyFailsWithSpecifiedCode)
{
    {
        auto entry = oss.get_entry_write("/dummy").value();
        entry->read.return_code = 1111;
    }

    file.Open("/dummy", O_RDONLY, {}, env);

    ASSERT_EQ(-1111, file.Read(nullptr, 0, 1000));
}

TEST_F(XrdOssMirageFileFixture, ReadWithPropertiesReturnsReadBytesNumberAtUnspecifiedPosition)
{
    {
        auto entry = oss.get_entry_write("/dummy").value();
        entry->read.return_code = 1111;
        entry->read.return_position = 5500;
    }

    file.Open("/dummy", O_RDONLY, {}, env);

    ASSERT_EQ(1000,  file.Read(nullptr, 0, 1000));
}

TEST_F(XrdOssMirageFileFixture, ReadWithPropertiesFailsWithSpecifiedCodeAtSpecifiedPosition)
{
    {
        auto entry = oss.get_entry_write("/dummy").value();
        entry->read.return_code = 1111;
        entry->read.return_position = 5500;
    }

    file.Open("/dummy", O_RDONLY, {}, env);

    ASSERT_EQ(-1111, file.Read(nullptr, 5000, 1000));
}

TEST_F(XrdOssMirageFileFixture, ReadWithPropertyPatternReturnsContentFullOfSpecifiedChar)
{
    oss.get_entry_write("/dummy").value()->pattern = "a";

    file.Open("/dummy", O_RDONLY, {}, env);

    char buffer[10]{};
    file.Read(&buffer, 0, sizeof(buffer));

    ASSERT_EQ(0, std::memcmp(buffer, "aaaaaaaaaa", sizeof(buffer)));
}

TEST_F(XrdOssMirageFileFixture, ReadWithPropertyPatternReturnsContentFullOfSpecifiedString)
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

TEST_F(XrdOssMirageFileFixture, WriteWithNegativeOffsetFailsWithEINVAL)
{
    file.Open("/dummy", O_WRONLY, {}, env);

    ASSERT_EQ(-EINVAL, file.Write(nullptr, -1, 1000));
}

TEST_F(XrdOssMirageFileFixture, WriteWithNegativeOffsetDoesNotChangeFileSize)
{
    file.Open("/dummy", O_WRONLY, {}, env);
    file.Write(nullptr, -1, 1000);
    file.Close();

    ASSERT_EQ(9999, oss.get_entry_read("/dummy").value().size);
}

TEST_F(XrdOssMirageFileFixture, WriteUpdatesItsSize)
{
    file.Open("/dummy", O_WRONLY, {}, env);
    file.Write(nullptr, 0, 1000);
    file.Close();

    ASSERT_EQ(10999, oss.get_entry_read("/dummy").value().size);
}

TEST_F(XrdOssMirageFileFixture, WriteReachingMaximumOffsetReturnsTheAmountOfWrittenBytes)
{
    file.Open("/dummy", O_WRONLY, {}, env);
    file.Ftruncate(9223372036854774807ULL);

    ASSERT_EQ(1000, file.Write(nullptr, 9223372036854774807LL, 1000));
}

TEST_F(XrdOssMirageFileFixture, WriteReachingMaximumOffsetChangesFileSize)
{
    file.Open("/dummy", O_WRONLY, {}, env);
    file.Ftruncate(9223372036854773808ULL);
    file.Write(nullptr, 9223372036854773808LL, 1000);
    file.Close();

    ASSERT_EQ(9223372036854774808ULL, oss.get_entry_read("/dummy").value().size);
}

TEST_F(XrdOssMirageFileFixture, WriteBeyondMaximumOffsetFailsWithFBIG)
{
    file.Open("/dummy", O_WRONLY, {}, env);
    file.Ftruncate(9223372036854774808ULL);

    ASSERT_EQ(-EFBIG, file.Write(nullptr, 9223372036854774808LL, 1000));
}

TEST_F(XrdOssMirageFileFixture, WriteBeyondMaximumOffsetDoesNotChangeFileSize)
{
    file.Open("/dummy", O_WRONLY, {}, env);
    file.Ftruncate(9223372036854774808ULL);
    file.Write(nullptr, 9223372036854774808LL, 1000);
    file.Close();

    ASSERT_EQ(9223372036854774808ULL, oss.get_entry_read("/dummy").value().size);
}

TEST_F(XrdOssMirageFileFixture, WriteWithPropertyReturnCodeOnlyFailsWithSpecifiedCode)
{
    {
        auto entry = oss.get_entry_write("/dummy").value();
        entry->write.return_code = 1111;
    }

    file.Open("/dummy", O_WRONLY, {}, env);

    ASSERT_EQ(-1111, file.Write(nullptr, 0, 1000));
}

TEST_F(XrdOssMirageFileFixture, WriteWithPropertiesReturnsWrittenBytesNumberAtUnspecifiedPosition)
{
    {
        auto entry = oss.get_entry_write("/dummy").value();
        entry->write.return_code = 1111;
        entry->write.return_position = 5500;
    }

    file.Open("/dummy", O_WRONLY, {}, env);

    ASSERT_EQ(1000,  file.Write(nullptr, 0, 1000));
}


TEST_F(XrdOssMirageFileFixture, WriteWithPropertiesFailsWithSpecifiedCodeAtSpecifiedPosition)
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

TEST_F(XrdOssMirageFileFixture, CloseShouldSucceed)
{
    file.Open("/dummy", O_WRONLY, {}, env);

    ASSERT_EQ(XrdOssOK, file.Close());
}

TEST_F(XrdOssMirageFileFixture, CloseWithExtendedAttributeCloseReturnCodeFailsWithSpecifiedCode)
{
    oss.get_entry_write("/dummy").value()->close.return_code = 1111;

    file.Open("/dummy", O_WRONLY, {}, env);

    ASSERT_EQ(-1111, file.Close());
}

TEST_F(XrdOssMirageFileFixture, CloseWithExtendedAttributeCloseReturnCodeStillReleasesFileLock)
{
    oss.get_entry_write("/dummy").value()->close.return_code = 1111;

    file.Open("/dummy", O_WRONLY, {}, env);
    file.Close();

    ASSERT_TRUE(oss.get_entry_write("/dummy"));
}
