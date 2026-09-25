// Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
// Test-only path/CGI authorizer, loaded by a real OFS server.
#include "XrdAcc/XrdAccAuthorize.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdVersion.hh"
#include <cstring>
namespace {
class PathAuth : public XrdAccAuthorize {
public:
  XrdAccPrivs Access(const XrdSecEntity *, const char *path, Access_Operation,
                    XrdOucEnv *env) override {
    const char *token = env ? env->Get("authz") : nullptr;
    if (!token) return XrdAccPriv_None;
    return ((!std::strcmp(path, "/a") && !std::strcmp(token, "alpha")) ||
            (!std::strcmp(path, "/b") && !std::strcmp(token, "beta")))
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
