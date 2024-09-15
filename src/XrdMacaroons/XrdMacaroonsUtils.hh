#pragma once

#include <string>

class XrdSysError;
class XrdOucGatherConf;

namespace Macaroons {

enum LogMask {
    Debug = 0x01,
    Info = 0x02,
    Warning = 0x04,
    Error = 0x08,
    All = 0xff
};

enum AuthzBehavior {
    PASSTHROUGH,
    ALLOW,
    DENY
};

bool GetSecretKey(const std::string &filename, XrdSysError *log, std::string &secret);

std::string NormalizeSlashes(const std::string &input);

struct XrdMacaroonsConfig {
private:
    XrdMacaroonsConfig(const XrdMacaroonsConfig &) = delete;

public:
    XrdMacaroonsConfig() = default;
    int mask;               // The configured log mask for XrdMacaroons
    AuthzBehavior behavior; // The behavior for handling authorization on failures.
    ssize_t maxDuration;    // The maximum acceptable duration of generated macaroons.
    std::string site;       // The sitename to use within a generated macaroon (or for verifying a macaroon).
    std::string secret;     // The secret string ot use when generating/verifying macaroons.
};

// An internal class solely for encapsulating the various macaroons configurations
// This class should be constructed at configuration time; it is subsequently constant.
class XrdMacaroonsConfigFactory {
public:
    static const XrdMacaroonsConfig &Get(XrdSysError &log);

private:
    XrdMacaroonsConfigFactory();

    // Static configuration method; made static to allow Authz and Handler object to reuse
    // this code.
    static bool Config(XrdSysError &log);

    static bool xsecretkey(XrdOucGatherConf &Config, XrdSysError &log, std::string &secret);
    static bool xsitename(XrdOucGatherConf &Config, XrdSysError &log, std::string &location);
    static bool xtrace(XrdOucGatherConf &Config, XrdSysError &log);
    static bool xmaxduration(XrdOucGatherConf &Config, XrdSysError &log, ssize_t &max_duration);

    static bool m_configured;
    static XrdMacaroonsConfig m_config;
};
}
