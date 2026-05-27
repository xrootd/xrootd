#include "XrdOssMirageFixture.hh"

#include <gtest/gtest.h>

#include <fcntl.h>

class XrdOssMirageFileFixture : public XrdOssMirageFixture
{
};

TEST_F(XrdOssMirageFileFixture, Stat)
{
    file.Open("/dummy", {}, O_RDONLY, env);

    struct stat buff;
    ASSERT_EQ(XrdOssOK, file.Fstat(&buff));
    ASSERT_EQ(9999, buff.st_size);
}

TEST_F(XrdOssMirageFileFixture, Truncate)
{
    file.Open("/dummy", O_WRONLY, {}, env);

    ASSERT_EQ(XrdOssOK, file.Ftruncate(1000));
    ASSERT_EQ(XrdOssOK, file.Close());
    ASSERT_EQ(1000, oss.getEntryRead("/dummy").value().size);
}

TEST_F(XrdOssMirageFileFixture, OpenWriteMode)
{
    ASSERT_EQ(XrdOssOK, file.Open("/dummy", O_WRONLY, {}, env));
}

TEST_F(XrdOssMirageFileFixture, OpenWriteModeInexistentFile)
{
    ASSERT_EQ(-ENOENT, file.Open("/inexistent", O_WRONLY, {}, env));
}

TEST_F(XrdOssMirageFileFixture, OpenWriteModeFileBeingWritten)
{
    auto entry = oss.getEntryWrite("/dummy");
    ASSERT_EQ(-ENOENT, file.Open("/dummy", O_WRONLY, {}, env));
}

TEST_F(XrdOssMirageFileFixture, OpenReadMode)
{
    ASSERT_EQ(XrdOssOK, file.Open("/dummy", O_RDONLY, {}, env));
}

TEST_F(XrdOssMirageFileFixture, OpenReadModeInexistentFile)
{
    ASSERT_EQ(-ENOENT, file.Open("/inexistent", O_RDONLY, {}, env));
}

TEST_F(XrdOssMirageFileFixture, OpenReadModeFileBeingWritten)
{
    auto entry = oss.getEntryWrite("/dummy");
    ASSERT_EQ(-ENOENT, file.Open("/dummy", O_RDONLY, {}, env));
}

TEST_F(XrdOssMirageFileFixture, OpenWithReturnCode)
{
    oss.getEntryWrite("/dummy").value()->open.return_code = 1111;

    ASSERT_EQ(-1111, file.Open("/dummy", {}, {}, env));
}

TEST_F(XrdOssMirageFileFixture, Read)
{
    file.Open("/dummy", O_RDONLY, {}, env);

    ASSERT_EQ(1000, file.Read(nullptr, 0, 1000));
}

TEST_F(XrdOssMirageFileFixture, ReadWithOffset)
{
    file.Open("/dummy", O_RDONLY, {}, env);

    ASSERT_EQ(1000, file.Read(nullptr, 1000, 1000));
}

TEST_F(XrdOssMirageFileFixture, ReadWithOffsetRemainingSmallerThanSize)
{
    file.Open("/dummy", O_RDONLY, {}, env);

    ASSERT_EQ(499,  file.Read(nullptr, 9500, 1000));
}

TEST_F(XrdOssMirageFileFixture, ReadWithReturnCodeAndPosition)
{
    {
        auto entry = oss.getEntryWrite("/dummy").value();
        entry->read.return_code = 1111;
        entry->read.return_position = 5500;
    }

    file.Open("/dummy", O_RDONLY, {}, env);

    ASSERT_EQ(1000,  file.Read(nullptr, 0, 1000));
    ASSERT_EQ(-1111, file.Read(nullptr, 5000, 1000));
}

TEST_F(XrdOssMirageFileFixture, ReadWithCharPattern)
{
    oss.getEntryWrite("/dummy").value()->pattern = "a";

    file.Open("/dummy", O_RDONLY, {}, env);

    char buffer[10]{};
    
    ASSERT_EQ(10, file.Read(&buffer, 0, sizeof(buffer)));
    ASSERT_EQ(0,  std::memcmp(buffer, "aaaaaaaaaa", sizeof(buffer)));
}

TEST_F(XrdOssMirageFileFixture, ReadWithStringPattern)
{
    oss.getEntryWrite("/dummy").value()->pattern = "abc";

    file.Open("/dummy", O_RDONLY, {}, env);

    char buffer[10]{};
    
    ASSERT_EQ(10, file.Read(&buffer, 0, sizeof(buffer)));
    ASSERT_EQ(0, std::memcmp(buffer, "abcabcabca", sizeof(buffer)));
}

TEST_F(XrdOssMirageFileFixture, Write)
{
    file.Open("/dummy", O_WRONLY, {}, env);

    ASSERT_EQ(1000, file.Read(nullptr, 0, 1000));
}

TEST_F(XrdOssMirageFileFixture, WriteUpdatesSize)
{
    file.Open("/dummy", O_WRONLY, {}, env);
    file.Write(nullptr, 0, 1000);
    file.Close();

    ASSERT_EQ(10999, oss.getEntryRead("/dummy").value().size);
}

TEST_F(XrdOssMirageFileFixture, WriteWithReturnCodeAndPosition)
{
    {
        auto entry = oss.getEntryWrite("/dummy").value();
        entry->write.return_code = 1111;
        entry->write.return_position = 5500;
    }

    file.Open("/dummy", O_WRONLY, {}, env);

    ASSERT_EQ(1000,  file.Write(nullptr, 0, 1000));
    ASSERT_EQ(-1111, file.Write(nullptr, 5000, 1000));
}

TEST_F(XrdOssMirageFileFixture, CloseReleasesFileLock)
{
    file.Open("/dummy", O_WRONLY, {}, env);

    ASSERT_EQ(XrdOssOK, file.Close());
    ASSERT_TRUE(oss.getEntryWrite("/dummy"));
}
