
#include <string>

#include "XrdHttp/XrdHttpExtHandler.hh"

namespace Macaroons {

class Handler : public XrdHttpExtHandler {
public:
    Handler(XrdSysError *log, const char *config, XrdOucEnv *myEnv) :
        m_location("test site"),
        m_secret("top secret")
    {}
    virtual ~Handler() {}

    virtual bool MatchesPath(const char *verb, const char *path) override;
    virtual int ProcessReq(XrdHttpExtReq &req) override;

    virtual int Init(const char *cfgfile) override {return 0;}

private:
    std::string m_location;
    std::string m_secret;
};

}
