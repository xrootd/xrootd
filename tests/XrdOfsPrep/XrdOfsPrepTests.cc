// Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
#include "StorageContract.hh"
#include "MemoryStorage.hh"
#include "XrdOfs/XrdOfsPrepRecord.hh"
#include "XrdAcc/XrdAccAuthorize.hh"
#include "XrdOfs/XrdOfsPrepPersist.hh"
#include "XrdOfs/XrdOfsPrepStore.hh"
#include "XrdOuc/XrdOucBuffer.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdSec/XrdSecEntityAttr.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <condition_variable>
#include <sstream>
#include <mutex>
#include <thread>
#include <vector>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace XrdOfsPrepProtocol;
namespace fs = std::filesystem;
extern "C" XrdOfsPrepare *XrdOfsAddPrepare(XrdOfsAddPrepareArguments);
namespace {
// Explicit allow-list identity fixture; production never uses null-env bypass.
XrdOucEnv *MappedTestEnvironment() {
  struct Auth : XrdAccAuthorize {
    XrdAccPrivs Access(const XrdSecEntity *, const char *, Access_Operation,
                      XrdOucEnv *) override { return XrdAccPriv_All; }
    int Audit(int, const XrdSecEntity *, const char *, Access_Operation,
              XrdOucEnv *) override { return 0; }
    int Test(XrdAccPrivs, Access_Operation) override { return 1; }
  };
  static Auth auth;
  static XrdOucEnv env;
  static const bool configured = (env.PutPtr("XrdAccAuthorize*", &auth), true);
  (void)configured;
  return &env;
}
struct Temporary {
  std::string path;
  Temporary() { char name[] = "/tmp/xrd-prep-test-XXXXXX"; auto *p = mkdtemp(name); if (!p) throw std::runtime_error("mkdtemp"); path = p; }
  ~Temporary() { std::error_code ec; fs::remove_all(path, ec); }
};
struct Args {
  XrdSfsPrep prep{};
  std::string id;
  XrdOucTListFIFO paths, opaque;
  Args(const std::string &request, int opts, std::initializer_list<std::string> files = {}, const std::string &cgi = "") : id(request) {
    int i = 0;
    for (const auto &path : files) { paths.Add(new XrdOucTList(path.c_str(), i++)); opaque.Add(new XrdOucTList(cgi.c_str())); }
    if (files.size() == 0 && !cgi.empty()) opaque.Add(new XrdOucTList(cgi.c_str()));
    prep.reqid = &id[0]; prep.opts = opts; prep.paths = paths.first; prep.oinfo = opaque.first;
  }
};
std::string Text(XrdOucErrInfo &error) {
  std::string text(error.getErrText(), error.getErrInfo());
  while (!text.empty() && !text.back()) text.pop_back();
  return text;
}
int Reply(XrdOucErrInfo &error, const Json &json, const std::string &suffix = {}) {
  auto text = json.dump() + suffix; void *memory = nullptr;
  if (posix_memalign(&memory, sizeof(void *), text.size() + 1)) throw std::bad_alloc();
  std::memcpy(memory, text.c_str(), text.size() + 1);
  error.setErrInfo(text.size() + 1, new XrdOucBuffer(static_cast<char *>(memory), text.size() + 1));
  return SFS_DATA;
}
bool Eventually(const std::function<bool()> &predicate) {
  auto end = std::chrono::steady_clock::now() + std::chrono::seconds(12);
  do { if (predicate()) return true; std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
  while (std::chrono::steady_clock::now() < end);
  return false;
}
class Backend : public XrdOfsPrepare {
public:
  mutable std::recursive_mutex mutex;
  Json records = Json::object();
  bool hold = false, unavailable = false, failAfterAccept = false, asyncCancel = false;
  unsigned stages = 0;

  void SetHold(bool value) {
    std::lock_guard<std::recursive_mutex> guard(mutex);
    hold = value;
  }
  void SetUnavailable(bool value) {
    std::lock_guard<std::recursive_mutex> guard(mutex);
    unavailable = value;
  }
  void SetFailAfterAccept(bool value) {
    std::lock_guard<std::recursive_mutex> guard(mutex);
    failAfterAccept = value;
  }
  void SetAsyncCancel(bool value) {
    std::lock_guard<std::recursive_mutex> guard(mutex);
    asyncCancel = value;
  }
  void SetFileState(const std::string &reqid, const std::string &path, const std::string &state, uint64_t finishedAt = 0, const std::string &error = "") {
    std::lock_guard<std::recursive_mutex> guard(mutex);
    auto it = records.find(reqid);
    if (it != records.end()) {
      for (auto &file : (*it)["files"]) {
        if (file["path"] == path) {
          file["state"] = state;
          if (finishedAt) file["finishedAt"] = finishedAt;
          else if (Terminal(state) && !file.contains("finishedAt"))
            file["finishedAt"] = 1;
          if (!error.empty()) file["error"] = error;
        }
      }
    }
  }
  void SetFileError(const std::string &reqid, const std::string &path, const std::string &error) {
    std::lock_guard<std::recursive_mutex> guard(mutex);
    auto it = records.find(reqid);
    if (it != records.end()) {
      for (auto &file : (*it)["files"]) {
        if (file["path"] == path) file["error"] = error;
      }
    }
  }
  unsigned GetStages() const {
    std::lock_guard<std::recursive_mutex> guard(mutex);
    return stages;
  }
  int begin(XrdSfsPrep &args, XrdOucErrInfo &error, const XrdSecEntity *) override { return Apply(args, error); }
  int cancel(XrdSfsPrep &args, XrdOucErrInfo &error, const XrdSecEntity *) override { return Apply(args, error); }
  int query(XrdSfsPrep &args, XrdOucErrInfo &error, const XrdSecEntity *) override {
    std::lock_guard<std::recursive_mutex> guard(mutex);
    if (unavailable) { error.setErrInfo(EIO, "offline"); return SFS_ERROR; }
    Json response = {{"schema", 1}, {"requestId", args.reqid}, {"known", false}, {"acknowledged", Json::array()}, {"files", Json::array()}};
    auto it = records.find(args.reqid);
    if (it != records.end()) {
      response = *it;
      if (!hold) for (auto &file : response["files"]) if (file["state"] == "STARTED") file["state"] = "COMPLETED";
    }
    return Reply(error, response);
  }
  int Apply(XrdSfsPrep &args, XrdOucErrInfo &error) {
    std::lock_guard<std::recursive_mutex> guard(mutex);
    XrdOucEnv env(args.oinfo ? args.oinfo->text : nullptr);
    const std::string op = env.Get("xrd.prepare.operation") ? env.Get("xrd.prepare.operation") : "";
    if (op.empty() || env.Get("authz")) { error.setErrInfo(EINVAL, "unsafe operation"); return SFS_ERROR; }
    auto &record = records[args.reqid];
    if (record.is_null()) record = {{"schema", 1}, {"requestId", args.reqid}, {"known", true}, {"acknowledged", Json::array()}, {"files", Json::array()}};
    for (const auto &ack : record["acknowledged"]) if (ack == op) return SFS_OK;
    if (args.opts & Prep_STAGE) {
      ++stages;
      for (auto *path = args.paths; path; path = path->next) record["files"].push_back({{"path", path->text}, {"state", hold ? "STARTED" : "COMPLETED"}});
    } else if (args.opts & Prep_CANCEL) {
      if (!asyncCancel) {
        for (auto *path = args.paths; path; path = path->next)
          for (auto &file : record["files"]) if (file["path"] == path->text && !Terminal(file["state"])) file["state"] = "CANCELLED";
      }
    }
    record["acknowledged"].push_back(op);
    if (failAfterAccept) { error.setErrInfo(EIO, "lost reply"); return SFS_ERROR; }
    return SFS_OK;
  }
};
std::string Submit(XrdOfsPrepPersist &coordinator, XrdSecEntity &owner, const std::string &cgi = "") {
  std::string id;
  if (!Eventually([&] {
    Args args("*", Prep_STAGE, {"/a", "/b"}, cgi); XrdOucErrInfo error("test");
    const auto rc = coordinator.begin(args.prep, error, &owner);
    if (rc == SFS_DATA) { id = Text(error); return true; }
    if (error.getErrInfo() != EAGAIN) throw std::runtime_error(error.getErrText());
    return false;
  })) throw std::runtime_error("admission timed out");
  return id;
}
Json Query(XrdOfsPrepPersist &coordinator, const std::string &id, XrdSecEntity &owner) {
  Args args(id, Prep_QUERY); XrdOucErrInfo error("test");
  if (coordinator.query(args.prep, error, &owner) != SFS_DATA) throw std::runtime_error(error.getErrText());
  return Json::parse(Text(error));
}
Json LoadRecord(const std::string &root, const std::string &id) {
  auto path = fs::path(root) / "requests" / id.substr(0, 2) / id.substr(2, 2) / (id + ".json");
  for (int attempt = 0; attempt < 10; ++attempt) {
    try {
      std::ifstream stream(path);
      if (stream) return Json::parse(stream);
    } catch (...) {}
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  std::ifstream stream(path);
  return Json::parse(stream);
}
fs::path RecordPath(const std::string &root, const std::string &id) {
  return fs::path(root) / "requests" / id.substr(0, 2) / id.substr(2, 2) / (id + ".json");
}
struct SyncHook {
  explicit SyncHook(std::function<void(int)> hook) { XrdOfsPrepStore::SetSyncHook(std::move(hook)); }
  ~SyncHook() { XrdOfsPrepStore::SetSyncHook(nullptr); }
};
Json PendingRecord(const std::string &id) {
  const auto now = XrdOfsPrepStore::Now();
  Json files = Json::array({{{"path", "/a"}, {"state", "SUBMITTED"}},
                            {{"path", "/b"}, {"state", "SUBMITTED"}}});
  return {{"schema", 1}, {"id", id}, {"backend", "test"},
          {"owner", {{"kind", "unix"}, {"name", "alice"}}},
          {"createdAt", now}, {"startedAt", now}, {"deleted", false},
          {"files", files}, {"operations", Json::array({
            {{"id", id + ":1"}, {"kind", "stage"}, {"state", "pending"}, {"files", files}}})}};
}
bool Recovered(XrdOfsPrepare &coordinator) {
  return Eventually([&] {
    Args empty("*", Prep_STAGE); XrdOucErrInfo error("test");
    const auto rc = coordinator.begin(empty.prep, error, nullptr);
    return rc == SFS_ERROR && error.getErrInfo() == EINVAL;
  });
}
}

TEST(PrepStore, AtomicRevisionShardingAndSingleWriter) {
  Temporary directory;
  std::string id = XrdOfsPrepStore::NewId();
  {
    XrdOfsPrepStore store(directory.path + "/state");
    EXPECT_THROW(XrdOfsPrepStore second(directory.path + "/state"), std::system_error);
    Json record = {{"schema", 1}, {"id", id}, {"value", "before"}};
    store.Save(record, true);
    EXPECT_EQ(store.Load(id)["revision"], 1);
    EXPECT_TRUE(fs::exists(store.Root() / "requests" / id.substr(0, 2) / id.substr(2, 2) / (id + ".json")));
    record["value"] = "after"; store.Save(record);
    EXPECT_EQ(store.Load(id)["revision"], 2);
    EXPECT_THROW(store.Save(record, true), std::system_error);
  }
  XrdOfsPrepStore reopened(directory.path + "/state");
  EXPECT_EQ(reopened.Load(id)["value"], "after");
}
TEST(PrepStore, RejectsCorruptionFutureVersionAndSymlinks) {
  Temporary directory; XrdOfsPrepStore store(directory.path);
  const auto id = store.NewId(); Json record = {{"schema", 1}, {"id", id}}; store.Save(record, true);
  auto path = store.Root() / "requests" / id.substr(0, 2) / id.substr(2, 2) / (id + ".json");
  std::ofstream(path) << "broken";
  EXPECT_THROW(store.Load(id), std::system_error);
  record["schema"] = 2; store.Save(record);
  EXPECT_THROW(store.Load(id), std::system_error);
  fs::remove(path); fs::create_symlink("/etc/passwd", path);
  EXPECT_THROW(store.Load(id), std::system_error);
  EXPECT_THROW(store.Load("../../etc/passwd"), std::system_error);
}
TEST(PrepProtocol, MetadataAndPaths) {
  EXPECT_EQ(Path("//a///b"), "/a/b");
  EXPECT_THROW(Path("/a/../b"), std::system_error);
  EXPECT_THROW(Path("/a\n/b"), std::system_error);
  EXPECT_EQ(Metadata({{"diskLifetime", "PT1H"}})["diskLifetime"], "PT1H");
  EXPECT_THROW(Metadata({{"diskLifetime", 5}}), std::system_error);
  EXPECT_EQ(Unhex(Hex("heterogeneous metadata")), "heterogeneous metadata");
}
TEST(PrepProtocol, RejectsInvalidPathsMetadataAndEncodings) {
  for (const std::string path : {"", "a", "/", "///", "/a/./b", "/a?b", "/a#b", "/a\177"})
    EXPECT_THROW(Path(path), std::system_error) << path;
  EXPECT_THROW(Path(std::string("/a\0b", 4)), std::system_error);
  EXPECT_EQ(Path("/" + std::string(1023, 'a')).size(), 1024u);
  EXPECT_THROW(Path("/" + std::string(1024, 'a')), std::system_error);
  for (const Json &value : {Json(), Json(1), Json("text"), Json::array()})
    EXPECT_THROW(Metadata(value), std::system_error) << value;
  EXPECT_THROW(Metadata({{"diskLifetime", ""}}), std::system_error);
  EXPECT_THROW(Metadata({{"targetedMetadata", Json::array()}}), std::system_error);
  EXPECT_THROW(Metadata({{"targetedMetadata", {{"large", std::string(MaxMetadata, 'a')}}}}), std::system_error);
  const Json metadata = {{"targetedMetadata", {{"site", "tape"}}}};
  EXPECT_EQ(Metadata(metadata), metadata);
  for (const std::string encoded : {"0", "gg", "FF", "0/"})
    EXPECT_THROW(Unhex(encoded), std::system_error) << encoded;
  EXPECT_THROW(Unhex(std::string(2 * MaxMetadata + 2, '0')), std::system_error);
  const std::string binary = std::string("\0\xff", 2) + std::string(MaxMetadata - 2, 'x');
  EXPECT_EQ(Unhex(Hex(binary)), binary);
  EXPECT_FALSE(IsId("00000000_0000-0000-0000-000000000000"));
  EXPECT_FALSE(IsId("AAAAAAAA-0000-0000-0000-000000000000"));
}
TEST(PrepStore, RejectsInvalidRootsAndShardSymlinks) {
  EXPECT_THROW(XrdOfsPrepStore("relative-state"), std::system_error);
  Temporary directory, outside;
  fs::create_directory_symlink(outside.path, fs::path(directory.path) / "link");
  EXPECT_THROW(XrdOfsPrepStore(directory.path + "/link"), std::system_error);
  XrdOfsPrepStore store(directory.path + "/state");
  const auto id = store.NewId();
  fs::create_directory_symlink(outside.path, store.Root() / "requests" / id.substr(0, 2));
  Json record = {{"schema", 1}, {"id", id}};
  EXPECT_THROW(store.Save(record, true), std::system_error);
  EXPECT_THROW(store.Load(id), std::system_error);
  EXPECT_TRUE(fs::is_empty(outside.path));
}
TEST(PrepStore, RejectsInvalidRecordsAndOversizeWrites) {
  Temporary directory; XrdOfsPrepStore store(directory.path);
  const auto id = store.NewId();
  Json record = {{"schema", 1}, {"id", id}};
  store.Save(record, true);
  const auto path = RecordPath(directory.path, id);
  for (const Json &invalid : {
         Json{{"schema", 1}, {"id", id}, {"revision", -1}},
         Json{{"schema", 1}, {"id", id}, {"revision", "1"}},
         Json{{"schema", 1}, {"id", store.NewId()}, {"revision", 1}},
         Json{{"id", id}, {"revision", 1}}, Json::array()}) {
    std::ofstream(path) << invalid.dump();
    EXPECT_THROW(store.Load(id), std::system_error) << invalid;
  }
  std::ofstream(path).close();
  EXPECT_THROW(store.Load(id), std::system_error);
  fs::resize_file(path, MaxRecord + 1);
  EXPECT_THROW(store.Load(id), std::system_error);
  store.Save(record);
  const auto saved = store.Load(id);
  record["large"] = std::string(MaxRecord, 'x');
  EXPECT_THROW(store.Save(record), std::system_error);
  EXPECT_EQ(store.Load(id), saved);
  store.Erase(id);
  EXPECT_THROW(store.Load(id), std::system_error);
  EXPECT_THROW(store.Erase(id), std::system_error);
}
TEST(PrepStore, RejectsFifoWithoutWaitingForAWriter) {
  Temporary directory; XrdOfsPrepStore store(directory.path);
  const auto id = store.NewId();
  Json record = {{"schema", 1}, {"id", id}};
  store.Save(record, true);
  const auto path = RecordPath(directory.path, id);
  fs::remove(path);
  ASSERT_EQ(mkfifo(path.c_str(), 0600), 0);
  // A child bounds the regression: a blocking open must fail, not hang the suite.
  const auto child = fork();
  ASSERT_GE(child, 0);
  if (!child) {
    try { store.Load(id); }
    catch (const std::system_error &ex) { _exit(ex.code().value() == EIO ? 0 : 1); }
    catch (...) { _exit(2); }
    _exit(3);
  }
  int status = 0;
  pid_t result = 0;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while ((result = waitpid(child, &status, WNOHANG)) == 0 &&
         std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  if (result == 0) {
    kill(child, SIGKILL);
    waitpid(child, &status, 0);
    FAIL() << "loading a FIFO blocked waiting for a writer";
  }
  ASSERT_EQ(result, child);
  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 0);
}
TEST(PrepStore, SyncFailuresPreserveAtomicPublicationAndCleanTemporaries) {
  Temporary directory; XrdOfsPrepStore store(directory.path);
  const auto id = store.NewId();
  Json record = {{"schema", 1}, {"id", id}, {"value", "before"}};
  store.Save(record, true);
  const auto shard = RecordPath(directory.path, id).parent_path();
  bool failFile = true;
  SyncHook hook([&](int fd) {
    struct stat status{};
    if (fstat(fd, &status)) throw std::runtime_error("fstat");
    if (S_ISREG(status.st_mode) == failFile) Fail(EIO, "injected fsync failure");
  });
  record["value"] = "after";
  EXPECT_THROW(store.Save(record), std::system_error);
  EXPECT_EQ(store.Load(id)["value"], "before");
  EXPECT_EQ(std::distance(fs::directory_iterator(shard), fs::directory_iterator()), 1);
  failFile = false;
  EXPECT_THROW(store.Save(record), std::system_error);
  // A failed directory sync is ambiguous: rename has already published the record.
  EXPECT_EQ(store.Load(id)["value"], "after");
  EXPECT_EQ(std::distance(fs::directory_iterator(shard), fs::directory_iterator()), 1);
  EXPECT_THROW(store.Erase(id), std::system_error);
  EXPECT_FALSE(fs::exists(RecordPath(directory.path, id)));
}
TEST(PrepPersist, AdmissionOwnershipSubsetAndTombstone) {
  Temporary directory; Backend backend; backend.SetHold(true);
  XrdSecEntity alice("unix"), bob("unix"); alice.name = const_cast<char *>("alice"); bob.name = const_cast<char *>("bob");
  XrdOfsPrepPersist coordinator(directory.path, "test", backend, MappedTestEnvironment(), nullptr);
  auto id = Submit(coordinator, alice, "authz=not-persisted");
  EXPECT_TRUE(IsId(id));
  ASSERT_TRUE(Eventually([&] { return Query(coordinator, id, alice)["files"][0]["state"] == "STARTED"; }));
  Args query(id, Prep_QUERY); XrdOucErrInfo error("test");
  EXPECT_EQ(coordinator.query(query.prep, error, &bob), SFS_ERROR);
  EXPECT_EQ(error.getErrInfo(), EACCES);
  Args invalid(id, Prep_CANCEL, {"/a", "/missing"});
  EXPECT_EQ(coordinator.cancel(invalid.prep, error, &alice), SFS_ERROR);
  EXPECT_EQ(error.getErrInfo(), EINVAL);
  EXPECT_EQ(Query(coordinator, id, alice)["files"][0]["state"], "STARTED");
  Args cancel(id, Prep_CANCEL, {"/a"});
  EXPECT_EQ(coordinator.cancel(cancel.prep, error, &alice), SFS_OK);
  ASSERT_TRUE(Eventually([&] { return Query(coordinator, id, alice)["files"][0]["state"] == "CANCELLED"; }));
  EXPECT_EQ(Query(coordinator, id, alice)["files"][1]["state"], "STARTED");
  Args erase(std::string(DeletePrefix) + id, Prep_CANCEL);
  EXPECT_EQ(coordinator.cancel(erase.prep, error, &alice), SFS_OK);
  EXPECT_EQ(coordinator.query(query.prep, error, &alice), SFS_ERROR);
  EXPECT_EQ(error.getErrInfo(), ENOENT);
  auto path = fs::path(directory.path) / "requests" / id.substr(0, 2) / id.substr(2, 2) / (id + ".json");
  std::ifstream stream(path);
  std::stringstream ss;
  ss << stream.rdbuf();
  std::string bytes = ss.str();
  EXPECT_EQ(bytes.find("not-persisted"), std::string::npos);
  EXPECT_TRUE(Json::parse(bytes)["deleted"].get<bool>());
}
TEST(PrepPersist, RestartAfterBackendAcceptedButReplyWasLost) {
  Temporary directory; Backend backend; backend.SetFailAfterAccept(true);
  XrdSecEntity alice("unix"); alice.name = const_cast<char *>("alice");
  std::string id;
  {
    XrdOfsPrepPersist coordinator(directory.path, "test", backend, MappedTestEnvironment(), nullptr);
    id = Submit(coordinator, alice);
    ASSERT_TRUE(Eventually([&] { return backend.GetStages() == 1; }));
  }
  {
    XrdOfsPrepPersist restarted(directory.path, "test", backend, MappedTestEnvironment(), nullptr);
    ASSERT_TRUE(Eventually([&] { return Query(restarted, id, alice)["files"][0]["state"] == "COMPLETED"; }));
  }
  EXPECT_EQ(backend.GetStages(), 1u);
}
TEST(PrepPersist, BackendOutageDoesNotLoseAcceptedIntent) {
  Temporary directory; Backend backend; backend.SetUnavailable(true);
  XrdSecEntity alice("unix"); alice.name = const_cast<char *>("alice"); std::string id;
  {
    XrdOfsPrepPersist coordinator(directory.path, "test", backend, MappedTestEnvironment(), nullptr);
    id = Submit(coordinator, alice);
    EXPECT_EQ(Query(coordinator, id, alice)["files"][0]["state"], "SUBMITTED");
  }
  backend.SetUnavailable(false);
  XrdOfsPrepPersist restarted(directory.path, "test", backend, MappedTestEnvironment(), nullptr);
  ASSERT_TRUE(Eventually([&] { return Query(restarted, id, alice)["files"][0]["state"] == "COMPLETED"; }));
}
TEST(PrepStore, RetryFsyncOnSubdirSyncFailure) {
  Temporary directory;
  int syncCalls = 0;
  bool failSync = false;
  XrdOfsPrepStore::SetSyncHook([&](int) {
    syncCalls++;
    if (failSync) throw std::system_error(make_error_code(std::errc::io_error), "injected sync failure");
  });
  std::string id = XrdOfsPrepStore::NewId();
  {
    XrdOfsPrepStore store(directory.path + "/state");
    failSync = true;
    syncCalls = 0;
    Json record = {{"schema", 1}, {"id", id}, {"value", "initial"}};
    EXPECT_THROW(store.Save(record, true), std::system_error);
    EXPECT_GT(syncCalls, 0);

    // Disable failure: the directory shards exist, but store must retry parent fsync
    failSync = false;
    syncCalls = 0;
    EXPECT_NO_THROW(store.Save(record, true));
    EXPECT_GT(syncCalls, 0);
    EXPECT_EQ(store.Load(id)["value"], "initial");

    // Saving another record in the same shard should not re-sync the parent directory
    std::string sameShardId = id.substr(0, 4) + XrdOfsPrepStore::NewId().substr(4);
    Json second = {{"schema", 1}, {"id", sameShardId}, {"value", "second"}};
    syncCalls = 0;
    EXPECT_NO_THROW(store.Save(second, true));
    EXPECT_EQ(store.Load(sameShardId)["value"], "second");
  }
  XrdOfsPrepStore::SetSyncHook(nullptr);
}
class RejectingBackend : public Backend {
public:
  int begin(XrdSfsPrep &, XrdOucErrInfo &error, const XrdSecEntity *) override {
    error.setErrInfo(ENOTSUP, "operation not supported");
    return SFS_ERROR;
  }
};
TEST(PrepPersist, PermanentBackendRejectionFailsRequest) {
  Temporary directory; RejectingBackend backend;
  XrdSecEntity alice("unix"); alice.name = const_cast<char *>("alice");
  XrdOfsPrepPersist coordinator(directory.path, "test", backend, MappedTestEnvironment(), nullptr);
  auto id = Submit(coordinator, alice);
  ASSERT_TRUE(Eventually([&] {
    auto status = Query(coordinator, id, alice);
    return status["files"][0]["state"] == "FAILED";
  }));
  auto status = Query(coordinator, id, alice);
  EXPECT_EQ(status["files"][0]["state"], "FAILED");
  EXPECT_EQ(status["files"][1]["state"], "FAILED");
  EXPECT_EQ(status["files"][0]["error"], "operation not supported");
  EXPECT_TRUE(status.contains("completedAt"));
}
class MutateRejectingBackend : public Backend {
public:
  bool rejectCancel = false;
  bool rejectEvict = false;

  void SetRejectCancel(bool v) {
    std::lock_guard<std::recursive_mutex> guard(mutex);
    rejectCancel = v;
  }
  void SetRejectEvict(bool v) {
    std::lock_guard<std::recursive_mutex> guard(mutex);
    rejectEvict = v;
  }
  int cancel(XrdSfsPrep &args, XrdOucErrInfo &error, const XrdSecEntity *e) override {
    std::lock_guard<std::recursive_mutex> guard(mutex);
    if (rejectCancel) {
      error.setErrInfo(ENOTSUP, "cancel not supported");
      return SFS_ERROR;
    }
    return Backend::cancel(args, error, e);
  }
  int begin(XrdSfsPrep &args, XrdOucErrInfo &error, const XrdSecEntity *e) override {
    std::lock_guard<std::recursive_mutex> guard(mutex);
    if ((args.opts & Prep_EVICT) && rejectEvict) {
      error.setErrInfo(ENOTSUP, "evict not supported");
      return SFS_ERROR;
    }
    return Backend::begin(args, error, e);
  }
};
TEST(PrepPersist, PermanentCancelAndReleaseRejectionAllowsRetry) {
  Temporary directory; MutateRejectingBackend backend; backend.SetHold(true);
  XrdSecEntity alice("unix"); alice.name = const_cast<char *>("alice");
  XrdOfsPrepPersist coordinator(directory.path, "test", backend, MappedTestEnvironment(), nullptr);
  auto id = Submit(coordinator, alice);
  ASSERT_TRUE(Eventually([&] { return Query(coordinator, id, alice)["files"][0]["state"] == "STARTED"; }));

  // Reject cancel with ENOTSUP
  backend.SetRejectCancel(true);
  Args cancelA(id, Prep_CANCEL, {"/a"});
  XrdOucErrInfo error("test");
  EXPECT_EQ(coordinator.cancel(cancelA.prep, error, &alice), SFS_OK);
  ASSERT_TRUE(Eventually([&] {
    auto status = Query(coordinator, id, alice);
    return status["files"][0].contains("cancelError");
  }));
  auto status = Query(coordinator, id, alice);
  EXPECT_EQ(status["files"][0]["state"], "STARTED");
  EXPECT_EQ(status["files"][0]["cancelError"], "cancel not supported");

  // Fix backend and retry cancel: cancelRequested was cleared, so retry is accepted
  backend.SetRejectCancel(false);
  EXPECT_EQ(coordinator.cancel(cancelA.prep, error, &alice), SFS_OK);
  ASSERT_TRUE(Eventually([&] {
    return Query(coordinator, id, alice)["files"][0]["state"] == "CANCELLED";
  }));
  status = Query(coordinator, id, alice);
  EXPECT_EQ(status["files"][0]["state"], "CANCELLED");
  EXPECT_FALSE(status["files"][0].contains("cancelError"));

  // Now test evict on /b: first let /b complete
  backend.SetHold(false);
  ASSERT_TRUE(Eventually([&] {
    return Query(coordinator, id, alice)["files"][1]["state"] == "COMPLETED";
  }));

  // Reject evict with ENOTSUP
  backend.SetRejectEvict(true);
  Args evictB("*", Prep_EVICT, {"/b"}, "xrd.prepare.request=" + id);
  EXPECT_EQ(coordinator.begin(evictB.prep, error, &alice), SFS_OK);
  ASSERT_TRUE(Eventually([&] {
    auto q = Query(coordinator, id, alice);
    return q["files"][1].contains("releaseError");
  }));
  status = Query(coordinator, id, alice);
  EXPECT_EQ(status["files"][1]["state"], "COMPLETED");
  EXPECT_EQ(status["files"][1]["releaseError"], "evict not supported");

  // Fix backend and retry evict
  backend.SetRejectEvict(false);
  EXPECT_EQ(coordinator.begin(evictB.prep, error, &alice), SFS_OK);
  ASSERT_TRUE(Eventually([&] {
    auto q = Query(coordinator, id, alice);
    return !q["files"][1].contains("releaseError");
  }));
}
class QueryMutateRejectingBackend : public Backend {
public:
  std::map<std::string, std::string> cancelErrors;
  std::map<std::string, std::string> releaseErrors;
  std::set<std::string> blockedAcks;

  void SetCancelError(const std::string &path, const std::string &err) {
    std::lock_guard<std::recursive_mutex> guard(mutex);
    if (err.empty()) cancelErrors.erase(path);
    else cancelErrors[path] = err;
  }
  void ClearCancelError(const std::string &path) {
    std::lock_guard<std::recursive_mutex> guard(mutex);
    cancelErrors.erase(path);
  }
  void SetReleaseError(const std::string &path, const std::string &err) {
    std::lock_guard<std::recursive_mutex> guard(mutex);
    if (err.empty()) releaseErrors.erase(path);
    else releaseErrors[path] = err;
  }
  void ClearReleaseError(const std::string &path) {
    std::lock_guard<std::recursive_mutex> guard(mutex);
    releaseErrors.erase(path);
  }
  void BlockAck(const std::string &ack) {
    std::lock_guard<std::recursive_mutex> guard(mutex);
    blockedAcks.insert(ack);
  }
  void UnblockAck(const std::string &ack) {
    std::lock_guard<std::recursive_mutex> guard(mutex);
    blockedAcks.erase(ack);
  }

  int cancel(XrdSfsPrep &args, XrdOucErrInfo &error, const XrdSecEntity *) override {
    std::lock_guard<std::recursive_mutex> guard(mutex);
    XrdOucEnv env(args.oinfo ? args.oinfo->text : nullptr);
    const std::string op = env.Get("xrd.prepare.operation") ? env.Get("xrd.prepare.operation") : "";
    if (op.empty() || env.Get("authz")) { error.setErrInfo(EINVAL, "unsafe operation"); return SFS_ERROR; }
    auto &record = records[args.reqid];
    if (record.is_null()) record = {{"schema", 1}, {"requestId", args.reqid}, {"known", true}, {"acknowledged", Json::array()}, {"files", Json::array()}};
    for (const auto &ack : record["acknowledged"]) if (ack == op) return SFS_OK;
    if (!asyncCancel) {
      for (auto *path = args.paths; path; path = path->next) {
        if (!cancelErrors.count(path->text)) {
          for (auto &file : record["files"])
            if (file["path"] == path->text && !Terminal(file["state"])) file["state"] = "CANCELLED";
        }
      }
    }
    record["acknowledged"].push_back(op);
    return SFS_OK;
  }

  int query(XrdSfsPrep &args, XrdOucErrInfo &error, const XrdSecEntity *) override {
    std::lock_guard<std::recursive_mutex> guard(mutex);
    if (unavailable) { error.setErrInfo(EIO, "offline"); return SFS_ERROR; }
    Json response = {{"schema", 1}, {"requestId", args.reqid}, {"known", false},
                     {"acknowledged", Json::array()}, {"files", Json::array()}};
    auto it = records.find(args.reqid);
    if (it != records.end()) {
      response = *it;
      if (!hold) {
        for (auto &file : response["files"]) {
          if (file["state"] == "STARTED") file["state"] = "COMPLETED";
        }
      }
      for (auto &file : response["files"]) {
        const auto p = file["path"].get<std::string>();
        auto cit = cancelErrors.find(p);
        if (cit != cancelErrors.end() && !cit->second.empty()) file["cancelError"] = cit->second;
        auto rit = releaseErrors.find(p);
        if (rit != releaseErrors.end() && !rit->second.empty()) file["releaseError"] = rit->second;
      }
      Json filteredAcks = Json::array();
      for (const auto &ack : response["acknowledged"]) {
        if (!blockedAcks.count(ack.get<std::string>())) filteredAcks.push_back(ack);
      }
      response["acknowledged"] = filteredAcks;
    }
    return Reply(error, response);
  }
};
TEST(PrepPersist, QueryReportedCancelAndReleaseRejectionAllowsRetry) {
  Temporary directory; QueryMutateRejectingBackend backend; backend.SetHold(true);
  XrdSecEntity alice("unix"); alice.name = const_cast<char *>("alice");
  XrdOfsPrepPersist coordinator(directory.path, "test", backend, MappedTestEnvironment(), nullptr);
  auto id = Submit(coordinator, alice);
  ASSERT_TRUE(Eventually([&] {
    auto status = Query(coordinator, id, alice);
    return status["files"][0]["state"] == "STARTED" && status["files"][1]["state"] == "STARTED";
  }));

  auto loadRecord = [&](const std::string &recordId) {
    return LoadRecord(directory.path, recordId);
  };
  XrdOucErrInfo error("test");

  // --- Test Cancel query rejection and retry ---
  // /a is in STARTED state.
  // 1. Initial cancel attempt R:2 fails via query
  backend.SetCancelError("/a", "drive busy");
  Args cancelA(id, Prep_CANCEL, {"/a"});
  EXPECT_EQ(coordinator.cancel(cancelA.prep, error, &alice), SFS_OK);

  ASSERT_TRUE(Eventually([&] {
    auto q = Query(coordinator, id, alice);
    return q["files"][0].value("cancelError", "") == "drive busy";
  }));
  EXPECT_EQ(loadRecord(id)["operations"].size(), 2);
  EXPECT_EQ(loadRecord(id)["operations"][1]["state"], "failed");

  // 2. Client retries cancel on /a, creating R:3
  const std::string op3 = id + ":3";
  backend.BlockAck(op3);
  // Deliberately KEEP backend reporting R:2's failure on /a
  EXPECT_EQ(coordinator.cancel(cancelA.prep, error, &alice), SFS_OK);
  EXPECT_EQ(loadRecord(id)["operations"].size(), 3);

  // Wait for R:3 to be dispatched
  ASSERT_TRUE(Eventually([&] {
    return loadRecord(id)["operations"][2]["state"] == "dispatched";
  }));

  // Assert that observing R:2's error in query does NOT erase R:3's cancelRequested intent
  // and does NOT restore the old error
  auto midRecord = loadRecord(id);
  EXPECT_TRUE(midRecord["files"][0].value("cancelRequested", false));
  EXPECT_FALSE(midRecord["files"][0].contains("cancelError"));

  // 3. Now backend acknowledges R:3 as successful and stops reporting cancelError
  backend.ClearCancelError("/a");
  backend.SetFileState(id, "/a", "CANCELLED");
  backend.UnblockAck(op3);

  // Assert that success marks file CANCELLED and clears cancelRequested
  ASSERT_TRUE(Eventually([&] {
    auto q = Query(coordinator, id, alice);
    auto rec = loadRecord(id);
    return q["files"][0]["state"] == "CANCELLED" &&
           !q["files"][0].contains("cancelError") &&
           !rec["files"][0].value("cancelRequested", false) &&
           rec["operations"][2]["state"] == "done";
  }));

  // 4. Assert that repeating the completed cancel mutation does NOT create another operation
  EXPECT_EQ(coordinator.cancel(cancelA.prep, error, &alice), SFS_OK);
  EXPECT_EQ(loadRecord(id)["operations"].size(), 3);

  // --- Test Evict (Release) query rejection and retry ---
  // Now test on /b: first let /b complete
  backend.SetHold(false);
  ASSERT_TRUE(Eventually([&] {
    return Query(coordinator, id, alice)["files"][1]["state"] == "COMPLETED";
  }));

  // 1. Initial evict attempt R:4 fails via query
  backend.SetReleaseError("/b", "tape offline");
  Args evictB("*", Prep_EVICT, {"/b"}, "xrd.prepare.request=" + id);
  EXPECT_EQ(coordinator.begin(evictB.prep, error, &alice), SFS_OK);

  // Wait for R:4 to be acknowledged and report releaseError
  ASSERT_TRUE(Eventually([&] {
    auto q = Query(coordinator, id, alice);
    return q["files"][1].value("releaseError", "") == "tape offline";
  }));
  EXPECT_EQ(loadRecord(id)["operations"].size(), 4);
  EXPECT_EQ(loadRecord(id)["operations"][3]["state"], "failed");

  // 2. Client retries evict on /b, creating R:5
  const std::string op5 = id + ":5";
  backend.BlockAck(op5);
  // Deliberately KEEP backend reporting R:4's failure on /b
  EXPECT_EQ(coordinator.begin(evictB.prep, error, &alice), SFS_OK);
  EXPECT_EQ(loadRecord(id)["operations"].size(), 5);
  EXPECT_EQ(loadRecord(id)["operations"][4]["state"], "pending");

  // Wait for R:5 to be dispatched
  ASSERT_TRUE(Eventually([&] {
    return loadRecord(id)["operations"][4]["state"] == "dispatched";
  }));

  // Assert that observing R:4's error in query does NOT erase R:5's releaseRequested intent
  // and does NOT restore the old error
  midRecord = loadRecord(id);
  EXPECT_TRUE(midRecord["files"][1].value("releaseRequested", false));
  EXPECT_FALSE(midRecord["files"][1].contains("releaseError"));

  // 3. Now backend acknowledges R:5 as successful and stops reporting releaseError
  backend.ClearReleaseError("/b");
  backend.UnblockAck(op5);

  // Assert that success clears the current public error and clears releaseRequested
  ASSERT_TRUE(Eventually([&] {
    auto q = Query(coordinator, id, alice);
    auto rec = loadRecord(id);
    return !q["files"][1].contains("releaseError") &&
           !rec["files"][1].value("releaseRequested", false) &&
           rec["files"][1].value("released", false) &&
           rec["operations"][4]["state"] == "done";
  }));

  // 4. Assert that repeating the completed release mutation does NOT create another operation
  EXPECT_EQ(coordinator.begin(evictB.prep, error, &alice), SFS_OK);
  EXPECT_EQ(loadRecord(id)["operations"].size(), 5);
}
TEST(PrepPersist, CancellationAfterCompletionPreservesStateAndTimestamp) {
  Temporary directory; Backend backend; backend.SetHold(false);
  XrdSecEntity alice("unix"); alice.name = const_cast<char *>("alice");
  XrdOfsPrepPersist coordinator(directory.path, "test", backend, MappedTestEnvironment(), nullptr);
  auto id = Submit(coordinator, alice);
  ASSERT_TRUE(Eventually([&] {
    auto q = Query(coordinator, id, alice);
    return q["files"][0]["state"] == "COMPLETED" && q["files"][1]["state"] == "COMPLETED";
  }));
  auto before = Query(coordinator, id, alice);
  const auto initialFinishedAt = before["files"][0]["finishedAt"].get<uint64_t>();
  EXPECT_EQ(before["files"][0]["state"], "COMPLETED");

  Args cancelA(id, Prep_CANCEL, {"/a"});
  XrdOucErrInfo error("test");
  EXPECT_EQ(coordinator.cancel(cancelA.prep, error, &alice), SFS_OK);

  ASSERT_TRUE(Eventually([&] {
    auto rec = LoadRecord(directory.path, id);
    return rec["operations"].size() >= 2 && rec["operations"][1]["state"] == "done";
  }));
  auto after = Query(coordinator, id, alice);
  EXPECT_EQ(after["files"][0]["state"], "COMPLETED");
  EXPECT_EQ(after["files"][0]["finishedAt"].get<uint64_t>(), initialFinishedAt);
  EXPECT_FALSE(after["files"][0].contains("cancelError"));

  auto rec = LoadRecord(directory.path, id);
  EXPECT_TRUE(rec["files"][0].value("cancelAcknowledged", false));
  EXPECT_FALSE(rec["files"][0].value("cancelRequested", false));

  EXPECT_EQ(coordinator.cancel(cancelA.prep, error, &alice), SFS_OK);
  EXPECT_EQ(LoadRecord(directory.path, id)["operations"].size(), 2);
}
TEST(PrepPersist, CancellationAfterFailurePreservesStateAndTimestamp) {
  Temporary directory; Backend backend; backend.SetHold(true);
  XrdSecEntity alice("unix"); alice.name = const_cast<char *>("alice");
  XrdOfsPrepPersist coordinator(directory.path, "test", backend, MappedTestEnvironment(), nullptr);
  auto id = Submit(coordinator, alice);
  ASSERT_TRUE(Eventually([&] {
    auto q = Query(coordinator, id, alice);
    return q["files"][0]["state"] == "STARTED";
  }));

  backend.SetFileState(id, "/a", "FAILED", 12345, "tape read error");

  ASSERT_TRUE(Eventually([&] {
    auto q = Query(coordinator, id, alice);
    return q["files"][0]["state"] == "FAILED" &&
           q["files"][0].value("error", "") == "tape read error";
  }));
  auto before = Query(coordinator, id, alice);
  const auto initialFinishedAt = before["files"][0]["finishedAt"].get<uint64_t>();

  Args cancelA(id, Prep_CANCEL, {"/a"});
  XrdOucErrInfo error("test");
  EXPECT_EQ(coordinator.cancel(cancelA.prep, error, &alice), SFS_OK);

  ASSERT_TRUE(Eventually([&] {
    auto rec = LoadRecord(directory.path, id);
    return rec["operations"].size() >= 2 && rec["operations"][1]["state"] == "done";
  }));
  auto after = Query(coordinator, id, alice);
  EXPECT_EQ(after["files"][0]["state"], "FAILED");
  EXPECT_EQ(after["files"][0]["error"], "tape read error");
  EXPECT_EQ(after["files"][0]["finishedAt"].get<uint64_t>(), initialFinishedAt);

  auto rec = LoadRecord(directory.path, id);
  EXPECT_TRUE(rec["files"][0].value("cancelAcknowledged", false));
  EXPECT_FALSE(rec["files"][0].value("cancelRequested", false));

  EXPECT_EQ(coordinator.cancel(cancelA.prep, error, &alice), SFS_OK);
  EXPECT_EQ(LoadRecord(directory.path, id)["operations"].size(), 2);
}
TEST(PrepPersist, CompletionRacingWithCancellationPreservesAuthoritativeState) {
  Temporary directory; QueryMutateRejectingBackend backend;
  backend.SetHold(true);
  backend.SetAsyncCancel(true);
  XrdSecEntity alice("unix"); alice.name = const_cast<char *>("alice");
  XrdOfsPrepPersist coordinator(directory.path, "test", backend, MappedTestEnvironment(), nullptr);
  auto id = Submit(coordinator, alice);
  ASSERT_TRUE(Eventually([&] {
    auto q = Query(coordinator, id, alice);
    return q["files"][0]["state"] == "STARTED";
  }));

  const std::string cancelOp = id + ":2";
  backend.BlockAck(cancelOp);

  Args cancelA(id, Prep_CANCEL, {"/a"});
  XrdOucErrInfo error("test");
  EXPECT_EQ(coordinator.cancel(cancelA.prep, error, &alice), SFS_OK);

  ASSERT_TRUE(Eventually([&] {
    auto rec = LoadRecord(directory.path, id);
    return rec["operations"].size() >= 2 && rec["operations"][1]["state"] == "dispatched";
  }));

  backend.SetFileState(id, "/a", "COMPLETED", 54321);
  backend.UnblockAck(cancelOp);

  ASSERT_TRUE(Eventually([&] {
    auto rec = LoadRecord(directory.path, id);
    return rec["operations"][1]["state"] == "done";
  }));

  auto status = Query(coordinator, id, alice);
  EXPECT_EQ(status["files"][0]["state"], "COMPLETED");
  EXPECT_EQ(status["files"][0]["finishedAt"].get<uint64_t>(), 54321u);

  auto rec = LoadRecord(directory.path, id);
  EXPECT_TRUE(rec["files"][0].value("cancelAcknowledged", false));
  EXPECT_EQ(coordinator.cancel(cancelA.prep, error, &alice), SFS_OK);
  EXPECT_EQ(LoadRecord(directory.path, id)["operations"].size(), 2);
}
class MockAuthorizer : public XrdAccAuthorize {
public:
  XrdAccPrivs Access(const XrdSecEntity *entity, const char *, Access_Operation,
                     XrdOucEnv *env) override {
    const char *token = env ? env->Get("authz") : nullptr;
    if (token) {
      std::string sub = token;
      for (const char *prefix : {"Bearer%20", "Bearer "})
        if (sub.compare(0, std::strlen(prefix), prefix) == 0) sub.erase(0, std::strlen(prefix));
      if (sub == "alice" || sub == "bob") {
        entity->eaAPI->Add("token.subject", sub, true);
        entity->eaAPI->Add("token.issuer", "test-issuer", true);
        return XrdAccPriv_All;
      }
    }
    return XrdAccPriv_None;
  }
  int Audit(int, const XrdSecEntity *, const char *, Access_Operation, XrdOucEnv *) override { return 1; }
  int Test(XrdAccPrivs privileges, Access_Operation) override { return privileges != XrdAccPriv_None; }
};
// Note: In native XRootD wire protocol (XrdXrootdProtocol), ID-only query
// (xrdfs query prepare <id>) carries no paths and thus no per-path opaque info
// over the wire; it relies on session authentication (XrdSecEntity). Per-file
// CGI is transported and authenticated with file paths (during stage, query with
// paths, subset cancel, and evict). The unit test below exercises the coordinator's
// handling of both request-level fallback CGI and per-path CGI mappings.
TEST(PrepPersist, NativeCgiAuthenticationPreserved) {
  Temporary directory; Backend backend; backend.SetHold(true);
  MockAuthorizer authorizer;
  XrdOucEnv env;
  env.PutPtr("XrdAccAuthorize*", &authorizer);
  XrdSecEntity client("unix"); client.name = const_cast<char *>("alice");
  XrdOfsPrepPersist coordinator(directory.path, "test", backend, &env, nullptr);

  std::string id = Submit(coordinator, client, "authz=alice");
  EXPECT_TRUE(IsId(id));

  // Native query without paths: args.paths is null, args.oinfo carries "authz=alice"
  Args queryAlice(id, Prep_QUERY, {}, "authz=alice");
  XrdOucErrInfo error("test");
  ASSERT_TRUE(Eventually([&] {
    if (coordinator.query(queryAlice.prep, error, &client) != SFS_DATA) return false;
    auto qres = Json::parse(Text(error));
    return qres["files"].size() == 2u && qres["files"][0]["state"] == "STARTED";
  }));

  // Query with wrong token is denied
  Args queryBob(id, Prep_QUERY, {}, "authz=bob");
  EXPECT_EQ(coordinator.query(queryBob.prep, error, &client), SFS_ERROR);
  EXPECT_EQ(error.getErrInfo(), EACCES);

  // Query without any token is denied
  Args queryAnon(id, Prep_QUERY);
  EXPECT_EQ(coordinator.query(queryAnon.prep, error, &client), SFS_ERROR);
  EXPECT_EQ(error.getErrInfo(), EACCES);

  // Subset cancel on /b only with correct CGI
  Args cancelB(id, Prep_CANCEL, {"/b"}, "authz=alice");
  EXPECT_EQ(coordinator.cancel(cancelB.prep, error, &client), SFS_OK);
  ASSERT_TRUE(Eventually([&] {
    return coordinator.query(queryAlice.prep, error, &client) == SFS_DATA &&
           Json::parse(Text(error))["files"][1]["state"] == "CANCELLED";
  }));

  // Subset cancel on /a with wrong CGI is denied
  Args cancelAWrong(id, Prep_CANCEL, {"/a"}, "authz=bob");
  EXPECT_EQ(coordinator.cancel(cancelAWrong.prep, error, &client), SFS_ERROR);
  EXPECT_EQ(error.getErrInfo(), EACCES);
}

TEST(PrepPersist, RejectsSubsetDeletionWithoutHidingTheRequest) {
  Temporary directory; Backend backend; backend.SetUnavailable(true);
  XrdSecEntity alice("unix"); alice.name = const_cast<char *>("alice");
  XrdOfsPrepPersist coordinator(directory.path, "test", backend, MappedTestEnvironment(), nullptr);
  const auto id = Submit(coordinator, alice);
  const auto before = LoadRecord(directory.path, id);
  Args erase(std::string(DeletePrefix) + id, Prep_CANCEL, {"/a"});
  XrdOucErrInfo error("test");
  EXPECT_EQ(coordinator.cancel(erase.prep, error, &alice), SFS_ERROR);
  EXPECT_EQ(error.getErrInfo(), EINVAL);
  EXPECT_EQ(LoadRecord(directory.path, id), before);
  EXPECT_EQ(Query(coordinator, id, alice)["files"].size(), 2u);
}

TEST(PrepPersist, CancellationBeforeDispatchStillExecutesPendingEviction) {
  Temporary directory; RejectingBackend backend;
  const auto id = XrdOfsPrepStore::NewId();
  auto record = PendingRecord(id);
  for (auto &file : record["files"]) file["cancelRequested"] = true;
  record["files"][0]["releaseRequested"] = true;
  record["operations"].push_back({{"id", id + ":2"}, {"kind", "evict"},
    {"state", "pending"}, {"files", Json::array({record["files"][0]})}});
  record["operations"].push_back({{"id", id + ":3"}, {"kind", "cancel"},
    {"state", "pending"}, {"files", record["files"]}});
  { XrdOfsPrepStore store(directory.path); store.Save(record, true); }
  XrdSecEntity alice("unix"); alice.name = const_cast<char *>("alice");
  XrdOfsPrepPersist coordinator(directory.path, "test", backend, MappedTestEnvironment(), nullptr);
  ASSERT_TRUE(Eventually([&] {
    return Query(coordinator, id, alice)["files"][0].contains("releaseError");
  }));
  record = LoadRecord(directory.path, id);
  EXPECT_EQ(record["files"][0]["state"], "CANCELLED");
  EXPECT_EQ(record["files"][1]["state"], "CANCELLED");
  EXPECT_FALSE(record["files"][0].value("releaseRequested", false));
  EXPECT_EQ(record["operations"][0]["state"], "done");
  EXPECT_EQ(record["operations"][1]["state"], "failed");
  EXPECT_EQ(record["operations"][2]["state"], "done");
  EXPECT_EQ(backend.GetStages(), 0u);
}

TEST(PrepPersist, ReconcilesPublishedMutationAfterDirectorySyncFailure) {
  Temporary directory; RejectingBackend backend;
  const auto id = XrdOfsPrepStore::NewId();
  auto record = PendingRecord(id);
  for (auto &file : record["files"]) file["state"] = "COMPLETED";
  record["operations"][0]["state"] = "done";
  record["completedAt"] = XrdOfsPrepStore::Now();
  { XrdOfsPrepStore store(directory.path); store.Save(record, true); }
  struct stat shard{};
  ASSERT_EQ(stat(RecordPath(directory.path, id).parent_path().c_str(), &shard), 0);
  std::atomic<bool> inject{false};
  // Install before starting the worker, and remove only after it has joined.
  SyncHook hook([&](int fd) {
    struct stat status{};
    if (fstat(fd, &status)) throw std::runtime_error("fstat");
    if (status.st_dev == shard.st_dev && status.st_ino == shard.st_ino && inject.exchange(false))
      Fail(EIO, "injected post-rename fsync failure");
  });
  XrdSecEntity alice("unix"); alice.name = const_cast<char *>("alice");
  XrdOfsPrepPersist coordinator(directory.path, "test", backend, MappedTestEnvironment(), nullptr);
  // A completed recovered record has no active queue entry.
  ASSERT_TRUE(Recovered(coordinator));
  inject = true;
  Args evict("*", Prep_EVICT, {"/a"}, "xrd.prepare.request=" + id);
  XrdOucErrInfo error("test");
  EXPECT_EQ(coordinator.begin(evict.prep, error, &alice), SFS_ERROR);
  EXPECT_EQ(error.getErrInfo(), EIO);
  EXPECT_FALSE(inject.load());
  ASSERT_TRUE(Eventually([&] {
    return Query(coordinator, id, alice)["files"][0].contains("releaseError");
  }));
  EXPECT_EQ(LoadRecord(directory.path, id)["operations"][1]["state"], "failed");
}

TEST(PrepPersist, ReconcilesFailedAdmissionOnlyWhenPublished) {
  for (bool beforePublication : {true, false}) {
    SCOPED_TRACE(beforePublication);
    Temporary directory; Backend backend;
    std::atomic<unsigned> phase{0};
    SyncHook hook([&](int fd) {
      struct stat status{};
      if (fstat(fd, &status)) throw std::runtime_error("fstat");
      if (phase == 1 && S_ISREG(status.st_mode)) {
        phase = beforePublication ? 3 : 2;
        if (beforePublication) Fail(EIO, "injected file fsync failure");
      } else if (phase == 2 && S_ISDIR(status.st_mode)) {
        phase = 3;
        Fail(EIO, "injected publication fsync failure");
      }
    });
    XrdSecEntity alice("unix"); alice.name = const_cast<char *>("alice");
    XrdOfsPrepPersist coordinator(directory.path, "test", backend, MappedTestEnvironment(), nullptr, 1);
    ASSERT_TRUE(Recovered(coordinator));
    Args args("*", Prep_STAGE, {"/a"}); XrdOucErrInfo error("test");
    phase = 1;
    EXPECT_EQ(coordinator.begin(args.prep, error, &alice), SFS_ERROR);
    EXPECT_EQ(error.getErrInfo(), EIO);
    EXPECT_EQ(phase.load(), 3u);
    std::string id;
    for (const auto &entry : fs::recursive_directory_iterator(directory.path))
      if (entry.path().extension() == ".json") id = entry.path().stem().string();
    if (beforePublication) {
      EXPECT_TRUE(id.empty());
      EXPECT_EQ(backend.GetStages(), 0u);
      // A failed unpublished write must release its sole admission slot.
      ASSERT_EQ(coordinator.begin(args.prep, error, &alice), SFS_DATA);
      id = Text(error);
    } else {
      ASSERT_TRUE(IsId(id));
    }
    ASSERT_TRUE(Eventually([&] {
      return Query(coordinator, id, alice)["files"][0]["state"] == "COMPLETED";
    }));
    EXPECT_EQ(backend.GetStages(), 1u);
  }
}

TEST(PrepPersist, RejectsInvalidAdmissionWithoutDispatch) {
  Temporary directory; Backend backend; backend.SetUnavailable(true);
  XrdSecEntity alice("unix"); alice.name = const_cast<char *>("alice");
  XrdOfsPrepPersist coordinator(directory.path, "test", backend, MappedTestEnvironment(), nullptr);
  Submit(coordinator, alice); // Wait for recovery before testing admission errors.
  XrdOucErrInfo error("test");
  for (const auto &value : {Json(), Json::array(), Json(17), Json("text")}) {
    Args args("*", Prep_STAGE, {"/a"}, "xrd.prepare.file=" + Hex(value.dump()));
    EXPECT_EQ(coordinator.begin(args.prep, error, &alice), SFS_ERROR);
    EXPECT_EQ(error.getErrInfo(), EINVAL);
  }
  Args duplicate("*", Prep_STAGE, {"/a", "//a/"});
  EXPECT_EQ(coordinator.begin(duplicate.prep, error, &alice), SFS_ERROR);
  EXPECT_EQ(error.getErrInfo(), EINVAL);
  Args empty("*", Prep_STAGE);
  EXPECT_EQ(coordinator.begin(empty.prep, error, &alice), SFS_ERROR);
  EXPECT_EQ(error.getErrInfo(), EINVAL);
  Args unsupported("*", Prep_STAGE | Prep_FRESH, {"/a"});
  EXPECT_EQ(coordinator.begin(unsupported.prep, error, &alice), SFS_ERROR);
  EXPECT_EQ(error.getErrInfo(), ENOTSUP);
  Args anonymous("*", Prep_STAGE, {"/a"});
  EXPECT_EQ(coordinator.begin(anonymous.prep, error, nullptr), SFS_ERROR);
  EXPECT_EQ(error.getErrInfo(), EACCES);
  EXPECT_EQ(backend.GetStages(), 0u);
}

class ArchiveBackend : public Backend {
public:
  std::string suffix;
  Json response = {{"schema", 1}, {"requestId", ArchiveQuery}, {"known", true},
                   {"acknowledged", Json::array()}, {"files", Json::array({
                     {{"path", "/a"}, {"locality", "DISK_AND_TAPE"}}})}};
  int query(XrdSfsPrep &, XrdOucErrInfo &error, const XrdSecEntity *) override {
    return Reply(error, response, suffix);
  }
};
TEST(PrepPersist, ValidatesArchiveBackendResponses) {
  Temporary directory; ArchiveBackend backend;
  XrdSecEntity alice("unix"); alice.name = const_cast<char *>("alice");
  XrdOfsPrepPersist coordinator(directory.path, "test", backend, MappedTestEnvironment(), nullptr);
  Args args(ArchiveQuery, Prep_QUERY, {"/a"}); XrdOucErrInfo error("test");
  ASSERT_EQ(coordinator.query(args.prep, error, &alice), SFS_DATA);
  EXPECT_EQ(Json::parse(Text(error)), backend.response["files"]);
  const auto valid = backend.response;
  backend.suffix = std::string("\0junk", 5);
  EXPECT_EQ(coordinator.query(args.prep, error, &alice), SFS_ERROR);
  EXPECT_EQ(error.getErrInfo(), EIO);
  backend.suffix.clear();
  const std::vector<std::function<void(Json &)>> corruptions = {
    [](Json &r) { r["schema"] = 2; },
    [](Json &r) { r["requestId"] = "other"; },
    [](Json &r) { r["known"] = "yes"; },
    [](Json &r) { r["known"] = false; }, // Unknown cannot include files.
    [](Json &r) { r["acknowledged"] = Json::object(); },
    [](Json &r) { r["acknowledged"] = Json::array({7}); },
    [](Json &r) { r["files"] = Json::object(); },
    [](Json &r) { r["files"] = Json::array(); },
    [](Json &r) { r["files"].push_back(r["files"][0]); },
    [](Json &r) { r["files"][0]["path"] = "/other"; },
    [](Json &r) { r["files"][0]["locality"] = "UNKNOWN"; },
    [](Json &r) { r["files"][0].erase("locality"); },
    [](Json &r) { r["files"][0]["error"] = 7; },
    [](Json &r) { r["files"][0]["cancelError"] = std::string(2049, 'x'); },
    [](Json &r) { r["files"][0]["releaseError"] = false; },
    [](Json &r) { r["files"][0]["startedAt"] = -1; },
    [](Json &r) { r["files"][0]["finishedAt"] = "later"; }
  };
  for (size_t i = 0; i < corruptions.size(); ++i) {
    SCOPED_TRACE(i);
    backend.response = valid;
    corruptions[i](backend.response);
    EXPECT_EQ(coordinator.query(args.prep, error, &alice), SFS_ERROR);
    EXPECT_EQ(error.getErrInfo(), EIO);
  }
  backend.response = valid;
  backend.response["known"] = false;
  backend.response["files"] = Json::array();
  EXPECT_EQ(coordinator.query(args.prep, error, &alice), SFS_ERROR);
  EXPECT_EQ(error.getErrInfo(), ENOTSUP);
}

TEST(PrepPersistEntry, RejectsInvalidConfiguration) {
  Temporary directory; Backend backend;
  XrdOucEnv runtime, config; config.PutPtr("xrdEnv*", &runtime);
  const auto root = directory.path + "/state";
  const auto valid = root + " test replay-safe";
  EXPECT_EQ(XrdOfsAddPrepare(nullptr, nullptr, nullptr, nullptr, nullptr, &config, &backend), nullptr);
  const std::vector<std::string> invalidOptions = {
         "", root, root + " test", root + " test unsafe",
         "relative test replay-safe", valid + " unknown=1", valid + " maxrequests",
         valid + " maxrequests=", valid + " maxrequests=-1", valid + " maxrequests=+1",
         valid + " maxrequests=1x", valid + " maxrequests=0", valid + " maxrequests=1000001",
         valid + " retention=0", valid + " retention=31536001",
         valid + " retention=99999999999999999999999999999999"};
  for (const auto &parms : invalidOptions) {
    SCOPED_TRACE(parms);
    std::unique_ptr<XrdOfsPrepare> wrapper(
      XrdOfsAddPrepare(nullptr, nullptr, parms.c_str(), nullptr, nullptr, &config, &backend));
    EXPECT_EQ(wrapper, nullptr);
    EXPECT_EQ(runtime.Get("xrd.prepare.profile"), nullptr);
  }
  EXPECT_EQ(XrdOfsAddPrepare(nullptr, nullptr, valid.c_str(), nullptr, nullptr, &config, nullptr), nullptr);
  EXPECT_EQ(XrdOfsAddPrepare(nullptr, nullptr, valid.c_str(), nullptr, nullptr, nullptr, &backend), nullptr);
  XrdOucEnv missingRuntime;
  EXPECT_EQ(XrdOfsAddPrepare(nullptr, nullptr, valid.c_str(), nullptr, nullptr, &missingRuntime, &backend), nullptr);
}

TEST(PrepPersistEntry, AcceptsLimitBoundariesAndPublishesProfile) {
  Temporary directory; Backend backend;
  for (const std::string limits : {"", " maxrequests=1 retention=1",
                                   " maxrequests=1000000 retention=31536000"}) {
    XrdOucEnv runtime, config; config.PutPtr("xrdEnv*", &runtime);
    const auto parms = directory.path + " test replay-safe" + limits;
    std::unique_ptr<XrdOfsPrepare> wrapper(
      XrdOfsAddPrepare(nullptr, nullptr, parms.c_str(), nullptr, nullptr, &config, &backend));
    ASSERT_NE(wrapper, nullptr) << limits;
    ASSERT_NE(runtime.Get("xrd.prepare.profile"), nullptr);
    EXPECT_STREQ(runtime.Get("xrd.prepare.profile"), "v1");
  }
}

TEST(PrepPersistEntry, RetainsProcessEnvironmentAndEnforcesAdmissionLimit) {
  Temporary directory; Backend backend; backend.SetUnavailable(true);
  MockAuthorizer authorizer; XrdOucEnv runtime;
  std::unique_ptr<XrdOfsPrepare> wrapper;
  const auto parms = directory.path + " test replay-safe maxrequests=1";
  {
    XrdOucEnv config; config.PutPtr("xrdEnv*", &runtime);
    wrapper.reset(XrdOfsAddPrepare(nullptr, nullptr, parms.c_str(), nullptr, nullptr, &config, &backend));
  }
  ASSERT_NE(wrapper, nullptr);
  // OFS publishes the authorizer only after loading its prepare wrapper.
  runtime.PutPtr("XrdAccAuthorize*", &authorizer);
  XrdSecEntity alice("unix"); alice.name = const_cast<char *>("alice");
  Args args("*", Prep_STAGE, {"/a"}, "authz=alice"); XrdOucErrInfo error("test");
  ASSERT_TRUE(Eventually([&] {
    const auto rc = wrapper->begin(args.prep, error, &alice);
    if (rc == SFS_DATA) return true;
    if (error.getErrInfo() != EAGAIN) throw std::runtime_error(error.getErrText());
    return false;
  }));
  EXPECT_TRUE(IsId(Text(error)));
  EXPECT_EQ(wrapper->begin(args.prep, error, &alice), SFS_ERROR);
  EXPECT_EQ(error.getErrInfo(), EDQUOT);
}

TEST(PrepStorage, FileContract) {
  Temporary directory;
  auto store = XrdOfsPrepFileStorage(directory.path);
  PrepStorageTest::Contract(*store);
}
TEST(PrepStorage, MemoryContract) {
  MemoryStorage store(std::make_shared<MemoryStorage::State>());
  PrepStorageTest::Contract(store);
}
TEST(PrepStorage, FileReportsUncertainCommitAndSurvivesReopen) {
  Temporary directory; auto record = PrepStorageTest::Record();
  {
    auto store = XrdOfsPrepFileStorage(directory.path);
    SyncHook hook([](int fd) {
      struct stat st{}; ASSERT_EQ(fstat(fd, &st), 0);
      if (S_ISDIR(st.st_mode)) Fail(EIO, "directory sync failed");
    });
    // Create shards before fault injection in the next attempt.
    EXPECT_THROW(store->Create(record), XrdOfsPrep::StorageError);
  }
  {
    auto store = XrdOfsPrepFileStorage(directory.path);
    record.revision = store->Create(record);
    SyncHook hook([](int fd) {
      struct stat st{}; ASSERT_EQ(fstat(fd, &st), 0);
      if (S_ISDIR(st.st_mode)) Fail(EIO, "directory sync failed");
    });
    record.deleted = true;
    try { store->Update(record, 1); FAIL(); }
    catch (const XrdOfsPrep::StorageError &ex) {
      EXPECT_EQ(ex.outcome, XrdOfsPrep::StorageError::Outcome::Unknown);
    }
  }
  auto reopened = XrdOfsPrepFileStorage(directory.path);
  auto restored = reopened->Read(record.id);
  EXPECT_TRUE(restored.deleted); EXPECT_EQ(restored.revision, 2u);
  PrepStorageTest::CheckRecord(restored);
}
TEST(PrepPersist, RecoversTypedStoreAndRetriesFailedScan) {
  auto state = std::make_shared<MemoryStorage::State>();
  Backend backend; backend.SetUnavailable(true);
  XrdSecEntity owner("unix"); owner.name = const_cast<char *>("alice");
  std::string id;
  {
    XrdOfsPrepPersist coordinator(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
    id = Submit(coordinator, owner);
  }
  state->failScan = true; backend.SetUnavailable(false);
  XrdOfsPrepPersist recovered(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
  EXPECT_TRUE(Eventually([&] { return Query(recovered, id, owner)["files"][0]["state"] == "COMPLETED"; }));
  EXPECT_EQ(backend.GetStages(), 1u);
  std::lock_guard<std::mutex> guard(state->mutex); EXPECT_GE(state->scans, 3u);
}
TEST(PrepPersist, NeverDispatchesAnUncertainTypedWrite) {
  auto state = std::make_shared<MemoryStorage::State>(); state->failUpdates = true;
  Backend backend;
  XrdSecEntity owner("unix"); owner.name = const_cast<char *>("alice");
  XrdOfsPrepPersist coordinator(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
  auto id = Submit(coordinator, owner);
  ASSERT_TRUE(Eventually([&] { std::lock_guard<std::mutex> guard(state->mutex); return state->updates >= 2; }));
  EXPECT_EQ(backend.GetStages(), 0u);
  { std::lock_guard<std::mutex> guard(state->mutex); state->failUpdates = false; }
  EXPECT_TRUE(Eventually([&] { return Query(coordinator, id, owner)["files"][0]["state"] == "COMPLETED"; }));
  EXPECT_EQ(backend.GetStages(), 1u);
}
TEST(PrepPersist, RetainsUncertainTypedAdmissionAndExpiresCompletedRecords) {
  auto state = std::make_shared<MemoryStorage::State>(); state->failCreate = true;
  Backend backend; XrdSecEntity owner("unix"); owner.name = const_cast<char *>("alice");
  {
    XrdOfsPrepPersist coordinator(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
    ASSERT_TRUE(Recovered(coordinator));
    Args args("*", Prep_STAGE, {"/a", "/b"}); XrdOucErrInfo error("test");
    EXPECT_EQ(coordinator.begin(args.prep, error, &owner), SFS_ERROR);
    EXPECT_EQ(error.getErrInfo(), EIO);
    EXPECT_TRUE(Eventually([&] { return backend.GetStages() == 1; }));
  }
  state->failCreate = false;
  auto old = PrepStorageTest::Record(); old.revision = 1;
  old.operations[0].kind = XrdOfsPrep::OperationKind::Stage;
  state->records[old.id] = old;
  XrdOfsPrepPersist coordinator(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr, 10000, 1);
  ASSERT_TRUE(Recovered(coordinator));
  std::lock_guard<std::mutex> guard(state->mutex); EXPECT_EQ(state->records.count(old.id), 0u);
}
TEST(PrepStorage, RejectsInvalidRecordSchemaAndEnums) {
  auto record = PrepStorageTest::Record();
  record.schema = 2; EXPECT_THROW(XrdOfsPrep::EncodeRecord(record), std::system_error);
  record.schema = 1; record.files[0].state = static_cast<XrdOfsPrep::FileState>(50);
  EXPECT_THROW(XrdOfsPrep::EncodeRecord(record), std::system_error);
  auto json = PendingRecord(XrdOfsPrepStorage::NewId());
  json["files"][0]["state"] = "INVALID";
  EXPECT_THROW(XrdOfsPrep::DecodeRecord(json), std::system_error);
  json["files"][0]["state"] = "SUBMITTED"; json["files"][0]["targetedMetadata"] = "invalid";
  EXPECT_THROW(XrdOfsPrep::DecodeRecord(json), std::system_error);
  json.erase("files"); EXPECT_THROW(XrdOfsPrep::DecodeRecord(json), std::system_error);
  Backend backend;
  EXPECT_THROW(XrdOfsPrepPersist(std::unique_ptr<XrdOfsPrepStorage>{}, "test", backend, MappedTestEnvironment(), nullptr), std::system_error);
}
TEST(PrepPersist, RetriesTransientReadDuringRecovery) {
  auto state = std::make_shared<MemoryStorage::State>(); state->failRead = true;
  const auto id = XrdOfsPrepStorage::NewId();
  auto record = XrdOfsPrep::DecodeRecord(PendingRecord(id)); record.revision = 1;
  state->records[id] = record;
  Backend backend; XrdSecEntity owner("unix"); owner.name = const_cast<char *>("alice");
  XrdOfsPrepPersist coordinator(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
  ASSERT_TRUE(Recovered(coordinator));
  XrdOucEnv runtime; coordinator.PublishProfile(runtime);
  EXPECT_STREQ(runtime.Get("xrd.prepare.profile"), "v1");
  EXPECT_TRUE(Eventually([&] { return Query(coordinator, id, owner)["files"][0]["state"] == "COMPLETED"; }));
  EXPECT_EQ(backend.GetStages(), 1u);
}

// Native services need no wire codec or independently persisted acknowledgement.
namespace {
class NativeBackend : public XrdOfsPrepBackend {
public:
  std::function<Result(const std::string &, const std::string &, XrdOfsPrep::OperationKind,
                       const XrdOfsPrep::File &)> execute;
  std::function<Result()> recover;
  std::function<Status()> observe;
  std::atomic<unsigned> calls{0}, recoveries{0};
  std::atomic<bool> online{true};
  Result Execute(const std::string &id, const std::string &op, XrdOfsPrep::OperationKind kind,
                 const XrdOfsPrep::File &file) override {
    ++calls;
    return execute ? execute(id, op, kind, file) : Result{Result::Applied, {}};
  }
  Result Recover(const std::string &, const std::string &, XrdOfsPrep::OperationKind,
                 const XrdOfsPrep::File &) override {
    ++recoveries;
    return recover ? recover() : Result{Result::Unknown, {}};
  }
  Status Observe(const std::string &) override { return observe ? observe() : Status{online, true, {}}; }
};
XrdOfsPrep::Record NativeRecord(const std::shared_ptr<MemoryStorage::State> &s, const std::string &id) {
  std::lock_guard<std::mutex> lock(s->mutex); return s->records.at(id);
}
}
TEST(NativePrepare, DispatchIsDurableAndMetadataIsTyped) {
  auto state = std::make_shared<MemoryStorage::State>(); NativeBackend backend;
  backend.execute = [&](const auto &id, const auto &op, auto, const auto &file) {
    auto r = NativeRecord(state, id);
    EXPECT_EQ(r.operations[0].id, op);
    EXPECT_EQ(r.operations[0].files[file.path == "/a" ? 0 : 1].state, XrdOfsPrep::FileState::Started);
    EXPECT_EQ(file.diskLifetime, "PT1H");
    EXPECT_EQ(std::get<std::string>(file.targetedMetadata->at("activity").data), "analysis");
    return NativeBackend::Result{NativeBackend::Result::Applied, {}};
  };
  XrdSecEntity owner("unix"); owner.name = const_cast<char *>("alice");
  XrdOfsPrepPersist c(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
  auto id = Submit(c, owner, "xrd.prepare.file=" + Hex(Json{{"diskLifetime", "PT1H"}, {"targetedMetadata", {{"activity", "analysis"}}}}.dump()));
  ASSERT_TRUE(Eventually([&] { return Query(c, id, owner).contains("completedAt"); }));
  EXPECT_EQ(backend.calls, 2u);
  EXPECT_EQ(NativeRecord(state, id).operations[0].state, XrdOfsPrep::OperationState::Done);
}
TEST(NativePrepare, RestartDoesNotRepeatAcknowledgedFiles) {
  auto state = std::make_shared<MemoryStorage::State>(); NativeBackend backend; backend.online = false;
  XrdSecEntity owner("unix"); owner.name = const_cast<char *>("alice"); std::string id;
  {
    XrdOfsPrepPersist c(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
    id = Submit(c, owner);
    ASSERT_TRUE(Eventually([&] { return NativeRecord(state, id).operations[0].state == XrdOfsPrep::OperationState::Done; }));
  }
  backend.online = true;
  XrdOfsPrepPersist c(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
  ASSERT_TRUE(Eventually([&] { return Query(c, id, owner).contains("completedAt"); }));
  EXPECT_EQ(backend.calls, 2u);
}
TEST(NativePrepare, UnknownOutcomeRequiresRecoveryEvenIfOnline) {
  auto state = std::make_shared<MemoryStorage::State>(); NativeBackend backend;
  backend.execute = [](const auto &, const auto &, auto, const auto &) { return NativeBackend::Result{}; };
  XrdSecEntity owner("unix"); owner.name = const_cast<char *>("alice"); std::string id;
  {
    XrdOfsPrepPersist c(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
    id = Submit(c, owner);
    ASSERT_TRUE(Eventually([&] { return backend.calls == 2; }));
  }
  backend.recover = [] { return NativeBackend::Result{NativeBackend::Result::Applied, {}}; };
  XrdOfsPrepPersist c(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
  ASSERT_TRUE(Eventually([&] { return Query(c, id, owner).contains("completedAt"); }));
  EXPECT_EQ(backend.calls, 2u); EXPECT_EQ(backend.recoveries, 2u);
}
TEST(NativePrepare, ExceptionsDoNotReplayAndDoNotBlockOtherFiles) {
  auto state = std::make_shared<MemoryStorage::State>(); NativeBackend backend;
  backend.execute = [](const auto &, const auto &, auto, const auto &file) {
    if (file.path == "/a") throw std::runtime_error("lost reply");
    return NativeBackend::Result{NativeBackend::Result::Applied, {}};
  };
  XrdSecEntity owner("unix"); owner.name = const_cast<char *>("alice");
  XrdOfsPrepPersist c(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
  auto id = Submit(c, owner);
  ASSERT_TRUE(Eventually([&] { return backend.recoveries > 0; }));
  auto result = Query(c, id, owner);
  EXPECT_FALSE(result.contains("completedAt"));
  EXPECT_EQ(result["files"][0]["state"], "SUBMITTED");
  EXPECT_EQ(result["files"][1]["state"], "COMPLETED");
  EXPECT_EQ(backend.calls, 2u);
}
TEST(NativePrepare, DefiniteRejectionAndSafeRetryAreDistinct) {
  auto state = std::make_shared<MemoryStorage::State>(); NativeBackend backend;
  unsigned attempts = 0;
  backend.execute = [&](const auto &, const auto &, auto, const auto &file) {
    if (file.path == "/a") return NativeBackend::Result{NativeBackend::Result::Rejected, "denied"};
    return NativeBackend::Result{++attempts == 1 ? NativeBackend::Result::Retry : NativeBackend::Result::Applied, {}};
  };
  XrdSecEntity owner("unix"); owner.name = const_cast<char *>("alice");
  XrdOfsPrepPersist c(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
  auto id = Submit(c, owner);
  ASSERT_TRUE(Eventually([&] { return Query(c, id, owner).contains("completedAt"); }));
  EXPECT_EQ(Query(c, id, owner)["files"][0]["error"], "denied");
  EXPECT_EQ(backend.calls, 3u); EXPECT_EQ(backend.recoveries, 0u);
}
TEST(NativePrepare, CancelAndReleaseUseTheSameJournal) {
  auto state = std::make_shared<MemoryStorage::State>(); NativeBackend backend; backend.online = false;
  XrdSecEntity owner("unix"); owner.name = const_cast<char *>("alice");
  XrdOfsPrepPersist c(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
  auto id = Submit(c, owner);
  ASSERT_TRUE(Eventually([&] { return NativeRecord(state, id).operations[0].state == XrdOfsPrep::OperationState::Done; }));
  Args cancel(id, Prep_CANCEL); XrdOucErrInfo error("test");
  ASSERT_EQ(c.cancel(cancel.prep, error, &owner), SFS_OK);
  ASSERT_TRUE(Eventually([&] { return Query(c, id, owner)["files"][0]["state"] == "CANCELLED"; }));
  Args release(id, Prep_EVICT, {"/a", "/b"}, "xrd.prepare.request=" + id);
  ASSERT_EQ(c.begin(release.prep, error, &owner), SFS_OK);
  ASSERT_TRUE(Eventually([&] { return NativeRecord(state, id).operations.back().state == XrdOfsPrep::OperationState::Done; }));
  EXPECT_EQ(backend.calls, 6u);
  EXPECT_TRUE(NativeRecord(state, id).files[0].released.value_or(false));
}
TEST(NativePrepare, UncertainCancelIsNotAcknowledgedOrRedispatched) {
  auto state = std::make_shared<MemoryStorage::State>(); NativeBackend backend;
  backend.execute = [](const auto &, const auto &, auto kind, const auto &) {
    return NativeBackend::Result{kind == XrdOfsPrep::OperationKind::Cancel ? NativeBackend::Result::Unknown : NativeBackend::Result::Applied, {}};
  };
  XrdSecEntity owner("unix"); owner.name = const_cast<char *>("alice");
  XrdOfsPrepPersist c(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
  auto id = Submit(c, owner);
  ASSERT_TRUE(Eventually([&] { return Query(c, id, owner).contains("completedAt"); }));
  Args cancel(id, Prep_CANCEL); XrdOucErrInfo error("test");
  ASSERT_EQ(c.cancel(cancel.prep, error, &owner), SFS_OK);
  ASSERT_TRUE(Eventually([&] { return backend.recoveries > 0; }));
  EXPECT_FALSE(NativeRecord(state, id).files[0].cancelAcknowledged.value_or(false));
  EXPECT_EQ(backend.calls, 4u);
}
TEST(NativePrepare, UncertainDispatchWriteNeverCallsBackend) {
  auto state = std::make_shared<MemoryStorage::State>(); state->failUpdates = true;
  NativeBackend backend; XrdSecEntity owner("unix"); owner.name = const_cast<char *>("alice"); std::string id;
  {
    XrdOfsPrepPersist c(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
    id = Submit(c, owner);
    ASSERT_TRUE(Eventually([&] { std::lock_guard<std::mutex> lock(state->mutex); return state->updates > 0; }));
  }
  EXPECT_EQ(backend.calls, 0u);
  { std::lock_guard<std::mutex> lock(state->mutex); state->failUpdates = false; }
  XrdOfsPrepPersist c(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
  ASSERT_TRUE(Eventually([&] { return backend.recoveries > 0; }));
  EXPECT_EQ(backend.calls, 1u); // Only the second, never-dispatched file.
}
TEST(NativePrepare, ArchiveInfoUsesTypedObservation) {
  auto state = std::make_shared<MemoryStorage::State>(); NativeBackend backend;
  XrdSecEntity owner("unix"); owner.name = const_cast<char *>("alice");
  XrdOfsPrepPersist c(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
  Args args(ArchiveQuery, Prep_QUERY, {"/a"}); XrdOucErrInfo error("test");
  ASSERT_EQ(c.query(args.prep, error, &owner), SFS_DATA);
  EXPECT_EQ(Json::parse(Text(error))[0]["locality"], "DISK_AND_TAPE");
  EXPECT_EQ(backend.calls, 0u);
}
TEST(NativePrepare, CancellationDuringCallbackPreservesNewIntent) {
  auto state = std::make_shared<MemoryStorage::State>(); NativeBackend backend; backend.online = false;
  XrdSecEntity owner("unix"); owner.name = const_cast<char *>("alice");
  XrdOfsPrepPersist c(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
  backend.execute = [&](const auto &id, const auto &, auto kind, const auto &file) {
    if (kind == XrdOfsPrep::OperationKind::Stage && file.path == "/a") {
      Args cancel(id, Prep_CANCEL); XrdOucErrInfo error;
      EXPECT_EQ(c.cancel(cancel.prep, error, &owner), SFS_OK);
    }
    return NativeBackend::Result{NativeBackend::Result::Applied, {}};
  };
  auto id = Submit(c, owner);
  ASSERT_TRUE(Eventually([&] { return Query(c, id, owner).contains("completedAt"); }));
  EXPECT_EQ(NativeRecord(state, id).operations.size(), 2u);
  EXPECT_EQ(Query(c, id, owner)["files"][0]["state"], "CANCELLED");
  EXPECT_EQ(backend.calls, 4u);
}
TEST(NativePrepare, LostStorageReplyAfterAcknowledgementDoesNotRepeatEffect) {
  auto state = std::make_shared<MemoryStorage::State>(); NativeBackend backend;
  backend.execute = [&](const auto &, const auto &, auto, const auto &) {
    std::lock_guard<std::mutex> lock(state->mutex); state->failUpdates = true;
    return NativeBackend::Result{NativeBackend::Result::Applied, {}};
  };
  XrdSecEntity owner("unix"); owner.name = const_cast<char *>("alice"); std::string id;
  {
    XrdOfsPrepPersist c(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
    id = Submit(c, owner);
    ASSERT_TRUE(Eventually([&] { return NativeRecord(state, id).operations[0].files[0].state == XrdOfsPrep::FileState::Completed; }));
  }
  { std::lock_guard<std::mutex> lock(state->mutex); state->failUpdates = false; }
  backend.execute = {};
  XrdOfsPrepPersist c(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
  ASSERT_TRUE(Eventually([&] { return Query(c, id, owner).contains("completedAt"); }));
  EXPECT_EQ(backend.calls, 2u);
}
TEST(NativePrepare, AuthoritativeRecoveryCanPermitSafeRetry) {
  auto state = std::make_shared<MemoryStorage::State>(); NativeBackend backend;
  auto id = XrdOfsPrepStorage::NewId(); auto json = PendingRecord(id);
  json["operations"][0]["state"] = "dispatched";
  json["operations"][0]["files"][0]["state"] = "STARTED";
  { MemoryStorage storage(state); storage.Create(XrdOfsPrep::DecodeRecord(json)); }
  backend.recover = [] { return NativeBackend::Result{NativeBackend::Result::Retry, {}}; };
  XrdSecEntity owner("unix"); owner.name = const_cast<char *>("alice");
  XrdOfsPrepPersist c(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
  ASSERT_TRUE(Eventually([&] { return Query(c, id, owner).contains("completedAt"); }));
  EXPECT_EQ(backend.recoveries, 1u); EXPECT_EQ(backend.calls, 2u);
}
TEST(NativePrepare, CancelBeforeDispatchNeedsNoTapeCalls) {
  auto state = std::make_shared<MemoryStorage::State>(); NativeBackend backend;
  auto id = XrdOfsPrepStorage::NewId(); auto json = PendingRecord(id);
  for (auto &file : json["files"]) file["cancelRequested"] = true;
  json["operations"].push_back({{"id", id + ":2"}, {"kind", "cancel"}, {"state", "pending"}, {"files", json["files"]}});
  { MemoryStorage storage(state); storage.Create(XrdOfsPrep::DecodeRecord(json)); }
  XrdSecEntity owner("unix"); owner.name = const_cast<char *>("alice");
  XrdOfsPrepPersist c(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
  ASSERT_TRUE(Eventually([&] { return Query(c, id, owner).contains("completedAt"); }));
  EXPECT_EQ(Query(c, id, owner)["files"][0]["state"], "CANCELLED");
  EXPECT_EQ(backend.calls, 0u);
}

TEST(PrepPersist, NullEnvironmentCannotReuseConnectionClaims) {
  auto state = std::make_shared<MemoryStorage::State>(); NativeBackend backend;
  XrdSecEntity owner("unix"); owner.name = const_cast<char *>("shared");
  owner.eaAPI->Add("token.issuer", "old-issuer", true);
  owner.eaAPI->Add("token.subject", "old-subject", true);
  XrdOfsPrepPersist c(std::make_unique<MemoryStorage>(state), "test", backend, nullptr, nullptr);
  ASSERT_TRUE(Recovered(c));
  Args args("*", Prep_STAGE, {"/a"}); XrdOucErrInfo error;
  EXPECT_EQ(c.begin(args.prep, error, &owner), SFS_ERROR);
  EXPECT_EQ(error.getErrInfo(), EACCES);
  EXPECT_EQ(backend.calls, 0u);
}

TEST(PrepPersist, VerifiedSfsHandoffChecksOwnerAndStoredPaths) {
  struct Authorizer : XrdOfsPrepAuthorizer {
    std::vector<std::string> checked;
    std::string deny;
    bool Authorize(const XrdSecEntity &, const std::string &path,
                   const std::string &cgi, XrdOfsPrep::Owner &owner) override {
      checked.push_back(path);
      if (path == deny) return false;
      // This fixture stands in for the SFS verifier, not a JWT implementation.
      XrdOucEnv input(cgi.c_str());
      if (!input.Get("issuer") || !input.Get("subject")) return false;
      owner.kind = "token"; owner.issuer = input.Get("issuer"); owner.subject = input.Get("subject");
      return true;
    }
  } auth;
  auto state = std::make_shared<MemoryStorage::State>(); NativeBackend backend; backend.online = false;
  XrdOucEnv env; env.PutPtr("XrdOfsPrepAuthorizer*", &auth);
  XrdSecEntity owner("unix"); owner.name = const_cast<char *>("anonymous");
  XrdOfsPrepPersist c(std::make_unique<MemoryStorage>(state), "test", backend, &env, nullptr);
  auto id = Submit(c, owner, "issuer=one&subject=alice&token=old");
  XrdOucErrInfo error;
  owner.name = const_cast<char *>("shared"); // Verification, not mapping, owns the request.
  for (const auto &credentials : {"issuer=two&subject=alice", "issuer=one&subject=bob"}) {
    Args query(id, Prep_QUERY, {}, credentials);
    EXPECT_EQ(c.query(query.prep, error, &owner), SFS_ERROR);
    EXPECT_EQ(error.getErrInfo(), EACCES);
    Args cancel(id, Prep_CANCEL, {}, credentials);
    EXPECT_EQ(c.cancel(cancel.prep, error, &owner), SFS_ERROR);
    EXPECT_EQ(error.getErrInfo(), EACCES);
    Args remove(std::string(DeletePrefix) + id, Prep_CANCEL, {}, credentials);
    EXPECT_EQ(c.cancel(remove.prep, error, &owner), SFS_ERROR);
    EXPECT_EQ(error.getErrInfo(), EACCES);
    Args release(id, Prep_EVICT, {}, std::string(credentials) + "&xrd.prepare.request=" + id);
    EXPECT_EQ(c.begin(release.prep, error, &owner), SFS_ERROR);
    EXPECT_EQ(error.getErrInfo(), EACCES);
  }
  Args query(id, Prep_QUERY, {}, "issuer=one&subject=alice&token=refreshed");
  auth.checked.clear();
  EXPECT_EQ(c.query(query.prep, error, &owner), SFS_DATA);
  EXPECT_EQ(auth.checked, (std::vector<std::string>{"/a", "/b"}));
  auth.deny = "/b";
  EXPECT_EQ(c.query(query.prep, error, &owner), SFS_ERROR);
  Args cancel(id, Prep_CANCEL, {"/a"}, "issuer=one&subject=alice");
  auth.checked.clear();
  EXPECT_EQ(c.cancel(cancel.prep, error, &owner), SFS_OK);
  EXPECT_EQ(auth.checked, (std::vector<std::string>{"/a"}));
  Args foreign(id, Prep_CANCEL, {"/foreign"}, "issuer=one&subject=alice");
  EXPECT_EQ(c.cancel(foreign.prep, error, &owner), SFS_ERROR);
  EXPECT_EQ(error.getErrInfo(), EINVAL);
  Args all(id, Prep_CANCEL, {}, "issuer=one&subject=alice");
  EXPECT_EQ(c.cancel(all.prep, error, &owner), SFS_ERROR);
  Args remove(std::string(DeletePrefix) + id, Prep_CANCEL, {}, "issuer=one&subject=alice");
  EXPECT_EQ(c.cancel(remove.prep, error, &owner), SFS_ERROR);
  Args release(id, Prep_EVICT, {}, "issuer=one&subject=alice&xrd.prepare.request=" + id);
  EXPECT_EQ(c.begin(release.prep, error, &owner), SFS_ERROR);
}

TEST(PrepPersist, CorruptTypedRecordsCannotReplayServiceActions) {
  using R = XrdOfsPrep::Record;
  const std::vector<std::function<void(R &)>> corrupt = {
    [](R &r) { r.operations[0].files[0].path = "/foreign"; },
    [](R &r) { r.files[1].path = r.files[0].path; },
    [](R &r) { r.operations.push_back(r.operations.front()); },
    [](R &r) { r.operations[0].kind = XrdOfsPrep::OperationKind::Cancel; },
    [](R &r) { r.operations.resize(98, r.operations.front()); },
    [](R &r) { r.completedAt = r.createdAt; },
    [](R &r) { r.createdAt = uint64_t(-1); },
    [](R &r) { r.files[0].finishedAt = r.createdAt; },
    [](R &r) { r.operations[0].files.pop_back(); },
    [](R &r) { r.owner.subject = "unbound"; },
    [](R &r) { r.files[0].path = "/a//b"; },
    [](R &r) { r.files[0].diskLifetime = std::string(1025, 'x'); },
    [](R &r) { r.operations[0].files.push_back(r.operations[0].files[0]); }
  };
  for (size_t i = 0; i < corrupt.size(); ++i) {
    SCOPED_TRACE(i);
    auto state = std::make_shared<MemoryStorage::State>(); NativeBackend backend;
    auto id = XrdOfsPrepStorage::NewId();
    auto record = XrdOfsPrep::DecodeRecord(PendingRecord(id)); record.revision = 1;
    corrupt[i](record);
    EXPECT_THROW(XrdOfsPrep::ValidateRecord(record), std::system_error);
    state->records[id] = record;
    XrdSecEntity owner("unix"); owner.name = const_cast<char *>("alice");
    {
      XrdOfsPrepPersist c(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
      ASSERT_TRUE(Recovered(c)); // Initial scan has examined the corrupt record.
      Args query(id, Prep_QUERY); XrdOucErrInfo error;
      EXPECT_EQ(c.query(query.prep, error, &owner), SFS_ERROR);
      EXPECT_EQ(error.getErrInfo(), EIO);
    }
    EXPECT_EQ(backend.calls, 0u); EXPECT_EQ(backend.recoveries, 0u);
    EXPECT_EQ(state->records.at(id).revision, 1u);
    // The JSON implementation reaches the identical replay validation boundary.
    Temporary directory;
    { XrdOfsPrepStore store(directory.path);
      auto json = XrdOfsPrep::EncodeRecord(record); json.erase("revision");
      store.Save(json, true);
    }
    {
      XrdOfsPrepPersist c(XrdOfsPrepFileStorage(directory.path), "test", backend, MappedTestEnvironment(), nullptr);
      ASSERT_TRUE(Recovered(c));
      Args query(id, Prep_QUERY); XrdOucErrInfo error;
      EXPECT_EQ(c.query(query.prep, error, &owner), SFS_ERROR);
      EXPECT_EQ(error.getErrInfo(), EIO);
    }
    EXPECT_EQ(backend.calls, 0u); EXPECT_EQ(backend.recoveries, 0u);
    EXPECT_EQ(LoadRecord(directory.path, id)["revision"], 1);
  }
}

TEST(NativePrepare, CancelOneInterestPreservesOtherRecallAndTerminalTimes) {
  auto state = std::make_shared<MemoryStorage::State>(); NativeBackend backend; backend.online = false;
  XrdSecEntity owner("unix"); owner.name = const_cast<char *>("alice");
  XrdOfsPrepPersist c(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
  auto first = Submit(c, owner), second = Submit(c, owner);
  ASSERT_TRUE(Eventually([&] { return backend.calls == 4; }));
  Args cancel(first, Prep_CANCEL); XrdOucErrInfo error;
  ASSERT_EQ(c.cancel(cancel.prep, error, &owner), SFS_OK);
  ASSERT_TRUE(Eventually([&] { return Query(c, first, owner)["files"][0]["state"] == "CANCELLED"; }));
  auto finished = Query(c, first, owner)["files"][0]["finishedAt"];
  backend.online = true;
  ASSERT_TRUE(Eventually([&] { return Query(c, second, owner).contains("completedAt"); }));
  EXPECT_EQ(Query(c, first, owner)["files"][0]["state"], "CANCELLED");
  EXPECT_EQ(Query(c, first, owner)["files"][0]["finishedAt"], finished);
  auto completed = Query(c, second, owner)["files"][0]["finishedAt"];
  Args late(second, Prep_CANCEL);
  ASSERT_EQ(c.cancel(late.prep, error, &owner), SFS_OK);
  ASSERT_TRUE(Eventually([&] { return NativeRecord(state, second).operations.back().state == XrdOfsPrep::OperationState::Done; }));
  EXPECT_EQ(Query(c, second, owner)["files"][0]["state"], "COMPLETED");
  EXPECT_EQ(Query(c, second, owner)["files"][0]["finishedAt"], completed);
}

TEST(NativePrepare, ForegroundObservationsCanOverlapWorkerAndEachOther) {
  auto state = std::make_shared<MemoryStorage::State>(); NativeBackend backend;
  std::promise<void> entered, release;
  auto gate = release.get_future().share(); auto started = entered.get_future();
  std::atomic<unsigned> observations{0};
  backend.execute = [&](const auto &, const auto &, auto, const auto &file) {
    if (file.path == "/a") { entered.set_value(); gate.wait(); }
    return NativeBackend::Result{NativeBackend::Result::Applied, {}};
  };
  backend.observe = [&] { ++observations; gate.wait(); return NativeBackend::Status{true, true, {}}; };
  XrdSecEntity owner("unix"); owner.name = const_cast<char *>("alice");
  XrdOfsPrepPersist c(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
  auto id = Submit(c, owner);
  const auto workerStarted = started.wait_for(std::chrono::seconds(5));
  auto query = [&] {
    XrdSecEntity client("unix"); client.name = const_cast<char *>("alice");
    Args args(ArchiveQuery, Prep_QUERY, {"/a"}); XrdOucErrInfo error;
    return c.query(args.prep, error, &client);
  };
  auto first = std::async(std::launch::async, query);
  auto second = std::async(std::launch::async, query);
  bool overlap = Eventually([&] { return observations == 2; });
  release.set_value(); // Always unblock callbacks before any fatal assertion.
  EXPECT_EQ(workerStarted, std::future_status::ready);
  EXPECT_TRUE(overlap);
  EXPECT_EQ(first.get(), SFS_DATA); EXPECT_EQ(second.get(), SFS_DATA);
  EXPECT_TRUE(Eventually([&] { return Query(c, id, owner).contains("completedAt"); }));
}

TEST(NativePrepare, QueuedCancelWaitsForRecoveryAndPreservesRecallCompletion) {
  auto state = std::make_shared<MemoryStorage::State>(); NativeBackend backend; backend.online = false;
  std::atomic<bool> cancelled{false};
  backend.execute = [](const auto &, const auto &, auto kind, const auto &) {
    return NativeBackend::Result{kind == XrdOfsPrep::OperationKind::Cancel ?
      NativeBackend::Result::Unknown : NativeBackend::Result::Applied, {}};
  };
  backend.recover = [&] { return NativeBackend::Result{cancelled ?
    NativeBackend::Result::Applied : NativeBackend::Result::Unknown, {}}; };
  XrdSecEntity owner("unix"); owner.name = const_cast<char *>("alice");
  XrdOfsPrepPersist c(std::make_unique<MemoryStorage>(state), "test", backend, MappedTestEnvironment(), nullptr);
  auto id = Submit(c, owner);
  ASSERT_TRUE(Eventually([&] { return Query(c, id, owner)["files"][0]["state"] == "STARTED"; }));
  Args cancel(id, Prep_CANCEL); XrdOucErrInfo error;
  ASSERT_EQ(c.cancel(cancel.prep, error, &owner), SFS_OK);
  ASSERT_TRUE(Eventually([&] { return backend.calls == 4; }));
  EXPECT_EQ(Query(c, id, owner)["files"][0]["state"], "STARTED");
  backend.online = true;
  ASSERT_TRUE(Eventually([&] { return Query(c, id, owner)["files"][0]["state"] == "COMPLETED"; }));
  auto finished = Query(c, id, owner)["files"][0]["finishedAt"];
  cancelled = true;
  ASSERT_TRUE(Eventually([&] { return Query(c, id, owner).contains("completedAt"); }));
  EXPECT_EQ(Query(c, id, owner)["files"][0]["state"], "COMPLETED");
  EXPECT_EQ(Query(c, id, owner)["files"][0]["finishedAt"], finished);
  EXPECT_EQ(backend.calls, 4u);
}

TEST(PrepStorage, RejectsNonIntegralAndNegativeEpochs) {
  auto valid = PendingRecord(XrdOfsPrepStorage::NewId());
  for (const auto &value : {Json(-1), Json(1.5), Json("123"), Json(true)}) {
    auto invalid = valid; invalid["createdAt"] = value;
    EXPECT_THROW(XrdOfsPrep::DecodeRecord(invalid), std::system_error);
    invalid = valid; invalid["files"][0]["finishedAt"] = value;
    EXPECT_THROW(XrdOfsPrep::DecodeRecord(invalid), std::system_error);
  }
}
