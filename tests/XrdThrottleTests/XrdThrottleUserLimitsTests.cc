#include "XrdThrottle/XrdThrottleManager.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdOuc/XrdOucTrace.hh"

#include <gtest/gtest.h>
#include <fstream>
#include <cstdio>
#include <unistd.h>

class XrdThrottleUserLimitsTests : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a logger (using stderr for test output)
        m_logger = new XrdSysLogger(STDERR_FILENO, 0);
        m_log = new XrdSysError(m_logger, "ThrottleTest");
        m_trace = new XrdOucTrace(m_log);
        m_manager = new XrdThrottleManager(m_log, m_trace);
        // Don't call Init() as it creates threads that may cause issues in tests
        // The LoadUserLimits function doesn't require Init() to be called
    }

    void TearDown() override {
        delete m_manager;
        delete m_trace;
        delete m_log;
        delete m_logger;
    }

    // Helper function to create a temporary config file
    std::string CreateTempConfig(const std::string& content) {
        char template_path[] = "/tmp/throttle_test_XXXXXX";
        int fd = mkstemp(template_path);
        if (fd < 0) {
            return "";
        }
        close(fd);

        std::ofstream file(template_path);
        file << content;
        file.close();

        return std::string(template_path);
    }

    // Helper function to remove a temporary file
    void RemoveTempFile(const std::string& path) {
        if (!path.empty()) {
            unlink(path.c_str());
        }
    }

    XrdSysLogger* m_logger;
    XrdSysError* m_log;
    XrdOucTrace* m_trace;
    XrdThrottleManager* m_manager;
};

TEST_F(XrdThrottleUserLimitsTests, LoadUserLimits_ValidConfig) {
    std::string config_content = R"(
[default]
name = *
maxconn = 200

[user1]
name = user1
maxconn = 25

[user2]
name = user2
maxconn = 50

[wildcarduser]
name = wildcarduser*
maxconn = 10
)";

    std::string config_file = CreateTempConfig(config_content);
    ASSERT_FALSE(config_file.empty()) << "Failed to create temporary config file";

    int result = m_manager->LoadUserLimits(config_file);
    EXPECT_EQ(0, result) << "LoadUserLimits should succeed";

    // Test exact matches
    EXPECT_EQ(25UL, m_manager->GetUserMaxConn("user1"));
    EXPECT_EQ(50UL, m_manager->GetUserMaxConn("user2"));

    // Test wildcard matches
    EXPECT_EQ(10UL, m_manager->GetUserMaxConn("wildcarduser1"));
    EXPECT_EQ(10UL, m_manager->GetUserMaxConn("wildcarduser2"));
    EXPECT_EQ(10UL, m_manager->GetUserMaxConn("wildcarduserabc"));

    // Test default catch-all
    EXPECT_EQ(200UL, m_manager->GetUserMaxConn("unknownuser"));
    EXPECT_EQ(200UL, m_manager->GetUserMaxConn("otheruser"));

    RemoveTempFile(config_file);
}

TEST_F(XrdThrottleUserLimitsTests, LoadUserLimits_NoDefault) {
    std::string config_content = R"(
[user1]
name = user1
maxconn = 25
)";

    std::string config_file = CreateTempConfig(config_content);
    ASSERT_FALSE(config_file.empty());

    int result = m_manager->LoadUserLimits(config_file);
    EXPECT_EQ(0, result);

    // Exact match should work
    EXPECT_EQ(25UL, m_manager->GetUserMaxConn("user1"));

    // Unknown user should return 0 (use global)
    EXPECT_EQ(0UL, m_manager->GetUserMaxConn("unknownuser"));

    RemoveTempFile(config_file);
}

TEST_F(XrdThrottleUserLimitsTests, LoadUserLimits_MissingName) {
    std::string config_content = R"(
[user1]
maxconn = 25
)";

    std::string config_file = CreateTempConfig(config_content);
    ASSERT_FALSE(config_file.empty());

    int result = m_manager->LoadUserLimits(config_file);
    EXPECT_EQ(0, result); // File loads but section is skipped

    // Should not have any limits loaded
    EXPECT_EQ(0UL, m_manager->GetUserMaxConn("user1"));

    RemoveTempFile(config_file);
}

TEST_F(XrdThrottleUserLimitsTests, LoadUserLimits_InvalidMaxconn) {
    std::string config_content = R"(
[user1]
name = user1
maxconn = 0

[user2]
name = user2
maxconn = -5
)";

    std::string config_file = CreateTempConfig(config_content);
    ASSERT_FALSE(config_file.empty());

    int result = m_manager->LoadUserLimits(config_file);
    EXPECT_EQ(0, result); // File loads but invalid sections are skipped

    // Should not have any limits loaded
    EXPECT_EQ(0UL, m_manager->GetUserMaxConn("user1"));
    EXPECT_EQ(0UL, m_manager->GetUserMaxConn("user2"));

    RemoveTempFile(config_file);
}

TEST_F(XrdThrottleUserLimitsTests, LoadUserLimits_NonexistentFile) {
    int result = m_manager->LoadUserLimits("/nonexistent/file.conf");
    EXPECT_NE(0, result) << "LoadUserLimits should fail for nonexistent file";
}

TEST_F(XrdThrottleUserLimitsTests, GetUserMaxConn_WildcardPriority) {
    std::string config_content = R"(
[default]
name = *
maxconn = 200

[user]
name = user*
maxconn = 50

[user1]
name = user1
maxconn = 25
)";

    std::string config_file = CreateTempConfig(config_content);
    ASSERT_FALSE(config_file.empty());

    int result = m_manager->LoadUserLimits(config_file);
    EXPECT_EQ(0, result);

    // Exact match should take highest priority
    EXPECT_EQ(25UL, m_manager->GetUserMaxConn("user1"));

    // Wildcard match should take priority over default
    EXPECT_EQ(50UL, m_manager->GetUserMaxConn("user2"));
    EXPECT_EQ(50UL, m_manager->GetUserMaxConn("userabc"));

    // Default should apply to others
    EXPECT_EQ(200UL, m_manager->GetUserMaxConn("otheruser"));

    RemoveTempFile(config_file);
}

TEST_F(XrdThrottleUserLimitsTests, ReloadUserLimits) {
    std::string config_content1 = R"(
[user1]
name = user1
maxconn = 25
)";

    std::string config_file = CreateTempConfig(config_content1);
    ASSERT_FALSE(config_file.empty());

    // Load initial config
    int result = m_manager->LoadUserLimits(config_file);
    EXPECT_EQ(0, result);
    EXPECT_EQ(25UL, m_manager->GetUserMaxConn("user1"));

    // Update config file
    std::string config_content2 = R"(
[user1]
name = user1
maxconn = 50
)";
    std::ofstream file(config_file);
    file << config_content2;
    file.close();

    // Reload by calling LoadUserLimits again (simulates ReloadUserLimits)
    result = m_manager->LoadUserLimits(config_file);
    EXPECT_EQ(0, result);
    EXPECT_EQ(50UL, m_manager->GetUserMaxConn("user1"));

    RemoveTempFile(config_file);
}

