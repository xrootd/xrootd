#include "XrdMacaroonsAuthz.hh"
#include "XrdMacaroonsHandler.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdOuc/XrdOucPinPath.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdHttp/XrdHttpExtHandler.hh"
#include "XrdAcc/XrdAccAuthorize.hh"
#include "XrdVersion.hh"

#include <stdexcept>
#include <dlfcn.h>

XrdVERSIONINFO(XrdAccAuthorizeObject, XrdMacaroons);
XrdVERSIONINFO(XrdAccAuthorizeObjAdd, XrdMacaroons);
XrdVERSIONINFO(XrdHttpGetExtHandler,  XrdMacaroons);

// Trick to access compiled version and directly call for the default object
// is taken from xrootd-scitokens.
static XrdVERSIONINFODEF(compiledVer, XrdAccTest, XrdVNUMBER, XrdVERSION);
extern XrdAccAuthorize *XrdAccDefaultAuthorizeObject(XrdSysLogger   *lp,
                                                     const char     *cfn,
                                                     const char     *parm,
                                                     XrdVersionInfo &myVer);

XrdSciTokensHelper *SciTokensHelper = nullptr;

extern "C" {

XrdAccAuthorize *XrdAccAuthorizeObjAdd(XrdSysLogger *log,
                                       const char   *config,
                                       const char   *params,
                                       XrdOucEnv    * /*not used*/,
                                       XrdAccAuthorize * chain_authz)
{
    try
    {
        auto new_authz = new Macaroons::Authz(log, config, chain_authz);
        SciTokensHelper = new_authz;
        return new_authz;
    }
    catch (std::runtime_error &e)
    {
        XrdSysError err(log, "macaroons");
        err.Emsg("Config", "Configuration of Macaroon authorization handler failed", e.what());
        return nullptr;
    }
}

XrdAccAuthorize *XrdAccAuthorizeObject(XrdSysLogger *log,
                                       const char   *config,
                                       const char   *parms)
{
    XrdAccAuthorize *chain_authz = nullptr;
    XrdSysError err(log, "macaroons");

    if (parms && parms[0]) {
        XrdOucString parms_str(parms);
        XrdOucString chained_lib;
        int from = parms_str.tokenize(chained_lib, 0, ' ');
        const char *chained_parms = nullptr;
        err.Emsg("Config", "Will chain library", chained_lib.c_str());
        if (from > 0)
        {
            parms_str.erasefromstart(from);
            if (parms_str.length())
            {
                err.Emsg("Config", "Will chain parameters", parms_str.c_str());
                chained_parms = parms_str.c_str();
            }
        }
        char resolvePath[2048];
        bool usedAltPath{true};
        if (!XrdOucPinPath(chained_lib.c_str(), usedAltPath, resolvePath, 2048)) {
            err.Emsg("Config", "Failed to locate appropriately versioned chained auth library:", parms);
            return nullptr;
        }
        void *handle_base = dlopen(resolvePath, RTLD_LOCAL|RTLD_NOW);
        if (handle_base == nullptr) {
            err.Emsg("Config", "Failed to base plugin ", resolvePath, dlerror());
            return nullptr;
        }

        XrdAccAuthorize *(*ep)(XrdSysLogger *, const char *, const char *);
        ep = (XrdAccAuthorize *(*)(XrdSysLogger *, const char *, const char *))
             (dlsym(handle_base, "XrdAccAuthorizeObject"));
        if (!ep)
        {
            dlclose(handle_base);
            err.Emsg("Config", "Unable to chain second authlib after macaroons", parms);
            return nullptr;
        }

        chain_authz = (*ep)(log, config, chained_parms);

        if (chain_authz == nullptr) {
          dlclose(handle_base);
          err.Emsg("Config", "Unable to chain second authlib after macaroons "
                    "which returned nullptr");
          return nullptr;
        }
    }
    else
    {
        chain_authz = XrdAccDefaultAuthorizeObject(log, config, parms, compiledVer);
    }
    try
    {
        auto new_authz = new Macaroons::Authz(log, config, chain_authz);
        SciTokensHelper = new_authz;
        return new_authz;
    }
    catch (const std::runtime_error &e)
    {
        err.Emsg("Config", "Configuration of Macaroon authorization handler failed", e.what());
        return nullptr;
    }
}


XrdHttpExtHandler *XrdHttpGetExtHandler(
    XrdSysError *log, const char * config,
    const char * parms, XrdOucEnv *env)
{
    void *authz_raw = env->GetPtr("XrdAccAuthorize*");
    XrdAccAuthorize *def_authz = static_cast<XrdAccAuthorize *>(authz_raw);

    log->Emsg("Initialize", "Creating new Macaroon handler object");
    try
    {
        return new Macaroons::Handler(log, config, env, def_authz);
    }
    catch (std::runtime_error &e)
    {
        log->Emsg("Config", "Generation of Macaroon handler failed", e.what());
        return nullptr;
    }
}


}
