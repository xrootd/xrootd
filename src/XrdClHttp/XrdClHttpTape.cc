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
#include <limits>
#include <memory>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <strings.h>
#include <utility>

#include <curl/curl.h>

namespace
{
using Json = nlohmann::json;

constexpr std::array<std::string_view, 6> kTapeLocalities = {{
  "DISK", "TAPE", "DISK_AND_TAPE", "LOST", "NONE", "UNAVAILABLE"
}};

struct TapeEndpoint
{
  std::string uri;
  std::string version;
  std::string sitename;
};

struct StageFileSpec
{
  std::string url;
  std::string path;
  std::string diskLifetime;
  Json targetedMetadata;
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

XrdCl::XRootDStatus ParseJsonResponse(const std::string &body,
                                      const std::string &responseName,
                                      Json &json)
{
  json = Json::parse(body, nullptr, false);
  if(!json.is_discarded()) return XrdCl::XRootDStatus();

  return ErrorStatus(XrdCl::errInvalidResponse,
    "malformed " + responseName + " response");
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
  std::vector<StageFileSpec> &files)
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

      StageFileSpec entry;
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
        entry.targetedMetadata = json["targetedMetadata"];
      }
      files.push_back(entry);
      continue;
    }

    StageFileSpec entry;
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

void SetPathsPostRequest(XrdClHttp::TapeHttpRequest &request,
                         const std::string &endpoint,
                         const std::string &resource,
                         const std::vector<std::string> &paths)
{
  request.method = XrdClHttp::HttpVerb::POST;
  request.url = XrdOucUtils::JoinUrl(endpoint, resource);
  request.body = PathsRequestBody(paths).dump();
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

Json StageRequestBody(const std::vector<StageFileSpec> &files,
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
    if(!file.targetedMetadata.is_null())
      item["targetedMetadata"] = file.targetedMetadata;
    body["files"].push_back(item);
  }
  return body;
}

bool CopyOptionalTimestamp(const Json &input, const char *key,
                           Json &output, bool &present)
{
  present = false;
  const auto item = input.find(key);
  if(item == input.end()) return true;
  if(!item->is_number_integer()) return false;

  std::uint64_t timestamp = 0;
  if(item->is_number_unsigned())
  {
    timestamp = item->get<std::uint64_t>();
  }
  else
  {
    const std::int64_t signedTimestamp = item->get<std::int64_t>();
    if(signedTimestamp < 0) return false;
    timestamp = static_cast<std::uint64_t>(signedTimestamp);
  }
  if(timestamp > static_cast<std::uint64_t>(
       std::numeric_limits<std::time_t>::max()))
  {
    return false;
  }

  output[key] = static_cast<std::time_t>(timestamp);
  present = true;
  return true;
}

XrdCl::XRootDStatus NormalizeStageFileStatusJson(
  const Json &input, Json &output)
{
  output = Json::object();
  const auto pathItem = input.find("path");
  if(pathItem == input.end() || !pathItem->is_string()
     || pathItem->get_ref<const std::string &>().empty())
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response contains a file entry without a non-empty "
      "string path");
  }
  const auto &path = pathItem->get_ref<const std::string &>();
  output["path"] = NormalizeTapePath(path);
  const bool hasOnDisk = input.contains("onDisk");
  const bool hasState = input.contains("state");
  if(hasOnDisk && !input["onDisk"].is_boolean())
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response contains a non-boolean onDisk field");
  }
  if(hasState && !input["state"].is_string())
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
    output["onDisk"] = input["onDisk"];
  }
  if(hasState && !input["state"].get_ref<const std::string &>().empty())
  {
    output["state"] = input["state"];
  }
  const auto errorItem = input.find("error");
  if(errorItem != input.end())
  {
    if(!errorItem->is_string())
    {
      return ErrorStatus(XrdCl::errInvalidResponse,
        "stage request response contains a non-string error field");
    }
    const auto &error = errorItem->get_ref<const std::string &>();
    if(!error.empty()) output["error"] = error;
  }
  bool hasStartedAt = false;
  bool hasFinishedAt = false;
  if(!CopyOptionalTimestamp(input, "startedAt", output, hasStartedAt)
     || !CopyOptionalTimestamp(input, "finishedAt", output, hasFinishedAt))
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response contains a non-integer file timestamp");
  }
  if(!hasState && (hasStartedAt || hasFinishedAt))
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response contains file timestamps without state");
  }
  return XrdCl::XRootDStatus();
}

XrdCl::XRootDStatus NormalizeStageStatusJson(const Json &input,
                                             Json &output)
{
  output = Json::object();
  if(!input.is_object())
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response is not a JSON object");
  }
  if(!input.contains("id") || !input["id"].is_string())
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response does not contain a string id");
  }
  if(input["id"].get_ref<const std::string &>().empty())
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response contains an empty id");
  }
  if(!input.contains("files") || !input["files"].is_array())
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response does not contain a files array");
  }

  output["id"] = input["id"];
  bool hasCreatedAt = false;
  bool hasStartedAt = false;
  bool hasCompletedAt = false;
  if(!CopyOptionalTimestamp(input, "createdAt", output, hasCreatedAt)
     || !CopyOptionalTimestamp(input, "startedAt", output, hasStartedAt)
     || !CopyOptionalTimestamp(input, "completedAt", output,
                               hasCompletedAt))
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response contains a non-integer timestamp");
  }
  if(!hasCreatedAt || !hasStartedAt)
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response is missing createdAt or startedAt");
  }

  output["files"] = Json::array();
  for(const auto &file : input["files"])
  {
    if(!file.is_object())
    {
      return ErrorStatus(XrdCl::errInvalidResponse,
        "stage request response contains a non-object file entry");
    }
    Json normalizedFile;
    XrdCl::XRootDStatus fileStatusResult =
      NormalizeStageFileStatusJson(file, normalizedFile);
    if(!fileStatusResult.IsOK()) return fileStatusResult;
    output["files"].push_back(std::move(normalizedFile));
  }
  return XrdCl::XRootDStatus();
}

XrdCl::XRootDStatus UnexpectedStatus(long statusCode,
                                     const std::string &body,
                                     const std::string &operation)
{
  return HttpErrorStatus(statusCode,
    operation + " failed: " + FormatProblemResponse(statusCode, body));
}

XrdCl::XRootDStatus EmptyResponseStatus(long statusCode,
                                        const std::string &body,
                                        const std::string &operation)
{
  if(statusCode < 200 || statusCode >= 300)
  {
    return UnexpectedStatus(statusCode, body, operation);
  }
  return XrdCl::XRootDStatus();
}

std::string CanonicalLocality(const std::string &locality)
{
  for(const auto name : kTapeLocalities)
  {
    if(strcasecmp(locality.c_str(), name.data()) == 0)
      return std::string(name);
  }
  return "UNKNOWN";
}

Json ArchiveInfoToJson(const Json *item, const std::string &url,
                       const std::string &path)
{
  Json result;
  result["url"] = url;
  result["path"] = path;

  if(!item)
  {
    result["error"] = "missing response item for path=" + path;
    return result;
  }

  if(item->contains("error"))
  {
    if((*item)["error"].is_string())
    {
      const std::string error = (*item)["error"].get<std::string>();
      if(error.empty()) result["locality"] = "UNKNOWN";
      else result["error"] = error;
    }
    else
    {
      result["error"] = "error field is not a string";
    }
    return result;
  }

  const auto localityItem = item->find("locality");
  if(localityItem == item->end() || !localityItem->is_string()
     || localityItem->get_ref<const std::string &>().empty())
  {
    result["error"] = "locality attribute missing";
    return result;
  }

  const auto &locality = localityItem->get_ref<const std::string &>();
  result["locality"] = CanonicalLocality(locality);
  return result;
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
    std::vector<StageFileSpec> stageFiles;
    std::vector<std::string> urls;
    std::vector<std::string> paths;
    std::string requestId;
  };

  namespace
  {
    XrdCl::XRootDStatus ParseDiscoveryResponse(const std::string &body,
                                               TapeEndpoint &endpoint)
    {
      Json json;
      XrdCl::XRootDStatus status =
        ParseJsonResponse(body, "discovery", json);
      if(!status.IsOK()) return status;

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
      const std::string &storageUrl,
      const std::vector<std::string> &urls,
      std::vector<std::string> &paths)
    {
      if(urls.empty())
      {
        return ErrorStatus(XrdCl::errInvalidArgs, "missing URL");
      }

      std::string storageEndpoint;
      std::string storagePath;
      std::string error;
      if(!UrlEndpointAndPath(storageUrl, storageEndpoint, storagePath, error))
      {
        return ErrorStatus(XrdCl::errInvalidArgs, error);
      }

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
        if(endpoint != storageEndpoint)
        {
          return ErrorStatus(XrdCl::errInvalidArgs,
            "archiveinfo URLs must belong to the storage endpoint");
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
      Json json;
      XrdCl::XRootDStatus status =
        ParseJsonResponse(body, "archiveinfo", json);
      if(!status.IsOK()) return status;
      if(!json.is_array())
      {
        return ErrorStatus(XrdCl::errInvalidResponse,
          "archiveinfo response is not a JSON array");
      }

      Json response = Json::array();
      for(std::size_t index = 0; index < paths.size(); ++index)
      {
        response.push_back(ArchiveInfoToJson(
          FindArchiveInfoItem(json, paths[index]), urls[index], paths[index]));
      }
      result = response.dump();
      return XrdCl::XRootDStatus();
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
      pImpl->initialStatus = ValidateArchiveUrls(
        pImpl->storageUrl, pImpl->urls, pImpl->paths);
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
          request.method = HttpVerb::POST;
          request.url = XrdOucUtils::JoinUrl(endpoint.uri, "/stage");
          request.body = stageBody.dump();
          break;
        }
        case Impl::Kind::StageStatus:
          request.url = XrdOucUtils::JoinUrl(
            endpoint.uri, "/stage/" + encodedRequestId);
          break;
        case Impl::Kind::StageCancel:
          SetPathsPostRequest(request, endpoint.uri,
            "/stage/" + encodedRequestId + "/cancel", pImpl->paths);
          break;
        case Impl::Kind::StageDelete:
          request.method = HttpVerb::DELETE;
          request.url = XrdOucUtils::JoinUrl(
            endpoint.uri, "/stage/" + encodedRequestId);
          break;
        case Impl::Kind::Release:
          SetPathsPostRequest(request, endpoint.uri,
            "/release/" + encodedRequestId, pImpl->paths);
          break;
        case Impl::Kind::ArchiveInfo:
          SetPathsPostRequest(request, endpoint.uri, "/archiveinfo",
                              pImpl->paths);
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
        Json json;
        XrdCl::XRootDStatus status =
          ParseJsonResponse(body, "stage submission", json);
        if(!status.IsOK()) return status;
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
        Json json;
        XrdCl::XRootDStatus status =
          ParseJsonResponse(body, "stage polling", json);
        if(!status.IsOK()) return status;
        Json normalizedStatus;
        status = NormalizeStageStatusJson(json, normalizedStatus);
        if(status.IsOK()) response = normalizedStatus.dump();
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
