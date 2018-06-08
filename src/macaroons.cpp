
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


XrdHttpExtHandler *XrdHttpGetExtHandler(XrdSysError *log, const char * config,
                                        const char * /*parms*/, XrdOucEnv *myEnv)
{
    log->Emsg("Initialize", "Creating new Macaroon handler object");
    return new Macaroons::Handler(log, config, myEnv);
/*  TODO: actually implement
    TPCHandler *retval{nullptr};
    retval = new TPCHandler(log, config, myEnv);
    return retval;
*/
}


}
