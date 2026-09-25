#ifndef XRD_OFS_PREP_BACKEND_HH
#define XRD_OFS_PREP_BACKEND_HH
// Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
#include "XrdOfsPrepStorage.hh"

// In-process tape services implement operations, not a second request registry.
// The coordinator persists per-file dispatch and acknowledgement in its store.
// Calls run without registry locks; implementations must bound their IO and
// ensure any required namespace updates are durable before returning Applied.
// Execute/Recover are serialized on one worker. Observe can run concurrently
// with them and with other Observe calls on foreground archiveinfo threads.
// Implementations MUST synchronize shared state, without calling back into the
// coordinator while holding locks that another callback needs.
class XrdOfsPrepBackend {
public:
  struct Result {
    // Applied(Stage) acknowledges durable acceptance of an asynchronous recall.
    // Applied(Cancel) means this request's interest is durably cancelled, not
    // merely that a cancellation command was queued. It need not stop a shared
    // physical recall. A queued cancellation returns Unknown until Recover can
    // establish its outcome. Applied(Evict) acknowledges durable release.
    // A previously terminal recall and its timestamps remain unchanged.
    // Rejected is a definite failure with no outstanding side effect.
    // Retry means definitely not dispatched; the coordinator may call Execute again.
    // Unknown (also any exception) must only be resolved through Recover.
    enum Outcome { Applied, Rejected, Retry, Unknown } outcome = Unknown;
    std::string error;
  };
  struct Status {
    bool disk = false, tape = false;
    // Namespace/locality lookup error shared by all requests for this path.
    // Do not report an error belonging to only one recall/request here.
    std::string error;
  };
  virtual ~XrdOfsPrepBackend() = default;
  // (operationId, file.path) identifies one side effect. requestId groups calls
  // belonging to the same recall. No client credentials are persisted or passed.
  virtual Result Execute(const std::string &requestId, const std::string &operationId,
                         XrdOfsPrep::OperationKind, const XrdOfsPrep::File &) = 0;
  // Called after an uncertain reply or restart with a recorded dispatch. Must
  // inspect an authoritative outcome or use backend deduplication; never blindly
  // repeat a side effect. The default leaves it pending for reconciliation.
  virtual Result Recover(const std::string &, const std::string &,
                         XrdOfsPrep::OperationKind, const XrdOfsPrep::File &) {
    return {Result::Unknown, "backend outcome requires reconciliation"};
  }
  virtual Status Observe(const std::string &path) = 0;
};
#endif
