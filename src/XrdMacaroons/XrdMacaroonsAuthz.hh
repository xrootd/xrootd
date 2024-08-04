
#include "XrdAcc/XrdAccAuthorize.hh"
#include "XrdSciTokens/XrdSciTokensHelper.hh"
#include "XrdSys/XrdSysError.hh"


class XrdSysError;

namespace Macaroons
{

class Authz final : public XrdAccAuthorize, public XrdSciTokensHelper
{
public:
    Authz(XrdSysLogger *lp, const char *parms, XrdAccAuthorize *chain);

    virtual ~Authz() {}

    virtual XrdAccPrivs Access(const XrdSecEntity     *Entity,
                               const char             *path,
                               const Access_Operation  oper,
                                     XrdOucEnv        *env) override;

    // Do a minimal validation that this is a non-expired token; used
    // for session tokens.
    virtual bool Validate(const char   *token,
                          std::string  &emsg,
                          long long    *expT,
                          XrdSecEntity *entP) override;

    virtual int Audit(const int accok, const XrdSecEntity *Entity,
                      const char *path, const Access_Operation oper,
                      XrdOucEnv *Env) override
    {
        return 0;
    }

    virtual int Test(const XrdAccPrivs priv,
                     const Access_Operation oper) override
    {
        return 0;
    }

    // Macaroons don't have a concept off an "issuers"; return an empty
    // list.
    virtual Issuers IssuerList() override {return Issuers();}

private:
    XrdAccPrivs OnMissing(const XrdSecEntity     *Entity,
                          const char             *path,
                          const Access_Operation  oper,
                                XrdOucEnv        *env);

    ssize_t m_max_duration;
    XrdAccAuthorize *m_chain;
    XrdSysError m_log;
    std::string m_secret;
    std::string m_location;
    int m_authz_behavior;
};

}
