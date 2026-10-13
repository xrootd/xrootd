#undef NDEBUG

#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include <XrdCl/XrdClLog.hh>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

#include <sys/param.h>

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

std::string DefaultTmpTokenPath() {
    char path[MAXPATHLEN + 8];
    snprintf(path, sizeof(path), "/tmp/bt_u%d", static_cast<int>(geteuid()));
    return path;
}

// GetBearerToken probes /tmp/bt_u<uid>. Move an ambient file aside so a miss
// is not decided by a token that happened to be there.
class IsolateDefaultTmpToken {
   public:
    IsolateDefaultTmpToken() {
        m_path = DefaultTmpTokenPath();
        if (access(m_path.c_str(), F_OK) != 0) return;

        m_backup = m_path + ".xrdclhttp-bearer-test-bak";
        unlink(m_backup.c_str());
        if (rename(m_path.c_str(), m_backup.c_str()) != 0) {
            m_blocked = true;
            m_reason = strerror(errno);
            return;
        }
        m_moved = true;
    }

    ~IsolateDefaultTmpToken() {
        if (m_blocked) return;
        unlink(m_path.c_str());
        if (m_moved) rename(m_backup.c_str(), m_path.c_str());
    }

    bool Blocked() const { return m_blocked; }
    const std::string& Path() const { return m_path; }
    const std::string& Reason() const { return m_reason; }

    IsolateDefaultTmpToken(const IsolateDefaultTmpToken&) = delete;
    IsolateDefaultTmpToken& operator=(const IsolateDefaultTmpToken&) = delete;

   private:
    std::string m_path;
    std::string m_backup;
    std::string m_reason;
    bool m_moved{false};
    bool m_blocked{false};
};

// Same path and lock as the XrdOuc and ztn token tests. CTest runs those
// suites in parallel, and each one probes /tmp/bt_u<uid>.
class DefaultTmpTokenLock {
   public:
    DefaultTmpTokenLock() {
        const std::string path = "/tmp/xrd-bt-u" + std::to_string(static_cast<int>(geteuid())) + ".lock";
        m_fd = open(path.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
        if (m_fd < 0) return;
        if (flock(m_fd, LOCK_EX) != 0) {
            close(m_fd);
            m_fd = -1;
        }
    }

    ~DefaultTmpTokenLock() {
        if (m_fd < 0) return;
        flock(m_fd, LOCK_UN);
        close(m_fd);
    }

    bool Ok() const { return m_fd >= 0; }

    DefaultTmpTokenLock(const DefaultTmpTokenLock&) = delete;
    DefaultTmpTokenLock& operator=(const DefaultTmpTokenLock&) = delete;

   private:
    int m_fd{-1};
};

}  // anonymous namespace

class XrdClHttpGetBearerTokenTests : public ::testing::Test {
   protected:
    void SetUp() override {
        ASSERT_TRUE(m_lock.Ok()) << "cannot lock the default bearer-token path";
        ClearDiscoveryEnv();
    }
    void TearDown() override { ClearDiscoveryEnv(); }

    DefaultTmpTokenLock m_lock;
};

TEST_F(XrdClHttpGetBearerTokenTests, ReturnsTokenFromEnvironment) {
    EnvGuard env("BEARER_TOKEN", "http-jwt");

    EXPECT_EQ("http-jwt", XrdClHttp::GetBearerToken());
}

TEST_F(XrdClHttpGetBearerTokenTests, NotFoundIsSilentWithoutLogger) {
    IsolateDefaultTmpToken isolate;
    if (isolate.Blocked()) {
        GTEST_SKIP() << "cannot move aside " << isolate.Path() << ": " << isolate.Reason();
    }

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
    const std::string sentence = "Bearer token discovery failed at " + path +
                                 ": token file must be readable only by its owner";
    const auto at = log.out->messages.find(sentence);
    ASSERT_NE(std::string::npos, at) << log.out->messages;
    EXPECT_EQ(std::string::npos, log.out->messages.find(sentence, at + sentence.size()));
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
