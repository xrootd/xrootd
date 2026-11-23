// Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
// TEST ONLY: fixed synthetic credentials; never installed or used in production.
#include "XrdAcc/XrdAccAuthorize.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdSec/XrdSecEntityAttr.hh"
#include "XrdVersion.hh"
#include <cstring>
#include <string>
namespace {
class TestAuth : public XrdAccAuthorize {
public:
  XrdAccPrivs Access(const XrdSecEntity *entity, const char *, Access_Operation,
                    XrdOucEnv *env = nullptr) override {
    std::string subject;
    const char *value = env ? env->Get("authz") : nullptr;
    if (value) {
      subject = value;
      for (const char *prefix : {"Bearer%20", "Bearer "})
        if (subject.compare(0, std::strlen(prefix), prefix) == 0) subject.erase(0, std::strlen(prefix));
      if (subject != "alice" && subject != "bob") return XrdAccPriv_None;
    } else if (entity && !std::strcmp(entity->prot, "unix") && entity->name) subject = "alice";
    else return XrdAccPriv_None;
    entity->eaAPI->Add("token.subject", subject, true);
    entity->eaAPI->Add("token.issuer", "test-only", true);
    return XrdAccPriv_All;
  }
  int Audit(int, const XrdSecEntity *, const char *, Access_Operation, XrdOucEnv *) override { return 1; }
  int Test(XrdAccPrivs privileges, Access_Operation) override { return privileges != XrdAccPriv_None; }
};
}
extern "C" XrdAccAuthorize *XrdAccAuthorizeObject(XrdSysLogger *, const char *, const char *) {
  return new TestAuth;
}
XrdVERSIONINFO(XrdAccAuthorizeObject, PrepTestAuth);
