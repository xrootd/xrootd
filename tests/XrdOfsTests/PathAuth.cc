// Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
// Test-only path/CGI authorizer, loaded by a real OFS server.
#include "XrdAcc/XrdAccAuthorize.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdSec/XrdSecEntityAttr.hh"
#include "XrdVersion.hh"
#include <cstring>
#include <string>
namespace {
class PathAuth : public XrdAccAuthorize {
public:
  XrdAccPrivs Access(const XrdSecEntity *entity, const char *path, Access_Operation,
                    XrdOucEnv *env) override {
    const char *token = env ? env->Get("authz") : nullptr;
    std::string name;
    if (entity) entity->eaAPI->Get("request.name", name);

    // Mimic a chained authorizer whose mapped name is available to later checks.
    // OFS must not let /a's mapping grant access to another prepare path.
    if (!std::strcmp(path, "/inherited"))
      return name == "alpha" ? XrdAccPriv_All : XrdAccPriv_None;
    if (!token || !name.empty()) return XrdAccPriv_None;
    if (!std::strcmp(path, "/a") && !std::strcmp(token, "alpha")) {
      if (entity) entity->eaAPI->Add("request.name", "alpha", true);
      return XrdAccPriv_All;
    }
    return !std::strcmp(path, "/b") && !std::strcmp(token, "beta")
           ? XrdAccPriv_All : XrdAccPriv_None;
  }
  int Audit(int, const XrdSecEntity *, const char *, Access_Operation, XrdOucEnv *) override { return 1; }
  int Test(XrdAccPrivs p, Access_Operation) override { return p != XrdAccPriv_None; }
};
}
extern "C" XrdAccAuthorize *XrdAccAuthorizeObject(XrdSysLogger *, const char *, const char *) {
  return new PathAuth;
}
XrdVERSIONINFO(XrdAccAuthorizeObject, PrepPathAuth);
