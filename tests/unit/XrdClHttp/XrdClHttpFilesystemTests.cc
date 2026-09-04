#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../helper/external_mock.h"

#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClXRootDResponses.hh"

#include "XrdClHttp/XrdClHttpFilesystem.hh"
#include "XrdClHttp/XrdClHttpOps.hh"

#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <unistd.h>

using namespace std::string_literals;

using namespace testing;

class MockStatOp : public XrdClHttp::CurlStatOp
{
public:
    template <typename... Args>
    MockStatOp(Args&&... args) : XrdClHttp::CurlStatOp(std::forward<Args>(args)...) {}
    MOCK_METHOD(void, ctor, (XrdCl::ResponseHandler *, const std::string &, struct timespec, XrdCl::Log *, bool, XrdClHttp::CreateConnCalloutType, XrdClHttp::HeaderCallout *));
    MOCK_METHOD(bool, IsDone, ());
    MOCK_METHOD(bool, HasFailed, ());
    MOCK_METHOD((std::pair<int64_t, bool>), GetStatInfo, ());
};

class MockCopyOp : public XrdClHttp::CurlOperation
{
public:
    template <typename... Args>
    MockCopyOp(Args&&... args) : XrdClHttp::CurlOperation(nullptr, "", timespec{}, XrdCl::DefaultEnv::GetLog(), nullptr, nullptr) {}
    MOCK_METHOD(void, ctor, (XrdCl::ResponseHandler *, const std::string &, const XrdClHttp::CurlCopyOp::Headers &, const std::string &, const XrdClHttp::CurlCopyOp::Headers &, const XrdClHttp::CurlCopyOp::Headers &, XrdClHttp::TpcMode, struct timespec, XrdCl::Log *, XrdClHttp::CreateConnCalloutType));
    MOCK_METHOD(void, SetProgressHandler, (XrdCl::ProgressHandler *handler));
    MOCK_METHOD(bool, IsDone, ());
    MOCK_METHOD(bool, IsSentSucessfully, ());
    MOCK_METHOD(std::string, GetSendingFailureMessage, ());
    MOCK_METHOD(void, Success, (), (override));
    MOCK_METHOD(HttpVerb, GetVerb, (), (override, const));
};

class MockProgressHandler : public XrdCl::ProgressHandler
{
public:
    MOCK_METHOD(void, HandleProgress, (std::size_t, std::size_t), (override));
};

namespace XrdClHttp
{
    using StatOp = ExternalMock<NiceMock<MockStatOp>>;
    using CopyOp = ExternalMock<NiceMock<MockCopyOp>>;
}

#include "XrdClHttp/XrdClHttpFilesystem.cc"

class HttpThirdPartyCopyFixture : public testing::Test
{
protected:
    Filesystem fs;
    // Suffixed with the pid because ctest may run the tests of this suite in
    // parallel, each in its own process, and they share the temp directory.
    const std::string token_file_path{(std::filesystem::temp_directory_path() /
        ("xrdclhttp-tests-token-file-" + std::to_string(getpid()))).string()};
    const std::string missing_token_file_path{(std::filesystem::temp_directory_path() /
        ("xrdclhttp-tests-missing-token-file-" + std::to_string(getpid()))).string()};

    HttpThirdPartyCopyFixture() :
        fs(std::string("mock://unresolvable:1094//file"), std::make_shared<HandlerQueue>(100), XrdCl::DefaultEnv::GetLog())
    {
    }

    void write_token_file(const std::string &content)
    {
        std::ofstream file(token_file_path);
        file << content;
    }

    void SetUp() override
    {
        // A previous run can leave the files behind. Start from a clean state.
        std::filesystem::remove(token_file_path);
        std::filesystem::remove(missing_token_file_path);

        XrdClHttp::StatOp::Initialize();
        XrdClHttp::StatOp::AddOperation([](auto *mock)
        {
            ON_CALL(*mock, ctor).WillByDefault(Invoke([](XrdCl::ResponseHandler *rh, auto&&...) {
                    rh->HandleResponse(nullptr, nullptr);
                }));
        });

        XrdClHttp::CopyOp::Initialize();
        XrdClHttp::CopyOp::AddOperation([](auto *mock)
        {
            ON_CALL(*mock, ctor).WillByDefault(Invoke([](XrdCl::ResponseHandler *rh, auto&&...) {
                    rh->HandleResponse(nullptr, nullptr);
                }));
        });
    }

    void TearDown() override
    {
        // Restore the default number of streams. This keeps each test
        // independent of the others.
        XrdCl::DefaultEnv::GetEnv()->PutInt("SubStreamsPerChannel", 1);

        std::filesystem::remove(token_file_path);

        ASSERT_LE(XrdClHttp::StatOp::TotalOperations(), 1) << "StatOp object was not created or its operations were not consumed";
        ASSERT_LE(XrdClHttp::CopyOp::TotalOperations(), 1) << "CopyOp object was not created or its operations were not consumed";
    }
};

TEST_F(HttpThirdPartyCopyFixture, SendsTheSourceSizeToTheProgressHandler)
{
    XrdClHttp::StatOp::AddOperation([](auto *mock)
    {
        EXPECT_CALL(*mock, IsDone()).WillOnce(Return(true));
        EXPECT_CALL(*mock, HasFailed()).WillOnce(Return(false));
        EXPECT_CALL(*mock, GetStatInfo()).WillOnce(Return(std::make_pair(9999, false)));
    });

    NiceMock<MockProgressHandler> mock_progress;
    EXPECT_CALL(mock_progress, HandleProgress(_, _));
    EXPECT_CALL(mock_progress, HandleProgress(0, 9999)).Times(1);

    fs.ThirdPartyCopy("https://unresolvable:1094//file_src", "https://unresolvable:1094//file_dst", nullptr, &mock_progress);
}

TEST_F(HttpThirdPartyCopyFixture, SendsAZeroSizeToTheProgressHandlerWhenTheStatFails)
{
    XrdClHttp::StatOp::AddOperation([](auto *mock)
    {
        EXPECT_CALL(*mock, IsDone()).WillOnce(Return(true));
        EXPECT_CALL(*mock, HasFailed()).WillOnce(Return(true));
        EXPECT_CALL(*mock, GetStatInfo()).Times(0);
    });

    NiceMock<MockProgressHandler> mock_progress;
    EXPECT_CALL(mock_progress, HandleProgress(0, 0)).Times(1);

    fs.ThirdPartyCopy("https://unresolvable:1094//file_src", "https://unresolvable:1094//file_dst", nullptr, &mock_progress);
}

TEST_F(HttpThirdPartyCopyFixture, SendsAZeroSizeToTheProgressHandlerWhenTheStatDoesNotFinish)
{
    XrdClHttp::StatOp::AddOperation([](auto *mock)
    {
        EXPECT_CALL(*mock, IsDone()).WillOnce(Return(false));
        EXPECT_CALL(*mock, HasFailed()).Times(0);
        EXPECT_CALL(*mock, GetStatInfo()).Times(0);
    });

    NiceMock<MockProgressHandler> mock_progress;
    EXPECT_CALL(mock_progress, HandleProgress(0, 0)).Times(1);

    fs.ThirdPartyCopy("https://unresolvable:1094//file_src", "https://unresolvable:1094//file_dst", nullptr, &mock_progress);
}

TEST_F(HttpThirdPartyCopyFixture, SendsAZeroSizeToTheProgressHandlerWhenTheStatDoneCheckThrows)
{
    XrdClHttp::StatOp::AddOperation([](auto *mock)
    {
        EXPECT_CALL(*mock, IsDone()).WillOnce(Throw(std::runtime_error("error")));
        EXPECT_CALL(*mock, HasFailed()).Times(0);
        EXPECT_CALL(*mock, GetStatInfo()).Times(0);
    });

    NiceMock<MockProgressHandler> mock_progress;
    EXPECT_CALL(mock_progress, HandleProgress(0, 0)).Times(1);

    fs.ThirdPartyCopy("https://unresolvable:1094//file_src", "https://unresolvable:1094//file_dst", nullptr, &mock_progress);
}

TEST_F(HttpThirdPartyCopyFixture, SendsAZeroSizeToTheProgressHandlerWhenTheStatThrows)
{
    XrdClHttp::StatOp::AddOperation([](auto *mock)
    {
        EXPECT_CALL(*mock, IsDone()).WillOnce(Return(true));
        EXPECT_CALL(*mock, HasFailed()).WillOnce(Throw(std::runtime_error("error")));
    });

    NiceMock<MockProgressHandler> mock_progress;
    EXPECT_CALL(mock_progress, HandleProgress(0, 0)).Times(1);

    fs.ThirdPartyCopy("https://unresolvable:1094//file_src", "https://unresolvable:1094//file_dst", nullptr, &mock_progress);
}

TEST_F(HttpThirdPartyCopyFixture, GivesTheProgressHandlerToTheCopyOperation)
{
    NiceMock<MockProgressHandler> mock_progress;

    XrdClHttp::CopyOp::AddOperation([&mock_progress](auto *mock)
    {
        EXPECT_CALL(*mock, SetProgressHandler(&mock_progress)).Times(1);
    });

    fs.ThirdPartyCopy("https://unresolvable:1094//file_src", "https://unresolvable:1094//file_dst", nullptr, &mock_progress);
}

TEST_F(HttpThirdPartyCopyFixture, ReturnsErrPipelineFailedWhenTheCopyIsNotSent)
{
    XrdClHttp::CopyOp::AddOperation([](auto *mock)
    {
        EXPECT_CALL(*mock, IsDone()).WillOnce(Return(true));
        EXPECT_CALL(*mock, IsSentSucessfully()).WillOnce(Return(false));
    });

    XrdCl::XRootDStatus status = fs.ThirdPartyCopy("https://unresolvable:1094//file_src", "https://unresolvable:1094//file_dst", nullptr, nullptr);
    ASSERT_EQ(status.status, XrdCl::stError);
    ASSERT_EQ(status.code, XrdCl::errPipelineFailed);
}

TEST_F(HttpThirdPartyCopyFixture, SendsTheCompletedProgressToTheProgressHandlerWhenTheCopySucceeds)
{
    XrdClHttp::StatOp::AddOperation([](auto *mock)
    {
        EXPECT_CALL(*mock, IsDone()).WillOnce(Return(true));
        EXPECT_CALL(*mock, HasFailed()).WillOnce(Return(false));
        EXPECT_CALL(*mock, GetStatInfo()).WillOnce(Return(std::make_pair(9999, false)));
    });

    XrdClHttp::CopyOp::AddOperation([](auto *mock)
    {
        EXPECT_CALL(*mock, IsDone()).WillOnce(Return(true));
        EXPECT_CALL(*mock, IsSentSucessfully()).WillOnce(Return(true));
    });

    NiceMock<MockProgressHandler> mock_progress;
    EXPECT_CALL(mock_progress, HandleProgress(0, 9999)).Times(1);
    EXPECT_CALL(mock_progress, HandleProgress(9999, 9999)).Times(1);

    fs.ThirdPartyCopy("https://unresolvable:1094//file_src", "https://unresolvable:1094//file_dst", nullptr, &mock_progress);
}

TEST_F(HttpThirdPartyCopyFixture, ReturnsOkWhenTheCopySucceeds)
{
    XrdClHttp::StatOp::AddOperation([](auto *mock)
    {
        EXPECT_CALL(*mock, IsDone()).WillOnce(Return(true));
        EXPECT_CALL(*mock, HasFailed()).WillOnce(Return(false));
        EXPECT_CALL(*mock, GetStatInfo()).WillOnce(Return(std::make_pair(9999, false)));
    });

    XrdClHttp::CopyOp::AddOperation([](auto *mock)
    {
        EXPECT_CALL(*mock, IsDone()).WillOnce(Return(true));
        EXPECT_CALL(*mock, IsSentSucessfully()).WillOnce(Return(true));
    });

    NiceMock<MockProgressHandler> mock_progress;

    XrdCl::XRootDStatus status = fs.ThirdPartyCopy("http://unresolvable:1094//file_src", "http://unresolvable:1094//file_dst", nullptr, &mock_progress);
    ASSERT_EQ(status.status, XrdCl::stOK);
}

TEST_F(HttpThirdPartyCopyFixture, SendsTheOverwriteHeaderAsFalseWithoutTheForceProperty)
{
    XrdClHttp::CopyOp::AddOperation([](auto *mock)
    {
        EXPECT_CALL(*mock, ctor(_, _, _, _, _, Contains(Pair("Overwrite"s, "F"s)), _, _, _, _));
    });

    fs.ThirdPartyCopy("https://unresolvable:1094//file_src", "https://unresolvable:1094//file_dst", nullptr, nullptr);
}

TEST_F(HttpThirdPartyCopyFixture, SendsTheOverwriteHeaderAsTrueWithTheForceProperty)
{
    XrdClHttp::CopyOp::AddOperation([](auto *mock)
    {
        EXPECT_CALL(*mock, ctor(_, _, _, _, _, Contains(Pair("Overwrite"s, "T"s)), _, _, _, _));
    });

    XrdCl::PropertyList properties;
    properties.Set("force", "1");

    fs.ThirdPartyCopy("https://unresolvable:1094//file_src", "https://unresolvable:1094//file_dst", &properties, nullptr);
}

TEST_F(HttpThirdPartyCopyFixture, UsesThePullModeWithoutTheThirdPartyModeProperty)
{
    XrdClHttp::CopyOp::AddOperation([](auto *mock)
    {
        EXPECT_CALL(*mock, ctor(_, _, _, _, _, _, XrdClHttp::TpcMode::Pull, _, _, _));
    });

    fs.ThirdPartyCopy("https://unresolvable:1094//file_src", "https://unresolvable:1094//file_dst", nullptr, nullptr);
}

TEST_F(HttpThirdPartyCopyFixture, UsesThePushModeWhenTheThirdPartyModeIsPush)
{
    XrdClHttp::CopyOp::AddOperation([](auto *mock)
    {
        EXPECT_CALL(*mock, ctor(_, _, _, _, _, _, XrdClHttp::TpcMode::Push, _, _, _));
    });

    XrdCl::PropertyList properties;
    properties.Set("thirdPartyMode", "push");

    fs.ThirdPartyCopy("https://unresolvable:1094//file_src", "https://unresolvable:1094//file_dst", &properties, nullptr);
}

TEST_F(HttpThirdPartyCopyFixture, SendsTheStreamCountHeaderAsOneByDefault)
{
    XrdClHttp::CopyOp::AddOperation([](auto *mock)
    {
        EXPECT_CALL(*mock, ctor(_, _, _, _, _, Contains(Pair("X-Number-Of-Streams"s, "1"s)), _, _, _, _));
    });

    fs.ThirdPartyCopy("https://unresolvable:1094//file_src", "https://unresolvable:1094//file_dst", nullptr, nullptr);
}

TEST_F(HttpThirdPartyCopyFixture, SendsTheStreamCountHeaderFromTheSubStreamsPerChannelSetting)
{
    XrdClHttp::CopyOp::AddOperation([](auto *mock)
    {
        EXPECT_CALL(*mock, ctor(_, _, _, _, _, Contains(Pair("X-Number-Of-Streams"s, "20"s)), _, _, _, _));
    });

    XrdCl::DefaultEnv::GetEnv()->PutInt("SubStreamsPerChannel", 20);

    fs.ThirdPartyCopy("https://unresolvable:1094//file_src", "https://unresolvable:1094//file_dst", nullptr, nullptr);
}

TEST_F(HttpThirdPartyCopyFixture, SendsNoAuthorizationHeaderWithoutTheTokenFileProperty)
{
    XrdClHttp::CopyOp::AddOperation([](auto *mock)
    {
        auto has_no_authorization_header = Not(Contains(Pair("Authorization"s, _)));

        EXPECT_CALL(*mock, ctor(_, _, has_no_authorization_header, _, has_no_authorization_header, _, _, _, _, _));
    });

    fs.ThirdPartyCopy("https://unresolvable:1094//file_src", "https://unresolvable:1094//file_dst", nullptr, nullptr);
}

TEST_F(HttpThirdPartyCopyFixture, ReturnsErrInvalidArgsWhenTheSourceIsNotHttp)
{
    XrdCl::XRootDStatus status = fs.ThirdPartyCopy("root://unresolvable:1094//file_src", "https://unresolvable:1094//file_dst", nullptr, nullptr);
    ASSERT_EQ(status.status, XrdCl::stError);
    ASSERT_EQ(status.code, XrdCl::errInvalidArgs);
}

TEST_F(HttpThirdPartyCopyFixture, ReturnsErrInvalidArgsWhenTheDestinationIsNotHttp)
{
    XrdCl::XRootDStatus status = fs.ThirdPartyCopy("https://unresolvable:1094//file_src", "root://unresolvable:1094//file_dst", nullptr, nullptr);
    ASSERT_EQ(status.status, XrdCl::stError);
    ASSERT_EQ(status.code, XrdCl::errInvalidArgs);
}

TEST_F(HttpThirdPartyCopyFixture, ReturnsOkWhenBothUrlsUseTheHttpProtocol)
{
    XrdClHttp::CopyOp::AddOperation([](auto *mock)
    {
        EXPECT_CALL(*mock, IsDone()).WillOnce(Return(true));
        EXPECT_CALL(*mock, IsSentSucessfully()).WillOnce(Return(true));
    });

    XrdCl::XRootDStatus status = fs.ThirdPartyCopy("http://unresolvable:1094//file_src", "http://unresolvable:1094//file_dst", nullptr, nullptr);
    ASSERT_EQ(status.status, XrdCl::stOK);
}

TEST_F(HttpThirdPartyCopyFixture, ReturnsErrAuthFailedWhenTheTokenFileCannotBeParsed)
{
    XrdCl::PropertyList properties;
    properties.Set("thirdPartyTokenFile", missing_token_file_path);

    XrdCl::XRootDStatus status = fs.ThirdPartyCopy("https://unresolvable:1094//file_src", "https://unresolvable:1094//file_dst", &properties, nullptr);
    ASSERT_EQ(status.status, XrdCl::stError);
    ASSERT_EQ(status.code, XrdCl::errAuthFailed);
}

TEST_F(HttpThirdPartyCopyFixture, SendsTheAuthorizationHeadersFromTheTokenFile)
{
    XrdClHttp::CopyOp::AddOperation([](auto *mock)
    {
        EXPECT_CALL(*mock, ctor(_, _, Contains(Pair("Authorization"s, "Bearer TokenA"s)), _,
                                Contains(Pair("Authorization"s, "Bearer TokenB"s)), _, _, _, _, _));
    });

    write_token_file("TokenA\nTokenB\n");

    XrdCl::PropertyList properties;
    properties.Set("thirdPartyTokenFile", token_file_path);

    fs.ThirdPartyCopy("https://unresolvable:1094//file_src", "https://unresolvable:1094//file_dst", &properties, nullptr);
}
