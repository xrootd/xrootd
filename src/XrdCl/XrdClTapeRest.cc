/******************************************************************************/
/*                                                                            */
/*                    X r d C l T a p e R e s t . c c                         */
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

#include "XrdVersion.hh"
#include "XrdCl/XrdClTapeRest.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdOuc/XrdOucJson.hh"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>

#include <curl/curl.h>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace
{
using Json = nlohmann::json;

struct HttpResponse
{
  long statusCode = 0;
  std::string body;
  std::string error;
};

std::string ToLower(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(),
    [](unsigned char c) { return std::tolower(c); });
  return value;
}

std::string Trim(const std::string &value)
{
  const auto begin = std::find_if_not(value.begin(), value.end(),
    [](unsigned char c) { return std::isspace(c); });
  const auto end = std::find_if_not(value.rbegin(), value.rend(),
    [](unsigned char c) { return std::isspace(c); }).base();
  if(begin >= end) return "";
  return std::string(begin, end);
}

std::string GetEnvValue(const char *name)
{
  const char *value = std::getenv(name);
  return value ? value : "";
}

bool FileExists(const std::string &path)
{
  if(path.empty()) return false;
  std::ifstream file(path);
  return file.good();
}

std::string DefaultProxyPath()
{
#ifdef _WIN32
  return "";
#else
  std::ostringstream path;
  path << "/tmp/x509up_u" << getuid();
  return path.str();
#endif
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

std::string JoinUrl(const std::string &base, const std::string &path)
{
  if(base.empty()) return path;
  if(path.empty()) return base;
  if(base.back() == '/' && path.front() == '/') return base + path.substr(1);
  if(base.back() != '/' && path.front() != '/') return base + "/" + path;
  return base + path;
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
  if(end == value.c_str()) return false;
  parsed = static_cast<int>(number);
  return true;
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
  path = CollapseSlashes(url.GetPath());
  return true;
}

std::string ReadBearerToken()
{
  std::string token = GetEnvValue("BEARER_TOKEN");
  if(!token.empty()) return Trim(token);

  const std::string tokenFile = GetEnvValue("BEARER_TOKEN_FILE");
  if(tokenFile.empty()) return "";

  std::ifstream in(tokenFile);
  if(!in) return "";

  std::string value;
  std::getline(in, value);
  return Trim(value);
}

std::pair<std::string, std::string>
ResolveX509Credentials(const XrdCl::TapeRestOptions &options)
{
  if(!options.cert.empty())
  {
    return {options.cert, options.key.empty() ? options.cert : options.key};
  }

  const std::string proxy = GetEnvValue("X509_USER_PROXY");
  if(FileExists(proxy)) return {proxy, proxy};

  const std::string defaultProxy = DefaultProxyPath();
  if(FileExists(defaultProxy)) return {defaultProxy, defaultProxy};

  const std::string envCert = GetEnvValue("X509_USER_CERT");
  const std::string envKey = GetEnvValue("X509_USER_KEY");
  if(!envCert.empty())
  {
    return {envCert, options.key.empty() ? envKey : options.key};
  }

  return {"", options.key};
}

size_t CurlWriteCallback(char *data, size_t size, size_t nmemb, void *userp)
{
  const size_t bytes = size * nmemb;
  auto *output = static_cast<std::string *>(userp);
  output->append(data, bytes);
  return bytes;
}

HttpResponse HttpRequest(const std::string &method, const std::string &url,
                         const std::string &body,
                         const XrdCl::TapeRestOptions &options)
{
  HttpResponse response;
  CURL *curl = curl_easy_init();
  if(!curl)
  {
    response.error = "unable to initialize curl";
    return response;
  }

  char errorBuffer[CURL_ERROR_SIZE];
  errorBuffer[0] = '\0';

  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(headers, "Accept: application/json");

  const std::string token = ReadBearerToken();
  if(!token.empty())
  {
    const std::string header = "Authorization: Bearer " + token;
    headers = curl_slist_append(headers, header.c_str());
  }

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "XrdCl/" XrdVERSION);
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  if(options.verbosity >= 3)
  {
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  }

  if(options.timeout >= 0)
  {
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, options.timeout);
  }

  const auto credentials = ResolveX509Credentials(options);
  if(!credentials.first.empty())
  {
    curl_easy_setopt(curl, CURLOPT_SSLCERT, credentials.first.c_str());
  }
  if(!credentials.second.empty())
  {
    curl_easy_setopt(curl, CURLOPT_SSLKEY, credentials.second.c_str());
  }

  if(method == "POST")
  {
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
      static_cast<long>(body.size()));
  }

  const CURLcode result = curl_easy_perform(curl);
  if(result != CURLE_OK)
  {
    response.error = errorBuffer[0] ? errorBuffer : curl_easy_strerror(result);
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.statusCode);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
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

Json ArchiveInfoRequestBody(const std::vector<std::string> &paths)
{
  Json body;
  body["paths"] = Json::array();
  for(const auto &path : paths)
  {
    body["paths"].push_back(path);
  }
  return body;
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
    if(CollapseSlashes(item["path"].get<std::string>()) == path)
    {
      return &item;
    }
  }
  return nullptr;
}

XrdCl::TapeRestArchiveInfo ArchiveInfoFromJson(const Json *item,
                                               const std::string &url,
                                               const std::string &path)
{
  XrdCl::TapeRestArchiveInfo result;
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
  result.locality = XrdCl::TapeRestClient::LocalityFromString(locality);
  if(result.locality == XrdCl::TapeRestLocality::Unknown)
  {
    result.error = "file locality reported as \"" + locality + "\"";
  }
  return result;
}

XrdCl::XRootDStatus ErrorStatus(uint16_t code, const std::string &message)
{
  return XrdCl::XRootDStatus(XrdCl::stError, code, 0, message);
}

void EnsureCurlInitialized()
{
  static std::once_flag once;
  std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

}

namespace XrdCl
{
  TapeRestClient::TapeRestClient( const TapeRestOptions &options ):
    pOptions( options )
  {
    EnsureCurlInitialized();
  }

  TapeRestClient::~TapeRestClient()
  {
  }

  XRootDStatus TapeRestClient::Discover( const std::string &url,
                                         TapeRestEndpoint &endpoint ) const
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

    TapeRestEndpoint selected;
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

      selectedVersion = parsedVersion;
      selected.uri = candidate["uri"].get<std::string>();
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

  XRootDStatus TapeRestClient::ArchiveInfo(
    const std::vector<std::string> &urls,
    std::vector<TapeRestArchiveInfo> &results ) const
  {
    results.clear();
    if(urls.empty())
    {
      return ErrorStatus(errInvalidArgs, "missing URL");
    }

    TapeRestEndpoint endpoint;
    XRootDStatus status = Discover(urls.front(), endpoint);
    if(!status.IsOK()) return status;

    std::vector<std::string> paths;
    paths.reserve(urls.size());
    for(const auto &url : urls)
    {
      std::string storageEndpoint;
      std::string path;
      std::string error;
      if(!UrlEndpointAndPath(url, storageEndpoint, path, error))
      {
        return ErrorStatus(errInvalidArgs, error);
      }
      paths.push_back(path);
    }

    const std::string archiveInfoUrl = JoinUrl(endpoint.uri, "/archiveinfo");
    const std::string requestBody = ArchiveInfoRequestBody(paths).dump();
    const HttpResponse response =
      HttpRequest("POST", archiveInfoUrl, requestBody, pOptions);

    if(!response.error.empty())
    {
      return ErrorStatus(errConnectionError,
        "archive polling call failed: " + response.error);
    }

    if(response.statusCode != 200)
    {
      return ErrorStatus(errErrorResponse,
        "archive polling call failed: "
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

  std::string TapeRestClient::LocalityToString( TapeRestLocality locality )
  {
    switch(locality)
    {
      case TapeRestLocality::Disk: return "DISK";
      case TapeRestLocality::Tape: return "TAPE";
      case TapeRestLocality::DiskAndTape: return "DISK_AND_TAPE";
      case TapeRestLocality::Lost: return "LOST";
      case TapeRestLocality::None: return "NONE";
      case TapeRestLocality::Unavailable: return "UNAVAILABLE";
      case TapeRestLocality::Unknown: break;
    }
    return "UNKNOWN";
  }

  TapeRestLocality TapeRestClient::LocalityFromString(
    const std::string &locality )
  {
    const std::string value = ToLower(locality);
    if(value == "disk") return TapeRestLocality::Disk;
    if(value == "tape") return TapeRestLocality::Tape;
    if(value == "disk_and_tape") return TapeRestLocality::DiskAndTape;
    if(value == "lost") return TapeRestLocality::Lost;
    if(value == "none") return TapeRestLocality::None;
    if(value == "unavailable") return TapeRestLocality::Unavailable;
    return TapeRestLocality::Unknown;
  }
}
