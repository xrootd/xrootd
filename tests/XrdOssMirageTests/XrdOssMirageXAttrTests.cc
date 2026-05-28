#include "XrdOssMirageFixture.hh"

#include <gtest/gtest.h>

#include <fcntl.h>

class XrdOssMirageXAttrFixture : public XrdOssMirageFixture
{
};

TEST_F(XrdOssMirageXAttrFixture, Set)
{
    ASSERT_EQ(0, xattr.Set("U.open.return_code", "1", 1, "/dummy", 0, 0));
    ASSERT_EQ(0, xattr.Set("U.read.return_code", "2", 1, "/dummy", 0, 0));
    ASSERT_EQ(0, xattr.Set("U.read.return_position", "3", 1, "/dummy", 0, 0));
    ASSERT_EQ(0, xattr.Set("U.write.return_code", "4", 1, "/dummy", 0, 0));
    ASSERT_EQ(0, xattr.Set("U.write.return_position", "5", 1, "/dummy", 0, 0));
    ASSERT_EQ(0, xattr.Set("U.pattern", "6", 1, "/dummy", 0, 0));

    auto entry = oss.getEntryRead("/dummy").value();
    ASSERT_EQ(1, entry.open.return_code);
    ASSERT_EQ(2, entry.read.return_code);
    ASSERT_EQ(3, entry.read.return_position);
    ASSERT_EQ(4, entry.write.return_code);
    ASSERT_EQ(5, entry.write.return_position);
    ASSERT_EQ("6", entry.pattern);
}

TEST_F(XrdOssMirageXAttrFixture, SetInvalidProperty)
{
    ASSERT_EQ(-EINVAL, xattr.Set("invalid", "1", 1, "/dummy", 0, 0));
}

TEST_F(XrdOssMirageXAttrFixture, SetWithInexistentFile)
{
    ASSERT_EQ(-EINVAL, xattr.Set("U.pattern", "1", 1, "/inexistent", 0, 0));
}

TEST_F(XrdOssMirageXAttrFixture, SetWithFileBeingWritten)
{
    auto entry = oss.getEntryWrite("/dummy").value();

    ASSERT_EQ(-EINVAL, xattr.Set("U.pattern", "1", 1, "/dummy", 0, 0));
}

TEST_F(XrdOssMirageXAttrFixture, SetOutOfRange)
{
    xattr.Set("U.open.return_code", "18446744073709551615", 20, "/dummy", 0, 0);
    xattr.Set("U.read.return_code", "18446744073709551615", 20, "/dummy", 0, 0);
    xattr.Set("U.read.return_position", "18446744073709551615", 20, "/dummy", 0, 0);
    xattr.Set("U.write.return_code", "18446744073709551615", 20, "/dummy", 0, 0);
    xattr.Set("U.write.return_position", "18446744073709551615", 20, "/dummy", 0, 0);

    auto entry = oss.getEntryRead("/dummy").value();
    ASSERT_EQ(0, entry.open.return_code);
    ASSERT_EQ(0, entry.read.return_code);
    ASSERT_EQ(0, entry.read.return_position);
    ASSERT_EQ(0, entry.write.return_code);
    ASSERT_EQ(0, entry.write.return_position);
}

TEST_F(XrdOssMirageXAttrFixture, Get)
{
    {
        auto entry = oss.getEntryWrite("/dummy").value();
        entry->open.return_code = 1;
        entry->read.return_code = 2;
        entry->read.return_position = 3;
        entry->write.return_code = 4;
        entry->write.return_position = 5;
        entry->pattern = "6";
    }

    char value = 0;
    ASSERT_EQ(1, xattr.Get("U.open.return_code", &value, 1, "/dummy", 0));
    ASSERT_EQ('1', value);

    ASSERT_EQ(1, xattr.Get("U.read.return_code", &value, 1, "/dummy", 0));
    ASSERT_EQ('2', value);

    ASSERT_EQ(1, xattr.Get("U.read.return_position", &value, 1, "/dummy", 0));
    ASSERT_EQ('3', value);

    ASSERT_EQ(1, xattr.Get("U.write.return_code", &value, 1, "/dummy", 0));
    ASSERT_EQ('4', value);

    ASSERT_EQ(1, xattr.Get("U.write.return_position", &value, 1, "/dummy", 0));
    ASSERT_EQ('5', value);

    ASSERT_EQ(1, xattr.Get("U.pattern", &value, 1, "/dummy", 0));
    ASSERT_EQ('6', value);
}

TEST_F(XrdOssMirageXAttrFixture, GetInvalidProperty)
{
    ASSERT_EQ(-EINVAL, xattr.Get("invalid", nullptr, 0, "/dummy", 0));
}

TEST_F(XrdOssMirageXAttrFixture, GetWithInexistentFile)
{
    ASSERT_EQ(-EINVAL, xattr.Get("U.pattern", nullptr, 0, "/inexistent", 0));
}

TEST_F(XrdOssMirageXAttrFixture, GetWithFileBeingWritten)
{
    auto entry = oss.getEntryWrite("/dummy").value();

    ASSERT_EQ(-EINVAL, xattr.Get("U.pattern", nullptr, 0, "/dummy", 0));
}


TEST_F(XrdOssMirageXAttrFixture, Del)
{
    {
        auto entry = oss.getEntryWrite("/dummy").value();
        entry->open.return_code = 1;
        entry->read.return_code = 2;
        entry->read.return_position = 3;
        entry->write.return_code = 4;
        entry->write.return_position = 5;
        entry->pattern = "6";
    }

    ASSERT_EQ(0, xattr.Del("U.open.return_code", "/dummy", 0));
    ASSERT_EQ(0, xattr.Del("U.read.return_code", "/dummy", 0));
    ASSERT_EQ(0, xattr.Del("U.read.return_position", "/dummy", 0));
    ASSERT_EQ(0, xattr.Del("U.write.return_code", "/dummy", 0));
    ASSERT_EQ(0, xattr.Del("U.write.return_position", "/dummy", 0));
    ASSERT_EQ(0, xattr.Del("U.pattern", "/dummy", 0));

    auto entry = oss.getEntryRead("/dummy").value();
    ASSERT_EQ(0, entry.open.return_code);
    ASSERT_EQ(0, entry.read.return_code);
    ASSERT_EQ(0, entry.read.return_position);
    ASSERT_EQ(0, entry.write.return_code);
    ASSERT_EQ(0, entry.write.return_position);
    ASSERT_EQ("", entry.pattern);
}

TEST_F(XrdOssMirageXAttrFixture, DelInvalidProperty)
{
    ASSERT_EQ(-EINVAL, xattr.Del("invalid", "/dummy", 0));
}

TEST_F(XrdOssMirageXAttrFixture, DelWithInexistentFile)
{
    ASSERT_EQ(-EINVAL, xattr.Del("U.pattern", "/inexistent", 0));
}

TEST_F(XrdOssMirageXAttrFixture, DelWithFileBeingWritten)
{
    auto entry = oss.getEntryWrite("/dummy").value();

    ASSERT_EQ(-EINVAL, xattr.Del("U.pattern", "/dummy", 0));
}
