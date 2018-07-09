
#include <stdexcept>
#include <dlfcn.h>

#include "handler.hh"
#include "authz.hh"

#include "XrdOuc/XrdOucPinPath.hh"
#include "XrdSys/XrdSysError.hh"
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
    XrdAccAuthorize *chain_authz;

    if (parms && parms[0]) {
        // TODO: tokenize string instead.
        XrdSysError *err = new XrdSysError(log, "authlib");
        char resolvePath[2048];
        bool usedAltPath{true};
        if (!XrdOucPinPath(parms, usedAltPath, resolvePath, 2048)) {
            err->Emsg("Config", "Failed to locate appropriately versioned chained auth library:", parms);
            delete err;
            return NULL;
        }
        void *handle_base = dlopen(resolvePath, RTLD_LOCAL|RTLD_NOW);
        if (handle_base == NULL) {
            err->Emsg("Config", "Failed to base plugin ", resolvePath, dlerror());
            delete err;
            return NULL;
        }

        XrdAccAuthorize *(*ep)(XrdSysLogger *, const char *, const char *);
        ep = (XrdAccAuthorize *(*)(XrdSysLogger *, const char *, const char *))
             (dlsym(handle_base, "XrdAccAuthorizeObject"));
        if (!ep)
        {
            err->Emsg("Config", "Unable to chain second authlib after macaroons", parms);
            delete err;
            return NULL;
        }
        chain_authz = (*ep)(log, config, NULL);
    }
    else
    {
        chain_authz = XrdAccDefaultAuthorizeObject(log, config, parms, compiledVer);
    }
    try
    {
        return new Macaroons::Authz(log, config, chain_authz);
    }
    catch (std::runtime_error e)
    {
        XrdSysError err(log, "macaroons");
        err.Emsg("Config", "Configuration of Macaroon authorization handler failed", e.what());
        return NULL;
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
        return NULL;
    }
}


}
