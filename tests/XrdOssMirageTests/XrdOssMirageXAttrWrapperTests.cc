#include "XrdOssMirageXAttrWrapperFixture.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fcntl.h>

using testing::_;
using testing::Return;
using testing::StrEq;

//
// Extended-attribute routing: attribute operations are served by XrdOssMirage
// when the path is under the prefix and forwarded to the wrapped plugin
// otherwise.
//

TEST_F(XrdOssMirageXAttrWrapperFixture, SetUnderPrefixIsHandledByMirage)
{
    EXPECT_CALL(mock_xattr, Set).Times(0);

    ASSERT_EQ(0, wrapper.Set("U.open.return_code", "7", 1, "/mirage/dummy", 0, 0));

    ASSERT_EQ(7, oss.get_entry_read("/mirage/dummy").value().open.return_code);
}

TEST_F(XrdOssMirageXAttrWrapperFixture, SetOutsidePrefixIsForwardedToWrapped)
{
    EXPECT_CALL(mock_xattr, Set(StrEq("U.open.return_code"), _, _, StrEq("/stacked/file"), _, _))
        .WillOnce(Return(WRAPPED_RC));

    ASSERT_EQ(WRAPPED_RC, wrapper.Set("U.open.return_code", "7", 1, "/stacked/file", 0, 0));
}

TEST_F(XrdOssMirageXAttrWrapperFixture, GetUnderPrefixIsHandledByMirage)
{
    EXPECT_CALL(mock_xattr, Get).Times(0);

    oss.get_entry_write("/mirage/dummy").value()->open.return_code = 7;

    char value = 0;
    ASSERT_EQ(1, wrapper.Get("U.open.return_code", &value, 1, "/mirage/dummy", 0));
    ASSERT_EQ('7', value);
}

TEST_F(XrdOssMirageXAttrWrapperFixture, GetOutsidePrefixIsForwardedToWrapped)
{
    EXPECT_CALL(mock_xattr, Get(StrEq("U.open.return_code"), _, _, StrEq("/stacked/file"), _))
        .WillOnce(Return(WRAPPED_RC));

    char value = 0;
    ASSERT_EQ(WRAPPED_RC, wrapper.Get("U.open.return_code", &value, 1, "/stacked/file", 0));
}

TEST_F(XrdOssMirageXAttrWrapperFixture, DelUnderPrefixIsHandledByMirage)
{
    EXPECT_CALL(mock_xattr, Del).Times(0);

    ASSERT_EQ(0, wrapper.Del("U.pattern", "/mirage/dummy", 0));
}

TEST_F(XrdOssMirageXAttrWrapperFixture, DelOutsidePrefixIsForwardedToWrapped)
{
    EXPECT_CALL(mock_xattr, Del(StrEq("U.pattern"), StrEq("/stacked/file"), _))
        .WillOnce(Return(WRAPPED_RC));

    ASSERT_EQ(WRAPPED_RC, wrapper.Del("U.pattern", "/stacked/file", 0));
}

TEST_F(XrdOssMirageXAttrWrapperFixture, ListUnderPrefixIsHandledByMirage)
{
    EXPECT_CALL(mock_xattr, List).Times(0);

    ASSERT_EQ(-ENOTSUP, wrapper.List(nullptr, "/mirage/dummy", 0, 0));
}

TEST_F(XrdOssMirageXAttrWrapperFixture, ListOutsidePrefixIsForwardedToWrapped)
{
    EXPECT_CALL(mock_xattr, List(_, StrEq("/stacked/file"), _, _)).WillOnce(Return(WRAPPED_RC));

    ASSERT_EQ(WRAPPED_RC, wrapper.List(nullptr, "/stacked/file", 0, 0));
}

TEST_F(XrdOssMirageXAttrWrapperFixture, SetThenGetUnderPrefixRoundTrips)
{
    EXPECT_CALL(mock_xattr, Set).Times(0);
    EXPECT_CALL(mock_xattr, Get).Times(0);

    ASSERT_EQ(0, wrapper.Set("U.pattern", "abc", 3, "/mirage/dummy", 0, 0));

    char value[3] = {};
    ASSERT_EQ(3, wrapper.Get("U.pattern", value, sizeof(value), "/mirage/dummy", 0));
    ASSERT_EQ("abc", std::string(value, 3));
}
