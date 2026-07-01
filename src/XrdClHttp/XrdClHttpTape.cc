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
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClEnv.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdOuc/XrdOucJson.hh"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include <curl/curl.h>

namespace
{
using Json = nlohmann::json;

struct HttpResponse
{
  long statusCode = 0;
  std::string body;
  std::string error;
};

struct X509Credentials
{
  std::string cert;
  std::string key;
};

using CurlHandle = std::unique_ptr<CURL, void (*)(CURL *)>;
using CurlHeaders = std::unique_ptr<curl_slist, void (*)(curl_slist *)>;

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

struct TapeOptions
{
  int timeout = -1;
};

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

struct TapeStageFile
{
  std::string url;
  std::string path;
  std::string diskLifetime;
  std::string targetedMetadata;
};

struct TapeStageResponse
{
  std::string requestId;
};

struct TapeStageFileStatus
{
  std::string path;
  bool onDisk = false;
  bool hasOnDisk = false;
  std::string state;
  std::string error;
  std::uint64_t startedAt = 0;
  bool hasStartedAt = false;
  std::uint64_t finishedAt = 0;
  bool hasFinishedAt = false;
};

struct TapeStageStatus
{
  std::string id;
  std::uint64_t createdAt = 0;
  bool hasCreatedAt = false;
  std::uint64_t startedAt = 0;
  bool hasStartedAt = false;
  std::uint64_t completedAt = 0;
  bool hasCompletedAt = false;
  std::vector<TapeStageFileStatus> files;
};

class TapeClient
{
  public:
    explicit TapeClient( const TapeOptions &options = TapeOptions() );

    XrdCl::XRootDStatus Discover( const std::string &url,
                                  TapeEndpoint &endpoint ) const;

    XrdCl::XRootDStatus Stage( const std::string &url,
                               const std::vector<TapeStageFile> &files,
                               TapeStageResponse &response ) const;

    XrdCl::XRootDStatus StageStatus( const std::string &url,
                                     const std::string &requestId,
                                     TapeStageStatus &response ) const;

    XrdCl::XRootDStatus StageCancel(
      const std::string &url,
      const std::string &requestId,
      const std::vector<std::string> &paths ) const;

    XrdCl::XRootDStatus StageDelete( const std::string &url,
                                     const std::string &requestId ) const;

    XrdCl::XRootDStatus Release(
      const std::string &url,
      const std::string &requestId,
      const std::vector<std::string> &paths ) const;

    XrdCl::XRootDStatus ArchiveInfo(
      const std::vector<std::string> &urls,
      std::vector<TapeArchiveInfo> &results ) const;

    static std::string LocalityToString( TapeLocality locality );
    static TapeLocality LocalityFromString( const std::string &locality );

  private:
    TapeOptions pOptions;
};

bool UseClientX509(XrdCl::Env *env)
{
  if(!env) return false;

  int disableX509 = 0;
  return env->GetInt("HttpDisableX509", disableX509) && !disableX509;
}

X509Credentials GetClientX509Credentials(XrdCl::Env *env)
{
  X509Credentials credentials;
  if(!env) return credentials;

  env->GetString("HttpClientCertFile", credentials.cert);
  env->GetString("HttpClientKeyFile", credentials.key);
  return credentials;
}

void ApplyClientX509Credentials(CURL *curl,
                                const X509Credentials &credentials)
{
  if(credentials.cert.empty()) return;

  XrdCl::Log *log = XrdCl::DefaultEnv::GetLog();
  if(log)
  {
    log->Debug(XrdClHttp::kLogXrdClHttp,
               "Using client X.509 credential found at %s",
               credentials.cert.c_str());
  }
  curl_easy_setopt(curl, CURLOPT_SSLCERT, credentials.cert.c_str());

  if(credentials.key.empty())
  {
    if(log)
    {
      log->Error(XrdClHttp::kLogXrdClHttp,
                 "X.509 client credential specified but not the client key");
    }
    return;
  }
  curl_easy_setopt(curl, CURLOPT_SSLKEY, credentials.key.c_str());
}

std::string ToLower(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(),
    [](unsigned char c) { return std::tolower(c); });
  return value;
}

std::string TrimCopy(std::string value)
{
  XrdCl::Utils::Trim(value);
  return value;
}

std::string GetEnvString(const std::string &key,
                         const std::string &shellKey)
{
  XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
  if(!env) return "";

  std::string value;
  if(!env->GetString(key, value) || value.empty())
  {
    env->ImportString(key, shellKey);
    env->GetString(key, value);
  }
  return value;
}

std::string CollapseSlashes(const std::string &path)
{
  std::string result;
  result.reserve(path.size());
  bool previousSlash = false;
  for(const char c : path)
  {
    const bool currentSlash = c == '/';
    if(currentSlash && previousSlash) continue;
    result += c;
    previousSlash = currentSlash;
  }
  return result.empty() ? "/" : result;
}

std::string NormalizeTapePath(const std::string &path)
{
  std::string result = CollapseSlashes(path);
  if(result.front() != '/') result.insert(result.begin(), '/');
  return result;
}

std::string JoinUrl(const std::string &base, const std::string &path)
{
  if(base.empty()) return path;
  if(path.empty()) return base;
  if(base.back() == '/' && path.front() == '/') return base + path.substr(1);
  if(base.back() != '/' && path.front() != '/') return base + "/" + path;
  return base + path;
}

bool IsUnreservedUrlChar(unsigned char c)
{
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
    || (c >= '0' && c <= '9') || c == '-' || c == '.'
    || c == '_' || c == '~';
}

std::string PercentEncodeUrlPathSegment(const std::string &value)
{
  static const char hex[] = "0123456789ABCDEF";
  std::string result;
  result.reserve(value.size());
  for(const unsigned char c : value)
  {
    if(IsUnreservedUrlChar(c))
    {
      result += static_cast<char>(c);
      continue;
    }
    result += '%';
    result += hex[c >> 4];
    result += hex[c & 0x0f];
  }
  return result;
}

bool ParseVersion(const std::string &version, int &parsed)
{
  std::string value = ToLower(version);
  if(!value.empty() && value.front() == 'v') value.erase(value.begin());
  if(value.empty() || !std::isdigit(static_cast<unsigned char>(value.front())))
  {
    return false;
  }

  char *end = nullptr;
  const long number = std::strtol(value.c_str(), &end, 10);
  if(end == value.c_str() || *end != '\0' || number < 0) return false;
  parsed = static_cast<int>(number);
  return true;
}

bool IsHttpEndpointUri(const std::string &uri)
{
  XrdCl::URL url(uri);
  if(!url.IsValid() || url.GetHostName().empty()) return false;

  const std::string protocol = ToLower(url.GetProtocol());
  return protocol == "http" || protocol == "https";
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

  std::string protocol = ToLower(url.GetProtocol());
  bool useUrlPort = true;
  if(protocol == "davs") protocol = "https";
  else if(protocol == "dav") protocol = "http";
  else if(protocol == "root" || protocol == "xroot")
  {
    protocol = "https";
    useUrlPort = false;
  }

  if(protocol != "http" && protocol != "https")
  {
    error = "unsupported URL protocol '" + url.GetProtocol()
      + "' for Tape REST API";
    return false;
  }

  std::ostringstream out;
  out << protocol << "://" << url.GetHostName();
  if(useUrlPort && url.GetPort() > 0) out << ":" << url.GetPort();
  endpoint = out.str();
  path = NormalizeTapePath(url.GetPath());
  return true;
}

std::string ReadBearerToken()
{
  std::string token = GetEnvString("BearerToken", "BEARER_TOKEN");
  if(!token.empty()) return TrimCopy(token);

  const std::string tokenFile =
    GetEnvString("BearerTokenFile", "BEARER_TOKEN_FILE");
  if(tokenFile.empty()) return "";

  std::ifstream in(tokenFile);
  if(!in) return "";

  std::string value;
  std::getline(in, value);
  return TrimCopy(value);
}

size_t CurlWriteCallback(char *data, size_t size, size_t nmemb, void *userp)
{
  const size_t bytes = size * nmemb;
  auto *output = static_cast<std::string *>(userp);
  output->append(data, bytes);
  return bytes;
}

bool AppendCurlHeader(CurlHeaders &headers, const std::string &header)
{
  curl_slist *updated = curl_slist_append(headers.get(), header.c_str());
  if(!updated) return false;
  if(updated != headers.get())
  {
    headers.release();
    headers.reset(updated);
  }
  return true;
}

HttpResponse HttpRequest(const std::string &method, const std::string &url,
                         const std::string &body,
                         const TapeOptions &options)
{
  HttpResponse response;
  XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
  CurlHandle curl(XrdClHttp::GetHandle(false), curl_easy_cleanup);
  if(!curl)
  {
    response.error = "unable to initialize curl";
    return response;
  }

  char errorBuffer[CURL_ERROR_SIZE];
  errorBuffer[0] = '\0';

  CurlHeaders headers(nullptr, curl_slist_free_all);
  if(!AppendCurlHeader(headers, "Accept: application/json"))
  {
    response.error = "unable to allocate HTTP headers";
    return response;
  }

  const std::string token = ReadBearerToken();
  if(!token.empty())
  {
    const std::string header = "Authorization: Bearer " + token;
    if(!AppendCurlHeader(headers, header))
    {
      response.error = "unable to allocate HTTP headers";
      return response;
    }
  }

  curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, errorBuffer);
  curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, CurlWriteCallback);
  curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response.body);
  curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
  curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
#if CURL_AT_LEAST_VERSION(7, 85, 0)
  curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "https,http");
  curl_easy_setopt(curl.get(), CURLOPT_REDIR_PROTOCOLS_STR, "https,http");
#else
  const long protocols = CURLPROTO_HTTP | CURLPROTO_HTTPS;
  curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS, protocols);
  curl_easy_setopt(curl.get(), CURLOPT_REDIR_PROTOCOLS, protocols);
#endif

  if(options.timeout >= 0)
  {
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, options.timeout);
  }

  if(UseClientX509(env))
  {
    ApplyClientX509Credentials(curl.get(), GetClientX509Credentials(env));
  }

  if(method == "POST")
  {
    if(!AppendCurlHeader(headers, "Content-Type: application/json"))
    {
      response.error = "unable to allocate HTTP headers";
      return response;
    }
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE,
      static_cast<long>(body.size()));
  }
  else if(method == "DELETE")
  {
    curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, "DELETE");
  }

  const CURLcode result = curl_easy_perform(curl.get());
  if(result != CURLE_OK)
  {
    response.error = errorBuffer[0] ? errorBuffer : curl_easy_strerror(result);
  }

  curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response.statusCode);
  return response;
}

std::string FormatProblemResponse(long statusCode, const std::string &body)
{
  std::ostringstream out;
  out << "HTTP " << statusCode;
  if(body.empty()) return out.str();

  try
  {
    const Json json = Json::parse(body);
    if(json.is_object())
    {
      const auto title = json.find("title");
      const auto detail = json.find("detail");

      if(title != json.end() && title->is_string())
      {
        out << ": " << title->get<std::string>();
        if(detail != json.end() && detail->is_string())
        {
          out << " - " << detail->get<std::string>();
        }
        return out.str();
      }
    }
  }
  catch(const std::exception &)
  {
  }

  out << ": " << body;
  return out.str();
}

XrdCl::XRootDStatus ErrorStatus(uint16_t code, const std::string &message)
{
  return XrdCl::XRootDStatus(XrdCl::stError, code, 0, message);
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

XrdCl::XRootDStatus DiscoverEndpoint(const TapeClient &client,
                                     const std::string &url,
                                     TapeEndpoint &endpoint)
{
  if(url.empty())
  {
    return ErrorStatus(XrdCl::errInvalidArgs, "missing URL");
  }
  return client.Discover(url, endpoint);
}

Json PathsRequestBody(const std::vector<std::string> &paths)
{
  Json body;
  body["paths"] = Json::array();
  for(const auto &path : paths)
  {
    body["paths"].push_back(path);
  }
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

Json StageRequestBody(const std::vector<TapeStageFile> &files,
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
      Json metadata;
      try
      {
        metadata = Json::parse(file.targetedMetadata);
      }
      catch(const std::exception &ex)
      {
        error = "invalid targetedMetadata JSON: " + std::string(ex.what());
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

bool JsonUInt64(const Json &json, const char *key, std::uint64_t &value,
                bool &hasValue)
{
  hasValue = false;
  const auto it = json.find(key);
  if(it == json.end()) return true;
  if(it->is_number_unsigned())
  {
    value = it->get<std::uint64_t>();
  }
  else if(it->is_number_integer() && it->get<std::int64_t>() >= 0)
  {
    value = static_cast<std::uint64_t>(it->get<std::int64_t>());
  }
  else
  {
    return false;
  }
  hasValue = true;
  return true;
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
  if(json.contains("onDisk") && json["onDisk"].is_boolean())
  {
    result.onDisk = json["onDisk"].get<bool>();
    result.hasOnDisk = true;
  }
  if(json.contains("state") && json["state"].is_string())
  {
    result.state = json["state"].get<std::string>();
  }
  if(json.contains("error") && json["error"].is_string())
  {
    result.error = json["error"].get<std::string>();
  }
  if(!JsonUInt64(json, "startedAt", result.startedAt, result.hasStartedAt)
     || !JsonUInt64(json, "finishedAt", result.finishedAt,
                    result.hasFinishedAt))
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response contains a non-integer file timestamp");
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
  if(!json.contains("files") || !json["files"].is_array())
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response does not contain a files array");
  }

  status.id = json["id"].get<std::string>();
  if(!JsonUInt64(json, "createdAt", status.createdAt, status.hasCreatedAt)
     || !JsonUInt64(json, "startedAt", status.startedAt, status.hasStartedAt)
     || !JsonUInt64(json, "completedAt", status.completedAt,
                    status.hasCompletedAt))
  {
    return ErrorStatus(XrdCl::errInvalidResponse,
      "stage request response contains a non-integer timestamp");
  }

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

XrdCl::XRootDStatus EmptyResponseStatus(const HttpResponse &response,
                                        const std::string &operation)
{
  if(!response.error.empty())
  {
    return ErrorStatus(XrdCl::errConnectionError,
      operation + " failed: " + response.error);
  }
  if(response.statusCode != 200 && response.statusCode != 204)
  {
    return ErrorStatus(XrdCl::errErrorResponse,
      operation + " failed: "
      + FormatProblemResponse(response.statusCode, response.body));
  }
  return XrdCl::XRootDStatus();
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
  result.locality = TapeClient::LocalityFromString(locality);
  if(result.locality == TapeLocality::Unknown)
  {
    result.error = "file locality reported as \"" + locality + "\"";
  }
  return result;
}

}

namespace
{
  using XrdCl::errConnectionError;
  using XrdCl::errErrorResponse;
  using XrdCl::errInvalidArgs;
  using XrdCl::errInvalidResponse;
  using XrdCl::errNotSupported;
  using XrdCl::XRootDStatus;

  TapeClient::TapeClient( const TapeOptions &options ):
    pOptions( options )
  {
  }

  XRootDStatus TapeClient::Discover( const std::string &url,
                                     TapeEndpoint &endpoint ) const
  {
    std::string storageEndpoint;
    std::string path;
    std::string error;
    if(!UrlEndpointAndPath(url, storageEndpoint, path, error))
    {
      return ErrorStatus(errInvalidArgs, error);
    }

    const std::string discoveryUrl =
      JoinUrl(storageEndpoint, "/.well-known/wlcg-tape-rest-api");
    const HttpResponse response =
      HttpRequest("GET", discoveryUrl, "", pOptions);

    if(!response.error.empty())
    {
      return ErrorStatus(errConnectionError,
        "failed to query " + discoveryUrl + ": " + response.error);
    }

    if(response.statusCode != 200)
    {
      return ErrorStatus(errErrorResponse,
        "failed to query " + discoveryUrl + ": "
        + FormatProblemResponse(response.statusCode, response.body));
    }

    Json json;
    try
    {
      json = Json::parse(response.body);
    }
    catch(const std::exception &ex)
    {
      return ErrorStatus(errInvalidResponse,
        "malformed discovery response: " + std::string(ex.what()));
    }

    if(!json.contains("sitename") || !json["sitename"].is_string())
    {
      return ErrorStatus(errInvalidResponse,
        "discovery response does not contain a string sitename");
    }
    if(!json.contains("endpoints") || !json["endpoints"].is_array())
    {
      return ErrorStatus(errInvalidResponse,
        "discovery response does not contain an endpoints array");
    }

    TapeEndpoint selected;
    int selectedVersion = -1;
    for(const auto &candidate : json["endpoints"])
    {
      if(!candidate.contains("uri") || !candidate["uri"].is_string()
         || !candidate.contains("version")
         || !candidate["version"].is_string())
      {
        continue;
      }

      int parsedVersion = -1;
      const auto version = candidate["version"].get<std::string>();
      if(!ParseVersion(version, parsedVersion)) continue;
      if(parsedVersion > 1 || parsedVersion < selectedVersion) continue;

      const auto uri = candidate["uri"].get<std::string>();
      if(!IsHttpEndpointUri(uri)) continue;

      selectedVersion = parsedVersion;
      selected.uri = uri;
      selected.version = version;
    }

    if(selected.uri.empty())
    {
      return ErrorStatus(errNotSupported,
        "discovery response does not advertise a supported v0/v1 endpoint");
    }

    selected.sitename = json["sitename"].get<std::string>();
    endpoint = selected;
    return XRootDStatus();
  }

  XRootDStatus TapeClient::Stage(
    const std::string &url, const std::vector<TapeStageFile> &files,
    TapeStageResponse &stageResponse ) const
  {
    stageResponse = TapeStageResponse();
    if(files.empty())
    {
      return ErrorStatus(errInvalidArgs, "missing stage files");
    }

    TapeEndpoint endpoint;
    XRootDStatus status = DiscoverEndpoint(*this, url, endpoint);
    if(!status.IsOK()) return status;

    std::string error;
    const Json body = StageRequestBody(files, error);
    if(!error.empty()) return ErrorStatus(errInvalidArgs, error);

    const std::string stageUrl = JoinUrl(endpoint.uri, "/stage");
    const HttpResponse response =
      HttpRequest("POST", stageUrl, body.dump(), pOptions);

    if(!response.error.empty())
    {
      return ErrorStatus(errConnectionError,
        "stage request submission failed: " + response.error);
    }

    if(response.statusCode != 201)
    {
      return ErrorStatus(errErrorResponse,
        "stage request submission failed: "
        + FormatProblemResponse(response.statusCode, response.body));
    }

    Json json;
    try
    {
      json = Json::parse(response.body);
    }
    catch(const std::exception &ex)
    {
      return ErrorStatus(errInvalidResponse,
        "malformed stage submission response: " + std::string(ex.what()));
    }

    if(!json.contains("requestId") || !json["requestId"].is_string())
    {
      return ErrorStatus(errInvalidResponse,
        "stage submission response does not contain a string requestId");
    }

    stageResponse.requestId = json["requestId"].get<std::string>();
    return XRootDStatus();
  }

  XRootDStatus TapeClient::StageStatus(
    const std::string &url, const std::string &requestId,
    TapeStageStatus &stageStatus ) const
  {
    stageStatus = TapeStageStatus();
    if(requestId.empty())
    {
      return ErrorStatus(errInvalidArgs, "missing stage request id");
    }

    TapeEndpoint endpoint;
    XRootDStatus status = DiscoverEndpoint(*this, url, endpoint);
    if(!status.IsOK()) return status;

    const std::string encodedRequestId =
      PercentEncodeUrlPathSegment(requestId);
    const std::string stageUrl =
      JoinUrl(endpoint.uri, "/stage/" + encodedRequestId);
    const HttpResponse response = HttpRequest("GET", stageUrl, "", pOptions);

    if(!response.error.empty())
    {
      return ErrorStatus(errConnectionError,
        "stage request polling failed: " + response.error);
    }

    if(response.statusCode != 200)
    {
      return ErrorStatus(errErrorResponse,
        "stage request polling failed: "
        + FormatProblemResponse(response.statusCode, response.body));
    }

    Json json;
    try
    {
      json = Json::parse(response.body);
    }
    catch(const std::exception &ex)
    {
      return ErrorStatus(errInvalidResponse,
        "malformed stage polling response: " + std::string(ex.what()));
    }
    return StageStatusFromJson(json, stageStatus);
  }

  XRootDStatus TapeClient::StageCancel(
    const std::string &url, const std::string &requestId,
    const std::vector<std::string> &inputs ) const
  {
    if(requestId.empty())
    {
      return ErrorStatus(errInvalidArgs, "missing stage request id");
    }
    if(inputs.empty())
    {
      return ErrorStatus(errInvalidArgs, "missing paths to cancel");
    }

    TapeEndpoint endpoint;
    XRootDStatus status = DiscoverEndpoint(*this, url, endpoint);
    if(!status.IsOK()) return status;

    std::vector<std::string> paths;
    status = PathsFromInputs(inputs, paths);
    if(!status.IsOK()) return status;

    const std::string encodedRequestId =
      PercentEncodeUrlPathSegment(requestId);
    const std::string cancelUrl =
      JoinUrl(endpoint.uri, "/stage/" + encodedRequestId + "/cancel");
    const HttpResponse response = HttpRequest(
      "POST", cancelUrl, PathsRequestBody(paths).dump(), pOptions);
    return EmptyResponseStatus(response, "stage request cancellation");
  }

  XRootDStatus TapeClient::StageDelete(
    const std::string &url, const std::string &requestId ) const
  {
    if(requestId.empty())
    {
      return ErrorStatus(errInvalidArgs, "missing stage request id");
    }

    TapeEndpoint endpoint;
    XRootDStatus status = DiscoverEndpoint(*this, url, endpoint);
    if(!status.IsOK()) return status;

    const std::string encodedRequestId =
      PercentEncodeUrlPathSegment(requestId);
    const std::string stageUrl =
      JoinUrl(endpoint.uri, "/stage/" + encodedRequestId);
    const HttpResponse response = HttpRequest("DELETE", stageUrl, "", pOptions);
    return EmptyResponseStatus(response, "stage request deletion");
  }

  XRootDStatus TapeClient::Release(
    const std::string &url, const std::string &requestId,
    const std::vector<std::string> &inputs ) const
  {
    if(requestId.empty())
    {
      return ErrorStatus(errInvalidArgs, "missing stage request id");
    }
    if(inputs.empty())
    {
      return ErrorStatus(errInvalidArgs, "missing paths to release");
    }

    TapeEndpoint endpoint;
    XRootDStatus status = DiscoverEndpoint(*this, url, endpoint);
    if(!status.IsOK()) return status;

    std::vector<std::string> paths;
    status = PathsFromInputs(inputs, paths);
    if(!status.IsOK()) return status;

    const std::string encodedRequestId =
      PercentEncodeUrlPathSegment(requestId);
    const std::string releaseUrl =
      JoinUrl(endpoint.uri, "/release/" + encodedRequestId);
    const HttpResponse response = HttpRequest(
      "POST", releaseUrl, PathsRequestBody(paths).dump(), pOptions);
    return EmptyResponseStatus(response, "stage request release");
  }

  XRootDStatus TapeClient::ArchiveInfo(
    const std::vector<std::string> &urls,
    std::vector<TapeArchiveInfo> &results ) const
  {
    results.clear();
    if(urls.empty())
    {
      return ErrorStatus(errInvalidArgs, "missing URL");
    }

    TapeEndpoint endpoint;
    XRootDStatus status = Discover(urls.front(), endpoint);
    if(!status.IsOK()) return status;

    std::string firstStorageEndpoint;
    std::string firstPath;
    std::string firstError;
    if(!UrlEndpointAndPath(urls.front(), firstStorageEndpoint, firstPath,
                           firstError))
    {
      return ErrorStatus(errInvalidArgs, firstError);
    }

    std::vector<std::string> paths;
    paths.reserve(urls.size());
    paths.push_back(firstPath);
    for(auto it = urls.begin() + 1; it != urls.end(); ++it)
    {
      std::string storageEndpoint;
      std::string path;
      std::string error;
      if(!UrlEndpointAndPath(*it, storageEndpoint, path, error))
      {
        return ErrorStatus(errInvalidArgs, error);
      }
      if(storageEndpoint != firstStorageEndpoint)
      {
        return ErrorStatus(errInvalidArgs,
          "archiveinfo URLs must belong to the same storage endpoint");
      }
      paths.push_back(path);
    }

    const std::string archiveInfoUrl = JoinUrl(endpoint.uri, "/archiveinfo");
    const std::string requestBody = PathsRequestBody(paths).dump();
    const HttpResponse response =
      HttpRequest("POST", archiveInfoUrl, requestBody, pOptions);

    if(!response.error.empty())
    {
      return ErrorStatus(errConnectionError,
        "archiveinfo call failed: " + response.error);
    }

    if(response.statusCode != 200)
    {
      return ErrorStatus(errErrorResponse,
        "archiveinfo call failed: "
        + FormatProblemResponse(response.statusCode, response.body));
    }

    Json json;
    try
    {
      json = Json::parse(response.body);
    }
    catch(const std::exception &ex)
    {
      return ErrorStatus(errInvalidResponse,
        "malformed archiveinfo response: " + std::string(ex.what()));
    }

    if(!json.is_array())
    {
      return ErrorStatus(errInvalidResponse,
        "archiveinfo response is not a JSON array");
    }

    results.reserve(paths.size());
    for(std::size_t i = 0; i < paths.size(); ++i)
    {
      results.push_back(ArchiveInfoFromJson(
        FindArchiveInfoItem(json, paths[i]), urls[i], paths[i]));
    }
    return XRootDStatus();
  }

  std::string TapeClient::LocalityToString( TapeLocality locality )
  {
    switch(locality)
    {
      case TapeLocality::Disk: return "DISK";
      case TapeLocality::Tape: return "TAPE";
      case TapeLocality::DiskAndTape: return "DISK_AND_TAPE";
      case TapeLocality::Lost: return "LOST";
      case TapeLocality::None: return "NONE";
      case TapeLocality::Unavailable: return "UNAVAILABLE";
      case TapeLocality::Unknown: break;
    }
    return "UNKNOWN";
  }

  TapeLocality TapeClient::LocalityFromString(
    const std::string &locality )
  {
    const std::string value = ToLower(locality);
    if(value == "disk") return TapeLocality::Disk;
    if(value == "tape") return TapeLocality::Tape;
    if(value == "disk_and_tape") return TapeLocality::DiskAndTape;
    if(value == "lost") return TapeLocality::Lost;
    if(value == "none") return TapeLocality::None;
    if(value == "unavailable") return TapeLocality::Unavailable;
    return TapeLocality::Unknown;
  }

  TapeOptions MakeOptions( int timeout )
  {
    TapeOptions options;
    options.timeout = timeout;
    return options;
  }

  std::vector<TapeStageFile> MakeStageFiles(
    const std::vector<std::array<std::string, 4>> &entries )
  {
    std::vector<TapeStageFile> files;
    files.reserve(entries.size());
    for(const auto &entry : entries)
    {
      TapeStageFile file;
      file.url = entry[0];
      file.path = entry[1];
      file.diskLifetime = entry[2];
      file.targetedMetadata = entry[3];
      files.push_back(file);
    }
    return files;
  }

  Json StageFileStatusToJson( const TapeStageFileStatus &status )
  {
    Json json;
    json["path"] = status.path;
    if(status.hasOnDisk) json["onDisk"] = status.onDisk;
    if(!status.state.empty()) json["state"] = status.state;
    if(!status.error.empty()) json["error"] = status.error;
    if(status.hasStartedAt) json["startedAt"] = status.startedAt;
    if(status.hasFinishedAt) json["finishedAt"] = status.finishedAt;
    return json;
  }

  Json StageStatusToJson( const TapeStageStatus &status )
  {
    Json json;
    json["id"] = status.id;
    if(status.hasCreatedAt) json["createdAt"] = status.createdAt;
    if(status.hasStartedAt) json["startedAt"] = status.startedAt;
    if(status.hasCompletedAt) json["completedAt"] = status.completedAt;
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
      json["locality"] = TapeClient::LocalityToString(info.locality);
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
  XrdCl::XRootDStatus TapeDiscover( const std::string &url,
                                    int timeout,
                                    std::string &uri,
                                    std::string &version,
                                    std::string &sitename )
  {
    TapeClient client(MakeOptions(timeout));
    TapeEndpoint endpoint;
    XrdCl::XRootDStatus status = client.Discover(url, endpoint);
    if(status.IsOK())
    {
      uri = endpoint.uri;
      version = endpoint.version;
      sitename = endpoint.sitename;
    }
    return status;
  }

  XrdCl::XRootDStatus TapeStage(
    const std::string &url,
    const std::vector<std::array<std::string, 4>> &files,
    int timeout,
    std::string &requestId )
  {
    TapeClient client(MakeOptions(timeout));
    TapeStageResponse response;
    XrdCl::XRootDStatus status =
      client.Stage(url, MakeStageFiles(files), response);
    if(status.IsOK()) requestId = response.requestId;
    return status;
  }

  XrdCl::XRootDStatus TapeStageStatus( const std::string &url,
                                       const std::string &requestId,
                                       int timeout,
                                       std::string &responseJson )
  {
    TapeClient client(MakeOptions(timeout));
    struct TapeStageStatus response;
    XrdCl::XRootDStatus status = client.StageStatus(url, requestId, response);
    responseJson = status.IsOK() ? StageStatusToJson(response).dump() : "{}";
    return status;
  }

  XrdCl::XRootDStatus TapeStageCancel(
      const std::string &url,
      const std::string &requestId,
      const std::vector<std::string> &paths,
      int timeout )
  {
    TapeClient client(MakeOptions(timeout));
    return client.StageCancel(url, requestId, paths);
  }

  XrdCl::XRootDStatus TapeStageDelete( const std::string &url,
                                       const std::string &requestId,
                                       int timeout )
  {
    TapeClient client(MakeOptions(timeout));
    return client.StageDelete(url, requestId);
  }

  XrdCl::XRootDStatus TapeRelease( const std::string &url,
                                   const std::string &requestId,
                                   const std::vector<std::string> &paths,
                                   int timeout )
  {
    TapeClient client(MakeOptions(timeout));
    return client.Release(url, requestId, paths);
  }

  XrdCl::XRootDStatus TapeArchiveInfo(
    const std::vector<std::string> &urls,
    int timeout,
    std::string &responseJson )
  {
    TapeClient client(MakeOptions(timeout));
    std::vector<struct TapeArchiveInfo> response;
    XrdCl::XRootDStatus status = client.ArchiveInfo(urls, response);

    Json json = Json::array();
    for(const auto &info : response)
    {
      json.push_back(ArchiveInfoToJson(info));
    }
    responseJson = json.dump();
    return status;
  }
}
