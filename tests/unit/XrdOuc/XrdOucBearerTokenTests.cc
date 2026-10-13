#undef NDEBUG

#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

#include "XrdOuc/XrdOucBearerToken.hh"
#include "XrdOuc/XrdOucEnv.hh"

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

const char* FailureReason(int errnum) {
    switch (errnum) {
        case EPERM:
            return "token file must be readable only by its owner";
        case EMSGSIZE:
            return "token exceeds the maximum allowed size";
        case EIO:
            return "unable to read the complete token from file";
        default:
            return strerror(errnum);
    }
}

// Same sentence Result::Failure() and the HTTP client log.
std::string ExpectedFailure(const std::string& entry, const std::string& location, int errnum) {
    const char* why = FailureReason(errnum);
    EXPECT_NE(nullptr, why) << "strerror(" << errnum << ") returned null";
    std::string msg = "Bearer token discovery failed at ";
    msg += location.empty() ? entry : location;
    msg += ": ";
    if (why) msg += why;
    return msg;
}

std::string ExpandedDefaultTmpPath() {
    char path[MAXPATHLEN + 8];
    snprintf(path, sizeof(path), "/tmp/bt_u%d", static_cast<int>(geteuid()));
    return path;
}

std::string XdgTokenPath(const std::string& runtimeDir) {
    return runtimeDir + "/bt_u" + std::to_string(geteuid());
}

// Discover always probes /tmp/bt_u<uid>. Move an ambient file aside so a test
// that expects a later step, or a total miss, is not decided by that file.
class IsolateDefaultTmpToken {
   public:
    IsolateDefaultTmpToken() {
        m_path = ExpandedDefaultTmpPath();
        if (access(m_path.c_str(), F_OK) != 0) return;

        m_backup = m_path + ".xrdouc-bearer-test-bak";
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

class TempDir {
   public:
    static TempDir Create() {
        char tmpl[] = "/tmp/xrdouc-bearer-xdg-XXXXXX";
        if (!mkdtemp(tmpl)) return {};
        TempDir dir;
        dir.m_path = tmpl;
        return dir;
    }

    TempDir() = default;
    TempDir(TempDir&& other) noexcept : m_path(std::move(other.m_path)) { other.m_path.clear(); }
    TempDir& operator=(TempDir&& other) noexcept {
        if (this != &other) {
            Reset();
            m_path = std::move(other.m_path);
            other.m_path.clear();
        }
        return *this;
    }
    ~TempDir() { Reset(); }

    bool Ok() const { return !m_path.empty(); }
    const std::string& Path() const { return m_path; }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

   private:
    void Reset() {
        if (m_path.empty()) return;
        unlink(XdgTokenPath(m_path).c_str());
        rmdir(m_path.c_str());
        m_path.clear();
    }

    std::string m_path;
};

void ExpectMiss(const XrdOucBearerToken::Result& result) {
    EXPECT_EQ(XrdOucBearerToken::Status::NotFound, result.status);
    EXPECT_TRUE(result.token.empty());
    EXPECT_TRUE(result.entry.empty());
    EXPECT_TRUE(result.location.empty());
    EXPECT_EQ(0, result.errnum);
}

void ExpectFound(const XrdOucBearerToken::Result& result, const std::string& entry,
                 const std::string& token, const std::string& location) {
    ASSERT_EQ(XrdOucBearerToken::Status::Found, result.status);
    EXPECT_EQ(entry, result.entry);
    EXPECT_EQ(token, result.token);
    EXPECT_EQ(location, result.location);
    EXPECT_EQ(0, result.errnum);
}

void ExpectError(const XrdOucBearerToken::Result& result, const std::string& entry,
                 const std::string& location, int errnum) {
    ASSERT_EQ(XrdOucBearerToken::Status::Error, result.status);
    EXPECT_TRUE(result.token.empty());
    EXPECT_EQ(entry, result.entry);
    EXPECT_EQ(location, result.location);
    EXPECT_EQ(errnum, result.errnum);
    EXPECT_EQ(ExpectedFailure(entry, location, errnum), result.Failure());
}

// CTest runs these cases as separate processes. Discovery always consults
// /tmp/bt_u<uid>, so the cases share one exclusive lock for the whole test.
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

class XrdOucBearerTokenTests : public ::testing::Test {
   protected:
    void SetUp() override {
        ASSERT_TRUE(m_lock.Ok()) << "cannot lock the default bearer-token path";
        ClearDiscoveryEnv();
    }
    void TearDown() override { ClearDiscoveryEnv(); }

    // Outlives locals in the test body, including IsolateDefaultTmpToken.
    DefaultTmpTokenLock m_lock;
};

TEST_F(XrdOucBearerTokenTests, FromEnvValue) {
    auto found = XrdOucBearerToken::FromEnvValue("  jwt-value  ");
    ASSERT_EQ(XrdOucBearerToken::Status::Found, found.status);
    EXPECT_EQ("jwt-value", found.token);
    EXPECT_TRUE(found.entry.empty());
    EXPECT_TRUE(found.location.empty());
    EXPECT_EQ(0, found.errnum);

    auto empty = XrdOucBearerToken::FromEnvValue("   ");
    ExpectMiss(empty);
    ExpectMiss(XrdOucBearerToken::FromEnvValue(nullptr));
    ExpectMiss(XrdOucBearerToken::FromEnvValue(""));

    // The size check uses the stripped token. The raw value is not logged
    const char* raw = "  0123456789";
    auto tooLarge = XrdOucBearerToken::FromEnvValue(raw, 5);
    ASSERT_EQ(XrdOucBearerToken::Status::Error, tooLarge.status);
    EXPECT_EQ(EMSGSIZE, tooLarge.errnum);
    EXPECT_TRUE(tooLarge.location.empty());
    EXPECT_TRUE(tooLarge.token.empty());
    EXPECT_TRUE(tooLarge.entry.empty());
    EXPECT_EQ(std::string::npos, tooLarge.Failure().find(raw));
    EXPECT_EQ(std::string::npos, tooLarge.Failure().find("0123456789"));

    // The bound matches retToken: a token of length maxSize is too big.
    auto atLimit = XrdOucBearerToken::FromEnvValue("0123456789", 10);
    ASSERT_EQ(XrdOucBearerToken::Status::Error, atLimit.status);
    EXPECT_EQ(EMSGSIZE, atLimit.errnum);
    EXPECT_TRUE(atLimit.token.empty());

    auto underLimit = XrdOucBearerToken::FromEnvValue("0123456789", 11);
    ASSERT_EQ(XrdOucBearerToken::Status::Found, underLimit.status);
    EXPECT_EQ("0123456789", underLimit.token);
}

TEST_F(XrdOucBearerTokenTests, ReadFileNotFound) {
    auto missing = TempTokenPath();
    ASSERT_FALSE(missing.empty());
    auto result = XrdOucBearerToken::ReadFile(missing.c_str());
    ExpectMiss(result);
    ExpectMiss(XrdOucBearerToken::ReadFile(nullptr));
    ExpectMiss(XrdOucBearerToken::ReadFile(""));
}

TEST_F(XrdOucBearerTokenTests, ReadFileRejectsGroupReadable) {
    auto path = TempTokenPath();
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(WriteTokenFile(path, "secret-token", S_IRUSR | S_IRGRP));

    auto result = XrdOucBearerToken::ReadFile(path.c_str());
    ASSERT_EQ(XrdOucBearerToken::Status::Error, result.status);
    EXPECT_EQ(EPERM, result.errnum);
    EXPECT_EQ(path, result.location);
    EXPECT_TRUE(result.entry.empty());
    EXPECT_TRUE(result.token.empty());

    unlink(path.c_str());
}

TEST_F(XrdOucBearerTokenTests, ReadFileRejectsWorldAccessible) {
    auto path = TempTokenPath();
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(WriteTokenFile(path, "secret-token", S_IRUSR | S_IROTH));

    auto result = XrdOucBearerToken::ReadFile(path.c_str());
    ASSERT_EQ(XrdOucBearerToken::Status::Error, result.status);
    EXPECT_EQ(EPERM, result.errnum);
    EXPECT_EQ(path, result.location);

    unlink(path.c_str());
}

TEST_F(XrdOucBearerTokenTests, ReadFileRejectsOversizedToken) {
    auto path = TempTokenPath();
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(WriteTokenFile(path, "0123456789", S_IRUSR | S_IWUSR));

    auto result = XrdOucBearerToken::ReadFile(path.c_str(), 5);
    ASSERT_EQ(XrdOucBearerToken::Status::Error, result.status);
    EXPECT_EQ(EMSGSIZE, result.errnum);
    EXPECT_EQ(path, result.location);
    EXPECT_TRUE(result.token.empty());

    unlink(path.c_str());
}

TEST_F(XrdOucBearerTokenTests, ReadFileRejectsTokenAtMaxSize) {
    auto path = TempTokenPath();
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(WriteTokenFile(path, "0123456789", S_IRUSR | S_IWUSR));

    auto result = XrdOucBearerToken::ReadFile(path.c_str(), 10);
    ASSERT_EQ(XrdOucBearerToken::Status::Error, result.status);
    EXPECT_EQ(EMSGSIZE, result.errnum);
    EXPECT_EQ(path, result.location);
    EXPECT_TRUE(result.token.empty());

    unlink(path.c_str());
}

TEST_F(XrdOucBearerTokenTests, ReadFileAllowsTrailingNewlineAtMaxSize) {
    auto path = TempTokenPath();
    ASSERT_FALSE(path.empty());
    // The file is 10 bytes, but the stripped token is shorter than the limit.
    ASSERT_TRUE(WriteTokenFile(path, "012345678\n", S_IRUSR | S_IWUSR));

    auto result = XrdOucBearerToken::ReadFile(path.c_str(), 10);
    ASSERT_EQ(XrdOucBearerToken::Status::Found, result.status);
    EXPECT_EQ("012345678", result.token);

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

TEST_F(XrdOucBearerTokenTests, ResolveEntryBearerTokenEnv) {
    EnvGuard env("BEARER_TOKEN", "env-token");

    auto result = XrdOucBearerToken::ResolveEntry("BEARER_TOKEN");
    ExpectFound(result, "", "env-token", "");
}

TEST_F(XrdOucBearerTokenTests, ResolveEntryDoesNotNameTheStep) {
    EXPECT_TRUE(XrdOucBearerToken::ResolveEntry("").entry.empty());
    EXPECT_TRUE(XrdOucBearerToken::ResolveEntry("NOT_A_DISCOVERY_STEP").entry.empty());
}

TEST_F(XrdOucBearerTokenTests, ResolveEntryBearerTokenFile) {
    auto path = TempTokenPath();
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(WriteTokenFile(path, "file-token", S_IRUSR | S_IWUSR));
    EnvGuard env("BEARER_TOKEN_FILE", path.c_str());

    auto result = XrdOucBearerToken::ResolveEntry("BEARER_TOKEN_FILE");
    ExpectFound(result, "", "file-token", path);

    unlink(path.c_str());
}

TEST_F(XrdOucBearerTokenTests, DiscoverPrefersBearerTokenOverFile) {
    auto path = TempTokenPath();
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(WriteTokenFile(path, "file-token", S_IRUSR | S_IWUSR));

    EnvGuard token("BEARER_TOKEN", "env-token");
    EnvGuard tokenFile("BEARER_TOKEN_FILE", path.c_str());

    auto result = XrdOucBearerToken::Discover();
    ExpectFound(result, "BEARER_TOKEN", "env-token", "");

    unlink(path.c_str());
}

TEST_F(XrdOucBearerTokenTests, DiscoverUsesBearerTokenFile) {
    auto path = TempTokenPath();
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(WriteTokenFile(path, "file-token", S_IRUSR | S_IWUSR));
    EnvGuard tokenFile("BEARER_TOKEN_FILE", path.c_str());

    auto result = XrdOucBearerToken::Discover();
    ExpectFound(result, "BEARER_TOKEN_FILE", "file-token", path);

    unlink(path.c_str());
}

TEST_F(XrdOucBearerTokenTests, DiscoverHonorsXrdZtnPath) {
    IsolateDefaultTmpToken isolate;
    if (isolate.Blocked()) {
        GTEST_SKIP() << "cannot move aside " << isolate.Path() << ": " << isolate.Reason();
    }

    auto path = TempTokenPath();
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(WriteTokenFile(path, "ztn-token", S_IRUSR | S_IWUSR));

    auto result = XrdOucBearerToken::Discover(0, path.c_str());
    ExpectFound(result, "xrd.ztn", "ztn-token", path);

    unlink(path.c_str());
}

TEST_F(XrdOucBearerTokenTests, BlankBearerTokenFallsThroughToFile) {
    auto path = TempTokenPath();
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(WriteTokenFile(path, "file-token", S_IRUSR | S_IWUSR));
    EnvGuard token("BEARER_TOKEN", "   \n");
    EnvGuard tokenFile("BEARER_TOKEN_FILE", path.c_str());

    auto result = XrdOucBearerToken::Discover();
    ExpectFound(result, "BEARER_TOKEN_FILE", "file-token", path);

    unlink(path.c_str());
}

TEST_F(XrdOucBearerTokenTests, DiscoverNamesXdgRuntimeDir) {
    auto dir = TempDir::Create();
    ASSERT_TRUE(dir.Ok());
    const auto path = XdgTokenPath(dir.Path());
    ASSERT_TRUE(WriteTokenFile(path, "xdg-token", S_IRUSR | S_IWUSR));
    EnvGuard xdg("XDG_RUNTIME_DIR", dir.Path().c_str());

    auto result = XrdOucBearerToken::Discover();
    ExpectFound(result, "XDG_RUNTIME_DIR", "xdg-token", path);
}

TEST_F(XrdOucBearerTokenTests, DiscoverNamesDefaultTmpPattern) {
    IsolateDefaultTmpToken isolate;
    if (isolate.Blocked()) {
        GTEST_SKIP() << "cannot move aside " << isolate.Path() << ": " << isolate.Reason();
    }

    const auto path = ExpandedDefaultTmpPath();
    ASSERT_TRUE(WriteTokenFile(path, "tmp-token", S_IRUSR | S_IWUSR));

    auto result = XrdOucBearerToken::Discover();
    ExpectFound(result, "/tmp/bt_u%d", "tmp-token", path);

    unlink(path.c_str());
}

TEST_F(XrdOucBearerTokenTests, XdgErrorDoesNotFallThroughToDefaultTmp) {
    IsolateDefaultTmpToken isolate;
    if (isolate.Blocked()) {
        GTEST_SKIP() << "cannot move aside " << isolate.Path() << ": " << isolate.Reason();
    }

    const auto tmpPath = ExpandedDefaultTmpPath();
    ASSERT_TRUE(WriteTokenFile(tmpPath, "tmp-token", S_IRUSR | S_IWUSR));

    auto dir = TempDir::Create();
    ASSERT_TRUE(dir.Ok());
    const auto xdgPath = XdgTokenPath(dir.Path());
    ASSERT_TRUE(WriteTokenFile(xdgPath, "xdg-secret", S_IRUSR | S_IRGRP));
    EnvGuard xdg("XDG_RUNTIME_DIR", dir.Path().c_str());

    auto result = XrdOucBearerToken::Discover();
    ExpectError(result, "XDG_RUNTIME_DIR", xdgPath, EPERM);

    unlink(tmpPath.c_str());
}

TEST_F(XrdOucBearerTokenTests, DefaultTmpDoesNotFallThroughToBrokenXrdZtn) {
    IsolateDefaultTmpToken isolate;
    if (isolate.Blocked()) {
        GTEST_SKIP() << "cannot move aside " << isolate.Path() << ": " << isolate.Reason();
    }

    const auto tmpPath = ExpandedDefaultTmpPath();
    ASSERT_TRUE(WriteTokenFile(tmpPath, "tmp-token", S_IRUSR | S_IWUSR));

    auto ztnPath = TempTokenPath();
    ASSERT_FALSE(ztnPath.empty());
    ASSERT_TRUE(WriteTokenFile(ztnPath, "ztn-secret", S_IRUSR | S_IRGRP));

    auto result = XrdOucBearerToken::Discover(0, ztnPath.c_str());
    ExpectFound(result, "/tmp/bt_u%d", "tmp-token", tmpPath);

    unlink(tmpPath.c_str());
    unlink(ztnPath.c_str());
}

TEST_F(XrdOucBearerTokenTests, FileErrorDoesNotFallThroughToXrdZtn) {
    auto badPath = TempTokenPath();
    ASSERT_FALSE(badPath.empty());
    ASSERT_TRUE(WriteTokenFile(badPath, "secret", S_IRUSR | S_IRGRP));
    auto ztnPath = TempTokenPath();
    ASSERT_FALSE(ztnPath.empty());
    ASSERT_TRUE(WriteTokenFile(ztnPath, "ztn-token", S_IRUSR | S_IWUSR));
    EnvGuard tokenFile("BEARER_TOKEN_FILE", badPath.c_str());

    auto result = XrdOucBearerToken::Discover(0, ztnPath.c_str());
    ExpectError(result, "BEARER_TOKEN_FILE", badPath, EPERM);

    unlink(badPath.c_str());
    unlink(ztnPath.c_str());
}

TEST_F(XrdOucBearerTokenTests, BearerTokenErrorDoesNotFallThroughToFile) {
    auto path = TempTokenPath();
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(WriteTokenFile(path, "file-token", S_IRUSR | S_IWUSR));
    EnvGuard token("BEARER_TOKEN", "  0123456789");
    EnvGuard tokenFile("BEARER_TOKEN_FILE", path.c_str());

    auto result = XrdOucBearerToken::Discover(5);
    ExpectError(result, "BEARER_TOKEN", "", EMSGSIZE);
    EXPECT_EQ(std::string::npos, result.Failure().find("0123456789"));

    unlink(path.c_str());
}

TEST_F(XrdOucBearerTokenTests, XrdZtnErrorIsLabeledWithThatStep) {
    IsolateDefaultTmpToken isolate;
    if (isolate.Blocked()) {
        GTEST_SKIP() << "cannot move aside " << isolate.Path() << ": " << isolate.Reason();
    }

    auto path = TempTokenPath();
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(WriteTokenFile(path, "secret-token", S_IRUSR | S_IWOTH));

    auto result = XrdOucBearerToken::Discover(0, path.c_str());
    ExpectError(result, "xrd.ztn", path, EPERM);
    EXPECT_EQ(std::string::npos, result.entry.find("bt_u"));
    EXPECT_EQ(std::string::npos, result.Failure().find("/tmp/bt_u"));

    unlink(path.c_str());
}

TEST_F(XrdOucBearerTokenTests, MissingXrdZtnPathIsAMissWithThatStepName) {
    IsolateDefaultTmpToken isolate;
    if (isolate.Blocked()) {
        GTEST_SKIP() << "cannot move aside " << isolate.Path() << ": " << isolate.Reason();
    }

    auto missing = TempTokenPath();
    ASSERT_FALSE(missing.empty());

    auto result = XrdOucBearerToken::Discover(0, missing.c_str());
    EXPECT_EQ(XrdOucBearerToken::Status::NotFound, result.status);
    EXPECT_EQ("xrd.ztn", result.entry);
    EXPECT_TRUE(result.token.empty());
    EXPECT_TRUE(result.location.empty());
    EXPECT_EQ(0, result.errnum);
}

TEST_F(XrdOucBearerTokenTests, WhitespaceXrdZtnFileIsAMissWithThatStepName) {
    IsolateDefaultTmpToken isolate;
    if (isolate.Blocked()) {
        GTEST_SKIP() << "cannot move aside " << isolate.Path() << ": " << isolate.Reason();
    }

    auto path = TempTokenPath();
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(WriteTokenFile(path, "  \n\t", S_IRUSR | S_IWUSR));

    auto result = XrdOucBearerToken::Discover(0, path.c_str());
    EXPECT_EQ(XrdOucBearerToken::Status::NotFound, result.status);
    EXPECT_EQ("xrd.ztn", result.entry);
    EXPECT_TRUE(result.token.empty());
    EXPECT_TRUE(result.location.empty());
    EXPECT_EQ(0, result.errnum);

    unlink(path.c_str());
}

TEST_F(XrdOucBearerTokenTests, EmptyXrdZtnPathIsNotConsulted) {
    IsolateDefaultTmpToken isolate;
    if (isolate.Blocked()) {
        GTEST_SKIP() << "cannot move aside " << isolate.Path() << ": " << isolate.Reason();
    }

    ExpectMiss(XrdOucBearerToken::Discover(0, static_cast<const char*>(nullptr)));
    ExpectMiss(XrdOucBearerToken::Discover(0, ""));
}

TEST_F(XrdOucBearerTokenTests, UnlimitedSizeAcceptsLongToken) {
    std::string token(64, 'a');
    EnvGuard env("BEARER_TOKEN", token.c_str());

    auto result = XrdOucBearerToken::Discover(0);
    ExpectFound(result, "BEARER_TOKEN", token, "");
}

TEST_F(XrdOucBearerTokenTests, DiscoverFromEnvUsesXrdZtn) {
    IsolateDefaultTmpToken isolate;
    if (isolate.Blocked()) {
        GTEST_SKIP() << "cannot move aside " << isolate.Path() << ": " << isolate.Reason();
    }

    auto path = TempTokenPath();
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(WriteTokenFile(path, "ztn-token", S_IRUSR | S_IWUSR));

    XrdOucEnv env;
    env.Put("xrd.ztn", path.c_str());

    auto result = XrdOucBearerToken::Discover(0, &env);
    ExpectFound(result, "xrd.ztn", "ztn-token", path);

    unlink(path.c_str());
}

TEST_F(XrdOucBearerTokenTests, DiscoverFromEnvLabelsXrdZtnFailure) {
    IsolateDefaultTmpToken isolate;
    if (isolate.Blocked()) {
        GTEST_SKIP() << "cannot move aside " << isolate.Path() << ": " << isolate.Reason();
    }

    auto path = TempTokenPath();
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(WriteTokenFile(path, "secret-token", S_IRUSR | S_IRGRP));

    XrdOucEnv env;
    env.Put("xrd.ztn", path.c_str());

    auto result = XrdOucBearerToken::Discover(0, &env);
    ExpectError(result, "xrd.ztn", path, EPERM);

    unlink(path.c_str());
}

TEST_F(XrdOucBearerTokenTests, DiscoverFromNullEnvAndMissingKeyAreMisses) {
    IsolateDefaultTmpToken isolate;
    if (isolate.Blocked()) {
        GTEST_SKIP() << "cannot move aside " << isolate.Path() << ": " << isolate.Reason();
    }

    ExpectMiss(XrdOucBearerToken::Discover(0, static_cast<XrdOucEnv*>(nullptr)));

    XrdOucEnv empty;
    ExpectMiss(XrdOucBearerToken::Discover(0, &empty));

    XrdOucEnv blank;
    blank.Put("xrd.ztn", "");
    ExpectMiss(XrdOucBearerToken::Discover(0, &blank));
}

TEST_F(XrdOucBearerTokenTests, EarlierTokenIgnoresBrokenXrdZtnInEnv) {
    auto path = TempTokenPath();
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(WriteTokenFile(path, "secret-token", S_IRUSR | S_IRGRP));
    EnvGuard token("BEARER_TOKEN", "env-token");

    XrdOucEnv env;
    env.Put("xrd.ztn", path.c_str());

    auto result = XrdOucBearerToken::Discover(0, &env);
    ExpectFound(result, "BEARER_TOKEN", "env-token", "");

    unlink(path.c_str());
}

TEST_F(XrdOucBearerTokenTests, FailureOmitsPermissionSuffixUnlessEperm) {
    XrdOucBearerToken::Result sized;
    sized.entry = "BEARER_TOKEN_FILE";
    sized.location = "/tmp/token";
    sized.errnum = EMSGSIZE;
    EXPECT_EQ(ExpectedFailure("BEARER_TOKEN_FILE", "/tmp/token", EMSGSIZE), sized.Failure());
    EXPECT_EQ("Bearer token discovery failed at /tmp/token: token exceeds the maximum allowed size",
              sized.Failure());

    XrdOucBearerToken::Result denied;
    denied.entry = "xrd.ztn";
    denied.location = "/tmp/token";
    denied.errnum = EPERM;
    const std::string text = denied.Failure();
    EXPECT_EQ(ExpectedFailure("xrd.ztn", "/tmp/token", EPERM), text);
    EXPECT_EQ("Bearer token discovery failed at /tmp/token: token file must be readable only by its owner",
              text);
}
