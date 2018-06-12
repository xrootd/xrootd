
#include <stdexcept>

#include "handler.hh"
#include "authz.hh"

#include "XrdSys/XrdSysLogger.hh"
#include "XrdHttp/XrdHttpExtHandler.hh"
#include "XrdAcc/XrdAccAuthorize.hh"
#include "XrdVersion.hh"

XrdVERSIONINFO(XrdAccAuthorizeObject, XrdMacaroons);
XrdVERSIONINFO(XrdHttpGetExtHandler,  XrdMacaroons);

// Trick to access compiled version and directly call for the default object
// is taken from xrootd-scitokens.
static XrdVERSIONINFODEF(compiledVer, XrdAccTest, XrdVNUMBER, XrdVERSION);
extern XrdAccAuthorize *XrdAccDefaultAuthorizeObject(XrdSysLogger   *lp,
                                                     const char     *cfn,
                                                     const char     *parm,
                                                     XrdVersionInfo &myVer);


extern "C" {

XrdAccAuthorize *XrdAccAuthorizeObject(XrdSysLogger *log,
                                       const char   *config,
                                       const char   *parms)
{
    XrdAccAuthorize *def_authz = XrdAccDefaultAuthorizeObject(log, config,
        parms, compiledVer);
    try
    {
        return new Macaroons::Authz(log, config, def_authz);
    }
    catch (std::runtime_error e)
    {
        // TODO: Upgrade to XrdSysError
        //log->Emsg("Config", "Configuration of Macaroon authorization handler failed", e.what());
        return nullptr;
    }
}


XrdHttpExtHandler *XrdHttpGetExtHandler(
    XrdSysError *log, const char * config,
    const char * parms, XrdOucEnv *env)
{
    XrdAccAuthorize *def_authz = XrdAccDefaultAuthorizeObject(log->logger(),
        config, parms, compiledVer);

    log->Emsg("Initialize", "Creating new Macaroon handler object");
    try
    {
        return new Macaroons::Handler(log, config, env, def_authz);
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
