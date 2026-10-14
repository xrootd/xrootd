// Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
#include "XrdOfsPrepRecord.hh"
#include <chrono>
#include <set>
#include <limits>
#include <type_traits>
#include <uuid/uuid.h>
using namespace XrdOfsPrepProtocol;
namespace XrdOfsPrep {
namespace {
const char *const fileStates[] = {"SUBMITTED", "STARTED", "COMPLETED", "FAILED", "CANCELLED"};
const char *const operationKinds[] = {"stage", "cancel", "evict"};
const char *const operationStates[] = {"pending", "dispatched", "done", "failed"};
template <typename T, size_t N> T Enum(const Json &value, const char *const (&names)[N]) {
  for (size_t i = 0; i < N; ++i) if (value == names[i]) return static_cast<T>(i);
  Fail(EIO, "invalid prepare record enum"); return T{};
}
template <typename T, size_t N> const char *Name(T value, const char *const (&names)[N]) {
  const auto index = static_cast<size_t>(value);
  if (index >= N) Fail(EIO, "invalid prepare record enum");
  return names[index];
}
uint64_t Unsigned(const Json &value) {
  if (!value.is_number_integer() ||
      (!value.is_number_unsigned() && value.get<int64_t>() < 0))
    Fail(EIO, "invalid unsigned prepare record field");
  return value.get<uint64_t>();
}
template <typename T> void ReadField(const Json &j, const char *key, std::optional<T> &value) {
  if (j.contains(key)) {
    if constexpr (std::is_same_v<T, uint64_t>) value = Unsigned(j.at(key));
    else value = j.at(key).get<T>();
  }
}
template <typename T> void WriteField(Json &j, const char *key, const std::optional<T> &value) {
  if (value) j[key] = *value;
}
Value DecodeValue(const Json &j) {
  Value value;
  if (j.is_object()) {
    Value::Object object;
    for (auto it = j.begin(); it != j.end(); ++it) object.emplace(it.key(), DecodeValue(it.value()));
    value.data = std::move(object);
  } else if (j.is_array()) {
    Value::Array array;
    for (const auto &item : j) array.push_back(DecodeValue(item));
    value.data = std::move(array);
  } else if (j.is_null()) value.data = nullptr;
  else if (j.is_boolean()) value.data = j.get<bool>();
  else if (j.is_number_unsigned()) value.data = j.get<uint64_t>();
  else if (j.is_number_integer()) value.data = j.get<int64_t>();
  else if (j.is_number_float()) value.data = j.get<double>();
  else if (j.is_string()) value.data = j.get<std::string>();
  else Fail(EIO, "invalid prepare metadata value");
  return value;
}
Json EncodeValue(const Value &value) {
  return std::visit([](const auto &item) -> Json {
    using T = std::decay_t<decltype(item)>;
    if constexpr (std::is_same_v<T, Value::Object>) {
      auto object = Json::object();
      for (const auto &entry : item) object[entry.first] = EncodeValue(entry.second);
      return object;
    } else if constexpr (std::is_same_v<T, Value::Array>) {
      auto array = Json::array();
      for (const auto &entry : item) array.push_back(EncodeValue(entry));
      return array;
    } else return item;
  }, value.data);
}
File DecodeFile(const Json &j) {
  File f;
  f.path = j.at("path").get<std::string>();
  f.state = Enum<FileState>(j.at("state"), fileStates);
  ReadField(j, "diskLifetime", f.diskLifetime);
  ReadField(j, "startedAt", f.startedAt);
  ReadField(j, "finishedAt", f.finishedAt);
  ReadField(j, "error", f.error);
  ReadField(j, "cancelError", f.cancelError);
  ReadField(j, "releaseError", f.releaseError);
  ReadField(j, "cancelRequested", f.cancelRequested);
  ReadField(j, "cancelAcknowledged", f.cancelAcknowledged);
  ReadField(j, "releaseRequested", f.releaseRequested);
  ReadField(j, "released", f.released);
  if (j.contains("targetedMetadata")) {
    auto value = DecodeValue(j.at("targetedMetadata"));
    f.targetedMetadata = std::get<Value::Object>(std::move(value.data));
  }
  return f;
}
Json EncodeFile(const File &f) {
  Json j = {{"path", f.path}, {"state", Name(f.state, fileStates)}};
  WriteField(j, "diskLifetime", f.diskLifetime);
  WriteField(j, "startedAt", f.startedAt);
  WriteField(j, "finishedAt", f.finishedAt);
  WriteField(j, "error", f.error);
  WriteField(j, "cancelError", f.cancelError);
  WriteField(j, "releaseError", f.releaseError);
  WriteField(j, "cancelRequested", f.cancelRequested);
  WriteField(j, "cancelAcknowledged", f.cancelAcknowledged);
  WriteField(j, "releaseRequested", f.releaseRequested);
  WriteField(j, "released", f.released);
  if (f.targetedMetadata) { Value value; value.data = *f.targetedMetadata; j["targetedMetadata"] = EncodeValue(value); }
  return j;
}
}
void ValidateOwner(const Owner &owner) {
  auto text = [](const std::optional<std::string> &s) {
    return s && !s->empty() && s->size() <= 4096 && s->find('\0') == std::string::npos;
  };
  if (owner.kind == "token") {
    if (!text(owner.issuer) || !text(owner.subject) || owner.name)
      Fail(EACCES, "invalid verified token owner");
  } else if (owner.kind.empty() || owner.kind.size() > 32 ||
             owner.kind.find('\0') != std::string::npos || !text(owner.name) ||
             owner.issuer || owner.subject || *owner.name == "anon" ||
             *owner.name == "anonymous" || *owner.name == "nobody" || *owner.name == "unknown") {
    Fail(EACCES, "invalid authenticated mapped owner");
  }
}
void ValidateRecord(const Record &r) {
  try {
    ValidateOwner(r.owner);
    // Reject signed-negative values decoded into unsigned fields and leave
    // headroom for retention arithmetic. Do not compare different clocks.
    auto timestamp = [](uint64_t value) {
      return value && value <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    };
    if (r.schema != 1 || !IsId(r.id) || r.backend.empty() ||
        r.backend.size() > 4096 || r.backend.find('\0') != std::string::npos ||
        !timestamp(r.createdAt) || !timestamp(r.startedAt) ||
        (r.completedAt && !timestamp(*r.completedAt)) ||
        r.files.empty() || r.files.size() > MaxFiles || r.operations.empty() ||
        r.operations.size() > 1 + 2 * MaxFiles)
      Fail(EIO, "invalid prepare record header");
    std::map<std::string, Json> manifest;
    bool terminal = true;
    for (const auto &f : r.files) {
      auto metadata = Metadata(EncodeFile(f));
      if (Path(f.path) != f.path || !manifest.emplace(f.path, metadata).second)
        Fail(EIO, "invalid prepare manifest path");
      const bool done = Terminal(Name(f.state, fileStates));
      terminal = terminal && done;
      // Backend timestamps can come from a different clock. Do not order them
      // against coordinator times; reject impossible state/presence instead.
      if ((f.startedAt && !timestamp(*f.startedAt)) ||
          (f.finishedAt && (!timestamp(*f.finishedAt) || !done)))
        Fail(EIO, "invalid prepare file timestamps");
    }
    if (r.completedAt && !terminal) Fail(EIO, "incomplete prepare has completion timestamp");
    for (size_t i = 0; i < r.operations.size(); ++i) {
      const auto &op = r.operations[i];
      if (op.id != r.id + ":" + std::to_string(i + 1) || op.files.empty() ||
          op.files.size() > r.files.size() ||
          (i == 0 ? op.kind != OperationKind::Stage : op.kind == OperationKind::Stage))
        Fail(EIO, "invalid prepare operation identity");
      Name(op.kind, operationKinds); Name(op.state, operationStates);
      std::set<std::string> paths;
      for (const auto &f : op.files) {
        Name(f.state, fileStates);
        auto member = manifest.find(f.path);
        if (member == manifest.end() || !paths.insert(f.path).second ||
            Metadata(EncodeFile(f)) != member->second)
          Fail(EIO, "prepare operation does not match admitted manifest");
      }
      if (i == 0 && paths.size() != manifest.size())
        Fail(EIO, "initial prepare operation must stage the full manifest");
    }
    if (EncodeRecord(r).dump().size() > MaxRecord) Fail(EIO, "oversized prepare record");
  } catch (const std::exception &) { Fail(EIO, "invalid durable prepare record"); }
}
Record DecodeRecord(const Json &j) {
  try {
    Record r;
    if (j.at("schema") != 1) Fail(EIO, "unsupported prepare record schema");
    if (!j.at("files").is_array() || !j.at("operations").is_array())
      Fail(EIO, "invalid prepare record arrays");
    r.id = j.at("id").get<std::string>();
    r.backend = j.at("backend").get<std::string>();
    r.revision = j.contains("revision") ? Unsigned(j.at("revision")) : 0;
    r.createdAt = Unsigned(j.at("createdAt"));
    r.startedAt = Unsigned(j.at("startedAt"));
    ReadField(j, "completedAt", r.completedAt);
    r.deleted = j.at("deleted").get<bool>();
    const auto &owner = j.at("owner");
    r.owner.kind = owner.at("kind").get<std::string>();
    ReadField(owner, "name", r.owner.name);
    ReadField(owner, "issuer", r.owner.issuer);
    ReadField(owner, "subject", r.owner.subject);
    for (const auto &file : j.at("files")) r.files.push_back(DecodeFile(file));
    for (const auto &operation : j.at("operations")) {
      Operation op;
      op.id = operation.at("id").get<std::string>();
      op.kind = Enum<OperationKind>(operation.at("kind"), operationKinds);
      op.state = Enum<OperationState>(operation.at("state"), operationStates);
      ReadField(operation, "error", op.error);
      for (const auto &file : operation.at("files")) op.files.push_back(DecodeFile(file));
      r.operations.push_back(std::move(op));
    }
    return r;
  } catch (const Json::exception &) { Fail(EIO, "invalid prepare record"); }
    catch (const std::bad_variant_access &) { Fail(EIO, "invalid prepare metadata object"); }
  return {};
}
Json EncodeRecord(const Record &r) {
  if (r.schema != 1) Fail(EIO, "unsupported prepare record schema");
  Json j = {{"schema", r.schema}, {"id", r.id}, {"backend", r.backend},
    {"revision", r.revision}, {"createdAt", r.createdAt}, {"startedAt", r.startedAt},
    {"deleted", r.deleted}, {"owner", {{"kind", r.owner.kind}}},
    {"files", Json::array()}, {"operations", Json::array()}};
  WriteField(j, "completedAt", r.completedAt);
  WriteField(j["owner"], "name", r.owner.name);
  WriteField(j["owner"], "issuer", r.owner.issuer);
  WriteField(j["owner"], "subject", r.owner.subject);
  for (const auto &file : r.files) j["files"].push_back(EncodeFile(file));
  for (const auto &op : r.operations) {
    Json operation = {{"id", op.id}, {"kind", Name(op.kind, operationKinds)},
      {"state", Name(op.state, operationStates)}, {"files", Json::array()}};
    WriteField(operation, "error", op.error);
    for (const auto &file : op.files) operation["files"].push_back(EncodeFile(file));
    j["operations"].push_back(std::move(operation));
  }
  return j;
}
}
uint64_t XrdOfsPrepStorage::Now() {
  return std::chrono::duration_cast<std::chrono::seconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
}
std::string XrdOfsPrepStorage::NewId() {
  uuid_t uuid;
  uuid_generate_random(uuid);
  char text[37]; uuid_unparse_lower(uuid, text); return text;
}
