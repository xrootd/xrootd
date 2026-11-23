#ifndef XRD_OFS_PREP_PERSIST_HH
#define XRD_OFS_PREP_PERSIST_HH
// Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
#include "XrdOfsPrepare.hh"
#include "XrdOfsPrepStorage.hh"
#include "XrdOfsPrepBackend.hh"
#include <cstdint>
#include <memory>
#include <string>
class XrdSysError;

// Alternative to XrdAccAuthorize for an SFS with its own verified identity and
// path authorization machinery. Register as "XrdOfsPrepAuthorizer*" in the
// constructor environment; it takes precedence over XrdAccAuthorize*.
// Called for EVERY selected path (stored manifest for ID-only requests), with
// this request's CGI. Return false on denial. On success supply a freshly
// verified token issuer+subject or a stable authenticated mapped identity.
// Never derive identity from unverified claims or cached connection attributes.
// The callback must be thread safe and outlive the coordinator.
class XrdOfsPrepAuthorizer {
public:
  virtual ~XrdOfsPrepAuthorizer() = default;
  virtual bool Authorize(const XrdSecEntity &, const std::string &path,
                         const std::string &cgi, XrdOfsPrep::Owner &) = 0;
};

class XrdOfsPrepPersist final : public XrdOfsPrepare {
public:
  // Takes ownership of storage. The backend, environment and logger must remain
  // alive until destruction has joined the recovery worker. EOS and other SFS
  // implementations can forward their prepare entry points to this coordinator.
  XrdOfsPrepPersist(std::unique_ptr<XrdOfsPrepStorage> storage,
                   const std::string &backendIdentity, XrdOfsPrepare &backend,
                   XrdOucEnv *environment, XrdSysError *log,
                   size_t maxRequests = 10000, uint64_t retention = 7 * 24 * 3600);
  // A null environment supplies no authorization and fails closed. Native
  // SFS integrations provide XrdOfsPrepAuthorizer* or XrdAccAuthorize*.
  // Native backends use this overload; storage also holds the dispatch journal.
  XrdOfsPrepPersist(std::unique_ptr<XrdOfsPrepStorage> storage,
                   const std::string &backendIdentity, XrdOfsPrepBackend &backend,
                   XrdOucEnv *environment, XrdSysError *log,
                   size_t maxRequests = 10000, uint64_t retention = 7 * 24 * 3600);
  XrdOfsPrepPersist(const std::string &root, const std::string &backendIdentity,
                   XrdOfsPrepare &backend, XrdOucEnv *environment,
                   XrdSysError *log, size_t maxRequests = 10000,
                   uint64_t retention = 7 * 24 * 3600);
  ~XrdOfsPrepPersist() override;
  // Publish only after the SFS routes native prepare calls to this instance.
  // The HTTP Tape API tests this contract, independently of the storage backend.
  void PublishProfile(XrdOucEnv &environment) const;
  int begin(XrdSfsPrep &, XrdOucErrInfo &, const XrdSecEntity *) override;
  int cancel(XrdSfsPrep &, XrdOucErrInfo &, const XrdSecEntity *) override;
  int query(XrdSfsPrep &, XrdOucErrInfo &, const XrdSecEntity *) override;
private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};
#endif
