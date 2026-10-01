#ifndef XRD_OFS_PREP_STORE_HH
#define XRD_OFS_PREP_STORE_HH
// Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
#include "XrdOfsPrepProtocol.hh"
#include "XrdOfsPrepStorage.hh"
#include <filesystem>
#include <functional>
#include <mutex>
#include <set>
#include <string>

// A single-writer metadata store. The caller serializes read/modify/write of an
// individual record; different records can be accessed concurrently. Every
// successful Save has passed file AND directory fsync. No tape data lives here.
class XrdOfsPrepStore final : public XrdOfsPrepStorage {
public:
  explicit XrdOfsPrepStore(const std::string &root);
  ~XrdOfsPrepStore() override;
  XrdOfsPrepStore(const XrdOfsPrepStore &) = delete;
  XrdOfsPrepStore &operator=(const XrdOfsPrepStore &) = delete;
  XrdOfsPrepProtocol::Json Load(const std::string &id) const;
  void Save(XrdOfsPrepProtocol::Json &record, bool create = false) const;
  void Erase(const std::string &id) const;
  const std::filesystem::path &Root() const { return m_root; }
  Record Read(const std::string &id) override;
  uint64_t Create(const Record &record) override;
  uint64_t Update(const Record &record, uint64_t expected) override;
  void Erase(const std::string &id, uint64_t expected) override;
  std::unique_ptr<Cursor> List() override;
  static void SetSyncHook(std::function<void(int)> hook);
private:
  int Subdir(int parent, const char *name, const std::string &relPath, bool create) const;
  int Directory(const std::string &id, bool create) const;
  std::filesystem::path m_root;
  int m_rootFd = -1;
  int m_lockFd = -1;
  mutable std::mutex m_shardMutex;
  mutable std::set<std::string> m_syncedShards;
  std::mutex m_revisionMutex;
};
#endif
