// Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
#include "XrdOfsPrepPersist.hh"
#include "XrdOfsPrepRecord.hh"
#include "XrdAcc/XrdAccAuthorize.hh"
#include "XrdOuc/XrdOucBuffer.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdSec/XrdSecEntityAttr.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysError.hh"
#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <thread>

using namespace XrdOfsPrepProtocol;
namespace {
using Clock = std::chrono::steady_clock;
std::string RequestCGI(const XrdSecEntity *client) {
  std::string cgi;
  if (client && client->eaAPI) client->eaAPI->Get("request.cgi", cgi);
  return cgi;
}
bool IsDefinitiveRejection(int err) {
  return err == ENOTSUP || err == EOPNOTSUPP || err == EINVAL ||
         err == EPERM || err == EACCES || err == ENOENT || err == ENOSYS ||
         err == E2BIG || err == EROFS || err == ENAMETOOLONG;
}
Json Principal(const XrdSecEntity *client) {
  if (!client) Fail(EACCES, "persistent prepare requires an authenticated identity");
  std::string subject, issuer, name;
  client->eaAPI->Get("token.subject", subject);
  client->eaAPI->Get("token.issuer", issuer);
  if (!subject.empty() && !issuer.empty())
    return {{"kind", "token"}, {"issuer", issuer}, {"subject", subject}};
  client->eaAPI->Get("request.name", name);
  if (name.empty() && client->name) name = client->name;
  if (name.empty() || name == "unknown" || name == "anon" || name == "nobody" || name == "anonymous")
    Fail(EACCES, "persistent prepare requires a stable authenticated identity");
  return {{"kind", std::string(client->prot)}, {"name", name}};
}
Json InputFiles(XrdSfsPrep &args) {
  Json files = Json::array();
  std::set<std::string> seen;
  auto *opaque = args.oinfo;
  for (auto *path = args.paths; path; path = path->next) {
    if (files.size() >= MaxFiles) Fail(E2BIG, "prepare batch exceeds 48 files");
    auto normalized = Path(path->text ? path->text : "");
    if (!seen.insert(normalized).second) Fail(EINVAL, "duplicate prepare path");
    XrdOucEnv env(opaque ? opaque->text : nullptr);
    Json file = Json::object();
    if (const char *metadata = env.Get("xrd.prepare.file")) {
      try { file = Metadata(Json::parse(Unhex(metadata))); }
      catch (const Json::exception &) { Fail(EINVAL, "invalid prepare file metadata"); }
    }
    file["path"] = normalized;
    files.push_back(std::move(file));
    if (opaque) opaque = opaque->next;
  }
  return files;
}
Json Status(const Json &record) {
  Json result = {{"id", record.at("id")}, {"createdAt", record.at("createdAt")},
                 {"startedAt", record.at("startedAt")}, {"files", Json::array()}};
  if (record.contains("completedAt")) result["completedAt"] = record["completedAt"];
  for (const auto &file : record.at("files")) {
    Json out = {{"path", file.at("path")}, {"state", file.at("state")}};
    for (const char *key : {"startedAt", "finishedAt", "error", "cancelError", "releaseError"})
      if (file.contains(key)) out[key] = file[key];
    result["files"].push_back(std::move(out));
  }
  return result;
}
bool Finished(const Json &record) {
  for (const auto &file : record.at("files"))
    if (!Terminal(file.at("state").get<std::string>())) return false;
  for (const auto &operation : record.at("operations"))
    if (operation.at("state") != "done" && operation.at("state") != "failed") return false;
  return true;
}
int Data(XrdOucErrInfo &error, const std::string &text) {
  if (text.size() + 1 > MaxRecord) Fail(E2BIG, "prepare response exceeds limit");
  void *memory = nullptr;
  if (posix_memalign(&memory, sizeof(void *), text.size() + 1))
    Fail(ENOMEM, "allocate prepare response");
  std::memcpy(memory, text.c_str(), text.size() + 1);
  auto *buffer = new XrdOucBuffer(static_cast<char *>(memory), text.size() + 1);
  error.setErrInfo(text.size() + 1, buffer);
  return SFS_DATA;
}
template <typename Function> int Guard(XrdOucErrInfo &error, Function action) {
  try { return action(); }
  catch (const std::system_error &ex) { error.setErrInfo(ex.code().value(), ex.what()); }
  catch (const std::exception &) { error.setErrInfo(EIO, "invalid prepare request state"); }
  return SFS_ERROR;
}
// Own all strings through the backend call; the GPI driver copies its arguments.
struct Arguments {
  XrdSfsPrep args{};
  XrdOucTListFIFO paths, opaque;
  std::string id;
  Arguments(const std::string &requestId, const Json &files, const Json *operation = nullptr)
    : id(requestId) {
    size_t index = 0;
    for (const auto &file : files) {
      const auto path = file.at("path").get<std::string>();
      paths.Add(new XrdOucTList(path.c_str(), index++));
      std::string cgi = "xrd.prepare.file=" + Hex(Metadata(file).dump());
      if (operation) cgi += "&xrd.prepare.operation=" + operation->at("id").get<std::string>();
      opaque.Add(new XrdOucTList(cgi.c_str()));
    }
    args.reqid = &id[0]; args.paths = paths.first; args.oinfo = opaque.first;
  }
};
}

struct XrdOfsPrepPersist::Impl {
  std::unique_ptr<XrdOfsPrepStorage> store;
  const std::string identity;
  XrdOfsPrepare *backend;
  XrdOfsPrepBackend *native;
  XrdOucEnv *environment;
  XrdSysError *log;
  const size_t maxRequests;
  const uint64_t retention;
  std::array<std::mutex, 256> locks;
  std::mutex queueMutex;
  std::condition_variable wake;
  std::map<std::string, Clock::time_point> due;
  std::atomic<bool> ready{false}, stopping{false};
  std::thread worker;
  std::unique_ptr<XrdOfsPrepStorage::Cursor> sweep;
  Clock::time_point nextSweep = Clock::now();

  Impl(std::unique_ptr<XrdOfsPrepStorage> storage, const std::string &id, XrdOfsPrepare *b, XrdOfsPrepBackend *n,
       XrdOucEnv *env, XrdSysError *logger, size_t maximum, uint64_t keep)
    : store(std::move(storage)), identity(id), backend(b), native(n), environment(env), log(logger),
      maxRequests(maximum), retention(keep) {
    if (!store) Fail(EINVAL, "persistent prepare requires a store");
    worker = std::thread([this] { Run(); });
  }
  ~Impl() { stopping = true; wake.notify_all(); if (worker.joinable()) worker.join(); }
  std::mutex &Lock(const std::string &id) {
    return locks[std::hash<std::string>{}(id) % locks.size()];
  }
  void Log(const std::string &id, const char *message) {
    if (log) log->Emsg("PrepPersist", id.c_str(), message);
  }
  static constexpr Clock::time_point Admitting{Clock::time_point::max() - std::chrono::seconds(1)};
  void Schedule(const std::string &id) {
    std::lock_guard<std::mutex> guard(queueMutex);
    auto it = due.find(id);
    if (it != due.end() && it->second == Admitting) return;
    due[id] = Clock::now(); wake.notify_one();
  }
  Json Load(const std::string &id, bool visible = true) {
    auto typed = store->Read(id);
    XrdOfsPrep::ValidateRecord(typed);
    if (typed.id != id) Fail(EIO, "prepare record ID mismatch");
    Json record = XrdOfsPrep::EncodeRecord(typed);
    if (record.at("backend") != identity) Fail(EIO, "prepare backend identity changed");
    if (!record.at("files").is_array() || record.at("files").empty() ||
        record.at("files").size() > MaxFiles ||
        !record.at("operations").is_array() || record.at("operations").empty())
      Fail(EIO, "invalid prepare manifest");
    if (visible && record.at("deleted").get<bool>()) Fail(ENOENT, "unknown prepare request");
    return record;
  }
  void Save(Json &record, bool create = false) {
    const auto typed = XrdOfsPrep::DecodeRecord(record);
    record["revision"] = create ? store->Create(typed) : store->Update(typed, typed.revision);
  }
  Json Authorize(const XrdSecEntity *client, const Json &files, XrdSfsPrep *input = nullptr) {
    if (!client) Fail(EACCES, "persistent prepare requires a client identity");
    auto *auth = environment ? static_cast<XrdAccAuthorize *>(
                      environment->GetPtr("XrdAccAuthorize*")) : nullptr;
    auto *verified = environment ? static_cast<XrdOfsPrepAuthorizer *>(
                      environment->GetPtr("XrdOfsPrepAuthorizer*")) : nullptr;
    if (!auth && !verified) Fail(EACCES, "persistent prepare requires path authorization");
    std::string requestCGI = RequestCGI(client);
    if (requestCGI.empty() && input && !input->paths && input->oinfo && input->oinfo->text)
      requestCGI = input->oinfo->text;
    std::map<std::string, std::string> pathCGI;
    if (input && input->paths) {
      auto *p = input->paths;
      auto *o = input->oinfo;
      while (p) {
        if (p->text) pathCGI[Path(p->text)] = (o && o->text) ? o->text : "";
        p = p->next;
        if (o) o = o->next;
      }
    }
    Json principal;
    for (const auto &file : files) {
      const auto path = file.at("path").get<std::string>();
      std::string cgi;
      auto it = pathCGI.find(path);
      if (it != pathCGI.end() && !it->second.empty()) cgi = it->second;
      else cgi = requestCGI;
      XrdOucEnv env(cgi.c_str(), 0, client);
      Json current;
      if (verified) {
        XrdOfsPrep::Owner owner;
        if (!verified->Authorize(*client, path, cgi, owner))
          Fail(EACCES, "prepare request access denied");
        XrdOfsPrep::ValidateOwner(owner);
        current = {{"kind", owner.kind}};
        if (owner.name) current["name"] = *owner.name;
        if (owner.issuer) current["issuer"] = *owner.issuer;
        if (owner.subject) current["subject"] = *owner.subject;
      } else {
        // Only this authorization call may supply verified token attributes.
        for (const char *key : {"request.name", "token.subject", "token.issuer"})
          client->eaAPI->Add(key, "", true);
        if (!auth->Access(client, path.c_str(), AOP_Read, &env))
          Fail(EACCES, "prepare request access denied");
        current = Principal(client);
      }
      if (!principal.is_null() && principal != current)
        Fail(EACCES, "all files must use the same prepare principal");
      principal = std::move(current);
    }
    return principal;
  }
  void Owner(const Json &record, const XrdSecEntity *client, const Json &files, XrdSfsPrep *input = nullptr) {
    if (Authorize(client, files, input) != record.at("owner"))
      Fail(EACCES, "prepare request belongs to another principal");
  }
  int Submit(XrdSfsPrep &args, XrdOucErrInfo &error, const XrdSecEntity *client) {
    if (!ready) Fail(EAGAIN, "prepare recovery is in progress");
    if (!(args.opts & Prep_STAGE)) Fail(ENOTSUP, "persistent prepare supports stage and evict");
    if (args.notify || (args.opts & (Prep_WMODE | Prep_COLOC | Prep_FRESH)))
      Fail(ENOTSUP, "prepare option not supported by durable profile v1");
    auto files = InputFiles(args);
    if (files.empty()) Fail(EINVAL, "empty prepare batch");
    auto owner = Authorize(client, files, &args);
    const auto id = XrdOfsPrepStorage::NewId();
    const auto now = XrdOfsPrepStorage::Now();
    for (auto &file : files) file["state"] = "SUBMITTED";
    Json record = {{"schema", 1}, {"id", id}, {"backend", identity}, {"owner", owner},
      {"createdAt", now}, {"startedAt", now}, {"deleted", false}, {"files", files},
      {"operations", Json::array({{{"id", id + ":1"}, {"kind", "stage"},
         {"state", "pending"}, {"files", files}}})}};
    {
      std::lock_guard<std::mutex> guard(queueMutex);
      if (due.size() >= maxRequests) Fail(EDQUOT, "too many active prepare requests");
      due[id] = Admitting;
    }
    try {
      Save(record, true);
    } catch (...) {
      // Publication can precede a failing directory fsync. Although the caller
      // receives no successful admission, that durable intent must be replayed.
      // Only release the slot when we can confirm nothing was published.
      bool missing = false;
      try { store->Read(id); }
      catch (const std::system_error &ex) { missing = ex.code().value() == ENOENT; }
      catch (...) {} // An unreadable published record still needs reconciliation.
      std::lock_guard<std::mutex> guard(queueMutex);
      if (missing) due.erase(id);
      else { due[id] = Clock::now(); wake.notify_one(); }
      throw;
    }
    {
      std::lock_guard<std::mutex> guard(queueMutex);
      due[id] = Clock::now();
      wake.notify_one();
    }
    return Data(error, id);
  }
  int Mutate(XrdSfsPrep &args, XrdOucErrInfo &, const XrdSecEntity *client,
             const std::string &kind, const std::string &id, bool deleting = false) {
    // A tombstone hides the whole request; never authorize/cancel only a subset.
    if (deleting && args.paths) Fail(EINVAL, "request deletion does not accept a file subset");
    std::lock_guard<std::mutex> guard(Lock(id));
    Json record = Load(id);
    auto selected = InputFiles(args);
    if (selected.empty()) selected = record.at("files");
    std::map<std::string, Json> members;
    for (const auto &file : record.at("files")) members[file.at("path")] = file;
    for (auto &file : selected) {
      auto it = members.find(file.at("path"));
      if (it == members.end()) Fail(EINVAL, "file does not belong to prepare request");
      file = it->second;
    }
    Owner(record, client, selected, &args);
    Json changed = Json::array();
    for (auto &file : record["files"]) {
      const auto path = file.at("path").get<std::string>();
      auto chosen = std::find_if(selected.begin(), selected.end(), [&](const Json &v) {
        return v.at("path") == path;
      });
      if (chosen != selected.end()) {
        if (kind == "cancel") {
          if (file.at("state") != "CANCELLED" &&
              !file.value("cancelAcknowledged", false) &&
              !file.value("cancelRequested", false)) {
            file["cancelRequested"] = true;
            file.erase("cancelError");
            file.erase("cancelAcknowledged");
            changed.push_back(*chosen);
          }
        } else if (kind == "evict") {
          if (!file.value("released", false) && !file.value("releaseRequested", false)) {
            file["releaseRequested"] = true;
            file.erase("releaseError");
            changed.push_back(*chosen);
          }
        }
      }
    }
    if (deleting) record["deleted"] = true;
    if (!changed.empty()) {
      if (native) for (auto &file : changed) {
        file["state"] = "SUBMITTED"; file.erase("error");
      }
      if (record["operations"].size() >= 1 + 2 * MaxFiles)
        Fail(EDQUOT, "too many prepare operations");
      record["operations"].push_back({{"id", id + ":" + std::to_string(record["operations"].size() + 1)},
        {"kind", kind}, {"state", "pending"}, {"files", changed}});
    }
    // A tombstone and cancellation intent are a single atomic record update.
    try { Save(record); }
    catch (...) {
      // Rename may have published the intent before directory fsync failed.
      // Reconcile it even when this request had already left the active queue.
      Schedule(id);
      throw;
    }
    Schedule(id);
    return SFS_OK;
  }
  Json BackendQuery(const std::string &id, const Json &files) {
    Arguments args(id, files); args.args.opts = Prep_QUERY;
    XrdOucErrInfo error("prepare-persist");
    XrdSecEntity service("prep"); service.name = const_cast<char *>("prepare"); service.tident = const_cast<char *>("prepare-persist");
    const int rc = backend->query(args.args, error, &service);
    if (rc != SFS_DATA) Fail(EAGAIN, "prepare backend query unavailable");
    const int length = error.getErrInfo();
    if (length <= 0 || static_cast<size_t>(length) > MaxRecord)
      Fail(EIO, "invalid prepare backend response length");
    std::string response(error.getErrText(), length);
    while (!response.empty() && response.back() == '\0') response.pop_back();
    if (response.find('\0') != std::string::npos)
      Fail(EIO, "prepare backend returned invalid JSON");
    Json result;
    try { result = Json::parse(response); }
    catch (const Json::exception &) { Fail(EIO, "prepare backend returned invalid JSON"); }
    if (result.at("schema") != 1 || !result.at("known").is_boolean() ||
        result.at("requestId") != id || !result.at("acknowledged").is_array() ||
        !result.at("files").is_array()) Fail(EIO, "unsupported prepare backend profile");
    for (const auto &operation : result.at("acknowledged"))
      if (!operation.is_string()) Fail(EIO, "invalid prepare acknowledgement");
    if (!result.at("known").get<bool>()) {
      if (!result.at("files").empty() || !result.at("acknowledged").empty())
        Fail(EIO, "inconsistent unknown prepare response");
      return result;
    }
    std::set<std::string> expected, actual;
    for (const auto &file : files) expected.insert(file.at("path"));
    for (const auto &file : result.at("files")) {
      const auto path = file.at("path").get<std::string>();
      if (!actual.insert(path).second || !expected.count(path)) Fail(EIO, "invalid backend file set");
      if (id != ArchiveQuery && !State(file.at("state").get<std::string>()))
        Fail(EIO, "invalid backend file state");
      for (const char *key : {"error", "cancelError", "releaseError"})
        if (file.contains(key) && (!file[key].is_string() || file[key].get<std::string>().size() > 2048))
          Fail(EIO, "invalid backend file error");
      for (const char *key : {"startedAt", "finishedAt"})
        if (file.contains(key) && !file[key].is_number_unsigned()) Fail(EIO, "invalid backend timestamp");
      if (id == ArchiveQuery && !file.contains("error")) {
        const auto locality = file.at("locality").get<std::string>();
        if (locality != "DISK" && locality != "TAPE" && locality != "DISK_AND_TAPE" && locality != "NONE")
          Fail(EIO, "invalid backend locality");
      }
    }
    if (actual != expected) Fail(EIO, "incomplete backend file set");
    return result;
  }
  // Native dispatch uses the same atomic record as admission and retention.
  // Only this worker changes operation results; client calls may append intents.
  bool ProcessNative(const std::string &id) {
    using namespace XrdOfsPrep;
    using Result = XrdOfsPrepBackend::Result;
    auto read = [&] { return DecodeRecord(Load(id, false)); };
    auto save = [&](Record &r) { auto json = EncodeRecord(r); Save(json); };
    size_t operations;
    {
      std::lock_guard<std::mutex> guard(Lock(id));
      auto record = read();
      // A request cancelled in full before dispatch needs no tape operation.
      if (record.operations.front().state == OperationState::Pending &&
          std::all_of(record.files.begin(), record.files.end(), [](const File &f) {
            return f.cancelRequested.value_or(false);
          })) {
        for (auto &file : record.files) {
          file.state = FileState::Cancelled;
          file.finishedAt = XrdOfsPrepStorage::Now();
          file.cancelRequested.reset(); file.cancelAcknowledged = true;
        }
        for (auto &op : record.operations)
          if (op.kind == OperationKind::Stage || op.kind == OperationKind::Cancel)
            op.state = OperationState::Done;
        save(record);
      }
      operations = record.operations.size();
    }
    for (size_t i = 0; i < operations && !stopping; ++i) {
      Operation op;
      { std::lock_guard<std::mutex> guard(Lock(id)); op = read().operations.at(i); }
      if (op.state == OperationState::Done || op.state == OperationState::Failed) continue;
      for (size_t j = 0; j < op.files.size() && !stopping; ++j) {
        File file;
        bool recovering;
        {
          std::lock_guard<std::mutex> guard(Lock(id));
          auto record = read();
          auto &pending = record.operations.at(i);
          file = pending.files.at(j);
          if (file.state == FileState::Completed || file.state == FileState::Failed) continue;
          recovering = file.state == FileState::Started;
          if (!recovering) {
            pending.state = OperationState::Dispatched;
            pending.files[j].state = FileState::Started;
            save(record); // No side effect until dispatch is durably recorded.
          }
        }
        // An exception leaves Started in storage, just like a lost reply/crash.
        Result result;
        try {
          result = recovering ? native->Recover(id, op.id, op.kind, file)
                              : native->Execute(id, op.id, op.kind, file);
        } catch (const std::exception &ex) { Log(id, ex.what()); }
          catch (...) { Log(id, "unknown native backend exception"); }
        if (result.outcome == Result::Unknown) continue;
        std::lock_guard<std::mutex> guard(Lock(id));
        auto record = read(); // Preserve intents appended during the callback.
        auto &pending = record.operations.at(i);
        auto &target = pending.files.at(j);
        if (result.outcome == Result::Retry) {
          target.state = FileState::Submitted;
        } else {
          const bool accepted = result.outcome == Result::Applied;
          target.state = accepted ? FileState::Completed : FileState::Failed;
          const auto reason = result.error.empty() ? "operation rejected by backend"
                                                   : result.error.substr(0, 2048);
          if (!accepted) target.error = reason;
          for (auto &local : record.files) if (local.path == file.path) {
            if (op.kind == OperationKind::Stage) {
              local.state = accepted ? FileState::Started : FileState::Failed;
              local.startedAt = XrdOfsPrepStorage::Now();
              if (!accepted) { local.error = reason; local.finishedAt = *local.startedAt; }
            } else if (op.kind == OperationKind::Cancel) {
              local.cancelRequested.reset();
              local.cancelAcknowledged = accepted;
              if (!accepted) local.cancelError = reason;
              else {
                local.cancelError.reset();
                if (local.state == FileState::Submitted || local.state == FileState::Started) {
                  local.state = FileState::Cancelled;
                  local.finishedAt = XrdOfsPrepStorage::Now();
                }
              }
            } else {
              local.releaseRequested.reset(); local.released = accepted;
              if (!accepted) local.releaseError = reason;
              else local.releaseError.reset();
            }
          }
        }
        save(record);
      }
      std::lock_guard<std::mutex> guard(Lock(id));
      auto record = read();
      auto &pending = record.operations.at(i);
      const bool complete = std::all_of(pending.files.begin(), pending.files.end(), [](const File &f) {
        return f.state == FileState::Completed || f.state == FileState::Failed;
      });
      if (!complete) break; // Do not overtake an unresolved stage/cancel/release.
      pending.state = std::any_of(pending.files.begin(), pending.files.end(), [](const File &f) {
        return f.state == FileState::Failed;
      }) ? OperationState::Failed : OperationState::Done;
      save(record);
    }
    Record snapshot;
    { std::lock_guard<std::mutex> guard(Lock(id)); snapshot = read(); }
    for (const auto &file : snapshot.files) {
      if (stopping) return false;
      if (file.state != FileState::Started) continue;
      const auto status = native->Observe(file.path);
      if (!status.disk && status.error.empty()) continue;
      std::lock_guard<std::mutex> guard(Lock(id));
      auto record = read();
      for (auto &local : record.files) if (local.path == file.path && local.state == FileState::Started) {
        local.state = status.error.empty() ? FileState::Completed : FileState::Failed;
        if (!status.error.empty()) local.error = status.error.substr(0, 2048);
        local.finishedAt = XrdOfsPrepStorage::Now();
      }
      save(record);
    }
    std::lock_guard<std::mutex> guard(Lock(id));
    auto record = Load(id, false);
    if (!Finished(record)) return false;
    if (!record.contains("completedAt")) {
      record["completedAt"] = XrdOfsPrepStorage::Now(); Save(record);
    }
    return true;
  }
  bool Process(const std::string &id) {
    if (native) return ProcessNative(id);
    Json snapshot;
    { std::lock_guard<std::mutex> guard(Lock(id)); snapshot = Load(id, false); }
    // Never hold a registry lock while invoking site code.
    auto observed = BackendQuery(id, snapshot.at("files"));
    Json pending;
    {
      std::lock_guard<std::mutex> guard(Lock(id));
      auto record = Load(id, false);
      Json before = record;
      for (const auto &file : observed.at("files")) {
        for (auto &local : record["files"]) if (local.at("path") == file.at("path")) {
          // Late backend replies cannot regress an already terminal result.
          if (!Terminal(local.at("state").get<std::string>())) {
            for (const char *key : {"state", "startedAt", "finishedAt", "error"})
              if (file.contains(key)) local[key] = file[key];
            if (Terminal(local.at("state").get<std::string>()) && !local.contains("finishedAt"))
              local["finishedAt"] = XrdOfsPrepStorage::Now();
          }
        }
      }
      std::set<std::string> acknowledged;
      for (const auto &value : observed.at("acknowledged")) acknowledged.insert(value);

      auto hasNewerOp = [&](size_t opIdx, const std::string &k, const std::string &path) {
        for (size_t j = opIdx + 1; j < record["operations"].size(); ++j) {
          const auto &other = record["operations"][j];
          if (other.at("kind") == k) {
            for (const auto &f : other.at("files")) {
              if (f.at("path") == path) return true;
            }
          }
        }
        return false;
      };

      for (size_t i = 0; i < record["operations"].size(); ++i) {
        auto &operation = record["operations"][i];
        const auto opId = operation.at("id").get<std::string>();
        if (!acknowledged.count(opId)) continue;
        if (operation.at("state") == "done" || operation.at("state") == "failed") continue;

        const auto kind = operation.at("kind").get<std::string>();
        if (kind == "cancel" || kind == "evict") {
          const std::string errorKey = kind == "cancel" ? "cancelError" : "releaseError";
          const std::string desired = kind == "cancel" ? "cancelRequested" : "releaseRequested";
          bool anyFailed = false;
          std::string failureReason;

          for (const auto &target : operation.at("files")) {
            const auto path = target.at("path").get<std::string>();
            auto obsIt = std::find_if(observed.at("files").begin(), observed.at("files").end(),
              [&](const Json &f) { return f.at("path") == path; });

            if (obsIt != observed.at("files").end() && obsIt->contains(errorKey)) {
              anyFailed = true;
              failureReason = (*obsIt)[errorKey].get<std::string>();
              if (!hasNewerOp(i, kind, path)) {
                for (auto &local : record["files"]) {
                  if (local.at("path") == path) {
                    local[errorKey] = failureReason;
                    local.erase(desired);
                    if (kind == "cancel") local.erase("cancelAcknowledged");
                    else if (kind == "evict") local.erase("released");
                    break;
                  }
                }
              }
            } else {
              if (!hasNewerOp(i, kind, path)) {
                for (auto &local : record["files"]) {
                  if (local.at("path") == path) {
                    local.erase(errorKey);
                    local.erase(desired);
                    if (kind == "cancel") {
                      local["cancelAcknowledged"] = true;
                    } else if (kind == "evict") {
                      local["released"] = true;
                    }
                    break;
                  }
                }
              }
            }
          }

          if (anyFailed) {
            operation["state"] = "failed";
            operation["error"] = failureReason;
          } else {
            operation["state"] = "done";
          }
        } else if (kind == "stage") {
          operation["state"] = "done";
        }
      }
      // Cancellation before the first dispatch can be completed without tape IO.
      bool allCancelled = true;
      for (const auto &file : record["files"]) allCancelled &= file.value("cancelRequested", false);
      if (allCancelled && record["operations"][0].at("state") == "pending" &&
          !observed.at("known").get<bool>()) {
        for (auto &file : record["files"]) {
          file["state"] = "CANCELLED";
          file["finishedAt"] = XrdOfsPrepStorage::Now();
          file.erase("cancelRequested");
          file.erase("cancelError");
          file["cancelAcknowledged"] = true;
        }
        for (auto &operation : record["operations"])
          if (operation.at("kind") == "stage" || operation.at("kind") == "cancel")
            operation["state"] = "done";
      }
      for (auto &operation : record["operations"]) {
        if (operation.at("state") != "done" && operation.at("state") != "failed") {
          operation["state"] = "dispatched";
          pending = operation;
          break;
        }
      }
      bool terminal = true;
      for (const auto &file : record["files"]) terminal &= Terminal(file.at("state").get<std::string>());
      if (terminal && !record.contains("completedAt")) record["completedAt"] = XrdOfsPrepStorage::Now();
      // A previous write of "dispatched" may have become visible before an
      // uncertain durability failure. Re-establish durability on every replay.
      if (record != before || !pending.is_null()) Save(record);
      if (Finished(record)) return true;
    }
    if (!pending.is_null()) {
      Arguments args(id, pending.at("files"), &pending);
      XrdOucErrInfo error("prepare-persist");
      XrdSecEntity service("prep"); service.name = const_cast<char *>("prepare"); service.tident = const_cast<char *>("prepare-persist");
      const auto kind = pending.at("kind").get<std::string>();
      args.args.opts = kind == "stage" ? Prep_STAGE : kind == "cancel" ? Prep_CANCEL : Prep_EVICT;
      const int rc = kind == "cancel" ? backend->cancel(args.args, error, &service)
                                      : backend->begin(args.args, error, &service);
      if (rc != SFS_OK && rc != SFS_DATA) {
        const int err = error.getErrInfo();
        if (!IsDefinitiveRejection(err)) Fail(EAGAIN, "prepare backend dispatch unavailable");
        const char *msg = error.getErrText();
        std::string reason = (msg && *msg) ? msg : "operation rejected by backend";
        std::lock_guard<std::mutex> guard(Lock(id));
        auto record = Load(id, false);
        size_t pendingIdx = 0;
        for (size_t i = 0; i < record["operations"].size(); ++i) {
          if (record["operations"][i].at("id") == pending.at("id")) {
            pendingIdx = i;
            break;
          }
        }
        auto hasNewerOp = [&](size_t opIdx, const std::string &k, const std::string &p) {
          for (size_t j = opIdx + 1; j < record["operations"].size(); ++j) {
            const auto &other = record["operations"][j];
            if (other.at("kind") == k) {
              for (const auto &f : other.at("files")) {
                if (f.at("path") == p) return true;
              }
            }
          }
          return false;
        };
        if (kind == "stage") {
          for (auto &file : record["files"]) {
            for (const auto &pf : pending.at("files")) {
              if (file.at("path") == pf.at("path") && !Terminal(file.at("state").get<std::string>())) {
                file["state"] = "FAILED";
                file["error"] = reason;
                file["finishedAt"] = XrdOfsPrepStorage::Now();
              }
            }
          }
        } else if (kind == "cancel") {
          for (auto &file : record["files"]) {
            for (const auto &pf : pending.at("files")) {
              if (file.at("path") == pf.at("path") && !hasNewerOp(pendingIdx, kind, file.at("path"))) {
                file["cancelError"] = reason;
                file.erase("cancelRequested");
                file.erase("cancelAcknowledged");
              }
            }
          }
        } else if (kind == "evict") {
          for (auto &file : record["files"]) {
            for (const auto &pf : pending.at("files")) {
              if (file.at("path") == pf.at("path") && !hasNewerOp(pendingIdx, kind, file.at("path"))) {
                file["releaseError"] = reason;
                file.erase("releaseRequested");
                file.erase("released");
              }
            }
          }
        }
        for (auto &operation : record["operations"]) {
          if (operation.at("id") == pending.at("id")) {
            operation["state"] = "failed";
            operation["error"] = reason;
          }
        }
        bool terminal = true;
        for (const auto &file : record["files"]) terminal &= Terminal(file.at("state").get<std::string>());
        if (terminal && !record.contains("completedAt")) record["completedAt"] = XrdOfsPrepStorage::Now();
        Save(record);
        if (Finished(record)) return true;
      }
    }
    return false;
  }
  void Scan(bool recovery) {
    if (!sweep) {
      if (!recovery && Clock::now() < nextSweep) return;
      sweep = store->List();
      if (!sweep) Fail(EIO, "prepare store returned no scan cursor");
    }
    XrdOfsPrepStorage::Page page;
    try { page = sweep->Next(64); }
    catch (...) { sweep.reset(); throw; }
    for (const auto &id : page.ids) {
      if (stopping) return;
      try {
        std::lock_guard<std::mutex> guard(Lock(id));
        auto record = Load(id, false);
        // Also reconcile records that became visible after an uncertain create
        // or were temporarily unreadable during the initial recovery scan.
        if (!Finished(record)) Schedule(id);
        else if (record.contains("completedAt") &&
                 XrdOfsPrepStorage::Now() > record.at("completedAt").get<uint64_t>() + retention)
          store->Erase(id, record.at("revision").get<uint64_t>());
      } catch (const std::system_error &ex) {
        if (recovery && ex.code().value() != ENOENT) Schedule(id);
        Log(id, ex.what());
      } catch (const std::exception &ex) { Log(id, ex.what()); }
    }
    if (page.done) {
      sweep.reset();
      ready = true;
      nextSweep = Clock::now() + std::chrono::seconds(60);
    }
  }
  void Run() {
    std::map<std::string, unsigned> failures;
    while (!stopping) {
      try { Scan(!ready); }
      catch (const std::exception &ex) { Log("scan", ex.what()); }
      if (!ready) { std::unique_lock<std::mutex> guard(queueMutex); wake.wait_for(guard, std::chrono::milliseconds(10)); continue; }
      std::string id;
      {
        std::unique_lock<std::mutex> guard(queueMutex);
        auto candidate = std::min_element(due.begin(), due.end(), [](const auto &a, const auto &b) { return a.second < b.second; });
        if (candidate == due.end() || candidate->second > Clock::now()) {
          wake.wait_for(guard, std::chrono::milliseconds(250)); continue;
        }
        id = candidate->first;
        // Keep it in the map during the call, so admission counts in-flight work.
        candidate->second = Clock::time_point::max();
      }
      bool finished = false;
      unsigned delay = 1;
      try { finished = Process(id); failures.erase(id); }
      catch (const std::exception &ex) {
        auto &count = failures[id]; count = std::min(count + 1, 5u);
        delay = 1u << count;
        Log(id, ex.what());
      }
      {
        std::lock_guard<std::mutex> guard(queueMutex);
        auto item = due.find(id);
        // Do not drop an operation added while a backend call was in flight.
        if (item != due.end() && item->second == Clock::time_point::max()) {
          if (finished) due.erase(item);
          else item->second = Clock::now() + std::chrono::seconds(delay);
        }
      }
    }
  }
};

XrdOfsPrepPersist::XrdOfsPrepPersist(const std::string &root, const std::string &identity,
  XrdOfsPrepare &backend, XrdOucEnv *environment, XrdSysError *log, size_t maxRequests, uint64_t retention)
  : XrdOfsPrepPersist(XrdOfsPrepFileStorage(root), identity, backend, environment, log, maxRequests, retention) {}
XrdOfsPrepPersist::XrdOfsPrepPersist(std::unique_ptr<XrdOfsPrepStorage> storage, const std::string &identity,
  XrdOfsPrepare &backend, XrdOucEnv *environment, XrdSysError *log, size_t maxRequests, uint64_t retention)
  : m_impl(new Impl(std::move(storage), identity, &backend, nullptr, environment, log, maxRequests, retention)) {}
XrdOfsPrepPersist::XrdOfsPrepPersist(std::unique_ptr<XrdOfsPrepStorage> storage, const std::string &identity,
  XrdOfsPrepBackend &backend, XrdOucEnv *environment, XrdSysError *log, size_t maxRequests, uint64_t retention)
  : m_impl(new Impl(std::move(storage), identity, nullptr, &backend, environment, log, maxRequests, retention)) {}
XrdOfsPrepPersist::~XrdOfsPrepPersist() = default;
void XrdOfsPrepPersist::PublishProfile(XrdOucEnv &environment) const {
  environment.Put("xrd.prepare.profile", "v1");
}
int XrdOfsPrepPersist::begin(XrdSfsPrep &args, XrdOucErrInfo &error, const XrdSecEntity *client) {
  return Guard(error, [&] {
    if (!(args.opts & Prep_EVICT)) return m_impl->Submit(args, error, client);
    std::string id;
    for (auto *opaque = args.oinfo; opaque; opaque = opaque->next) {
      XrdOucEnv env(opaque->text);
      const char *value = env.Get("xrd.prepare.request");
      if (!value || (!id.empty() && id != value)) Fail(EINVAL, "evict requires a consistent prepare request context");
      id = value;
    }
    if (!IsId(id)) Fail(EINVAL, "evict requires a prepare request context");
    return m_impl->Mutate(args, error, client, "evict", id);
  });
}
int XrdOfsPrepPersist::cancel(XrdSfsPrep &args, XrdOucErrInfo &error, const XrdSecEntity *client) {
  return Guard(error, [&] {
    std::string id = args.reqid ? args.reqid : "";
    bool deleting = id.compare(0, std::strlen(DeletePrefix), DeletePrefix) == 0;
    if (deleting) id.erase(0, std::strlen(DeletePrefix));
    if (!IsId(id)) Fail(ENOENT, "unknown prepare request");
    return m_impl->Mutate(args, error, client, "cancel", id, deleting);
  });
}
int XrdOfsPrepPersist::query(XrdSfsPrep &args, XrdOucErrInfo &error, const XrdSecEntity *client) {
  return Guard(error, [&] {
    const std::string id = args.reqid ? args.reqid : "";
    if (id == ArchiveQuery) {
      auto files = InputFiles(args);
      if (files.empty()) Fail(EINVAL, "empty archive information query");
      m_impl->Authorize(client, files, &args);
      if (m_impl->native) {
        Json result = Json::array();
        for (const auto &file : files) {
          const auto path = file.at("path").get<std::string>();
          const auto status = m_impl->native->Observe(path);
          Json out = {{"path", path}};
          if (!status.error.empty()) out["error"] = status.error.substr(0, 2048);
          else out["locality"] = status.disk ? (status.tape ? "DISK_AND_TAPE" : "DISK")
                                              : (status.tape ? "TAPE" : "NONE");
          result.push_back(std::move(out));
        }
        return Data(error, result.dump());
      }
      auto result = m_impl->BackendQuery(id, files);
      if (!result.at("known").get<bool>()) Fail(ENOTSUP, "backend does not support locality queries");
      return Data(error, result.at("files").dump());
    }
    std::lock_guard<std::mutex> guard(m_impl->Lock(id));
    auto record = m_impl->Load(id);
    m_impl->Owner(record, client, record.at("files"), &args);
    return Data(error, Status(record).dump());
  });
}
