#ifndef XRD_OFS_PREP_PROTOCOL_HH
#define XRD_OFS_PREP_PROTOCOL_HH
// Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
// Version 1 of the optional durable prepare backend profile. No HTTP types.
#include "XrdOuc/XrdOucJson.hh"
#include <cerrno>
#include <cctype>
#include <stdexcept>
#include <string>
#include <system_error>

namespace XrdOfsPrepProtocol {
using Json = nlohmann::json;
constexpr size_t MaxFiles = 48;
constexpr size_t MaxMetadata = 1024;
constexpr size_t MaxRecord = 4 * 1024 * 1024;
constexpr const char *ArchiveQuery = "@xrdprep-v1:archiveinfo";
constexpr const char *DeletePrefix = "@xrdprep-v1:delete:";

inline void Fail(int error, const char *message) {
  throw std::system_error(error, std::generic_category(), message);
}
inline bool IsId(const std::string &id) {
  if (id.size() != 36) return false;
  for (size_t i = 0; i < id.size(); ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (id[i] != '-') return false;
    } else if (!((id[i] >= '0' && id[i] <= '9') || (id[i] >= 'a' && id[i] <= 'f')))
      return false;
  }
  return true;
}
inline std::string Path(const std::string &path) {
  if (path.empty() || path.front() != '/' || path.size() > 1024)
    Fail(EINVAL, "prepare paths must be absolute and at most 1024 bytes");
  std::string result;
  size_t start = 1;
  while (start <= path.size()) {
    auto end = path.find('/', start);
    if (end == std::string::npos) end = path.size();
    auto component = path.substr(start, end - start);
    if (component == "." || component == "..") Fail(EINVAL, "path traversal is not allowed");
    for (unsigned char c : component)
      if (c < 32 || c == 127 || c == '?' || c == '#')
        Fail(EINVAL, "path cannot be represented by native prepare");
    if (!component.empty()) result += "/" + component;
    start = end + 1;
  }
  if (result.empty()) Fail(EINVAL, "prepare requires a file path");
  return result;
}
inline std::string Hex(const std::string &data) {
  static const char digits[] = "0123456789abcdef";
  std::string out;
  out.reserve(data.size() * 2);
  for (unsigned char c : data) { out += digits[c >> 4]; out += digits[c & 15]; }
  return out;
}
inline std::string Unhex(const std::string &data) {
  if (data.size() % 2 || data.size() > 2 * MaxMetadata) Fail(EINVAL, "invalid prepare metadata size");
  auto nibble = [](char c) -> unsigned {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    Fail(EINVAL, "invalid prepare metadata encoding"); return 0;
  };
  std::string out;
  for (size_t i = 0; i < data.size(); i += 2)
    out += static_cast<char>((nibble(data[i]) << 4) | nibble(data[i + 1]));
  return out;
}
inline Json Metadata(const Json &file) {
  Json out = Json::object();
  if (file.contains("diskLifetime")) {
    if (!file["diskLifetime"].is_string() || file["diskLifetime"].get<std::string>().empty())
      Fail(EINVAL, "diskLifetime must be a duration string");
    out["diskLifetime"] = file["diskLifetime"];
  }
  if (file.contains("targetedMetadata")) {
    if (!file["targetedMetadata"].is_object()) Fail(EINVAL, "targetedMetadata must be an object");
    out["targetedMetadata"] = file["targetedMetadata"];
  }
  if (out.dump().size() > MaxMetadata) Fail(E2BIG, "per-file metadata exceeds 1024 bytes");
  return out;
}
inline bool Terminal(const std::string &state) {
  return state == "COMPLETED" || state == "FAILED" || state == "CANCELLED";
}
inline bool State(const std::string &state) {
  return state == "SUBMITTED" || state == "STARTED" || Terminal(state);
}
}
#endif
