#include "XrdOssMirageFixture.hh"

#include <gtest/gtest.h>

#include <fcntl.h>

class XrdOssMirageXAttrFixture : public XrdOssMirageFixture
{
};

TEST_F(XrdOssMirageXAttrFixture, SetPropertiesShouldSucceed)
{
    EXPECT_EQ(0, xattr.Set("U.open.return_code", "1", 1, "/dummy", 0, 0));
    EXPECT_EQ(0, xattr.Set("U.read.return_code", "2", 1, "/dummy", 0, 0));
    EXPECT_EQ(0, xattr.Set("U.read.return_position", "3", 1, "/dummy", 0, 0));
    EXPECT_EQ(0, xattr.Set("U.write.return_code", "4", 1, "/dummy", 0, 0));
    EXPECT_EQ(0, xattr.Set("U.write.return_position", "5", 1, "/dummy", 0, 0));
    EXPECT_EQ(0, xattr.Set("U.pattern", "6", 1, "/dummy", 0, 0));
}

TEST_F(XrdOssMirageXAttrFixture, SetPropertiesChangesTheirValues)
{
    xattr.Set("U.open.return_code", "1", 1, "/dummy", 0, 0);
    xattr.Set("U.read.return_code", "2", 1, "/dummy", 0, 0);
    xattr.Set("U.read.return_position", "3", 1, "/dummy", 0, 0);
    xattr.Set("U.write.return_code", "4", 1, "/dummy", 0, 0);
    xattr.Set("U.write.return_position", "5", 1, "/dummy", 0, 0);
    xattr.Set("U.pattern", "6", 1, "/dummy", 0, 0);

    auto entry = oss.get_entry_read("/dummy").value();

    EXPECT_EQ(1, entry.open.return_code);
    EXPECT_EQ(2, entry.read.return_code);
    EXPECT_EQ(3, entry.read.return_position);
    EXPECT_EQ(4, entry.write.return_code);
    EXPECT_EQ(5, entry.write.return_position);
    EXPECT_EQ("6", entry.pattern);
}

TEST_F(XrdOssMirageXAttrFixture, SetInvalidPropertyReturnsINVALError)
{
    ASSERT_EQ(-EINVAL, xattr.Set("invalid", "1", 1, "/dummy", 0, 0));
}

TEST_F(XrdOssMirageXAttrFixture, SetPropertyOfAFileThatDoesNotExistReturnsINVALError)
{
    ASSERT_EQ(-EINVAL, xattr.Set("U.pattern", "1", 1, "/inexistent", 0, 0));
}

TEST_F(XrdOssMirageXAttrFixture, SetPropertyOfAFileThatIsBeingWrittenReturnsINVALError)
{
    auto entry = oss.get_entry_write("/dummy").value();

    ASSERT_EQ(-EINVAL, xattr.Set("U.pattern", "1", 1, "/dummy", 0, 0));
}

TEST_F(XrdOssMirageXAttrFixture, SetPropertiesWithValuesOutOfRangeReturnsINVALError)
{
    ASSERT_EQ(-EINVAL, xattr.Set("U.open.return_code", "18446744073709551615", 20, "/dummy", 0, 0));
    ASSERT_EQ(-EINVAL, xattr.Set("U.read.return_code", "18446744073709551615", 20, "/dummy", 0, 0));
    ASSERT_EQ(-EINVAL, xattr.Set("U.read.return_position", "18446744073709551615", 20, "/dummy", 0, 0));
    ASSERT_EQ(-EINVAL, xattr.Set("U.write.return_code", "18446744073709551615", 20, "/dummy", 0, 0));
    ASSERT_EQ(-EINVAL, xattr.Set("U.write.return_position", "18446744073709551615", 20, "/dummy", 0, 0));
}

TEST_F(XrdOssMirageXAttrFixture, SetPropertiesWithValuesOutOfRangeDoesNotChangeTheirValues)
{
    xattr.Set("U.open.return_code", "18446744073709551615", 20, "/dummy", 0, 0);
    xattr.Set("U.read.return_code", "18446744073709551615", 20, "/dummy", 0, 0);
    xattr.Set("U.read.return_position", "18446744073709551615", 20, "/dummy", 0, 0);
    xattr.Set("U.write.return_code", "18446744073709551615", 20, "/dummy", 0, 0);
    xattr.Set("U.write.return_position", "18446744073709551615", 20, "/dummy", 0, 0);

    auto entry = oss.get_entry_read("/dummy").value();

    EXPECT_EQ(0, entry.open.return_code);
    EXPECT_EQ(0, entry.read.return_code);
    EXPECT_EQ(0, entry.read.return_position);
    EXPECT_EQ(0, entry.write.return_code);
    EXPECT_EQ(0, entry.write.return_position);
}

TEST_F(XrdOssMirageXAttrFixture, GetPropertiesReturnsNumberOfReadBytes)
{
    {
        auto entry = oss.get_entry_write("/dummy").value();
        entry->open.return_code = 1;
        entry->read.return_code = 2;
        entry->read.return_position = 3;
        entry->write.return_code = 4;
        entry->write.return_position = 5;
        entry->pattern = "6";
    }

    char value = 0;
    EXPECT_EQ(1, xattr.Get("U.open.return_code", &value, 1, "/dummy", 0));
    EXPECT_EQ(1, xattr.Get("U.read.return_code", &value, 1, "/dummy", 0));
    EXPECT_EQ(1, xattr.Get("U.read.return_position", &value, 1, "/dummy", 0));
    EXPECT_EQ(1, xattr.Get("U.write.return_code", &value, 1, "/dummy", 0));
    EXPECT_EQ(1, xattr.Get("U.write.return_position", &value, 1, "/dummy", 0));
    EXPECT_EQ(1, xattr.Get("U.pattern", &value, 1, "/dummy", 0));
}

TEST_F(XrdOssMirageXAttrFixture, GetPropertiesReturnsTheirChangedValues)
{
    {
        auto entry = oss.get_entry_write("/dummy").value();
        entry->open.return_code = 1;
        entry->read.return_code = 2;
        entry->read.return_position = 3;
        entry->write.return_code = 4;
        entry->write.return_position = 5;
        entry->pattern = "6";
    }

    char value = 0;
    xattr.Get("U.open.return_code", &value, 1, "/dummy", 0);
    EXPECT_EQ('1', value);

    xattr.Get("U.read.return_code", &value, 1, "/dummy", 0);
    EXPECT_EQ('2', value);

    xattr.Get("U.read.return_position", &value, 1, "/dummy", 0);
    EXPECT_EQ('3', value);

    xattr.Get("U.write.return_code", &value, 1, "/dummy", 0);
    EXPECT_EQ('4', value);

    xattr.Get("U.write.return_position", &value, 1, "/dummy", 0);
    EXPECT_EQ('5', value);

    xattr.Get("U.pattern", &value, 1, "/dummy", 0);
    EXPECT_EQ('6', value);
}

TEST_F(XrdOssMirageXAttrFixture, GetInvalidPropertyReturnsINVALError)
{
    ASSERT_EQ(-EINVAL, xattr.Get("invalid", nullptr, 0, "/dummy", 0));
}

TEST_F(XrdOssMirageXAttrFixture, GetPropertyOfAFileThatDoesNotExistReturnsINVALError)
{
    ASSERT_EQ(-EINVAL, xattr.Get("U.pattern", nullptr, 0, "/inexistent", 0));
}

TEST_F(XrdOssMirageXAttrFixture, GetPropertyOfAFileThatIsBeingWrittenReturnsINVALError)
{
    auto entry = oss.get_entry_write("/dummy").value();

    ASSERT_EQ(-EINVAL, xattr.Get("U.pattern", nullptr, 0, "/dummy", 0));
}

TEST_F(XrdOssMirageXAttrFixture, DeletePropertiesShouldSucceed)
{
    EXPECT_EQ(0, xattr.Del("U.open.return_code", "/dummy", 0));
    EXPECT_EQ(0, xattr.Del("U.read.return_code", "/dummy", 0));
    EXPECT_EQ(0, xattr.Del("U.read.return_position", "/dummy", 0));
    EXPECT_EQ(0, xattr.Del("U.write.return_code", "/dummy", 0));
    EXPECT_EQ(0, xattr.Del("U.write.return_position", "/dummy", 0));
    EXPECT_EQ(0, xattr.Del("U.pattern", "/dummy", 0));
}

TEST_F(XrdOssMirageXAttrFixture, DeletePropertiesResetsTheirValues)
{
    {
        auto entry = oss.get_entry_write("/dummy").value();
        entry->open.return_code = 1;
        entry->read.return_code = 2;
        entry->read.return_position = 3;
        entry->write.return_code = 4;
        entry->write.return_position = 5;
        entry->pattern = "6";
    }

    xattr.Del("U.open.return_code", "/dummy", 0);
    xattr.Del("U.read.return_code", "/dummy", 0);
    xattr.Del("U.read.return_position", "/dummy", 0);
    xattr.Del("U.write.return_code", "/dummy", 0);
    xattr.Del("U.write.return_position", "/dummy", 0);
    xattr.Del("U.pattern", "/dummy", 0);

    auto entry = oss.get_entry_read("/dummy").value();

    EXPECT_EQ(0, entry.open.return_code);
    EXPECT_EQ(0, entry.read.return_code);
    EXPECT_EQ(0, entry.read.return_position);
    EXPECT_EQ(0, entry.write.return_code);
    EXPECT_EQ(0, entry.write.return_position);
    EXPECT_EQ("", entry.pattern);
}

TEST_F(XrdOssMirageXAttrFixture, DeleteInvalidPropertyReturnsINVALError)
{
    ASSERT_EQ(-EINVAL, xattr.Del("invalid", "/dummy", 0));
}

TEST_F(XrdOssMirageXAttrFixture, DeletePropertyOfAFileThatDoesNotExistReturnsINVALError)
{
    ASSERT_EQ(-EINVAL, xattr.Del("U.pattern", "/inexistent", 0));
}

TEST_F(XrdOssMirageXAttrFixture, DeletePropertyOfAFileThatIsBeingWrittenReturnsINVALError)
{
    auto entry = oss.get_entry_write("/dummy").value();

    ASSERT_EQ(-EINVAL, xattr.Del("U.pattern", "/dummy", 0));
}
