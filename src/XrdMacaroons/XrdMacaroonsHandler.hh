
#ifndef __XRD_MACAROONS_HANDLER_H
#define __XRD_MACAROONS_HANDLER_H

#include <string>
#include <memory>
#include <stdexcept>
#include <vector>

#include "XrdMacaroonsGenerate.hh"
#include "XrdMacaroonsUtils.hh"

#include "XrdHttp/XrdHttpExtHandler.hh"

class XrdOucEnv;
class XrdOucStream;
class XrdSecEntity;
class XrdAccAuthorize;

namespace Macaroons {

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
        const auto &configObj = XrdMacaroonsConfigFactory::Get(*log);
        m_max_duration = configObj.maxDuration;
    }

    virtual ~Handler();

    virtual bool MatchesPath(const char *verb, const char *path) override;
    virtual int ProcessReq(XrdHttpExtReq &req) override;

    virtual int Init(const char *cfgfile) override {return 0;}

private:
    std::string GenerateID(const std::string &, const XrdSecEntity &, const std::string &, const std::vector<std::string> &, time_t);
    std::bitset<AOP_LastOp> GenerateActivities(const XrdHttpExtReq &, const std::string &) const;
    std::string GenerateActivitiesStr(const std::bitset<AOP_LastOp> &opers) const;


    int ProcessOAuthConfig(XrdHttpExtReq &req);
    int ProcessTokenRequest(XrdHttpExtReq& req);
    int GenerateMacaroonResponse(XrdHttpExtReq& req, const std::string &response, const std::vector<std::string> &, ssize_t validity, bool oauth_response);

    ssize_t m_max_duration;
    XrdAccAuthorize *m_chain;
    XrdSysError *m_log;
    XrdMacaroonsGenerator m_generator;
};

}

#endif
