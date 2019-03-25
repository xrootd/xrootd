
#include <string>
#include <memory>
#include <stdexcept>
#include <vector>

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

class Handler : public XrdHttpExtHandler {
public:
    Handler(XrdSysError *log, const char *config, XrdOucEnv *myEnv,
        XrdAccAuthorize *chain) :
        m_max_duration(86400),
        m_chain(chain),
        m_log(log)
    {
        AuthzBehavior behavior;
        if (!Config(config, myEnv, m_log, m_location, m_secret, m_max_duration, behavior))
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
    std::string GenerateID(const std::string &, const XrdSecEntity &, const std::string &, const std::vector<std::string> &, const std::string &);
    std::string GenerateActivities(const XrdHttpExtReq &, const std::string &) const;

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
    std::string m_location;
    std::string m_secret;
};

}
