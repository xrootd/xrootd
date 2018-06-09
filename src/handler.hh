
#include <string>
#include <memory>

#include "XrdHttp/XrdHttpExtHandler.hh"

class XrdSecEntity;
class XrdAccAuthorize;

namespace Macaroons {

class Handler : public XrdHttpExtHandler {
public:
    Handler(XrdSysError *log, const char *config, XrdOucEnv *myEnv) :
        m_log(log),
        m_location("test site"),
        m_secret("top secret")
    {}
    virtual ~Handler() {}

    virtual bool MatchesPath(const char *verb, const char *path) override;
    virtual int ProcessReq(XrdHttpExtReq &req) override;

    virtual int Init(const char *cfgfile) override {return 0;}

private:
    std::string GenerateID(const XrdSecEntity &, const std::string &, const std::string &);
    std::string GenerateActivities(const XrdHttpExtReq &) const;

    std::unique_ptr<XrdAccAuthorize> m_chain;
    XrdSysError *m_log;
    std::string m_location;
    std::string m_secret;
};

}
