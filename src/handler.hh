
#include <string>
#include <memory>
#include <stdexcept>

#include "XrdHttp/XrdHttpExtHandler.hh"

class XrdOucEnv;
class XrdOucStream;
class XrdSecEntity;
class XrdAccAuthorize;

namespace Macaroons {

class Handler : public XrdHttpExtHandler {
public:
    Handler(XrdSysError *log, const char *config, XrdOucEnv *myEnv,
        XrdAccAuthorize *chain) :
        m_chain(chain),
        m_log(log)
    {
        if (!Config(config, myEnv, m_log, m_location, m_secret))
        {
            throw std::runtime_error("Macaroon handler config failed.");
        }
    }

    virtual ~Handler();

    virtual bool MatchesPath(const char *verb, const char *path) override;
    virtual int ProcessReq(XrdHttpExtReq &req) override;

    virtual int Init(const char *cfgfile) override {return 0;}

private:
    std::string GenerateID(const XrdSecEntity &, const std::string &, const std::string &);
    std::string GenerateActivities(const XrdHttpExtReq &) const;

    // Static configuration method; made static to allow Authz object to reuse
    // this code.
    static bool Config(const char *config, XrdOucEnv *env, XrdSysError *log,
        std::string &location, std::string &secret);
    static bool xsecretkey(XrdOucStream &Config, XrdSysError *log, std::string &secret);
    static bool xsitename(XrdOucStream &Config, XrdSysError *log, std::string &location);

    XrdAccAuthorize *m_chain;
    XrdSysError *m_log;
    std::string m_location;
    std::string m_secret;
};

}
