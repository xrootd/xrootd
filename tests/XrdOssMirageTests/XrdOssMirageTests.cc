#include "XrdOssMirage/XrdOssMirage.hh"

#include "XrdOuc/XrdOucEnv.hh"

#include <gtest/gtest.h>

#include <memory>

class XrdOssMirageTest : public testing::Test {
protected:
    void SetUp() override
    {
      oss.Create(nullptr, "/dummy", {}, env, XRDOSS_new);
      oss.Truncate("/dummy", 9999);
    }

    XrdOssMirage oss;
    XrdOucEnv env;
};

TEST_F(XrdOssMirageTest, CreateFile)
{
    ASSERT_EQ(XrdOssOK, oss.Create(nullptr, "/newfile", {}, env, XRDOSS_new));
}

TEST_F(XrdOssMirageTest, CreateFileCreatesEntry)
{
    oss.Create(nullptr, "/newfile", {}, env, XRDOSS_new);

    ASSERT_TRUE(oss.getEntryRead("/newfile"));
}

TEST_F(XrdOssMirageTest, CreateFileThatAlreadyExists)
{
    ASSERT_EQ(-EEXIST, oss.Create(nullptr, "/dummy", {}, env, XRDOSS_new));
}

TEST_F(XrdOssMirageTest, CreateFileThatAlreadyExistsWithoutNew)
{
    ASSERT_EQ(XrdOssOK, oss.Create(nullptr, "/dummy", {}, env));
}

TEST_F(XrdOssMirageTest, CreateFileThatIsBeingWritten)
{
    auto entry = oss.getEntryWrite("/dummy");

    ASSERT_EQ(-EBUSY, oss.Create(nullptr, "/dummy", {}, env));
}

TEST_F(XrdOssMirageTest, CreateFileResetsSize)
{
    oss.Create(nullptr, "/dummy", {}, env);

    ASSERT_EQ(0, oss.getEntryRead("/dummy").value().size);
}

TEST_F(XrdOssMirageTest, CreateFileDoesNotResetWriteProperties)
{
    {
        auto entry = oss.getEntryWrite("/dummy").value();
        entry->write.return_code     = 1;
        entry->write.return_position = 1;
    }

    oss.Create(nullptr, "/dummy", {}, env);

    auto entry = oss.getEntryRead("/dummy").value();
    ASSERT_EQ(1, entry.write.return_code);
    ASSERT_EQ(1, entry.write.return_position);
}

TEST_F(XrdOssMirageTest, Rename)
{
    ASSERT_EQ(XrdOssOK, oss.Rename("/dummy", "/dummy_renamed"));
}

TEST_F(XrdOssMirageTest, RenameMovesNewEntry)
{
    ASSERT_EQ(XrdOssOK, oss.Rename("/dummy", "/dummy_renamed"));
    ASSERT_TRUE(oss.getEntryRead("/dummy_renamed"));
    ASSERT_EQ(9999, oss.getEntryRead("/dummy_renamed").value().size);
}

TEST_F(XrdOssMirageTest, RenameInexistentFile)
{
    ASSERT_EQ(-ENOENT, oss.Rename("/inexistent", "/dummy"));
}

TEST_F(XrdOssMirageTest, RenameToAlreadyExistentFile)
{
    oss.Create(nullptr, "/dummy_from", {}, env, XRDOSS_new);

    ASSERT_EQ(-EEXIST, oss.Rename("/dummy_from", "/dummy"));
}

TEST_F(XrdOssMirageTest, Stat)
{
    struct stat buff{};
    ASSERT_EQ(XrdOssOK, oss.Stat("/dummy", &buff));
    ASSERT_EQ(9999, buff.st_size);
}

TEST_F(XrdOssMirageTest, StatInexistentFile)
{
    ASSERT_EQ(-ENOENT, oss.Stat("/inexistent", nullptr));
}

TEST_F(XrdOssMirageTest, Truncate)
{
    ASSERT_EQ(XrdOssOK, oss.Truncate("/dummy", 1000));
    ASSERT_EQ(1000, oss.getEntryRead("/dummy").value().size);
}

TEST_F(XrdOssMirageTest, TruncateInexistentFile)
{
    ASSERT_EQ(-ENOENT, oss.Truncate("/inexistent", 1000));
}

TEST_F(XrdOssMirageTest, TruncateFileThatIsBeingWritten)
{
    auto entry = oss.getEntryWrite("/dummy");

    ASSERT_EQ(-EBUSY, oss.Truncate("/dummy", 1000));
}

TEST_F(XrdOssMirageTest, Unlink)
{
    ASSERT_EQ(XrdOssOK, oss.Unlink("/dummy"));
}

TEST_F(XrdOssMirageTest, UnlinkRemovesEntry)
{
    oss.Unlink("/dummy");

    ASSERT_FALSE(oss.getEntryRead("/dummy"));
}

TEST_F(XrdOssMirageTest, UnlinkInexistentFile)
{
    ASSERT_EQ(-ENOENT, oss.Unlink("/inexistent"));
}
