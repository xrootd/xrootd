/******************************************************************************/
/*                                                                            */
/*                    X r d C l H t t p T a p e . c c                         */
/*                                                                            */
/* (c) 2026 by the XRootD Collaboration                                       */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/******************************************************************************/

#include "XrdClHttpTape.hh"

#include "XrdClHttpUtil.hh"

#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdOuc/XrdOucJson.hh"
#include "XrdOuc/XrdOucUtils.hh"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <memory>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <strings.h>

#include <curl/curl.h>

namespace
{
using Json = nlohmann::json;

enum class TapeLocality
{
  Disk,
  Tape,
  DiskAndTape,
  Lost,
  None,
  Unavailable,
  Unknown
};

constexpr std::array<std::pair<TapeLocality, std::string_view>, 6>
  kTapeLocalities = {{
    {TapeLocality::Disk, "DISK"},
    {TapeLocality::Tape, "TAPE"},
    {TapeLocality::DiskAndTape, "DISK_AND_TAPE"},
    {TapeLocality::Lost, "LOST"},
    {TapeLocality::None, "NONE"},
    {TapeLocality::Unavailable, "UNAVAILABLE"}
  }};

struct TapeEndpoint
{
  std::string uri;
  std::string version;
  std::string sitename;
};

struct TapeArchiveInfo
{
  std::string url;
  std::string path;
  TapeLocality locality = TapeLocality::Unknown;
  std::string error;
};

struct TapeStageFileStatus
{
  std::string path;
  std::string state;
  std::string error;
  std::time_t startedAt = 0;
  std::time_t finishedAt = 0;
  std::uint8_t flags = 0;

  enum Flag : std::uint8_t
  {
    IsOnDisk = 1 << 0,
    HasOnDisk = 1 << 1,
    HasStartedAt = 1 << 2,
    HasFinishedAt = 1 << 3
  };

  bool OnDisk() const { return (flags & IsOnDisk) != 0; }
  bool HasOnDiskValue() const { return (flags & HasOnDisk) != 0; }
  bool HasStartedAtValue() const { return (flags & HasStartedAt) != 0; }
  bool HasFinishedAtValue() const { return (flags & HasFinishedAt) != 0; }

  void SetOnDisk(bool value)
  {
    flags |= HasOnDisk;
    if(value) flags |= IsOnDisk;
    else flags = static_cast<std::uint8_t>(flags & ~IsOnDisk);
  }
};

struct TapeStageStatus
{
  std::string id;
  std::vector<TapeStageFileStatus> files;
  std::time_t createdAt = 0;
  std::time_t startedAt = 0;
  std::time_t completedAt = 0;
  std::uint8_t flags = 0;

  enum Flag : std::uint8_t
  {
    HasCreatedAt = 1 << 0,
    HasStartedAt = 1 << 1,
    HasCompletedAt = 1 << 2
  };

  bool HasCreatedAtValue() const { return (flags & HasCreatedAt) != 0; }
  bool HasStartedAtValue() const { return (flags & HasStartedAt) != 0; }
  bool HasCompletedAtValue() const { return (flags & HasCompletedAt) != 0; }
};

std::string NormalizeTapePath(const std::string &path)
{
  std::string result = path.empty() ? "/" : path;
  if(result.front() != '/') result.insert(result.begin(), '/');
  return XrdOucUtils::NormalizePath(result);
}

std::string PercentEncodeUrlPathSegment(const std::string &value, CURL *curl)
{
  if(!curl) throw std::bad_alloc();

  char *escaped =
    curl_easy_escape(curl, value.c_str(), static_cast<int>(value.size()));
  if(!escaped) throw std::bad_alloc();
  std::string result(escaped);
  curl_free(escaped);
  return result;
}

bool IsHttpEndpointUri(const std::string &uri)
{
  XrdCl::URL url(uri);
  if(!url.IsValid() || url.GetHostName().empty()) return false;

  const std::string &protocol = url.GetProtocol();
  return strcasecmp(protocol.c_str(), "http") == 0
         || strcasecmp(protocol.c_str(), "https") == 0;
}

bool UrlEndpointAndPath(const std::string &input, std::string &endpoint,
                        std::string &path, std::string &error)
{
  XrdCl::URL url(input);
  if(!url.IsValid() || url.GetHostName().empty())
  {
    error = "invalid URL '" + input + "'";
    return false;
  }

  std::string protocol = url.GetProtocol();
  std::transform(protocol.begin(), protocol.end(), protocol.begin(),
    [](unsigned char c) { return std::tolower(c); });
  if(protocol == "davs") protocol = "https";
  else if(protocol == "dav") protocol = "http";

  if(protocol != "http" && protocol != "https")
  {
    error = "unsupported URL protocol '" + url.GetProtocol()
      + "' for Tape REST API";
    return false;
  }

  std::ostringstream out;
  out << protocol << "://" << url.GetHostName();
  if(url.GetPort() > 0) out << ":" << url.GetPort();
  endpoint = out.str();
  path = NormalizeTapePath(url.GetPath());
  return true;
}

std::string FormatProblemResponse(long statusCode, const std::string &body)
{
  std::string detail;

  if(!body.empty())
  {
    const Json json = Json::parse(body, nullptr, false);
    if(!json.is_discarded() && json.is_object())
    {
      const auto title = json.find("title");
      const auto problemDetail = json.find("detail");

      if(title != json.end() && title->is_string())
      {
        detail = title->get<std::string>();
      }
      if(problemDetail != json.end() && problemDetail->is_string())
      {
        if(!detail.empty()) detail += " - ";
        detail += problemDetail->get<std::string>();
      }
    }
    if(detail.empty()) detail = body;
  }

  std::ostringstream out;
  out << "HTTP " << statusCode;
  if(!detail.empty()) out << ": " << detail;
  return out.str();
}

XrdCl::XRootDStatus ErrorStatus(uint16_t code, const std::string &message)
{
  uint32_t errorNumber = EIO;
  switch(code)
  {
    case XrdCl::errInvalidArgs: errorNumber = EINVAL; break;
    case XrdCl::errNotSupported: errorNumber = ENOTSUP; break;
    case XrdCl::errNotImplemented: errorNumber = ENOSYS; break;
    case XrdCl::errInvalidResponse: errorNumber = EBADMSG; break;
    default: break;
  }
  return XrdCl::XRootDStatus(
    XrdCl::stError, code, errorNumber, message);
}

const std::string kStructuredStagePrefix = "xrdclhttp.tape.stage:";

bool IsValidRequestId(const std::string &requestId)
{
  return !requestId.empty()
         && requestId.find_first_of("\r\n") == std::string::npos;
}

XrdCl::XRootDStatus ValidateTapePrepareFlags(
  XrdCl::PrepareFlags::Flags flags)
{
  const int requested = static_cast<int>(flags);
  const int supported =
    static_cast<int>(XrdCl::PrepareFlags::Stage)
    | static_cast<int>(XrdCl::PrepareFlags::Cancel)
    | static_cast<int>(XrdCl::PrepareFlags::Evict);

  if(requested & ~supported)
  {
    return ErrorStatus(XrdCl::errNotSupported,
      "HTTP Tape REST prepare supports stage, cancel, and evict only");
  }

  int operations = 0;
  if(flags & XrdCl::PrepareFlags::Stage) ++operations;
  if(flags & XrdCl::PrepareFlags::Cancel) ++operations;
  if(flags & XrdCl::PrepareFlags::Evict) ++operations;

  if(operations == 0)
  {
    return ErrorStatus(XrdCl::errNotSupported,
      "HTTP Tape REST prepare supports stage, cancel, and evict only");
  }
  if(operations > 1)
  {
    return ErrorStatus(XrdCl::errInvalidArgs,
      "HTTP Tape REST prepare expects exactly one operation flag");
  }
  return XrdCl::XRootDStatus();
}

XrdCl::XRootDStatus PrepareStageFiles(
  const std::vector<std::string> &fileList,
  std::vector<XrdClHttp::TapeStageFileSpec> &files)
{
  if(fileList.empty())
  {
    return ErrorStatus(XrdCl::errInvalidArgs,
      "stage requires at least one file");
  }

  files.clear();
  files.reserve(fileList.size());
  for(const auto &file : fileList)
  {
    if(file.compare(0, kStructuredStagePrefix.size(),
                    kStructuredStagePrefix) == 0)
    {
      const Json json = Json::parse(
        file.substr(kStructuredStagePrefix.size()), nullptr, false);
      if(json.is_discarded())
      {
        return ErrorStatus(XrdCl::errInvalidArgs,
          "malformed structured tape stage entry");
      }
      if(!json.is_object())
      {
        return ErrorStatus(XrdCl::errInvalidArgs,
          "structured tape stage entry must be a JSON object");
      }

      XrdClHttp::TapeStageFileSpec entry;
      if(json.contains("url"))
      {
        if(!json["url"].is_string())
        {
          return ErrorStatus(XrdCl::errInvalidArgs,
            "structured tape stage entry url must be a string");
        }
        entry.url = json["url"].get<std::string>();
      }
      if(json.contains("path"))
      {
        if(!json["path"].is_string())
        {
          return ErrorStatus(XrdCl::errInvalidArgs,
            "structured tape stage entry path must be a string");
        }
        entry.path = json["path"].get<std::string>();
      }
      if(entry.url.empty() && entry.path.empty())
      {
        return ErrorStatus(XrdCl::errInvalidArgs,
          "structured tape stage entry requires url or path");
      }
      if(json.contains("diskLifetime"))
      {
        if(!json["diskLifetime"].is_string())
        {
          return ErrorStatus(XrdCl::errInvalidArgs,
            "structured tape stage entry diskLifetime must be a string");
        }
        entry.diskLifetime = json["diskLifetime"].get<std::string>();
      }
      if(json.contains("targetedMetadata"))
      {
        if(!json["targetedMetadata"].is_object())
        {
          return ErrorStatus(XrdCl::errInvalidArgs,
            "structured tape stage entry targetedMetadata must be a JSON object");
        }
        entry.targetedMetadata = json["targetedMetadata"].dump();
      }
      files.push_back(entry);
      continue;
    }

    XrdClHttp::TapeStageFileSpec entry;
    entry.url = file;
    files.push_back(entry);
  }
  return XrdCl::XRootDStatus();
}

// Map an unexpected HTTP response code to an XRootDStatus, reusing the
// plugin-wide HTTP-to-XRootD status conversion for error codes.
XrdCl::XRootDStatus HttpErrorStatus(long statusCode,
                                    const std::string &message)
{
  const unsigned status = statusCode < 0 ? 0 : statusCode;
  if(!XrdClHttp::HTTPStatusIsError(status))
  {
    return ErrorStatus(XrdCl::errErrorResponse, message);
  }
  const auto [code, errNo] = XrdClHttp::HTTPStatusConvert(status);
  return XrdCl::XRootDStatus(XrdCl::stError, code, errNo, message);
}

bool PathFromInput(const std::string &input, std::string &path,
                   std::string &error)
{
  if(input.empty())
  {
    error = "empty path";
    return false;
  }
  if(input.front() == '/')
  {
    path = NormalizeTapePath(input);
    return true;
  }

  std::string endpoint;
  return UrlEndpointAndPath(input, endpoint, path, error);
}

Json PathsRequestBody(const std::vector<std::string> &paths)
{
  Json body;
  body["paths"] = paths;
  return body;
}

XrdCl::XRootDStatus PathsFromInputs(const std::vector<std::string> &inputs,
                                    std::vector<std::string> &paths)
{
  paths.clear();
  paths.reserve(inputs.size());
  for(const auto &input : inputs)
  {
    std::string path;
    std::string error;
    if(!PathFromInput(input, path, error))
    {
      return ErrorStatus(XrdCl::errInvalidArgs, error);
    }
    paths.push_back(path);
  }
  return XrdCl::XRootDStatus();
}

Json StageRequestBody(
  const std::vector<XrdClHttp::TapeStageFileSpec> &files,
                      std::string &error)
{
  Json body;
  body["files"] = Json::array();
  for(const auto &file : files)
  {
    const std::string input = file.path.empty() ? file.url : file.path;
    if(input.empty())
    {
      error = "stage file is missing both URL and path";
      return Json();
    }

    std::string path;
    if(!PathFromInput(input, path, error)) return Json();

    Json item;
    item["path"] = path;
    if(!file.diskLifetime.empty()) item["diskLifetime"] = file.diskLifetime;
    if(!file.targetedMetadata.empty())
    {
      const Json metadata = Json::parse(
        file.targetedMetadata, nullptr, false);
      if(metadata.is_discarded())
      {
        error = "invalid targetedMetadata JSON";
        return Json();
      }
      if(!metadata.is_object())
      {
        error = "targetedMetadata must be a JSON object";
        return Json();
      }
      item["targetedMetadata"] = metadata;
    }
    body["files"].push_back(item);
  }
  return body;
}

XrdCl::XRootDStatus StageFileStatusFromJson(
  const Json &json, TapeStageFileStatus &result)
{
  result = TapeStageFileStatus();
  if(json.contains("path") && json["path"].is_string())
  {
    result.path = NormalizeTapePath(json["path"].get<std::string>());
  }
  else
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response contains a file entry without a string path");
  }
  const bool hasOnDisk = json.contains("onDisk");
  const bool hasState = json.contains("state");
  if(hasOnDisk && !json["onDisk"].is_boolean())
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response contains a non-boolean onDisk field");
  }
  if(hasState && !json["state"].is_string())
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response contains a non-string state field");
  }
  if(hasOnDisk && hasState)
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response contains both onDisk and state");
  }
  if(hasOnDisk)
  {
    result.SetOnDisk(json["onDisk"].get<bool>());
  }
  if(hasState)
  {
    result.state = json["state"].get<std::string>();
  }
  if(json.contains("error") && !json["error"].is_string())
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response contains a non-string error field");
  }
  if(json.contains("error"))
  {
    result.error = json["error"].get<std::string>();
  }
  bool hasStartedAt = false;
  bool hasFinishedAt = false;
  if(!XrdOucJsonUtils::GetOptionalUnsignedInteger(
       json, "startedAt", result.startedAt, hasStartedAt)
     || !XrdOucJsonUtils::GetOptionalUnsignedInteger(
       json, "finishedAt", result.finishedAt, hasFinishedAt))
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response contains a non-integer file timestamp");
  }
  if(hasStartedAt) result.flags |= TapeStageFileStatus::HasStartedAt;
  if(hasFinishedAt) result.flags |= TapeStageFileStatus::HasFinishedAt;
  if(!hasState && (hasStartedAt || hasFinishedAt))
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response contains file timestamps without state");
  }
  return XrdCl::XRootDStatus();
}

XrdCl::XRootDStatus StageStatusFromJson(const Json &json,
                                        TapeStageStatus &status)
{
  status = TapeStageStatus();
  if(!json.is_object())
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response is not a JSON object");
  }
  if(!json.contains("id") || !json["id"].is_string())
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response does not contain a string id");
  }
  if(json["id"].get_ref<const std::string &>().empty())
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response contains an empty id");
  }
  if(!json.contains("files") || !json["files"].is_array())
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response does not contain a files array");
  }

  status.id = json["id"].get<std::string>();
  bool hasCreatedAt = false;
  bool hasStartedAt = false;
  bool hasCompletedAt = false;
  if(!XrdOucJsonUtils::GetOptionalUnsignedInteger(
       json, "createdAt", status.createdAt, hasCreatedAt)
     || !XrdOucJsonUtils::GetOptionalUnsignedInteger(
       json, "startedAt", status.startedAt, hasStartedAt)
     || !XrdOucJsonUtils::GetOptionalUnsignedInteger(
       json, "completedAt", status.completedAt, hasCompletedAt))
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response contains a non-integer timestamp");
  }
  if(!hasCreatedAt || !hasStartedAt)
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response is missing createdAt or startedAt");
  }
  if(hasCreatedAt) status.flags |= TapeStageStatus::HasCreatedAt;
  if(hasStartedAt) status.flags |= TapeStageStatus::HasStartedAt;
  if(hasCompletedAt) status.flags |= TapeStageStatus::HasCompletedAt;

  status.files.reserve(json["files"].size());
  for(const auto &file : json["files"])
  {
    if(!file.is_object())
    {
      return ErrorStatus(XrdCl::errInvalidResponse,
        "stage request response contains a non-object file entry");
    }
    TapeStageFileStatus fileStatus;
    XrdCl::XRootDStatus fileStatusResult =
      StageFileStatusFromJson(file, fileStatus);
    if(!fileStatusResult.IsOK()) return fileStatusResult;
    status.files.push_back(fileStatus);
  }
  return XrdCl::XRootDStatus();
}

XrdCl::XRootDStatus EmptyResponseStatus(long statusCode,
                                        const std::string &body,
                                        const std::string &operation)
{
  if(statusCode < 200 || statusCode >= 300)
  {
    return HttpErrorStatus(statusCode,
      operation + " failed: "
      + FormatProblemResponse(statusCode, body));
  }
  return XrdCl::XRootDStatus();
}

TapeLocality LocalityFromString(const std::string &locality)
{
  for(const auto &[value, name] : kTapeLocalities)
  {
    if(strcasecmp(locality.c_str(), name.data()) == 0) return value;
  }
  return TapeLocality::Unknown;
}

std::string LocalityToString(TapeLocality locality)
{
  for(const auto &[value, name] : kTapeLocalities)
  {
    if(locality == value) return std::string(name);
  }
  return "UNKNOWN";
}

const Json *FindArchiveInfoItem(const Json &response, const std::string &path)
{
  if(!response.is_array()) return nullptr;

  for(const auto &item : response)
  {
    if(!item.is_object() || !item.contains("path") || !item["path"].is_string())
    {
      continue;
    }
    if(NormalizeTapePath(item["path"].get<std::string>()) == path)
    {
      return &item;
    }
  }
  return nullptr;
}

TapeArchiveInfo ArchiveInfoFromJson(const Json *item,
                                               const std::string &url,
                                               const std::string &path)
{
  TapeArchiveInfo result;
  result.url = url;
  result.path = path;

  if(!item)
  {
    result.error = "missing response item for path=" + path;
    return result;
  }

  if(item->contains("error"))
  {
    if((*item)["error"].is_string())
    {
      result.error = (*item)["error"].get<std::string>();
    }
    else
    {
      result.error = "error field is not a string";
    }
    return result;
  }

  if(!item->contains("locality") || !(*item)["locality"].is_string())
  {
    result.error = "locality attribute missing";
    return result;
  }

  const std::string locality = (*item)["locality"].get<std::string>();
  result.locality = LocalityFromString(locality);
  if(result.locality == TapeLocality::Unknown)
  {
    result.error = "file locality reported as \"" + locality + "\"";
  }
  return result;
}

}

namespace
{
  Json StageFileStatusToJson( const TapeStageFileStatus &status )
  {
    Json json;
    json["path"] = status.path;
    if(status.HasOnDiskValue()) json["onDisk"] = status.OnDisk();
    if(!status.state.empty()) json["state"] = status.state;
    if(!status.error.empty()) json["error"] = status.error;
    if(status.HasStartedAtValue()) json["startedAt"] = status.startedAt;
    if(status.HasFinishedAtValue()) json["finishedAt"] = status.finishedAt;
    return json;
  }

  Json StageStatusToJson( const TapeStageStatus &status )
  {
    Json json;
    json["id"] = status.id;
    if(status.HasCreatedAtValue()) json["createdAt"] = status.createdAt;
    if(status.HasStartedAtValue()) json["startedAt"] = status.startedAt;
    if(status.HasCompletedAtValue()) json["completedAt"] = status.completedAt;
    json["files"] = Json::array();
    for(const auto &file : status.files)
    {
      json["files"].push_back(StageFileStatusToJson(file));
    }
    return json;
  }

  Json ArchiveInfoToJson( const TapeArchiveInfo &info )
  {
    Json json;
    json["url"] = info.url;
    json["path"] = info.path;
    if(info.error.empty())
    {
      json["locality"] = LocalityToString(info.locality);
    }
    else
    {
      json["error"] = info.error;
    }
    return json;
  }
}

namespace XrdClHttp
{
  struct TapeOperation::Impl
  {
    enum class Kind
    {
      Invalid,
      Discover,
      Stage,
      StageStatus,
      StageCancel,
      StageDelete,
      Release,
      ArchiveInfo
    };

    enum class Phase
    {
      Initial,
      Discovery,
      Request,
      Complete
    };

    std::string storageUrl;
    Kind kind = Kind::Invalid;
    Phase phase = Phase::Initial;
    XrdCl::XRootDStatus initialStatus;
    std::vector<TapeStageFileSpec> stageFiles;
    std::vector<std::string> urls;
    std::vector<std::string> paths;
    std::string requestId;
  };

  namespace
  {
    XrdCl::XRootDStatus ParseDiscoveryResponse(const std::string &body,
                                               TapeEndpoint &endpoint)
    {
      const Json json = Json::parse(body, nullptr, false);
      if(json.is_discarded())
      {
        return ErrorStatus(XrdCl::errInvalidResponse,
          "malformed discovery response");
      }

      if(!json.contains("sitename") || !json["sitename"].is_string())
      {
        return ErrorStatus(XrdCl::errInvalidResponse,
          "discovery response does not contain a string sitename");
      }
      if(!json.contains("endpoints") || !json["endpoints"].is_array())
      {
        return ErrorStatus(XrdCl::errInvalidResponse,
          "discovery response does not contain an endpoints array");
      }

      TapeEndpoint selected;
      int selectedVersion = -1;
      for(const auto &candidate : json["endpoints"])
      {
        if(!candidate.is_object()
           || !candidate.contains("uri") || !candidate["uri"].is_string()
           || !candidate.contains("version")
           || !candidate["version"].is_string())
        {
          continue;
        }

        const std::string version = candidate["version"].get<std::string>();
        std::string_view versionNumber(version);
        if(!versionNumber.empty()
           && (versionNumber.front() == 'v' || versionNumber.front() == 'V'))
        {
          versionNumber.remove_prefix(1);
        }

        int parsedVersion = -1;
        try
        {
          parsedVersion = XrdOucUtils::touint8_t(versionNumber);
        }
        catch(const std::invalid_argument &)
        {
          continue;
        }
        catch(const std::out_of_range &)
        {
          continue;
        }
        if(parsedVersion > 1 || parsedVersion < selectedVersion)
        {
          continue;
        }

        const std::string uri = candidate["uri"].get<std::string>();
        if(!IsHttpEndpointUri(uri)) continue;

        selectedVersion = parsedVersion;
        selected.uri = uri;
        selected.version = version;
      }

      if(selected.uri.empty())
      {
        return ErrorStatus(XrdCl::errNotSupported,
          "discovery response does not advertise a supported v0/v1 endpoint");
      }

      selected.sitename = json["sitename"].get<std::string>();
      endpoint = selected;
      return XrdCl::XRootDStatus();
    }

    XrdCl::XRootDStatus ValidateArchiveUrls(
      const std::vector<std::string> &urls,
      std::vector<std::string> &paths)
    {
      if(urls.empty())
      {
        return ErrorStatus(XrdCl::errInvalidArgs, "missing URL");
      }

      std::string firstEndpoint;
      paths.clear();
      paths.reserve(urls.size());
      for(const auto &url : urls)
      {
        std::string endpoint;
        std::string path;
        std::string error;
        if(!UrlEndpointAndPath(url, endpoint, path, error))
        {
          return ErrorStatus(XrdCl::errInvalidArgs, error);
        }
        if(firstEndpoint.empty()) firstEndpoint = endpoint;
        else if(endpoint != firstEndpoint)
        {
          return ErrorStatus(XrdCl::errInvalidArgs,
            "archiveinfo URLs must belong to the same storage endpoint");
        }
        paths.push_back(path);
      }
      return XrdCl::XRootDStatus();
    }

    XrdCl::XRootDStatus ParseArchiveInfoResponse(
      const std::string &body,
      const std::vector<std::string> &urls,
      const std::vector<std::string> &paths,
      std::string &result)
    {
      const Json json = Json::parse(body, nullptr, false);
      if(json.is_discarded())
      {
        return ErrorStatus(XrdCl::errInvalidResponse,
          "malformed archiveinfo response");
      }
      if(!json.is_array())
      {
        return ErrorStatus(XrdCl::errInvalidResponse,
          "archiveinfo response is not a JSON array");
      }

      Json response = Json::array();
      for(std::size_t index = 0; index < paths.size(); ++index)
      {
        response.push_back(ArchiveInfoToJson(ArchiveInfoFromJson(
          FindArchiveInfoItem(json, paths[index]), urls[index], paths[index])));
      }
      result = response.dump();
      return XrdCl::XRootDStatus();
    }

    XrdCl::XRootDStatus UnexpectedStatus(long statusCode,
                                         const std::string &body,
                                         const std::string &operation)
    {
      return HttpErrorStatus(statusCode,
        operation + " failed: " + FormatProblemResponse(statusCode, body));
    }
  }

  TapeOperation::TapeOperation(
    const std::string &url,
    const std::vector<std::string> &fileList,
    XrdCl::PrepareFlags::Flags flags ):
    pImpl(std::make_unique<Impl>())
  {
    pImpl->storageUrl = url;
    pImpl->initialStatus = ValidateTapePrepareFlags(flags);
    if(!pImpl->initialStatus.IsOK()) return;

    if(flags & XrdCl::PrepareFlags::Stage)
    {
      pImpl->kind = Impl::Kind::Stage;
      pImpl->initialStatus = PrepareStageFiles(fileList, pImpl->stageFiles);
      return;
    }

    if(fileList.size() < 2)
    {
      pImpl->initialStatus = ErrorStatus(XrdCl::errInvalidArgs,
        "cancel and evict require a request id and at least one path");
      return;
    }

    pImpl->requestId = fileList.front();
    if(!IsValidRequestId(pImpl->requestId))
    {
      pImpl->initialStatus = ErrorStatus(XrdCl::errInvalidArgs,
        "cancel and evict require a non-empty request id");
      return;
    }
    pImpl->initialStatus = PathsFromInputs(
      std::vector<std::string>(fileList.begin() + 1, fileList.end()),
      pImpl->paths);
    if(!pImpl->initialStatus.IsOK()) return;
    pImpl->kind = (flags & XrdCl::PrepareFlags::Cancel)
      ? Impl::Kind::StageCancel : Impl::Kind::Release;
  }

  TapeOperation::TapeOperation(
    const std::string &url,
    XrdCl::QueryCode::Code queryCode,
    const XrdCl::Buffer &arg ):
    pImpl(std::make_unique<Impl>())
  {
    pImpl->storageUrl = url;
    std::vector<std::string> args;
    XrdCl::Utils::splitString(args, arg.ToString(), "\n");
    if(queryCode == XrdCl::QueryCode::Prepare)
    {
      if(args.size() != 1 || !IsValidRequestId(args.front()))
      {
        pImpl->initialStatus = ErrorStatus(XrdCl::errInvalidArgs,
          "prepare query expects a single request id");
        return;
      }
      pImpl->kind = Impl::Kind::StageStatus;
      pImpl->requestId = args.front();
      return;
    }

    if(queryCode != XrdCl::QueryCode::Opaque)
    {
      pImpl->initialStatus = ErrorStatus(XrdCl::errNotSupported,
        "unsupported HTTP query");
      return;
    }
    if(args.empty() || args.front().find('\r') != std::string::npos)
    {
      pImpl->initialStatus = ErrorStatus(XrdCl::errInvalidArgs,
        "missing opaque query command");
      return;
    }

    if(args.front() == "tape.discover")
    {
      pImpl->kind = Impl::Kind::Discover;
      return;
    }
    if(args.front() == "tape.stage_delete")
    {
      if(args.size() != 2 || !IsValidRequestId(args[1]))
      {
        pImpl->initialStatus = ErrorStatus(XrdCl::errInvalidArgs,
          "tape.stage_delete expects a request id");
        return;
      }
      pImpl->kind = Impl::Kind::StageDelete;
      pImpl->requestId = args[1];
      return;
    }
    if(args.front() == "tape.archiveinfo")
    {
      if(args.size() < 2)
      {
        pImpl->initialStatus = ErrorStatus(XrdCl::errInvalidArgs,
          "tape.archiveinfo expects non-empty URLs");
        return;
      }
      pImpl->urls.assign(args.begin() + 1, args.end());
      for(const auto &archiveUrl : pImpl->urls)
      {
        if(archiveUrl.find('\r') != std::string::npos)
        {
          pImpl->initialStatus = ErrorStatus(XrdCl::errInvalidArgs,
            "tape.archiveinfo expects non-empty URLs");
          return;
        }
      }
      pImpl->initialStatus = ValidateArchiveUrls(pImpl->urls, pImpl->paths);
      if(pImpl->initialStatus.IsOK()) pImpl->kind = Impl::Kind::ArchiveInfo;
      return;
    }

    pImpl->initialStatus = ErrorStatus(XrdCl::errNotSupported,
      "unsupported HTTP opaque query");
  }

  TapeOperation::~TapeOperation() = default;

  XrdCl::XRootDStatus TapeOperation::Start(TapeHttpRequest &request)
  {
    if(!pImpl->initialStatus.IsOK()) return pImpl->initialStatus;
    if(pImpl->kind == Impl::Kind::Invalid)
    {
      return ErrorStatus(XrdCl::errNotSupported,
        "unsupported HTTP Tape REST operation");
    }

    std::string endpoint;
    std::string path;
    std::string error;
    if(!UrlEndpointAndPath(pImpl->storageUrl, endpoint, path, error))
    {
      return ErrorStatus(XrdCl::errInvalidArgs, error);
    }

    request = TapeHttpRequest();
    request.url = XrdOucUtils::JoinUrl(
      endpoint, "/.well-known/wlcg-tape-rest-api");
    pImpl->phase = Impl::Phase::Discovery;
    return XrdCl::XRootDStatus();
  }

  XrdCl::XRootDStatus TapeOperation::Advance(
    CURL *curl,
    long statusCode,
    const std::string &body,
    TapeHttpRequest &request,
    std::string &response,
    bool &complete )
  {
    complete = false;
    response.clear();

    if(pImpl->phase == Impl::Phase::Discovery)
    {
      if(statusCode != 200)
      {
        complete = true;
        return UnexpectedStatus(statusCode, body, "Tape REST discovery");
      }

      TapeEndpoint endpoint;
      XrdCl::XRootDStatus status = ParseDiscoveryResponse(body, endpoint);
      if(!status.IsOK())
      {
        complete = true;
        return status;
      }
      if(pImpl->kind == Impl::Kind::Discover)
      {
        Json result;
        result["uri"] = endpoint.uri;
        result["version"] = endpoint.version;
        result["sitename"] = endpoint.sitename;
        response = result.dump();
        complete = true;
        pImpl->phase = Impl::Phase::Complete;
        return XrdCl::XRootDStatus();
      }

      request = TapeHttpRequest();
      const std::string encodedRequestId = pImpl->requestId.empty()
        ? "" : PercentEncodeUrlPathSegment(pImpl->requestId, curl);
      switch(pImpl->kind)
      {
        case Impl::Kind::Stage:
        {
          std::string error;
          Json stageBody = StageRequestBody(pImpl->stageFiles, error);
          if(!error.empty())
          {
            complete = true;
            return ErrorStatus(XrdCl::errInvalidArgs, error);
          }
          request.method = TapeHttpMethod::Post;
          request.url = XrdOucUtils::JoinUrl(endpoint.uri, "/stage");
          request.body = stageBody.dump();
          break;
        }
        case Impl::Kind::StageStatus:
          request.url = XrdOucUtils::JoinUrl(
            endpoint.uri, "/stage/" + encodedRequestId);
          break;
        case Impl::Kind::StageCancel:
          request.method = TapeHttpMethod::Post;
          request.url = XrdOucUtils::JoinUrl(endpoint.uri,
            "/stage/" + encodedRequestId + "/cancel");
          request.body = PathsRequestBody(pImpl->paths).dump();
          break;
        case Impl::Kind::StageDelete:
          request.method = TapeHttpMethod::Delete;
          request.url = XrdOucUtils::JoinUrl(
            endpoint.uri, "/stage/" + encodedRequestId);
          break;
        case Impl::Kind::Release:
          request.method = TapeHttpMethod::Post;
          request.url = XrdOucUtils::JoinUrl(
            endpoint.uri, "/release/" + encodedRequestId);
          request.body = PathsRequestBody(pImpl->paths).dump();
          break;
        case Impl::Kind::ArchiveInfo:
          request.method = TapeHttpMethod::Post;
          request.url = XrdOucUtils::JoinUrl(endpoint.uri, "/archiveinfo");
          request.body = PathsRequestBody(pImpl->paths).dump();
          break;
        case Impl::Kind::Invalid:
        case Impl::Kind::Discover:
          complete = true;
          return ErrorStatus(XrdCl::errInternal,
            "invalid Tape REST operation state");
      }
      pImpl->phase = Impl::Phase::Request;
      return XrdCl::XRootDStatus();
    }

    if(pImpl->phase != Impl::Phase::Request)
    {
      complete = true;
      return ErrorStatus(XrdCl::errInternal,
        "invalid Tape REST response state");
    }

    complete = true;
    pImpl->phase = Impl::Phase::Complete;
    switch(pImpl->kind)
    {
      case Impl::Kind::Stage:
      {
        if(statusCode != 201)
          return UnexpectedStatus(statusCode, body, "stage request submission");
        const Json json = Json::parse(body, nullptr, false);
        if(json.is_discarded())
        {
          return ErrorStatus(XrdCl::errInvalidResponse,
            "malformed stage submission response");
        }
        if(!json.contains("requestId") || !json["requestId"].is_string()
           || json["requestId"].get_ref<const std::string &>().empty())
        {
          return ErrorStatus(XrdCl::errInvalidResponse,
            "stage submission response does not contain a non-empty string "
            "requestId");
        }
        response = json["requestId"].get<std::string>();
        return XrdCl::XRootDStatus();
      }
      case Impl::Kind::StageStatus:
      {
        if(statusCode != 200)
          return UnexpectedStatus(statusCode, body, "stage request polling");
        const Json json = Json::parse(body, nullptr, false);
        if(json.is_discarded())
        {
          return ErrorStatus(XrdCl::errInvalidResponse,
            "malformed stage polling response");
        }
        TapeStageStatus stageStatus;
        XrdCl::XRootDStatus status = StageStatusFromJson(json, stageStatus);
        if(status.IsOK()) response = StageStatusToJson(stageStatus).dump();
        return status;
      }
      case Impl::Kind::StageCancel:
        return EmptyResponseStatus(statusCode, body,
          "stage request cancellation");
      case Impl::Kind::StageDelete:
        return EmptyResponseStatus(statusCode, body, "stage request deletion");
      case Impl::Kind::Release:
        return EmptyResponseStatus(statusCode, body, "stage request release");
      case Impl::Kind::ArchiveInfo:
        if(statusCode != 200)
          return UnexpectedStatus(statusCode, body, "archiveinfo call");
        return ParseArchiveInfoResponse(body, pImpl->urls, pImpl->paths,
                                        response);
      case Impl::Kind::Invalid:
      case Impl::Kind::Discover:
        return ErrorStatus(XrdCl::errInternal,
          "invalid Tape REST operation state");
    }
    return ErrorStatus(XrdCl::errInternal, "invalid Tape REST operation");
  }

  std::string TapeProblemResponse(long statusCode, const std::string &body)
  {
    return FormatProblemResponse(statusCode, body);
  }
}
