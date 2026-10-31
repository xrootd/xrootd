#include "XrdOssMirageFixture.hh"

#include <gtest/gtest.h>

TEST_F(OssMirageTest, CreateNewFileShouldSucceed)
{
    ASSERT_EQ(XrdOssOK, oss.Create(nullptr, "/newfile", {}, env, XRDOSS_new));
}

TEST_F(OssMirageTest, CreateNewFileCreatesAnEntry)
{
    oss.Create(nullptr, "/newfile", {}, env, XRDOSS_new);

    ASSERT_TRUE(oss.get_entry_read("/newfile"));
}

TEST_F(OssMirageTest, CreateFileThatAlreadyExistsFailsWithEXIST)
{
    ASSERT_EQ(-EEXIST, oss.Create(nullptr, "/dummy", {}, env, XRDOSS_new));
}

TEST_F(OssMirageTest, OverwriteFileThatAlreadyExistsShouldSucceed)
{
    ASSERT_EQ(XrdOssOK, oss.Create(nullptr, "/dummy", {}, env));
}

TEST_F(OssMirageTest, OverwriteFileThatIsBeingWrittenFailsWithBUSY)
{
    auto entry = oss.get_entry_write("/dummy");

    ASSERT_EQ(-EBUSY, oss.Create(nullptr, "/dummy", {}, env));
}

TEST_F(OssMirageTest, OverwriteFileResetsItsSize)
{
    oss.Create(nullptr, "/dummy", {}, env);

    ASSERT_EQ(0, oss.get_entry_read("/dummy").value().size);
}

TEST_F(OssMirageTest, OverwriteFileDoesNotResetItsWriteExtendedAttributes)
{
    {
        auto entry = oss.get_entry_write("/dummy").value();
        entry->write.return_code     = 1;
        entry->write.return_position = 1;
    }

    oss.Create(nullptr, "/dummy", {}, env);

    auto entry = oss.get_entry_read("/dummy").value();
    ASSERT_EQ(1, entry.write.return_code);
    ASSERT_EQ(1, entry.write.return_position);
}

TEST_F(OssMirageTest, RenameFileThatAlreadyExistsShouldSucceed)
{
    ASSERT_EQ(XrdOssOK, oss.Rename("/dummy", "/dummy_renamed"));
}

TEST_F(OssMirageTest, RenameFileThatAlreadyExistsMovesEntryToAnotherFilePath)
{
    oss.Rename("/dummy", "/dummy_renamed");

    ASSERT_TRUE(oss.get_entry_read("/dummy_renamed"));
    ASSERT_EQ(9999, oss.get_entry_read("/dummy_renamed").value().size);
}

TEST_F(OssMirageTest, RenameFileThatDoesNotExistFailsWithNOENT)
{
    ASSERT_EQ(-ENOENT, oss.Rename("/inexistent", "/dummy"));
}

TEST_F(OssMirageTest, RenameFileToAnotherFileThatAlreadyExistsFailsWithEXIST)
{
    oss.Create(nullptr, "/dummy_from", {}, env, XRDOSS_new);

    ASSERT_EQ(-EEXIST, oss.Rename("/dummy_from", "/dummy"));
}

TEST_F(OssMirageTest, StatFileThatAlreadyExistsShouldSucceed)
{
    struct stat buff{};
    ASSERT_EQ(XrdOssOK, oss.Stat("/dummy", &buff));
}

TEST_F(OssMirageTest, StatFileThatAlreadyExistsReturnsCorrectSize)
{
    struct stat buff{};
    oss.Stat("/dummy", &buff);

    ASSERT_EQ(9999, buff.st_size);
}

TEST_F(OssMirageTest, StatFileThatDoesNotExistFailsWithNOENT)
{
    ASSERT_EQ(-ENOENT, oss.Stat("/inexistent", nullptr));
}

TEST_F(OssMirageTest, TruncateFileThatAlreadyExistsShouldSucceed)
{
    ASSERT_EQ(XrdOssOK, oss.Truncate("/dummy", 1000));
}

TEST_F(OssMirageTest, TruncateFileThatAlreadyExistsReducesSize)
{
    oss.Truncate("/dummy", 1000);
    
    ASSERT_EQ(1000, oss.get_entry_read("/dummy").value().size);
}

TEST_F(OssMirageTest, TruncateFileThatAlreadyExistsIncreasesSize)
{
    oss.Truncate("/dummy", 1000000);
    
    ASSERT_EQ(1000000, oss.get_entry_read("/dummy").value().size);
}

TEST_F(OssMirageTest, TruncateFileToMaximumOffsetShouldSucceed)
{
    ASSERT_EQ(XrdOssOK, oss.Truncate("/dummy", 9223372036854775807ULL));
}

TEST_F(OssMirageTest, TruncateFileToMaximumOffsetChangesSize)
{
    oss.Truncate("/dummy", 9223372036854775807ULL);

    ASSERT_EQ(9223372036854775807ULL, oss.get_entry_read("/dummy").value().size);
}

TEST_F(OssMirageTest, TruncateFileBeyondMaximumOffsetFailsWithFBIG)
{
    ASSERT_EQ(-EFBIG, oss.Truncate("/dummy", 9223372036854775808ULL));
}

TEST_F(OssMirageTest, TruncateFileBeyondMaximumOffsetDoesNotChangeSize)
{
    oss.Truncate("/dummy", 9223372036854775808ULL);

    ASSERT_EQ(9999, oss.get_entry_read("/dummy").value().size);
}

TEST_F(OssMirageTest, TruncateFileThatDoesNotExistFailsWithNOENT)
{
    ASSERT_EQ(-ENOENT, oss.Truncate("/inexistent", 1000));
}

TEST_F(OssMirageTest, TruncateFileThatIsBeingWrittenFailsWithBUSY)
{
    auto entry = oss.get_entry_write("/dummy");

    ASSERT_EQ(-EBUSY, oss.Truncate("/dummy", 1000));
}

TEST_F(OssMirageTest, UnlinkFileThatAlreadyExistsShouldSucceed)
{
    ASSERT_EQ(XrdOssOK, oss.Unlink("/dummy"));
}

TEST_F(OssMirageTest, UnlinkFileThatAlreadyExistsRemovesEntry)
{
    oss.Unlink("/dummy");

    ASSERT_FALSE(oss.get_entry_read("/dummy"));
}

TEST_F(OssMirageTest, UnlinkFileThatDoesNotExistFailsWithNOENT)
{
    ASSERT_EQ(-ENOENT, oss.Unlink("/inexistent"));
}
