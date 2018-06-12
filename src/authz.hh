
#include "XrdAcc/XrdAccAuthorize.hh"
#include "XrdSys/XrdSysLogger.hh"

namespace Macaroons
{

class Authz : public XrdAccAuthorize
{
public:
    Authz(XrdSysLogger *lp, const char *parms, XrdAccAuthorize *chain);

    virtual ~Authz() {}

    virtual XrdAccPrivs Access(const XrdSecEntity     *Entity,
                               const char             *path,
                               const Access_Operation  oper,
                                     XrdOucEnv        *env);

    virtual int Audit(const int accok, const XrdSecEntity *Entity,
                      const char *path, const Access_Operation oper,
                      XrdOucEnv *Env)
    {
        return 0;
    }

    virtual int Test(const XrdAccPrivs priv,
                     const Access_Operation oper)
    {
        return 0;
    }

private:
    std::string m_secretkey;
    std::string m_location;
};

}
