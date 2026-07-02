#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace testing;

struct Base
{
    virtual int do_it(int i)
    {
        return i;
    }
};

struct MockBase : public Base
{
    MOCK_METHOD(int, do_it, (int), (override));
};

TEST(ExampleTest, gMockWorks)
{
    NiceMock<MockBase> mock;
    ON_CALL(mock, do_it(_)).WillByDefault(Invoke(
        [](auto i) {
            return i * 2;
        }));

    ASSERT_EQ(20, mock.do_it(10));
    ASSERT_EQ(30, mock.do_it(15));
}
