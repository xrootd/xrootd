#ifndef XRD_PREP_MEMORY_STORAGE_HH
#define XRD_PREP_MEMORY_STORAGE_HH
// Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
#include "XrdOfs/XrdOfsPrepStorage.hh"
#include <mutex>
class MemoryStorage final : public XrdOfsPrepStorage {
public:
  struct State {
    std::mutex mutex;
    std::map<std::string, Record> records;
    bool writer = false, failCreate = false, failUpdates = false, failScan = false, failRead = false;
    unsigned scans = 0, updates = 0;
  };
  std::shared_ptr<State> state;
  explicit MemoryStorage(std::shared_ptr<State> s) : state(std::move(s)) {
    std::lock_guard<std::mutex> guard(state->mutex);
    if (state->writer) Error(EBUSY);
    state->writer = true;
  }
  ~MemoryStorage() override { std::lock_guard<std::mutex> guard(state->mutex); state->writer = false; }
  Record Read(const std::string &id) override {
    std::lock_guard<std::mutex> guard(state->mutex);
    if (state->failRead) { state->failRead = false; Error(EIO); }
    auto it = state->records.find(id); if (it == state->records.end()) Error(ENOENT);
    return it->second;
  }
  uint64_t Create(const Record &r) override {
    std::lock_guard<std::mutex> guard(state->mutex);
    if (state->records.count(r.id)) Error(EEXIST);
    state->records[r.id] = r; state->records[r.id].revision = 1;
    if (state->failCreate) Error(EIO, true);
    return 1;
  }
  uint64_t Update(const Record &r, uint64_t expected) override {
    std::lock_guard<std::mutex> guard(state->mutex);
    Check(r.id, expected); ++state->updates;
    state->records[r.id] = r; state->records[r.id].revision = expected + 1;
    if (state->failUpdates) Error(EIO, true);
    return expected + 1;
  }
  void Erase(const std::string &id, uint64_t expected) override {
    std::lock_guard<std::mutex> guard(state->mutex); Check(id, expected); state->records.erase(id);
  }
  class Scan final : public Cursor {
    std::shared_ptr<State> state;
    std::string after;
  public:
    explicit Scan(std::shared_ptr<State> s) : state(std::move(s)) {}
    Page Next(size_t limit) override {
      std::lock_guard<std::mutex> guard(state->mutex);
      ++state->scans;
      if (state->failScan) { state->failScan = false; Error(EIO); }
      Page page; auto it = state->records.upper_bound(after);
      while (it != state->records.end() && limit--) { after = it->first; page.ids.push_back(it++->first); }
      page.done = it == state->records.end(); return page;
    }
  };
  std::unique_ptr<Cursor> List() override { return std::make_unique<Scan>(state); }
private:
  [[noreturn]] static void Error(int code, bool unknown = false) {
    using E = XrdOfsPrep::StorageError;
    throw E(code, "memory store fault", unknown ? E::Outcome::Unknown : E::Outcome::Unchanged);
  }
  void Check(const std::string &id, uint64_t expected) {
    auto it = state->records.find(id); if (it == state->records.end()) Error(ENOENT);
    if (it->second.revision != expected) Error(EAGAIN);
  }
};
#endif
