
#ifndef __XRD_MACAROONS_HANDLER_H
#define __XRD_MACAROONS_HANDLER_H

#include <string>
#include <memory>
#include <stdexcept>
#include <vector>

#include "XrdMacaroonsGenerate.hh"

#include "XrdHttp/XrdHttpExtHandler.hh"

class XrdOucEnv;
class XrdOucStream;
class XrdSecEntity;
class XrdAccAuthorize;

namespace Macaroons {

enum LogMask {
    Debug = 0x01,
    Info = 0x02,
    Warning = 0x04,
    Error = 0x08,
    All = 0xff
};

// 'Normalize' the macaroon path.  This only takes care of double slashes
// but, as is common in XRootD, it doesn't treat these as a hierarchy.
// For example, these result in the same path:
//
//   /foo/bar -> /foo/bar
//   //foo////bar -> /foo/bar
//
// These are all distinct:
//
//   /foo/bar  -> /foo/bar
//   /foo/bar/ -> /foo/bar/
//   /foo/baz//../bar -> /foo/baz/../bar
//
std::string NormalizeSlashes(const std::string &);

class Handler : public XrdHttpExtHandler {
public:
    Handler(XrdSysError *log, const char *config, XrdOucEnv *myEnv,
        XrdAccAuthorize *chain) :
        m_max_duration(86400),
        m_chain(chain),
        m_log(log),
        m_generator(*m_log)
    {
        AuthzBehavior behavior;
        std:: string location, secret;
        if (!Config(config, myEnv, m_log, location, secret, m_max_duration, behavior))
        {
            throw std::runtime_error("Macaroon handler config failed.");
        }
    }

    enum AuthzBehavior {
        PASSTHROUGH,
        ALLOW,
        DENY
    };

    virtual ~Handler();

    virtual bool MatchesPath(const char *verb, const char *path) override;
    virtual int ProcessReq(XrdHttpExtReq &req) override;

    virtual int Init(const char *cfgfile) override {return 0;}

    // Static configuration method; made static to allow Authz object to reuse
    // this code.
    static bool Config(const char *config, XrdOucEnv *env, XrdSysError *log,
        std::string &location, std::string &secret, ssize_t &max_duration,
        AuthzBehavior &behavior);

private:
    std::string GenerateID(const std::string &, const XrdSecEntity &, const std::string &, const std::vector<std::string> &, time_t);
    std::bitset<AOP_LastOp> GenerateActivities(const XrdHttpExtReq &, const std::string &) const;
    std::string GenerateActivitiesStr(const std::bitset<AOP_LastOp> &opers) const;


    int ProcessOAuthConfig(XrdHttpExtReq &req);
    int ProcessTokenRequest(XrdHttpExtReq& req);
    int GenerateMacaroonResponse(XrdHttpExtReq& req, const std::string &response, const std::vector<std::string> &, ssize_t validity, bool oauth_response);

    static bool xsecretkey(XrdOucStream &Config, XrdSysError *log, std::string &secret);
    static bool xsitename(XrdOucStream &Config, XrdSysError *log, std::string &location);
    static bool xtrace(XrdOucStream &Config, XrdSysError *log);
    static bool xmaxduration(XrdOucStream &Config, XrdSysError *log, ssize_t &max_duration);

    ssize_t m_max_duration;
    XrdAccAuthorize *m_chain;
    XrdSysError *m_log;
    XrdMacaroonsGenerator m_generator;
};

}

#endif
