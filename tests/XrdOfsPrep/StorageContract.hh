#ifndef XRD_PREP_STORAGE_CONTRACT_HH
#define XRD_PREP_STORAGE_CONTRACT_HH
// Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
// Shared with out-of-tree backend tests; uses only the installed storage API.
#include "XrdOfs/XrdOfsPrepStorage.hh"
#include <gtest/gtest.h>
#include <functional>
#include <set>
#include <thread>
#include <atomic>
namespace PrepStorageTest {
inline XrdOfsPrep::Record Record() {
  XrdOfsPrep::Record r;
  r.id = XrdOfsPrepStorage::NewId(); r.backend = "test";
  r.owner.kind = "token"; r.owner.issuer = "issuer"; r.owner.subject = "alice";
  r.createdAt = 100; r.startedAt = 101; r.completedAt = 102;
  XrdOfsPrep::File f;
  f.path = "/a"; f.state = XrdOfsPrep::FileState::Completed;
  f.diskLifetime = "PT1H"; f.startedAt = 101; f.finishedAt = 102;
  f.error = "stage"; f.cancelError = "cancel"; f.releaseError = "release";
  f.cancelRequested = true; f.cancelAcknowledged = false;
  f.releaseRequested = false; f.released = true;
  XrdOfsPrep::Value array;
  array.data = XrdOfsPrep::Value::Array{{nullptr}, {true}, {int64_t(-7)},
    {uint64_t(18446744073709551615ULL)}, {1.25}, {std::string("a\0b", 3)},
    {XrdOfsPrep::Value::Object{{"nested", {std::string("value")}}}}};
  f.targetedMetadata = XrdOfsPrep::Value::Object{{"values", array}};
  r.files.push_back(f);
  XrdOfsPrep::Operation op;
  op.id = r.id + ":1"; op.files = r.files;
  op.kind = XrdOfsPrep::OperationKind::Evict;
  op.state = XrdOfsPrep::OperationState::Failed; op.error = "rejected";
  r.operations.push_back(op);
  return r;
}
inline void Error(int code, const std::function<void()> &action) {
  try { action(); FAIL() << "expected errno " << code; }
  catch (const std::system_error &ex) { EXPECT_EQ(ex.code().value(), code); }
}
inline void CheckRecord(const XrdOfsPrep::Record &r) {
  EXPECT_EQ(r.schema, 1u); EXPECT_EQ(r.backend, "test");
  EXPECT_EQ(r.owner.kind, "token"); EXPECT_EQ(r.owner.issuer, "issuer");
  EXPECT_EQ(r.owner.subject, "alice"); EXPECT_FALSE(r.owner.name);
  EXPECT_EQ(r.createdAt, 100u); EXPECT_EQ(r.startedAt, 101u); EXPECT_EQ(r.completedAt, 102u);
  ASSERT_EQ(r.files.size(), 1u); const auto &f = r.files[0];
  EXPECT_EQ(f.path, "/a"); EXPECT_EQ(f.state, XrdOfsPrep::FileState::Completed);
  EXPECT_EQ(f.diskLifetime, "PT1H"); EXPECT_EQ(f.startedAt, 101u); EXPECT_EQ(f.finishedAt, 102u);
  EXPECT_EQ(f.error, "stage"); EXPECT_EQ(f.cancelError, "cancel"); EXPECT_EQ(f.releaseError, "release");
  EXPECT_EQ(f.cancelRequested, true); EXPECT_EQ(f.cancelAcknowledged, false);
  EXPECT_EQ(f.releaseRequested, false); EXPECT_EQ(f.released, true);
  ASSERT_TRUE(f.targetedMetadata);
  const auto &a = std::get<XrdOfsPrep::Value::Array>(f.targetedMetadata->at("values").data);
  ASSERT_EQ(a.size(), 7u);
  EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(a[0].data));
  EXPECT_TRUE(std::get<bool>(a[1].data)); EXPECT_EQ(std::get<int64_t>(a[2].data), -7);
  EXPECT_EQ(std::get<uint64_t>(a[3].data), UINT64_MAX);
  EXPECT_EQ(std::get<double>(a[4].data), 1.25);
  EXPECT_EQ(std::get<std::string>(a[5].data), std::string("a\0b", 3));
  EXPECT_EQ(std::get<std::string>(std::get<XrdOfsPrep::Value::Object>(a[6].data).at("nested").data), "value");
  ASSERT_EQ(r.operations.size(), 1u);
  EXPECT_EQ(r.operations[0].id, r.id + ":1");
  EXPECT_EQ(r.operations[0].kind, XrdOfsPrep::OperationKind::Evict);
  EXPECT_EQ(r.operations[0].state, XrdOfsPrep::OperationState::Failed);
  EXPECT_EQ(r.operations[0].error, "rejected");
  ASSERT_EQ(r.operations[0].files.size(), 1u);
  EXPECT_EQ(r.operations[0].files[0].targetedMetadata->size(), 1u);
}
inline void Contract(XrdOfsPrepStorage &store) {
  auto r = Record();
  Error(ENOENT, [&] { store.Read(r.id); });
  EXPECT_EQ(store.Create(r), 1u);
  Error(EEXIST, [&] { store.Create(r); });
  r = store.Read(r.id); CheckRecord(r);
  EXPECT_EQ(r.revision, 1u); EXPECT_FALSE(r.deleted);
  // Competing writes based on the same snapshot: exactly one can commit.
  std::atomic<unsigned> success{0}, conflict{0}, other{0};
  auto change = [&] {
    auto changed = r; changed.deleted = true;
    try { if (store.Update(changed, 1) == 2) ++success; else ++other; }
    catch (const std::system_error &ex) { if (ex.code().value() == EAGAIN) ++conflict; else ++other; }
    catch (...) { ++other; }
  };
  std::thread one(change), two(change); one.join(); two.join();
  EXPECT_EQ(success, 1u); EXPECT_EQ(conflict, 1u); EXPECT_EQ(other, 0u);
  EXPECT_TRUE(store.Read(r.id).deleted);
  Error(EAGAIN, [&] { store.Erase(r.id, 1); });
  EXPECT_EQ(store.Read(r.id).revision, 2u);
  store.Erase(r.id, 2);
  Error(ENOENT, [&] { store.Read(r.id); });
  Error(ENOENT, [&] { store.Update(r, 1); });
  Error(ENOENT, [&] { store.Erase(r.id, 2); });
  std::set<std::string> expected, actual;
  for (unsigned i = 0; i < 9; ++i) {
    auto entry = Record(); store.Create(entry); expected.insert(entry.id);
  }
  auto cursor = store.List(); ASSERT_TRUE(cursor);
  bool done = false;
  for (unsigned pages = 0; pages < 100 && !done; ++pages) {
    auto page = cursor->Next(2); EXPECT_LE(page.ids.size(), 2u);
    actual.insert(page.ids.begin(), page.ids.end()); done = page.done;
  }
  EXPECT_TRUE(done); EXPECT_EQ(actual, expected);
  for (const auto &id : expected) store.Erase(id, 1);
}
}
#endif
