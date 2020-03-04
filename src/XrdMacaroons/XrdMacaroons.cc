#include <stdexcept>
#include <dlfcn.h>

#include "XrdMacaroonsHandler.hh"
#include "XrdMacaroonsAuthz.hh"

#include "XrdOuc/XrdOucString.hh"
#include "XrdOuc/XrdOucPinPath.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysPlugin.hh"
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

//------------------------------------------------------------------------------
//! Handle chained authorization plugins specified directly as a parameter in
//! the parms variable or in the key=value format such as "chain_authz=lib.so"
//!
//! @param err error object user ofr logging
//! @param config path to configuration file
//! @param parms string representing the configuration options given to the
//!        libXrdMacaroons library
//!
//! @return authorization plugin object to be chained
//------------------------------------------------------------------------------
XrdAccAuthorize* HandleChainedAuthz(XrdSysError* err, const char* config,
                                    const char* parms)
{
  if (!parms) {
    return NULL;
  }

  XrdOucString sparms = parms;
  XrdOucString token = "chain_authz=";

  if (sparms.beginswith(token.c_str())) {
    sparms.erase(0, token.length());
  }

  err->Log(Macaroons::LogMask::Info, __FUNCTION__,
           "Try to chain authz:", sparms.c_str());

  // Trim whitespaces
  while (sparms.length() && (sparms[0] == ' ')) {
    sparms.erase(0, 1);
  }

  if (sparms.length() == 0) {
    err->Log(Macaroons::LogMask::Error, __FUNCTION__,
             "No authz lib specified in the chain option");
    return NULL;
  }

  int pos = sparms.find(' ');
  const char* new_parms = ((pos == STR_NPOS) ||
                           (pos == sparms.length() -1)) ? NULL : &sparms[pos + 1];
  XrdOucString lib_name(sparms, 0, pos);
  char resolve_path[2048];
  bool no_alt_path = false;

  if (!XrdOucPinPath(lib_name.c_str(), no_alt_path, resolve_path,
                     sizeof(resolve_path))) {
    err->Log(Macaroons::LogMask::Error, __FUNCTION__,
             "Failed to locate library path for", lib_name.c_str());
    return NULL;
  }

  // Try to load the XrdAccAuthorizeObject provided by the given library
  XrdAccAuthorize *(*authz_ep)(XrdSysLogger*, const char*, const char*);
  XrdSysPlugin authz_plugin(err, resolve_path, "authz", &compiledVer, 1);
  void* authz_addr = authz_plugin.getPlugin("XrdAccAuthorizeObject", 0, 0);
  authz_plugin.Persist();
  authz_ep = (XrdAccAuthorize * (*)(XrdSysLogger*, const char*,
                                    const char*))(authz_addr);
  XrdAccAuthorize* authz_obj = NULL;

  if (authz_ep && (authz_obj = authz_ep(err->logger(), config, new_parms))) {
    err->Log(Macaroons::LogMask::Info, __FUNCTION__,
             "Successfully chained authz plugin from", resolve_path);
    err->Log(Macaroons::LogMask::Info, __FUNCTION__, "Chained authz plugin "
             "with params \"", (new_parms ? new_parms : ""), "\"");
    return authz_obj;
  } else {
    err->Log(Macaroons::LogMask::Error, __FUNCTION__,
             "Failed loading authz plugin from", resolve_path);
    return NULL;
  }
}

extern "C" {

XrdAccAuthorize *XrdAccAuthorizeObject(XrdSysLogger *log,
                                       const char   *config,
                                       const char   *parms)
{
    static XrdAccAuthorize* sMacaroonsAuthz = NULL;
    XrdOucString version = XrdVERSION;
    XrdSysError err(log, "authz_macaroons_");
    err.Say("++++++ XrdMacaroons(authz) plugin ", version.c_str());

    if (sMacaroonsAuthz) {
      err.Say("------ XrdMacaroons(authz) plugin already loaded and available");
      return sMacaroonsAuthz;
    }

    XrdAccAuthorize *chain_authz = NULL;

    if (parms && parms[0]) {
      chain_authz = HandleChainedAuthz(&err, config, parms);
    } else {
      chain_authz = XrdAccDefaultAuthorizeObject(log, config, parms, compiledVer);
    }

    try {
      sMacaroonsAuthz = (XrdAccAuthorize*)new Macaroons::Authz(log, config, chain_authz);
      err.Say("------ XrdMacaroons(authz) initialization successful");
    } catch (const std::runtime_error &e) {
      err.Say("------ XrdMacaroons(authz) initialization failed!");
    }

    return sMacaroonsAuthz;
}


XrdHttpExtHandler*
XrdHttpGetExtHandler(XrdSysError *eDest, const char * config,
                     const char * parms, XrdOucEnv *env)
{
  XrdOucString version = XrdVERSION;
  eDest->Say("++++++ XrdMacaroons(http) plugin ", version.c_str());

  XrdAccAuthorize *authz = NULL;

  if (parms) {
    authz = HandleChainedAuthz(eDest, config, parms);
  }

  // Fall back to default authz if nothing else specified
  if (!authz)  {
    authz = XrdAccDefaultAuthorizeObject(eDest->logger(),
                                         config, parms, compiledVer);
  }

  try {
    XrdHttpExtHandler* obj = (XrdHttpExtHandler*)
      new Macaroons::Handler(eDest, config, env, authz);
    eDest->Say("------ XrdMacaroons(http) intialization successful");
    return obj;
  } catch (const std::runtime_error &e)  {
    eDest->Say("------ XrdMacaroons(http) initialization failed!");
    return NULL;
  }
}

}
