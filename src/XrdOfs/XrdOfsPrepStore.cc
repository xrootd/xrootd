// Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
#include "XrdOfsPrepStore.hh"
#include "XrdOfsPrepRecord.hh"
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits>
#include <vector>

using namespace XrdOfsPrepProtocol;
namespace {
std::function<void(int)> g_syncHook;
struct Fd {
  int value;
  explicit Fd(int fd) : value(fd) { if (fd < 0) Fail(errno, "open prepare storage"); }
  ~Fd() { close(value); }
  Fd(const Fd &) = delete;
  Fd &operator=(const Fd &) = delete;
};
void Sync(int fd) {
  if (g_syncHook) g_syncHook(fd);
  int rc;
  do { rc = fsync(fd); } while (rc < 0 && errno == EINTR);
  if (rc < 0) Fail(errno, "synchronize prepare storage");
}
}
void XrdOfsPrepStore::SetSyncHook(std::function<void(int)> hook) {
  g_syncHook = std::move(hook);
}
int XrdOfsPrepStore::Subdir(int parent, const char *name, const std::string &relPath, bool create) const {
  if (create) {
    bool needSync = false;
    {
      std::lock_guard<std::mutex> guard(m_shardMutex);
      needSync = (m_syncedShards.find(relPath) == m_syncedShards.end());
    }
    if (needSync) {
      if (mkdirat(parent, name, 0700) != 0 && errno != EEXIST)
        Fail(errno, "create prepare shard");
      Sync(parent);
      std::lock_guard<std::mutex> guard(m_shardMutex);
      m_syncedShards.insert(relPath);
    }
  }
  int fd = openat(parent, name, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (fd < 0) Fail(errno, "open prepare shard");
  return fd;
}
XrdOfsPrepStore::XrdOfsPrepStore(const std::string &root) : m_root(root) {
  if (!m_root.is_absolute()) Fail(EINVAL, "prepare state root must be absolute");
  // Record every missing ancestor before mkdir, not only the final directory.
  // Synchronize all newly created directory entries before accepting requests.
  std::vector<std::filesystem::path> created;
  for (auto path = m_root; !std::filesystem::exists(path); path = path.parent_path())
    created.push_back(path);
  std::filesystem::create_directories(m_root);
  for (const auto &path : created) {
    Fd child(open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC)); Sync(child.value);
    Fd parent(open(path.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC)); Sync(parent.value);
  }
  Fd dir(open(m_root.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
  struct stat status{};
  if (fstat(dir.value, &status) || status.st_uid != geteuid())
    Fail(EACCES, "prepare state root must be owned by the service account");
  if (fchmod(dir.value, 0700)) Fail(errno, "secure prepare state root");
  Fd lock(openat(dir.value, ".lock", O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600));
  if (flock(lock.value, LOCK_EX | LOCK_NB)) Fail(errno, "prepare store already has a writer");
  Fd requests(Subdir(dir.value, "requests", "requests", true));
  // Synchronize the root and its parent, including the new root directory entry.
  Sync(dir.value);
  Fd parent(open(m_root.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
  Sync(parent.value);
  m_rootFd = dup(dir.value);
  m_lockFd = dup(lock.value);
  if (m_rootFd < 0 || m_lockFd < 0) {
    if (m_rootFd >= 0) close(m_rootFd);
    if (m_lockFd >= 0) close(m_lockFd);
    Fail(errno, "retain prepare store descriptors");
  }
  fcntl(m_rootFd, F_SETFD, FD_CLOEXEC);
  fcntl(m_lockFd, F_SETFD, FD_CLOEXEC);
}
XrdOfsPrepStore::~XrdOfsPrepStore() {
  if (m_lockFd >= 0) close(m_lockFd);
  if (m_rootFd >= 0) close(m_rootFd);
}
int XrdOfsPrepStore::Directory(const std::string &id, bool create) const {
  if (!IsId(id)) Fail(ENOENT, "unknown prepare request");
  // UUIDv4's first bytes are uniformly random. The persisted layout fixes this
  // derivation; no implementation-dependent std::hash is used.
  const std::string s1 = id.substr(0, 2);
  const std::string s2 = id.substr(2, 2);
  Fd requests(Subdir(m_rootFd, "requests", "requests", create));
  Fd first(Subdir(requests.value, s1.c_str(), "requests/" + s1, create));
  return Subdir(first.value, s2.c_str(), "requests/" + s1 + "/" + s2, create);
}
Json XrdOfsPrepStore::Load(const std::string &id) const {
  Fd dir(Directory(id, false));
  // Inspect special files without blocking (e.g. a FIFO with no writer).
  Fd fd(openat(dir.value, (id + ".json").c_str(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC));
  struct stat status{};
  if (fstat(fd.value, &status)) Fail(errno, "stat prepare record");
  if (!S_ISREG(status.st_mode) || status.st_size <= 0 ||
      static_cast<uint64_t>(status.st_size) > MaxRecord)
    Fail(EIO, "invalid prepare record size or type");
  std::string contents(static_cast<size_t>(status.st_size), '\0');
  size_t offset = 0;
  while (offset < contents.size()) {
    const auto count = read(fd.value, &contents[offset], contents.size() - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) Fail(count < 0 ? errno : EIO, "read prepare record");
    offset += count;
  }
  try {
    auto record = Json::parse(contents);
    if (record.at("schema") != 1 || record.at("id") != id ||
        !record.at("revision").is_number_unsigned())
      Fail(EIO, "unsupported or corrupt prepare record");
    return record;
  } catch (const Json::exception &) { Fail(EIO, "corrupt prepare JSON record"); }
  return {};
}
void XrdOfsPrepStore::Save(Json &record, bool create) const {
  const std::string id = record.at("id").get<std::string>();
  record["revision"] = record.value("revision", uint64_t(0)) + 1;
  auto contents = record.dump();
  if (contents.size() > MaxRecord) Fail(E2BIG, "prepare record exceeds storage limit");
  Fd dir(Directory(id, true));
  const std::string temporary = ".tmp-" + NewId();
  const std::string destination = id + ".json";
  try {
    Fd fd(openat(dir.value, temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL |
                 O_NOFOLLOW | O_CLOEXEC, 0600));
    size_t offset = 0;
    while (offset < contents.size()) {
      const auto count = write(fd.value, contents.data() + offset, contents.size() - offset);
      if (count < 0 && errno == EINTR) continue;
      if (count <= 0) Fail(count < 0 ? errno : EIO, "write prepare record");
      offset += count;
    }
    Sync(fd.value);
    if (create) {
      // Publish without replacing an existing UUID, even on a collision.
      if (linkat(dir.value, temporary.c_str(), dir.value, destination.c_str(), 0))
        Fail(errno, "publish prepare request");
      if (unlinkat(dir.value, temporary.c_str(), 0)) Fail(errno, "remove prepare temporary");
    } else if (renameat(dir.value, temporary.c_str(), dir.value, destination.c_str()))
      Fail(errno, "replace prepare record");
    Sync(dir.value);
  } catch (...) {
    unlinkat(dir.value, temporary.c_str(), 0);
    throw;
  }
}
void XrdOfsPrepStore::Erase(const std::string &id) const {
  Fd dir(Directory(id, false));
  if (unlinkat(dir.value, (id + ".json").c_str(), 0)) Fail(errno, "remove prepare record");
  Sync(dir.value);
}

namespace {
using StorageError = XrdOfsPrep::StorageError;
[[noreturn]] void Unchanged(int code, const char *message) {
  throw StorageError(code, message, StorageError::Outcome::Unchanged);
}
// fsync can fail after publication. Never classify such a failure as rollback.
template <typename Function> auto Write(Function action) -> decltype(action()) {
  try { return action(); }
  catch (const std::system_error &ex) {
    throw StorageError(ex.code().value(), ex.what(), StorageError::Outcome::Unknown);
  }
}
class FileCursor final : public XrdOfsPrepStorage::Cursor {
  std::filesystem::recursive_directory_iterator current;
public:
  explicit FileCursor(const std::filesystem::path &root) : current(root / "requests") {}
  XrdOfsPrepStorage::Page Next(size_t limit) override {
    if (!limit) Fail(EINVAL, "prepare scan limit must be positive");
    XrdOfsPrepStorage::Page page;
    const std::filesystem::recursive_directory_iterator end;
    while (current != end && limit--) {
      const auto path = current->path();
      const bool regular = std::filesystem::is_regular_file(current->symlink_status());
      ++current;
      if (regular && path.extension() == ".json" && IsId(path.stem().string()))
        page.ids.push_back(path.stem().string());
    }
    page.done = current == end;
    return page;
  }
};
}
XrdOfsPrepStorage::Record XrdOfsPrepStore::Read(const std::string &id) {
  return XrdOfsPrep::DecodeRecord(Load(id));
}
uint64_t XrdOfsPrepStore::Create(const Record &record) {
  std::lock_guard<std::mutex> guard(m_revisionMutex);
  if (record.revision) Unchanged(EINVAL, "new prepare record has a revision");
  auto json = XrdOfsPrep::EncodeRecord(record);
  try { Load(record.id); }
  catch (const std::system_error &ex) {
    if (ex.code().value() != ENOENT) throw;
    return Write([&] { Save(json, true); return json.at("revision").get<uint64_t>(); });
  }
  Unchanged(EEXIST, "prepare request already exists");
}
uint64_t XrdOfsPrepStore::Update(const Record &record, uint64_t expected) {
  std::lock_guard<std::mutex> guard(m_revisionMutex);
  auto json = XrdOfsPrep::EncodeRecord(record);
  if (record.revision != expected) Unchanged(EINVAL, "inconsistent prepare revision");
  if (expected == std::numeric_limits<uint64_t>::max()) Unchanged(EOVERFLOW, "prepare revision exhausted");
  if (Load(record.id).at("revision").get<uint64_t>() != expected)
    Unchanged(EAGAIN, "prepare revision changed");
  return Write([&] { Save(json); return json.at("revision").get<uint64_t>(); });
}
void XrdOfsPrepStore::Erase(const std::string &id, uint64_t expected) {
  std::lock_guard<std::mutex> guard(m_revisionMutex);
  if (Load(id).at("revision").get<uint64_t>() != expected)
    Unchanged(EAGAIN, "prepare revision changed");
  Write([&] { Erase(id); });
}
std::unique_ptr<XrdOfsPrepStorage::Cursor> XrdOfsPrepStore::List() {
  return std::make_unique<FileCursor>(m_root);
}
std::unique_ptr<XrdOfsPrepStorage> XrdOfsPrepFileStorage(const std::string &root) {
  return std::make_unique<XrdOfsPrepStore>(root);
}
