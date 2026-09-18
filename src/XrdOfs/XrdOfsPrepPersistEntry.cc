// Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
#include "XrdOfsPrepPersist.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdVersion.hh"
#include <sstream>
#include <stdexcept>

extern "C" XrdOfsPrepare *XrdOfsAddPrepare(XrdOfsAddPrepareArguments) {
  try {
    std::istringstream input(parms ? parms : "");
    std::string root, identity, guarantee, extra;
    if (!(input >> root >> identity >> guarantee) || guarantee != "replay-safe" || !prepP)
      throw std::runtime_error("expected: <absolute-state-root> <backend-identity> replay-safe [maxrequests=N] [retention=N]");
    size_t maxRequests = 10000;
    uint64_t retention = 7 * 24 * 3600;
    while (input >> extra) {
      const auto equal = extra.find('=');
      if (equal == std::string::npos) throw std::runtime_error("invalid persistent prepare option");
      const auto key = extra.substr(0, equal), value = extra.substr(equal + 1);
      if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error("invalid persistent prepare limit");
      const auto number = std::stoull(value);
      if (!number) throw std::runtime_error("persistent prepare limits must be positive");
      if (key == "maxrequests" && number <= 1000000) maxRequests = number;
      else if (key == "retention" && number <= 365 * 24 * 3600) retention = number;
      else throw std::runtime_error("unknown or out-of-range persistent prepare option");
    }
    // The OFS configuration environment is temporary. Retain only the outer
    // process environment, where OFS publishes its authorizer after loading us.
    auto *runtime = envP ? static_cast<XrdOucEnv *>(envP->GetPtr("xrdEnv*")) : nullptr;
    if (!runtime) throw std::runtime_error("persistent prepare requires the process environment");
    auto *result = new XrdOfsPrepPersist(root, identity, *prepP, runtime, eDest, maxRequests, retention);
    runtime->Put("xrd.prepare.profile", "v1");
    return result;
  } catch (const std::exception &ex) {
    if (eDest) eDest->Emsg("PrepPersist", ex.what());
    return nullptr;
  }
}
XrdVERSIONINFO(XrdOfsAddPrepare, PrepPersist);
