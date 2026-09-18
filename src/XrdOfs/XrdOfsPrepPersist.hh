#ifndef XRD_OFS_PREP_PERSIST_HH
#define XRD_OFS_PREP_PERSIST_HH
// Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
#include "XrdOfsPrepare.hh"
#include <cstdint>
#include <memory>
#include <string>
class XrdSysError;

class XrdOfsPrepPersist final : public XrdOfsPrepare {
public:
  XrdOfsPrepPersist(const std::string &root, const std::string &backendIdentity,
                   XrdOfsPrepare &backend, XrdOucEnv *environment,
                   XrdSysError *log, size_t maxRequests = 10000,
                   uint64_t retention = 7 * 24 * 3600);
  ~XrdOfsPrepPersist() override;
  int begin(XrdSfsPrep &, XrdOucErrInfo &, const XrdSecEntity *) override;
  int cancel(XrdSfsPrep &, XrdOucErrInfo &, const XrdSecEntity *) override;
  int query(XrdSfsPrep &, XrdOucErrInfo &, const XrdSecEntity *) override;
private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};
#endif
