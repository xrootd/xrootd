/******************************************************************************/
/* Black-box characterization of the ztn *client* bearer-token discovery path */
/*                                                                            */
/* These tests load libXrdSecztn and drive only the stable public ABI:        */
/*                                                                            */
/*   XrdSecProtocolztnObject('c', ...) -> getCredentials()                    */
/*                                                                            */
/* Client discovery order under test (WLCG bearer-token discovery):           */
/*   1. BEARER_TOKEN                                                          */
/*   2. BEARER_TOKEN_FILE                                                     */
/*   3. $XDG_RUNTIME_DIR/bt_u<uid>                                            */
/*   4. /tmp/bt_u<uid>                                                        */
/*   5. opaque xrd.ztn=<path> (via XrdOucErrInfo env)                         */
/******************************************************************************/

#undef NDEBUG

#include <arpa/inet.h>
#include <dlfcn.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <utility>

#include "XrdNet/XrdNetAddr.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdSec/XrdSecInterface.hh"

#ifndef XRD_SECZTN_LIB
#error "XRD_SECZTN_LIB must be defined to the built libXrdSecztn path"
#endif

namespace {

using ZtnObjectFn = XrdSecProtocol *(*)(const char /*mode*/, const char * /*host*/,
                                        XrdNetAddrInfo &, const char * /*parms*/,
                                        XrdOucErrInfo *);

// Server-advertised client parms: <opts>:<maxTokenBytes>:
constexpr const char *kDefaultParms = "0:4096:";

//------------------------------------------------------------------------------
// Environment helpers
//------------------------------------------------------------------------------

class EnvVar {
   public:
    EnvVar(const char *name, const char *value) : m_name(name) {
        if (const char *old = getenv(name)) {
            m_had = true;
            m_old = old;
        }
        if (value)
            setenv(name, value, 1);
        else
            unsetenv(name);
    }

    ~EnvVar() {
        if (m_had)
            setenv(m_name.c_str(), m_old.c_str(), 1);
        else
            unsetenv(m_name.c_str());
    }

    EnvVar(const EnvVar &) = delete;
    EnvVar &operator=(const EnvVar &) = delete;

   private:
    std::string m_name;
    std::string m_old;
    bool m_had{false};
};

// Saves BEARER_TOKEN / BEARER_TOKEN_FILE / XDG_RUNTIME_DIR, clears them for the
// test, then restores the ambient values (so CI/local env is not permanently
// unset across the process).
class DiscoveryEnvGuard {
   public:
    DiscoveryEnvGuard() {
        for (auto &slot : m_saved) {
            if (const char *old = getenv(slot.name)) {
                slot.had = true;
                slot.value = old;
            }
            unsetenv(slot.name);
        }
    }

    ~DiscoveryEnvGuard() {
        for (auto &slot : m_saved) {
            if (slot.had)
                setenv(slot.name, slot.value.c_str(), 1);
            else
                unsetenv(slot.name);
        }
    }

    DiscoveryEnvGuard(const DiscoveryEnvGuard &) = delete;
    DiscoveryEnvGuard &operator=(const DiscoveryEnvGuard &) = delete;

   private:
    struct Saved {
        const char *name;
        bool had{false};
        std::string value;
    };

    std::array<Saved, 3> m_saved{{{"BEARER_TOKEN", false, {}},
                                  {"BEARER_TOKEN_FILE", false, {}},
                                  {"XDG_RUNTIME_DIR", false, {}}}};
};

std::string DefaultTmpTokenPath() {
    return "/tmp/bt_u" + std::to_string(static_cast<int>(geteuid()));
}

std::string XdgTokenFileName() {
    return "bt_u" + std::to_string(static_cast<int>(geteuid()));
}

// Discovery always probes /tmp/bt_u<uid>. Move any ambient file aside for the
// test duration and restore it afterwards so local/CI runs leave no residue.
class IsolateDefaultTmpToken {
   public:
    IsolateDefaultTmpToken() {
        m_path = DefaultTmpTokenPath();
        if (access(m_path.c_str(), F_OK) != 0) return;

        m_backup = m_path + ".xrdsecztn-test-bak";
        unlink(m_backup.c_str());
        if (rename(m_path.c_str(), m_backup.c_str()) != 0) {
            m_blocked = true;
            m_reason = strerror(errno);
            return;
        }
        m_moved = true;
    }

    ~IsolateDefaultTmpToken() {
        if (!m_moved) return;
        unlink(m_path.c_str());
        rename(m_backup.c_str(), m_path.c_str());
    }

    bool Blocked() const { return m_blocked; }
    const std::string &Path() const { return m_path; }
    const std::string &Reason() const { return m_reason; }

    IsolateDefaultTmpToken(const IsolateDefaultTmpToken &) = delete;
    IsolateDefaultTmpToken &operator=(const IsolateDefaultTmpToken &) = delete;

   private:
    std::string m_path;
    std::string m_backup;
    std::string m_reason;
    bool m_moved{false};
    bool m_blocked{false};
};

//------------------------------------------------------------------------------
// Temporary dirs / token files
//------------------------------------------------------------------------------

class TempDir {
   public:
    static TempDir Create() {
        char tmpl[] = "/tmp/xrdsecztn-xdg-XXXXXX";
        if (!mkdtemp(tmpl)) return {};
        TempDir dir;
        dir.m_path = tmpl;
        return dir;
    }

    TempDir() = default;
    TempDir(TempDir &&other) noexcept : m_path(std::move(other.m_path)) {
        other.m_path.clear();
    }
    TempDir &operator=(TempDir &&other) noexcept {
        if (this != &other) {
            Reset();
            m_path = std::move(other.m_path);
            other.m_path.clear();
        }
        return *this;
    }

    ~TempDir() { Reset(); }

    bool Ok() const { return !m_path.empty(); }
    const std::string &Path() const { return m_path; }

    TempDir(const TempDir &) = delete;
    TempDir &operator=(const TempDir &) = delete;

   private:
    void Reset() {
        if (m_path.empty()) return;
        // Best-effort: remove known token filename then the directory.
        unlink((m_path + "/" + XdgTokenFileName()).c_str());
        rmdir(m_path.c_str());
        m_path.clear();
    }

    std::string m_path;
};

class TokenFile {
   public:
    static TokenFile Create(const std::string &contents, mode_t mode) {
        char tmpl[] = "/tmp/xrdsecztn-token-XXXXXX";
        int fd = mkstemp(tmpl);
        if (fd < 0) return {};
        close(fd);
        return WriteAt(tmpl, contents, mode, /*owned=*/true);
    }

    // Write (or overwrite) a token at an exact path. If owned, unlink on destroy.
    static TokenFile WriteAt(const std::string &path, const std::string &contents, mode_t mode,
                             bool owned) {
        TokenFile file;
        file.m_path = path;
        file.m_owned = owned;
        std::ofstream out(file.m_path, std::ios::trunc);
        if (!out) {
            file.m_path.clear();
            return file;
        }
        out << contents;
        out.close();
        if (chmod(file.m_path.c_str(), mode) != 0) {
            if (owned) unlink(file.m_path.c_str());
            file.m_path.clear();
        }
        return file;
    }

    TokenFile() = default;
    TokenFile(TokenFile &&other) noexcept
        : m_path(std::move(other.m_path)), m_owned(other.m_owned) {
        other.m_path.clear();
        other.m_owned = false;
    }
    TokenFile &operator=(TokenFile &&other) noexcept {
        if (this != &other) {
            Reset();
            m_path = std::move(other.m_path);
            m_owned = other.m_owned;
            other.m_path.clear();
            other.m_owned = false;
        }
        return *this;
    }

    ~TokenFile() { Reset(); }

    bool Ok() const { return !m_path.empty(); }
    const char *Path() const { return m_path.c_str(); }

    TokenFile(const TokenFile &) = delete;
    TokenFile &operator=(const TokenFile &) = delete;

   private:
    void Reset() {
        if (m_owned && !m_path.empty()) unlink(m_path.c_str());
        m_path.clear();
        m_owned = false;
    }

    std::string m_path;
    bool m_owned{false};
};

//------------------------------------------------------------------------------
// Credential wire format (public on-the-wire contract of a ztn token reply)
//------------------------------------------------------------------------------

#pragma pack(push, 1)
struct ZtnTokenCredHdr {
    char id[4];    // "ztn\0"
    char ver;      // 0
    char opr;      // 'T' = token
    char rsvd[2];
    uint16_t len;  // htons(token_bytes + 1)  (includes trailing NUL)
};
#pragma pack(pop)

std::string TokenFromCredentials(const XrdSecCredentials *cred) {
    if (!cred || !cred->buffer) return {};
    if (cred->size < static_cast<int>(sizeof(ZtnTokenCredHdr))) return {};

    const auto *hdr = reinterpret_cast<const ZtnTokenCredHdr *>(cred->buffer);
    if (std::strncmp(hdr->id, "ztn", 3) != 0 || hdr->opr != 'T') return {};

    // Layout: TokenHdr(8) + htons(len) + token[len] where len includes trailing NUL.
    const uint16_t n = ntohs(hdr->len);
    if (n < 1) return {};
    if (cred->size < static_cast<int>(sizeof(ZtnTokenCredHdr) + n)) return {};

    const char *tkn = cred->buffer + sizeof(ZtnTokenCredHdr);
    return std::string(tkn, n - 1);
}

//------------------------------------------------------------------------------
// Plugin loader + thin protocol handle
//------------------------------------------------------------------------------

class ZtnPlugin {
   public:
    ZtnPlugin() {
        m_lib = dlopen(XRD_SECZTN_LIB, RTLD_NOW);
        if (!m_lib) {
            m_error = dlerror() ? dlerror() : "dlopen(libXrdSecztn) failed";
            return;
        }
        m_object = reinterpret_cast<ZtnObjectFn>(dlsym(m_lib, "XrdSecProtocolztnObject"));
        if (!m_object)
            m_error = dlerror() ? dlerror() : "dlsym(XrdSecProtocolztnObject) failed";
    }

    ~ZtnPlugin() {
        if (m_lib) dlclose(m_lib);
    }

    bool Ok() const { return m_object != nullptr; }
    const std::string &Error() const { return m_error; }

    // Client mode requires a TLS endpoint; parms come from the server sectoken.
    XrdSecProtocol *NewClient(const char *parms, XrdOucErrInfo *ei) {
        m_ep.Set("localhost", 0);
        m_ep.SetTLS(true);
        return m_object('c', "localhost", m_ep, parms, ei);
    }

    ZtnPlugin(const ZtnPlugin &) = delete;
    ZtnPlugin &operator=(const ZtnPlugin &) = delete;

   private:
    void *m_lib{nullptr};
    ZtnObjectFn m_object{nullptr};
    std::string m_error;
    XrdNetAddr m_ep;
};

class Protocol {
   public:
    Protocol(ZtnPlugin &plugin, const char *parms, XrdOucErrInfo *ei)
        : m_prot(plugin.NewClient(parms, ei)) {}

    ~Protocol() {
        if (m_prot) m_prot->Delete();
    }

    XrdSecProtocol *Get() const { return m_prot; }
    explicit operator bool() const { return m_prot != nullptr; }

    Protocol(const Protocol &) = delete;
    Protocol &operator=(const Protocol &) = delete;

   private:
    XrdSecProtocol *m_prot{nullptr};
};

}  // namespace

class XrdSecztnClientTokenTest : public ::testing::Test {
   protected:
    void SetUp() override { ASSERT_TRUE(m_plugin.Ok()) << m_plugin.Error(); }

    // Declared first so ambient discovery env is cleared for the whole test and
    // restored when the fixture is destroyed (after TearDown).
    DiscoveryEnvGuard m_env;
    ZtnPlugin m_plugin;
};

//--- success paths ------------------------------------------------------------

TEST_F(XrdSecztnClientTokenTest, ReadsBearerTokenFromEnvironment) {
    EnvVar token("BEARER_TOKEN", "  env-token  ");

    XrdOucErrInfo ei;
    Protocol prot(m_plugin, kDefaultParms, &ei);
    ASSERT_TRUE(prot) << ei.getErrText();

    XrdSecCredentials *cred = prot.Get()->getCredentials(nullptr, &ei);
    ASSERT_NE(nullptr, cred) << ei.getErrText();
    EXPECT_EQ("env-token", TokenFromCredentials(cred));
    delete cred;
}

TEST_F(XrdSecztnClientTokenTest, ReadsBearerTokenFromOwnerOnlyFile) {
    auto file = TokenFile::Create("file-token\n", S_IRUSR | S_IWUSR);
    ASSERT_TRUE(file.Ok());
    EnvVar tokenFile("BEARER_TOKEN_FILE", file.Path());

    XrdOucErrInfo ei;
    Protocol prot(m_plugin, kDefaultParms, &ei);
    ASSERT_TRUE(prot) << ei.getErrText();

    XrdSecCredentials *cred = prot.Get()->getCredentials(nullptr, &ei);
    ASSERT_NE(nullptr, cred) << ei.getErrText();
    EXPECT_EQ("file-token", TokenFromCredentials(cred));
    delete cred;
}

TEST_F(XrdSecztnClientTokenTest, PrefersEnvironmentOverTokenFile) {
    auto file = TokenFile::Create("file-token", S_IRUSR | S_IWUSR);
    ASSERT_TRUE(file.Ok());
    EnvVar token("BEARER_TOKEN", "env-token");
    EnvVar tokenFile("BEARER_TOKEN_FILE", file.Path());

    XrdOucErrInfo ei;
    Protocol prot(m_plugin, kDefaultParms, &ei);
    ASSERT_TRUE(prot) << ei.getErrText();

    XrdSecCredentials *cred = prot.Get()->getCredentials(nullptr, &ei);
    ASSERT_NE(nullptr, cred) << ei.getErrText();
    EXPECT_EQ("env-token", TokenFromCredentials(cred));
    delete cred;
}

TEST_F(XrdSecztnClientTokenTest, ReadsTokenFromXdgRuntimeDir) {
    auto dir = TempDir::Create();
    ASSERT_TRUE(dir.Ok());

    const auto path = dir.Path() + "/" + XdgTokenFileName();
    auto file = TokenFile::WriteAt(path, "xdg-token", S_IRUSR | S_IWUSR, /*owned=*/true);
    ASSERT_TRUE(file.Ok());
    EnvVar xdg("XDG_RUNTIME_DIR", dir.Path().c_str());

    XrdOucErrInfo ei;
    Protocol prot(m_plugin, kDefaultParms, &ei);
    ASSERT_TRUE(prot) << ei.getErrText();

    XrdSecCredentials *cred = prot.Get()->getCredentials(nullptr, &ei);
    ASSERT_NE(nullptr, cred) << ei.getErrText();
    EXPECT_EQ("xdg-token", TokenFromCredentials(cred));
    delete cred;
}

TEST_F(XrdSecztnClientTokenTest, ReadsTokenFromDefaultTmpPath) {
    IsolateDefaultTmpToken isolate;
    if (isolate.Blocked()) {
        GTEST_SKIP() << "cannot move aside " << isolate.Path() << ": " << isolate.Reason();
    }

    // owned=true: unlink our test file on scope exit. IsolateDefaultTmpToken is
    // destroyed after this TokenFile, so any ambient original is restored next.
    auto file = TokenFile::WriteAt(DefaultTmpTokenPath(), "tmp-token", S_IRUSR | S_IWUSR,
                                   /*owned=*/true);
    ASSERT_TRUE(file.Ok());

    XrdOucErrInfo ei;
    Protocol prot(m_plugin, kDefaultParms, &ei);
    ASSERT_TRUE(prot) << ei.getErrText();

    XrdSecCredentials *cred = prot.Get()->getCredentials(nullptr, &ei);
    ASSERT_NE(nullptr, cred) << ei.getErrText();
    EXPECT_EQ("tmp-token", TokenFromCredentials(cred));
    delete cred;
}

TEST_F(XrdSecztnClientTokenTest, ReadsTokenFromXrdZtnOpaquePath) {
    IsolateDefaultTmpToken isolate;
    if (isolate.Blocked()) {
        GTEST_SKIP() << "cannot move aside " << isolate.Path() << ": " << isolate.Reason();
    }

    auto file = TokenFile::Create("ztn-token", S_IRUSR | S_IWUSR);
    ASSERT_TRUE(file.Ok());

    XrdOucEnv opaque;
    opaque.Put("xrd.ztn", file.Path());

    XrdOucErrInfo ei;
    ei.setEnv(&opaque);

    Protocol prot(m_plugin, kDefaultParms, &ei);
    ASSERT_TRUE(prot) << ei.getErrText();

    XrdSecCredentials *cred = prot.Get()->getCredentials(nullptr, &ei);
    ASSERT_NE(nullptr, cred) << ei.getErrText();
    EXPECT_EQ("ztn-token", TokenFromCredentials(cred));
    delete cred;

    ei.setEnv(nullptr);
}

//--- failure paths ------------------------------------------------------------

TEST_F(XrdSecztnClientTokenTest, RejectsGroupReadableTokenFile) {
    auto file = TokenFile::Create("secret-token", S_IRUSR | S_IRGRP);
    ASSERT_TRUE(file.Ok());
    EnvVar tokenFile("BEARER_TOKEN_FILE", file.Path());

    XrdOucErrInfo ei;
    Protocol prot(m_plugin, kDefaultParms, &ei);
    ASSERT_TRUE(prot) << ei.getErrText();

    XrdSecCredentials *cred = prot.Get()->getCredentials(nullptr, &ei);
    EXPECT_EQ(nullptr, cred);
    EXPECT_EQ(EPERM, ei.getErrInfo());
}

TEST_F(XrdSecztnClientTokenTest, RejectsTokenFileLargerThanMaxSize) {
    auto file = TokenFile::Create("0123456789", S_IRUSR | S_IWUSR);
    ASSERT_TRUE(file.Ok());
    EnvVar tokenFile("BEARER_TOKEN_FILE", file.Path());

    XrdOucErrInfo ei;
    Protocol prot(m_plugin, "0:8:", &ei);  // max token size = 8 bytes
    ASSERT_TRUE(prot) << ei.getErrText();

    XrdSecCredentials *cred = prot.Get()->getCredentials(nullptr, &ei);
    EXPECT_EQ(nullptr, cred);
    EXPECT_EQ(EMSGSIZE, ei.getErrInfo());
}

TEST_F(XrdSecztnClientTokenTest, FailsWhenNoTokenIsAvailable) {
    IsolateDefaultTmpToken isolate;
    if (isolate.Blocked()) {
        GTEST_SKIP() << "cannot move aside " << isolate.Path() << ": " << isolate.Reason();
    }

    XrdOucErrInfo ei;
    Protocol prot(m_plugin, kDefaultParms, &ei);
    ASSERT_TRUE(prot) << ei.getErrText();

    XrdSecCredentials *cred = prot.Get()->getCredentials(nullptr, &ei);
    EXPECT_EQ(nullptr, cred);
    EXPECT_EQ(ENOPROTOOPT, ei.getErrInfo());
}
