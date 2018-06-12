
#include <stdexcept>

#include "handler.hh"

#include "XrdSys/XrdSysLogger.hh"
#include "XrdHttp/XrdHttpExtHandler.hh"
#include "XrdAcc/XrdAccAuthorize.hh"
#include "XrdVersion.hh"

XrdVERSIONINFO(XrdAccAuthorizeObject, XrdMacaroons);
XrdVERSIONINFO(XrdHttpGetExtHandler,  XrdMacaroons);

extern "C" {

XrdAccAuthorize *XrdAccAuthorizeObject(XrdSysLogger *log,
                                       const char   *config,
                                       const char   *parm)
{
    return nullptr;
/*  TODO: actually implement
    std::unique_ptr<XrdAccAuthorize> def_authz(XrdAccDefaultAuthorizeObject(lp, cfn, parm, compiledVer));
    XrdAccMacaroons *authz{nullptr};
    authz = new XrdAccMacaroons(lp, parm, std::move(def_authz));
    return authz;
*/
}


XrdHttpExtHandler *XrdHttpGetExtHandler(
    XrdSysError *log, const char * config,
    const char * /*parms*/, XrdOucEnv *env)
{
    log->Emsg("Initialize", "Creating new Macaroon handler object");
    try
    {
        return new Macaroons::Handler(log, config, env);
    }
    catch (std::runtime_error e)
    {
        log->Emsg("Config", "Generation of Macaroon handler failed", e.what());
        return nullptr;
    }
/*  TODO: actually implement
    TPCHandler *retval{nullptr};
    retval = new TPCHandler(log, config, myEnv);
    return retval;
*/
}


}
