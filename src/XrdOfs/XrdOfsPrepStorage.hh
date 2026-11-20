#ifndef XRD_OFS_PREP_STORAGE_HH
#define XRD_OFS_PREP_STORAGE_HH
// Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

// Storage-independent durable prepare records. Metadata is application data;
// neither its in-memory representation nor the record requires a JSON codec.
// Numeric values are preserved; a codec may read nonnegative signed integers
// back as uint64_t (as the existing JSON file format does).
namespace XrdOfsPrep {
struct Value {
  using Array = std::vector<Value>;
  using Object = std::map<std::string, Value>;
  std::variant<std::nullptr_t, bool, int64_t, uint64_t, double,
               std::string, Array, Object> data{nullptr};
};
enum class FileState { Submitted, Started, Completed, Failed, Cancelled };
enum class OperationKind { Stage, Cancel, Evict };
enum class OperationState { Pending, Dispatched, Done, Failed };
struct Owner {
  std::string kind;
  std::optional<std::string> name, issuer, subject;
};
struct File {
  std::string path;
  FileState state = FileState::Submitted;
  std::optional<std::string> diskLifetime;
  std::optional<Value::Object> targetedMetadata;
  std::optional<uint64_t> startedAt, finishedAt;
  std::optional<std::string> error, cancelError, releaseError;
  std::optional<bool> cancelRequested, cancelAcknowledged, releaseRequested, released;
};
struct Operation {
  std::string id;
  OperationKind kind = OperationKind::Stage;
  OperationState state = OperationState::Pending;
  std::vector<File> files;
  std::optional<std::string> error;
};
struct Record {
  uint32_t schema = 1;
  std::string id, backend;
  uint64_t revision = 0, createdAt = 0, startedAt = 0;
  std::optional<uint64_t> completedAt;
  Owner owner;
  bool deleted = false;
  std::vector<File> files;
  std::vector<Operation> operations;
};

// A failed write may nevertheless have committed. Unknown requires rereading
// and reconciliation, never blindly repeating a side effect with a new ID.
class StorageError : public std::system_error {
public:
  enum class Outcome { Unchanged, Unknown };
  StorageError(int code, const std::string &message, Outcome outcome)
    : std::system_error(code, std::generic_category(), message), outcome(outcome) {}
  const Outcome outcome;
};
}

// Implementations must be thread safe and provide atomic, durable records.
// Each namespace has ONE active coordinator. The store must hold an exclusive
// writer lock, or its owner must enforce leadership for the coordinator's full
// lifetime, including in-flight backend calls. CAS alone is not leadership.
// Calls must have bounded I/O timeouts so coordinator shutdown can join workers.
// No backend action may run before its intent has been durably committed.
class XrdOfsPrepStorage {
public:
  using Record = XrdOfsPrep::Record;
  struct Page {
    std::vector<std::string> ids;
    bool done = false;
  };
  class Cursor {
  public:
    virtual ~Cursor() = default;
    // Bound both work and returned IDs by limit. Empty intermediate pages are
    // allowed. A complete scan must visit records present throughout the scan;
    // duplicates and concurrent insertions/deletions are allowed.
    virtual Page Next(size_t limit) = 0;
  };
  virtual ~XrdOfsPrepStorage() = default;
  virtual Record Read(const std::string &id) = 0; // ENOENT only if absent
  // Return the committed revision (1 for Create, expected + 1 for Update).
  // Create never overwrites (EEXIST); Update and Erase compare the revision
  // atomically (EAGAIN on conflict, ENOENT if absent). Mutations report
  // StorageError; other exceptions must conservatively be treated as Unknown.
  virtual uint64_t Create(const Record &record) = 0;
  virtual uint64_t Update(const Record &record, uint64_t expected) = 0;
  virtual void Erase(const std::string &id, uint64_t expected) = 0;
  virtual std::unique_ptr<Cursor> List() = 0;
  static std::string NewId();
  static uint64_t Now();
};
// The default JSON file implementation. The returned object holds the root's
// exclusive writer lock until destruction; existing on-disk records are kept.
std::unique_ptr<XrdOfsPrepStorage> XrdOfsPrepFileStorage(const std::string &root);
#endif
