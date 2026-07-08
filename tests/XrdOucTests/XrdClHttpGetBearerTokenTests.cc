#undef NDEBUG

#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <XrdCl/XrdClLog.hh>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <string>

#include "XrdClHttp/XrdClHttpUtil.hh"

namespace {

class EnvGuard {
   public:
    EnvGuard(const char* name, const char* value) {
        m_name = name;
        if (const char* old = getenv(name)) {
            m_hadOld = true;
            m_oldValue = old;
        }
        if (value) {
            setenv(name, value, 1);
        } else {
            unsetenv(name);
        }
    }

    ~EnvGuard() {
        if (m_hadOld) {
            setenv(m_name.c_str(), m_oldValue.c_str(), 1);
        } else {
            unsetenv(m_name.c_str());
        }
    }

   private:
    std::string m_name;
    bool m_hadOld{false};
    std::string m_oldValue;
};

class StringLogOut : public XrdCl::LogOut {
   public:
    std::string messages;

    void Write(const std::string& message) override { messages += message; }
};

struct TestLogger {
    StringLogOut* out{nullptr};
    XrdCl::Log logger;

    TestLogger() {
        out = new StringLogOut();
        logger.SetOutput(out);
        logger.SetLevel(XrdCl::Log::DebugMsg);
        logger.SetMask("warning", XrdClHttp::kLogXrdClHttp);
        logger.SetMask("debug", XrdClHttp::kLogXrdClHttp);
    }

    ~TestLogger() { logger.SetOutput(new XrdCl::LogOutCerr()); }
};

std::string TempTokenPath() {
    char path[] = "/tmp/xrdclhttp-bearer-XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) {
        ADD_FAILURE() << "mkstemp failed: " << strerror(errno);
        return {};
    }
    close(fd);
    unlink(path);
    return std::string(path);
}

bool WriteTokenFile(const std::string& path, const std::string& contents, mode_t mode) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << contents;
    out.close();
    return chmod(path.c_str(), mode) == 0;
}

void ClearDiscoveryEnv() {
    unsetenv("BEARER_TOKEN");
    unsetenv("BEARER_TOKEN_FILE");
    unsetenv("XDG_RUNTIME_DIR");
}

}  // anonymous namespace

class XrdClHttpGetBearerTokenTests : public ::testing::Test {
   protected:
    void SetUp() override { ClearDiscoveryEnv(); }
    void TearDown() override { ClearDiscoveryEnv(); }
};

TEST_F(XrdClHttpGetBearerTokenTests, ReturnsTokenFromEnvironment) {
    EnvGuard env("BEARER_TOKEN", "http-jwt");

    EXPECT_EQ("http-jwt", XrdClHttp::GetBearerToken());
}

TEST_F(XrdClHttpGetBearerTokenTests, NotFoundIsSilentWithoutLogger) {
    TestLogger log;

    EXPECT_TRUE(XrdClHttp::GetBearerToken(&log.logger).empty());
    EXPECT_TRUE(log.out->messages.empty());
}

TEST_F(XrdClHttpGetBearerTokenTests, DiscoveryErrorIsLogged) {
    auto path = TempTokenPath();
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(WriteTokenFile(path, "secret-token", S_IRUSR | S_IRGRP));
    EnvGuard tokenFile("BEARER_TOKEN_FILE", path.c_str());

    TestLogger log;

    EXPECT_TRUE(XrdClHttp::GetBearerToken(&log.logger).empty());
    EXPECT_NE(std::string::npos, log.out->messages.find("Bearer token discovery failed"));
    EXPECT_NE(std::string::npos, log.out->messages.find(path));
    EXPECT_NE(std::string::npos, log.out->messages.find("readable only by its owner"));
    unlink(path.c_str());
}

TEST_F(XrdClHttpGetBearerTokenTests, DiscoveryErrorWithoutLoggerReturnsEmpty) {
    auto path = TempTokenPath();
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(WriteTokenFile(path, "secret-token", S_IRUSR | S_IRGRP));
    EnvGuard tokenFile("BEARER_TOKEN_FILE", path.c_str());

    EXPECT_TRUE(XrdClHttp::GetBearerToken(nullptr).empty());

    unlink(path.c_str());
}
