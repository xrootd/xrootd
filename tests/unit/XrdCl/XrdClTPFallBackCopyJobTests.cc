#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClTPFallBackCopyJob.hh"
#include "XrdCl/XrdClStatus.hh"
#include "XrdCl/XrdClPlugInInterface.hh"
#include "XrdCl/XrdClPlugInManager.hh"
#include "XrdCl/XrdClThirdPartyCopyPlugIn.hh"


using namespace testing;
using namespace XrdCl;

class MockFactory: public XrdCl::PlugInFactory
{
public:
  MOCK_METHOD(FilePlugIn *, CreateFile, (const std::string &), (override));
  MOCK_METHOD(FileSystemPlugIn *, CreateFileSystem, (const std::string &), (override));
};

class MockPluginFileSystem : public XrdCl::FileSystemPlugIn,
                             public XrdCl::ThirdPartyCopyPlugIn
{
public:
  MOCK_METHOD(XRootDStatus, ThirdPartyCopy, (const std::string &, const std::string &, const PropertyList *, ProgressHandler *, time_t), (override));
};

class MockCopyProgressHandler : public CopyProgressHandler
{
public:
  MOCK_METHOD(void, JobProgress, (uint32_t, uint64_t, uint64_t), (override));
};

class TPFallBackCopyJobFixture : public testing::Test
{
protected:
    // Arbitrary values. They show that the job returns the plug-in status.
    static constexpr uint16_t plugin_status = 123;
    static constexpr uint16_t plugin_code = 456;

    NiceMock<MockFactory> *factory;
    NiceMock<MockPluginFileSystem> *plugin_filesystem;

    PropertyList properties;
    PropertyList results;
    TPFallBackCopyJob job;

    TPFallBackCopyJobFixture() :
        factory(new NiceMock<MockFactory>),
        plugin_filesystem(new NiceMock<MockPluginFileSystem>),
        job(0, &properties, &results)
    {
    }

    void SetUp() override
    {
        ON_CALL(*factory, CreateFileSystem(_)).WillByDefault(Return(plugin_filesystem));
        ON_CALL(*plugin_filesystem, ThirdPartyCopy(_, _, _, _, _)).WillByDefault(Return(XRootDStatus{plugin_status, plugin_code}));

        DefaultEnv::GetPlugInManager()->RegisterFactory("mock://*", factory);

        properties.Set("source", "mock://unresolvable:1094//src");
        properties.Set("target", "mock://unresolvable:1094//dst");
        job.Init();
    }

    void TearDown() override
    {
        DefaultEnv::GetPlugInManager()->RegisterFactory("mock://*", nullptr);
    }
};

TEST_F(TPFallBackCopyJobFixture, RootProtocolCreatesNoPlugInFileSystem)
{
    Mock::AllowLeak(plugin_filesystem);

    EXPECT_CALL(*factory, CreateFileSystem(_)).Times(0);

    properties.Set("source", "root://unresolvable:1094//src");
    properties.Set("target", "root://unresolvable:1094//dst");
    job.Init();

    job.Run();
}

TEST_F(TPFallBackCopyJobFixture, RootProtocolDoesNotReturnErrNotSupported)
{
    Mock::AllowLeak(plugin_filesystem);

    properties.Set("source", "root://unresolvable:1094//src");
    properties.Set("target", "root://unresolvable:1094//dst");
    job.Init();

    XRootDStatus status = job.Run();
    ASSERT_NE(status.code, errNotSupported);
}

TEST_F(TPFallBackCopyJobFixture, UnregisteredProtocolCreatesNoPlugInFileSystem)
{
    Mock::AllowLeak(plugin_filesystem);

    EXPECT_CALL(*factory, CreateFileSystem(_)).Times(0);

    properties.Set("source", "noprot://unresolvable:1094//src");
    job.Init();

    job.Run();
}

TEST_F(TPFallBackCopyJobFixture, UnregisteredProtocolReturnsErrNotSupported)
{
    Mock::AllowLeak(plugin_filesystem);

    properties.Set("source", "noprot://unresolvable:1094//src");
    job.Init();

    XRootDStatus status = job.Run();
    ASSERT_EQ(status.status, stError);
    ASSERT_EQ(status.code, errNotSupported);
}

TEST_F(TPFallBackCopyJobFixture, RegisteredProtocolCreatesThePlugInFileSystemForTheSourceUrl)
{
    EXPECT_CALL(*factory, CreateFileSystem("mock://unresolvable:1094//src")).Times(1);

    job.Run();
}

TEST_F(TPFallBackCopyJobFixture, RegisteredProtocolCallsThirdPartyCopyWithTheSourceAndTargetUrls)
{
    EXPECT_CALL(*plugin_filesystem, ThirdPartyCopy("mock://unresolvable:1094//src", "mock://unresolvable:1094//dst", _, _, _)).Times(1);

    job.Run();
}

TEST_F(TPFallBackCopyJobFixture, RegisteredProtocolReturnsTheStatusOfThirdPartyCopy)
{
    XRootDStatus status = job.Run();
    ASSERT_EQ(status.status, plugin_status);
    ASSERT_EQ(status.code, plugin_code);
}

TEST_F(TPFallBackCopyJobFixture, ProgressHandlerReceivesTheProgressOfThirdPartyCopy)
{
    MockCopyProgressHandler progress_handler;
    EXPECT_CALL(progress_handler, JobProgress(0, 1, 2)).Times(1);

    ON_CALL(*plugin_filesystem, ThirdPartyCopy("mock://unresolvable:1094//src", "mock://unresolvable:1094//dst", _, _, _))
        .WillByDefault(Invoke(
            [](const std::string &, const std::string &, const PropertyList *, ProgressHandler *progress, time_t) {
                progress->HandleProgress(1, 2);
                return XRootDStatus{};
            }));

    job.Run(&progress_handler);
}

TEST_F(TPFallBackCopyJobFixture, ProgressHandlerReceivesTheLastKnownTotalWhenThirdPartyCopyOmitsIt)
{
    MockCopyProgressHandler progress_handler;
    EXPECT_CALL(progress_handler, JobProgress(_, _, _)).Times(1);
    EXPECT_CALL(progress_handler, JobProgress(0, 3, 2)).Times(1);

    ON_CALL(*plugin_filesystem, ThirdPartyCopy("mock://unresolvable:1094//src", "mock://unresolvable:1094//dst", _, _, _))
        .WillByDefault(Invoke(
            [](const std::string &, const std::string &, const PropertyList *, ProgressHandler *progress, time_t) {
                progress->HandleProgress(1, 2);
                progress->HandleProgress(3);
                return XRootDStatus{};
            }));

    job.Run(&progress_handler);
}

TEST_F(TPFallBackCopyJobFixture, ThirdPartyCopyReceivesNoProgressHandlerWhenTheJobRunsWithoutOne)
{
    EXPECT_CALL(*plugin_filesystem, ThirdPartyCopy(_, _, _, IsNull(), _)).Times(1);

    job.Run();
}
