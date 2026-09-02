/******************************************************************************/
/*                                                                            */
/*              X r d H t t p T a p e A p i S t o r e . c c                 */
/*                                                                            */
/* (c) 2026 by the XRootD Collaboration                                       */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/******************************************************************************/

#include "XrdHttpTapeApiStore.hh"

#include "XrdOuc/XrdOucUtils.hh"

#include <cctype>
#include <chrono>
#include <fstream>
#include <set>
#include <system_error>
#include <uuid/uuid.h>

namespace
{
using Json = nlohmann::json;
namespace fs = std::filesystem;

XrdHttpTapeApiStore::Status Error(int code, const std::string &message)
{
  return {code, message};
}

bool IsWithin(const fs::path &root, const fs::path &path)
{
  auto rootPart = root.begin();
  auto pathPart = path.begin();
  while(rootPart != root.end() && pathPart != path.end())
  {
    if(*rootPart != *pathPart) return false;
    ++rootPart;
    ++pathPart;
  }
  return rootPart == root.end();
}

bool IsRegularFile(const fs::path &path, bool &regular)
{
  std::error_code error;
  const fs::file_status status = fs::status(path, error);
  if(error == std::errc::no_such_file_or_directory)
  {
    regular = false;
    return true;
  }
  if(error) return false;
  regular = fs::is_regular_file(status);
  return true;
}
}

XrdHttpTapeApiStore::XrdHttpTapeApiStore(fs::path root)
  : m_root(std::move(root)),
    m_archiveRoot(m_root / "archive"),
    m_diskRoot(m_root / "disk"),
    m_requestsRoot(m_root / "requests")
{
}

XrdHttpTapeApiStore::Status XrdHttpTapeApiStore::Initialize()
{
  std::error_code error;
  fs::create_directories(m_archiveRoot, error);
  if(error) return Error(500, "could not create the tape archive directory");
  fs::create_directories(m_diskRoot, error);
  if(error) return Error(500, "could not create the disk directory");
  fs::create_directories(m_requestsRoot, error);
  if(error) return Error(500, "could not create the request directory");

  m_root = fs::weakly_canonical(m_root, error);
  if(error) return Error(500, "could not resolve the Tape API directory");
  m_archiveRoot = m_root / "archive";
  m_diskRoot = m_root / "disk";
  m_requestsRoot = m_root / "requests";
  return {};
}

std::uint64_t XrdHttpTapeApiStore::Now()
{
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(
    std::chrono::duration_cast<std::chrono::seconds>(now).count());
}

std::string XrdHttpTapeApiStore::GenerateRequestId()
{
  uuid_t value;
  uuid_generate_random(value);
  char text[37] = {};
  uuid_unparse_lower(value, text);
  return text;
}

bool XrdHttpTapeApiStore::IsRequestId(const std::string &requestId)
{
  if(requestId.size() != 36) return false;
  for(std::size_t index = 0; index < requestId.size(); ++index)
  {
    if(index == 8 || index == 13 || index == 18 || index == 23)
    {
      if(requestId[index] != '-') return false;
    }
    else if(!std::isxdigit(static_cast<unsigned char>(requestId[index])))
    {
      return false;
    }
  }
  return true;
}

XrdHttpTapeApiStore::Status XrdHttpTapeApiStore::ResolvePath(
  const fs::path &root, const std::string &path, std::string &normalized,
  fs::path &resolved) const
{
  normalized = XrdOucUtils::NormalizePath(path);
  if(normalized.empty() || normalized.front() != '/')
  {
    return Error(400, "file paths must be absolute");
  }

  const fs::path relative = fs::path(normalized).relative_path();
  for(const auto &part : relative)
  {
    if(part == "." || part == "..")
    {
      return Error(400, "file paths must not contain traversal components");
    }
  }

  std::error_code error;
  resolved = fs::weakly_canonical(root / relative, error);
  if(error || !IsWithin(root, resolved))
  {
    return Error(400, "file path is outside the configured storage root");
  }
  return {};
}

XrdHttpTapeApiStore::Status XrdHttpTapeApiStore::WriteRequest(
  const Json &request) const
{
  const std::string requestId = request.at("id").get<std::string>();
  const fs::path destination = m_requestsRoot / (requestId + ".json");
  const fs::path temporary = m_requestsRoot / (requestId + ".json.tmp");

  std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
  if(!output) return Error(500, "could not write the stage request");
  output << request.dump();
  output.close();
  if(!output)
  {
    std::error_code ignored;
    fs::remove(temporary, ignored);
    return Error(500, "could not write the stage request");
  }

  std::error_code error;
  fs::rename(temporary, destination, error);
  if(error)
  {
    fs::remove(temporary, error);
    return Error(500, "could not persist the stage request");
  }
  return {};
}

XrdHttpTapeApiStore::Status XrdHttpTapeApiStore::ReadRequest(
  const std::string &requestId, Json &request) const
{
  if(!IsRequestId(requestId)) return Error(404, "unknown stage request");

  std::ifstream input(m_requestsRoot / (requestId + ".json"),
                      std::ios::binary);
  if(!input) return Error(404, "unknown stage request");
  try
  {
    input >> request;
  }
  catch(const std::exception &)
  {
    return Error(500, "could not read the stage request");
  }
  return {};
}

Json XrdHttpTapeApiStore::StatusResponse(const Json &request)
{
  Json response = {
    {"id", request.at("id")},
    {"createdAt", request.at("createdAt")},
    {"startedAt", request.at("startedAt")},
    {"files", Json::array()}
  };
  if(request.contains("completedAt"))
  {
    response["completedAt"] = request["completedAt"];
  }

  for(const auto &stored : request.at("files"))
  {
    Json file = {{"path", stored.at("path")}, {"state", stored.at("state")}};
    for(const char *field : {"startedAt", "finishedAt", "error"})
    {
      if(stored.contains(field)) file[field] = stored[field];
    }
    response["files"].push_back(std::move(file));
  }
  return response;
}

XrdHttpTapeApiStore::Status XrdHttpTapeApiStore::CreateStage(
  const Json &files, std::string &requestId)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  requestId = GenerateRequestId();
  const std::uint64_t createdAt = Now();
  Json request = {
    {"id", requestId},
    {"createdAt", createdAt},
    {"startedAt", createdAt},
    {"files", Json::array()}
  };

  for(const auto &input : files)
  {
    Json file = input;
    std::string normalized;
    fs::path archivePath;
    Status status = ResolvePath(m_archiveRoot,
                                input.at("path").get<std::string>(),
                                normalized, archivePath);
    file["path"] = normalized;
    file["startedAt"] = createdAt;

    fs::path diskPath;
    if(status) status = ResolvePath(m_diskRoot, normalized,
                                    normalized, diskPath);
    bool archived = false;
    if(status && !IsRegularFile(archivePath, archived))
    {
      status = Error(500, "could not inspect the archived file");
    }
    std::error_code error;
    if(status && archived && fs::file_size(archivePath, error) == 0)
    {
      status = Error(400, "zero-length files cannot be staged from tape");
    }
    if(error) status = Error(500, "could not inspect the archived file");

    bool onDisk = false;
    if(status && !IsRegularFile(diskPath, onDisk))
    {
      status = Error(500, "could not inspect the disk file");
    }
    if(status && archived && !onDisk)
    {
      error.clear();
      fs::create_directories(diskPath.parent_path(), error);
      if(!error)
      {
        fs::copy_file(archivePath, diskPath, fs::copy_options::overwrite_existing,
                      error);
      }
      if(error) status = Error(500, "could not stage the archived file");
    }
    file["finishedAt"] = Now();
    if(status && archived)
    {
      file["state"] = "COMPLETED";
    }
    else
    {
      file["state"] = "FAILED";
      file["error"] = status ? "file is not stored on tape" : status.message;
    }
    request["files"].push_back(std::move(file));
  }

  request["completedAt"] = Now();
  return WriteRequest(request);
}

XrdHttpTapeApiStore::Status XrdHttpTapeApiStore::GetStage(
  const std::string &requestId, Json &response)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  Json request;
  const Status status = ReadRequest(requestId, request);
  if(!status) return status;
  response = StatusResponse(request);
  return {};
}

XrdHttpTapeApiStore::Status XrdHttpTapeApiStore::ValidateRequestPaths(
  const Json &request, const Json &paths,
  std::vector<std::string> &normalized) const
{
  std::set<std::string> requestPaths;
  for(const auto &file : request.at("files"))
  {
    requestPaths.insert(file.at("path").get<std::string>());
  }

  normalized.clear();
  for(const auto &path : paths)
  {
    std::string value;
    fs::path ignored;
    Status status = ResolvePath(m_diskRoot, path.get<std::string>(),
                                value, ignored);
    if(!status) return status;
    if(requestPaths.count(value) == 0)
    {
      return Error(400, "file does not belong to the stage request");
    }
    normalized.push_back(std::move(value));
  }
  return {};
}

XrdHttpTapeApiStore::Status XrdHttpTapeApiStore::CancelStage(
  const std::string &requestId, const Json &paths)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  Json request;
  Status status = ReadRequest(requestId, request);
  if(!status) return status;
  std::vector<std::string> normalized;
  return ValidateRequestPaths(request, paths, normalized);
}

XrdHttpTapeApiStore::Status XrdHttpTapeApiStore::DeleteStage(
  const std::string &requestId)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  Json request;
  Status status = ReadRequest(requestId, request);
  if(!status) return status;

  std::error_code error;
  if(!fs::remove(m_requestsRoot / (requestId + ".json"), error) || error)
  {
    return Error(500, "could not delete the stage request");
  }
  return {};
}

XrdHttpTapeApiStore::Status XrdHttpTapeApiStore::Release(
  const std::string &requestId, const Json &paths)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  Json request;
  Status status = ReadRequest(requestId, request);
  if(!status) return status;

  std::vector<std::string> normalized;
  status = ValidateRequestPaths(request, paths, normalized);
  if(!status) return status;

  for(const auto &path : normalized)
  {
    std::string ignored;
    fs::path diskPath;
    status = ResolvePath(m_diskRoot, path, ignored, diskPath);
    if(!status) return status;
    std::error_code error;
    fs::remove(diskPath, error);
    if(error) return Error(500, "could not release the disk replica");
  }
  return {};
}

XrdHttpTapeApiStore::Status XrdHttpTapeApiStore::ArchiveInfo(
  const Json &paths, Json &response)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  response = Json::array();
  for(const auto &path : paths)
  {
    Json item;
    std::string normalized;
    fs::path archivePath;
    Status status = ResolvePath(m_archiveRoot, path.get<std::string>(),
                                normalized, archivePath);
    item["path"] = status ? normalized : path.get<std::string>();

    fs::path diskPath;
    if(status) status = ResolvePath(m_diskRoot, normalized,
                                    normalized, diskPath);
    bool archived = false;
    if(status && !IsRegularFile(archivePath, archived))
    {
      status = Error(500, "could not inspect the archived file");
    }
    bool onDisk = false;
    if(status && !IsRegularFile(diskPath, onDisk))
    {
      status = Error(500, "could not inspect the disk file");
    }

    if(!status)
    {
      item["error"] = status.message;
    }
    else if(archived && onDisk)
    {
      item["locality"] = "DISK_AND_TAPE";
    }
    else if(archived)
    {
      item["locality"] = "TAPE";
    }
    else if(onDisk)
    {
      item["locality"] = "DISK";
    }
    else
    {
      item["error"] = "file does not exist";
    }
    response.push_back(std::move(item));
  }
  return {};
}
