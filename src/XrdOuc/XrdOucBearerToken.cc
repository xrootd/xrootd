#include "XrdOuc/XrdOucBearerToken.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucUtils.hh"

#include <fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

namespace {
// Callers omit the limit or pass 0. A smaller positive value is a tighter cap,
// such as the ztn handshake size. Anything above the ceiling uses the ceiling.
size_t TokenLimit(size_t maxSize) {
    if (maxSize == 0 || maxSize > XrdOucBearerToken::MaxSize) return XrdOucBearerToken::MaxSize;
    return maxSize;
}
}  // namespace

XrdOucBearerToken::Result XrdOucBearerToken::FromEnvValue(const char* value, size_t maxSize) {
    if (!value || !*value) return {};

    std::string token(value);
    XrdOucUtils::trim(token);
    if (token.empty()) return {};

    // location stays empty so the token is not logged. The bound is exclusive:
    // a token as long as the limit is too big, matching ztn retToken.
    const size_t limit = TokenLimit(maxSize);
    if (token.size() >= limit) return {Status::Error, {}, EMSGSIZE, {}, {}};

    return {Status::Found, std::move(token), 0, {}, {}};
}

XrdOucBearerToken::Result XrdOucBearerToken::ReadFile(const char* path, size_t maxSize) {
    if (!path || !*path) return {};

    struct stat st;
    if (stat(path, &st) != 0) {
        if (errno == ENOENT) return {};
        return {Status::Error, {}, errno, path, {}};
    }

    const size_t limit = TokenLimit(maxSize);
    if (static_cast<size_t>(st.st_size) > limit) return {Status::Error, {}, EMSGSIZE, path, {}};

    // make sure the file is not accessible to anyone but the owner
    if (st.st_mode & (S_IRWXG | S_IRWXO)) return {Status::Error, {}, EPERM, path, {}};

    int fd = open(path, O_RDONLY);
    if (fd < 0) return {Status::Error, {}, errno, path, {}};

    std::string token(static_cast<size_t>(st.st_size), '\0');
    ssize_t nread = read(fd, &token[0], static_cast<size_t>(st.st_size));
    int readErr = errno;
    close(fd);

    if (nread != st.st_size) return {Status::Error, {}, nread < 0 ? readErr : EIO, path, {}};

    XrdOucUtils::trim(token);
    if (token.empty()) return {};

    if (token.size() >= limit) return {Status::Error, {}, EMSGSIZE, path, {}};

    return {Status::Found, std::move(token), 0, path, {}};
}

XrdOucBearerToken::Result XrdOucBearerToken::ResolveEntry(std::string_view entry, size_t maxSize) {
    if (entry.empty()) return {};

    // absolute path to the token file
    if (entry.front() == '/') {
        char path[MAXPATHLEN + 8];
        const std::string fmt(entry);
        snprintf(path, sizeof(path), fmt.c_str(), static_cast<int>(geteuid()));
        return ReadFile(path, maxSize);
    }

    const std::string envName(entry);
    const char* env = getenv(envName.c_str());
    if (!env || !*env) return {};

    if (entry.size() >= 4 && !entry.compare(entry.size() - 4, 4, "_DIR")) {
        const std::string path = std::string(env) + "/bt_u" + std::to_string(geteuid());
        return ReadFile(path.c_str(), maxSize);
    }

    if (entry.size() >= 5 && !entry.compare(entry.size() - 5, 5, "_FILE"))
        return ReadFile(env, maxSize);

    return FromEnvValue(env, maxSize);
}

XrdOucBearerToken::Result XrdOucBearerToken::Discover(size_t maxSize, const char* xrdZtnPath) {
    static constexpr std::array<std::string_view, 4> dfltLoc = {"BEARER_TOKEN", "BEARER_TOKEN_FILE",
                                                                "XDG_RUNTIME_DIR", "/tmp/bt_u%d"};

    for (const auto loc : dfltLoc) {
        auto result = ResolveEntry(loc, maxSize);
        if (result.status != Status::NotFound) {
            result.entry.assign(loc.data(), loc.size());
            return result;
        }
    }

    if (xrdZtnPath && *xrdZtnPath) {
        auto result = ReadFile(xrdZtnPath, maxSize);
        result.entry = "xrd.ztn";
        return result;
    }

    return {};
}

XrdOucBearerToken::Result XrdOucBearerToken::Discover(size_t maxSize, XrdOucEnv* env) {
    const char* xrdZtnPath = env ? env->Get("xrd.ztn") : nullptr;
    return Discover(maxSize, xrdZtnPath);
}

std::string XrdOucBearerToken::Result::Failure() const {
    const char* reason = nullptr;
    switch (errnum) {
        case EPERM:
            reason = "token file must be readable only by its owner";
            break;
        case EMSGSIZE:
            reason = "token exceeds the maximum allowed size";
            break;
        case EIO:
            reason = "unable to read the complete token from file";
            break;
        default:
            reason = strerror(errnum);
            break;
    }

    std::string msg = "Bearer token discovery failed at ";
    msg += location.empty() ? entry : location;
    msg += ": ";
    if (reason) msg += reason;
    return msg;
}
