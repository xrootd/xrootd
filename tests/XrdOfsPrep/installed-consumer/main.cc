// Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
// Deliberately includes installed public headers only, with no source-tree paths.
#include <XrdOfs/XrdOfsPrepPersist.hh>
#include <XrdOuc/XrdOucEnv.hh>
#include <XrdOuc/XrdOucErrInfo.hh>
#include <XrdOuc/XrdOucTList.hh>
#include <XrdSec/XrdSecEntity.hh>
#include <XrdSfs/XrdSfsInterface.hh>
#include <filesystem>
#include <string>
#include <unistd.h>

class Backend : public XrdOfsPrepBackend {
  Result Execute(const std::string &, const std::string &, XrdOfsPrep::OperationKind,
                 const XrdOfsPrep::File &) override { return {Result::Applied, {}}; }
  Status Observe(const std::string &) override { return {true, true, {}}; }
};
class Authorizer : public XrdOfsPrepAuthorizer {
  bool Authorize(const XrdSecEntity &, const std::string &, const std::string &,
                 XrdOfsPrep::Owner &owner) override {
    owner.kind = "unix"; owner.name = "installed-consumer"; return true;
  }
};
int main() {
  char root[] = "/tmp/xrd-consumer-XXXXXX";
  if (!mkdtemp(root)) return 1;
  int rc = 0;
  {
    Backend backend; Authorizer authorizer; XrdOucEnv env;
    env.PutPtr("XrdOfsPrepAuthorizer*", &authorizer);
    XrdOfsPrepPersist coordinator(XrdOfsPrepFileStorage(root), "test", backend, &env, nullptr);
    coordinator.PublishProfile(env);
    XrdSecEntity client("unix"); XrdOucTList path("/file");
    XrdSfsPrep args{}; args.reqid = const_cast<char *>("@xrdprep-v1:archiveinfo");
    args.opts = Prep_QUERY; args.paths = &path; XrdOucErrInfo error;
    if (coordinator.query(args, error, &client) != SFS_DATA ||
        std::string(error.getErrText()).find("DISK_AND_TAPE") == std::string::npos) rc = 2;
  }
  std::filesystem::remove_all(root);
  return rc;
}
