#undef NDEBUG

#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

#include "XrdOuc/XrdOucBearerToken.hh"

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

std::string TempTokenPath() {
    char path[] = "/tmp/xrdouc-bearer-XXXXXX";
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

class XrdOucBearerTokenTests : public ::testing::Test {
   protected:
    void SetUp() override { ClearDiscoveryEnv(); }
    void TearDown() override { ClearDiscoveryEnv(); }
};

TEST_F(XrdOucBearerTokenTests, StripTrimsWhitespace) {
    EXPECT_EQ("token", XrdOucBearerToken::Strip("  token \n"));
    EXPECT_EQ("token", XrdOucBearerToken::Strip("\ttoken\r"));
    EXPECT_TRUE(XrdOucBearerToken::Strip("   ").empty());
}

TEST_F(XrdOucBearerTokenTests, FromEnvValue) {
    auto found = XrdOucBearerToken::FromEnvValue("  jwt-value  ");
    ASSERT_EQ(XrdOucBearerToken::Status::Found, found.status);
    EXPECT_EQ("jwt-value", found.token);

    auto empty = XrdOucBearerToken::FromEnvValue("   ");
    EXPECT_EQ(XrdOucBearerToken::Status::NotFound, empty.status);

    auto tooLarge = XrdOucBearerToken::FromEnvValue("0123456789", 5);
    ASSERT_EQ(XrdOucBearerToken::Status::Error, tooLarge.status);
    EXPECT_EQ(EMSGSIZE, tooLarge.errnum);
}

TEST_F(XrdOucBearerTokenTests, ReadFileNotFound) {
    auto result = XrdOucBearerToken::ReadFile("/tmp/xrdouc-bearer-missing-token");
    EXPECT_EQ(XrdOucBearerToken::Status::NotFound, result.status);
}

TEST_F(XrdOucBearerTokenTests, ReadFileRejectsGroupReadable) {
    auto path = TempTokenPath();
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(WriteTokenFile(path, "secret-token", S_IRUSR | S_IRGRP));

    auto result = XrdOucBearerToken::ReadFile(path.c_str());
    ASSERT_EQ(XrdOucBearerToken::Status::Error, result.status);
    EXPECT_EQ(EPERM, result.errnum);
    EXPECT_EQ(path, result.location);

    unlink(path.c_str());
}

TEST_F(XrdOucBearerTokenTests, ReadFileAcceptsOwnerOnlyToken) {
    auto path = TempTokenPath();
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(WriteTokenFile(path, "  bearer-jwt  \n", S_IRUSR | S_IWUSR));

    auto result = XrdOucBearerToken::ReadFile(path.c_str());
    ASSERT_EQ(XrdOucBearerToken::Status::Found, result.status);
    EXPECT_EQ("bearer-jwt", result.token);

    unlink(path.c_str());
}

TEST_F(XrdOucBearerTokenTests, TryEntryBearerTokenEnv) {
    EnvGuard env("BEARER_TOKEN", "env-token");

    auto result = XrdOucBearerToken::TryEntry("BEARER_TOKEN");
    ASSERT_EQ(XrdOucBearerToken::Status::Found, result.status);
    EXPECT_EQ("env-token", result.token);
}

TEST_F(XrdOucBearerTokenTests, TryEntryBearerTokenFile) {
    auto path = TempTokenPath();
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(WriteTokenFile(path, "file-token", S_IRUSR | S_IWUSR));
    EnvGuard env("BEARER_TOKEN_FILE", path.c_str());

    auto result = XrdOucBearerToken::TryEntry("BEARER_TOKEN_FILE");
    ASSERT_EQ(XrdOucBearerToken::Status::Found, result.status);
    EXPECT_EQ("file-token", result.token);

    unlink(path.c_str());
}

TEST_F(XrdOucBearerTokenTests, DiscoverPrefersBearerTokenOverFile) {
    auto path = TempTokenPath();
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(WriteTokenFile(path, "file-token", S_IRUSR | S_IWUSR));

    EnvGuard token("BEARER_TOKEN", "env-token");
    EnvGuard tokenFile("BEARER_TOKEN_FILE", path.c_str());

    auto result = XrdOucBearerToken::Discover();
    ASSERT_EQ(XrdOucBearerToken::Status::Found, result.status);
    EXPECT_EQ("env-token", result.token);

    unlink(path.c_str());
}

TEST_F(XrdOucBearerTokenTests, DiscoverUsesBearerTokenFile) {
    auto path = TempTokenPath();
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(WriteTokenFile(path, "file-token", S_IRUSR | S_IWUSR));
    EnvGuard tokenFile("BEARER_TOKEN_FILE", path.c_str());

    auto result = XrdOucBearerToken::Discover();
    ASSERT_EQ(XrdOucBearerToken::Status::Found, result.status);
    EXPECT_EQ("file-token", result.token);

    unlink(path.c_str());
}

TEST_F(XrdOucBearerTokenTests, DiscoverHonorsXrdZtnPath) {
    auto path = TempTokenPath();
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(WriteTokenFile(path, "ztn-token", S_IRUSR | S_IWUSR));

    auto result = XrdOucBearerToken::Discover(0, path.c_str());
    ASSERT_EQ(XrdOucBearerToken::Status::Found, result.status);
    EXPECT_EQ("ztn-token", result.token);

    unlink(path.c_str());
}
