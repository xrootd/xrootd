// Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
// Test-only handlers using the real common HTTP helpers; never installed.
#include "XrdHttp/XrdHttpExtHandler.hh"
#include "XrdVersion.hh"
#include <arpa/inet.h>
#include <cstring>

namespace {
class Legacy final : public XrdHttpExtHandler {
public:
  bool MatchesPath(const char *, const char *path) override {
    return path && std::strncmp(path, "/legacy/", 8) == 0;
  }
  int Init(const char *) override { return 0; }
  int ProcessReq(XrdHttpExtReq &req) override {
    if (req.resource == "/legacy/simple")
      return req.SendSimpleResp(200, nullptr, "Content-Type: application/json", "{}", 2);
    const bool fail = req.resource == "/legacy/fail";
    int rc = req.StartSimpleResp(200, nullptr, "Content-Type: application/json", fail ? 8 : 2, true);
    if (rc < 0) return rc;
    rc = req.SendData("{}", 2);
    return fail ? -1 : rc; // Deliberate response failure after headers.
  }
};
class BoundedBridge final : public XrdHttpExtHandlerBridge {
public:
  bool MatchesPath(const char *, const char *path) override {
    return path && std::strcmp(path, "/bridge-test/overflow") == 0;
  }
  bool RequiresBridge(const char *, const char *) const override { return true; }
  int Init(const char *) override { return 0; }
  int ProcessReq(XrdHttpExtReq &req) override {
    ClientRequest query{};
    const std::string payload = "@xrdprep-v1:archiveinfo\n/a?authz=alice\n";
    query.header.requestid = htons(kXR_query);
    query.query.infotype = htons(kXR_QPrep);
    query.header.dlen = htonl(payload.size());
    return req.RunNative(query, payload,
      [](XrdHttpExtReq &completion, const XrdHttpExtReq::NativeResponse &result) {
        const bool bounded = result.kind == XrdHttpExtReq::NativeResponse::Error &&
          result.data == "Native prepare response exceeds configured limit";
        return completion.SendSimpleResp(bounded ? 502 : 500, nullptr,
                                         "Content-Type: application/json", "{}", 2);
      }, 1);
  }
};
}
extern "C" XrdHttpExtHandler *XrdHttpGetExtHandler(XrdHttpExtHandlerArgs) {
  if (parms && std::strcmp(parms, "bridge") == 0) return new BoundedBridge;
  return new Legacy;
}
XrdVERSIONINFO(XrdHttpGetExtHandler, PrepTestHttpHandler);
