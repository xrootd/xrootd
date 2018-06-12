
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
    Handler(XrdSysError *log, const char *config, XrdOucEnv *myEnv) :
        m_chain(nullptr),
        m_log(log)
    {
        if (!Config(config, myEnv))
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

    bool Config(const char *config, XrdOucEnv *env);
    bool xsecretkey(XrdOucStream &Config);
    bool xsitename(XrdOucStream &Config);

    XrdAccAuthorize *m_chain;
    XrdSysError *m_log;
    std::string m_location;
    std::string m_secret;
};

}
