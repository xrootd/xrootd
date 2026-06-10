#include "XrdOssMirageFixture.hh"

#include <gtest/gtest.h>

TEST_F(XrdOssMirageFixture, CreatingNewFileReturnsOk)
{
    ASSERT_EQ(XrdOssOK, oss.Create(nullptr, "/newfile", {}, env, XRDOSS_new));
}

TEST_F(XrdOssMirageFixture, CreatingNewFileCreatesAnEntry)
{
    oss.Create(nullptr, "/newfile", {}, env, XRDOSS_new);

    ASSERT_TRUE(oss.get_entry_read("/newfile"));
}

TEST_F(XrdOssMirageFixture, CreatingFileThatAlreadyExistsReturnsEXISTError)
{
    ASSERT_EQ(-EEXIST, oss.Create(nullptr, "/dummy", {}, env, XRDOSS_new));
}

TEST_F(XrdOssMirageFixture, OverwritingFileThatAlreadyExistsReturnsOk)
{
    ASSERT_EQ(XrdOssOK, oss.Create(nullptr, "/dummy", {}, env));
}

TEST_F(XrdOssMirageFixture, OverwritingFileThatIsBeingWrittenReturnsBUSYError)
{
    auto entry = oss.get_entry_write("/dummy");

    ASSERT_EQ(-EBUSY, oss.Create(nullptr, "/dummy", {}, env));
}

TEST_F(XrdOssMirageFixture, OverwritingFileResetsItsSize)
{
    oss.Create(nullptr, "/dummy", {}, env);

    ASSERT_EQ(0, oss.get_entry_read("/dummy").value().size);
}

TEST_F(XrdOssMirageFixture, OverwritingFileDoesNotResetItsWriteExtendedAttributes)
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

TEST_F(XrdOssMirageFixture, RenamingFileThatAlreadyExistsReturnsOk)
{
    ASSERT_EQ(XrdOssOK, oss.Rename("/dummy", "/dummy_renamed"));
}

TEST_F(XrdOssMirageFixture, RenamingFileThatAlreadyExistsMovesEntryToAnotherFilePath)
{
    oss.Rename("/dummy", "/dummy_renamed");

    ASSERT_TRUE(oss.get_entry_read("/dummy_renamed"));
    ASSERT_EQ(9999, oss.get_entry_read("/dummy_renamed").value().size);
}

TEST_F(XrdOssMirageFixture, RenamingFileThatDoesNotExistReturnsNOENTError)
{
    ASSERT_EQ(-ENOENT, oss.Rename("/inexistent", "/dummy"));
}

TEST_F(XrdOssMirageFixture, RenamingFileToAnotherFileThatAlreadyExistsReturnsEXISTError)
{
    oss.Create(nullptr, "/dummy_from", {}, env, XRDOSS_new);

    ASSERT_EQ(-EEXIST, oss.Rename("/dummy_from", "/dummy"));
}

TEST_F(XrdOssMirageFixture, StatingFileThatAlreadyExistsReturnsOk)
{
    struct stat buff{};
    ASSERT_EQ(XrdOssOK, oss.Stat("/dummy", &buff));
}

TEST_F(XrdOssMirageFixture, StatingFileThatAlreadyExistsReturnsCorrectSize)
{
    struct stat buff{};
    oss.Stat("/dummy", &buff);

    ASSERT_EQ(9999, buff.st_size);
}

TEST_F(XrdOssMirageFixture, StatingFileThatDoesNotExistReturnsNOENTError)
{
    ASSERT_EQ(-ENOENT, oss.Stat("/inexistent", nullptr));
}

TEST_F(XrdOssMirageFixture, TruncatingFileThatAlreadyExistsReturnsOk)
{
    ASSERT_EQ(XrdOssOK, oss.Truncate("/dummy", 1000));
}

TEST_F(XrdOssMirageFixture, TruncatingFileThatAlreadyExistsChangesItsSize)
{
    oss.Truncate("/dummy", 1000);
    
    ASSERT_EQ(1000, oss.get_entry_read("/dummy").value().size);
}

TEST_F(XrdOssMirageFixture, TruncatingFileThatDoesNotExistReturnsNOENTError)
{
    ASSERT_EQ(-ENOENT, oss.Truncate("/inexistent", 1000));
}

TEST_F(XrdOssMirageFixture, TruncatingFileThatIsBeingWrittenReturnsBUSYError)
{
    auto entry = oss.get_entry_write("/dummy");

    ASSERT_EQ(-EBUSY, oss.Truncate("/dummy", 1000));
}

TEST_F(XrdOssMirageFixture, UnlinkingFileThatAlreadyExistsReturnsOk)
{
    ASSERT_EQ(XrdOssOK, oss.Unlink("/dummy"));
}

TEST_F(XrdOssMirageFixture, UnlinkingFileThatAlreadyExistsRemovesEntry)
{
    oss.Unlink("/dummy");

    ASSERT_FALSE(oss.get_entry_read("/dummy"));
}

TEST_F(XrdOssMirageFixture, UnlinkingFileThatDoesNotExistReturnsNOENTError)
{
    ASSERT_EQ(-ENOENT, oss.Unlink("/inexistent"));
}
