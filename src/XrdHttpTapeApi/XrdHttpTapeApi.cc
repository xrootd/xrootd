/******************************************************************************/
/*                                                                            */
/*                  X r d H t t p T a p e A p i . c c                         */
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

#include "XrdHttp/XrdHttpExtHandler.hh"
#include "XrdHttpTapeApiStore.hh"
#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucJson.hh"
#include "XrdOuc/XrdOucTUtils.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdVersion.hh"

#include <climits>
#include <cstdlib>
#include <exception>
#include <string>

namespace
{
using Json = nlohmann::json;

constexpr char kDiscoveryPath[] = "/.well-known/wlcg-tape-rest-api";
constexpr char kStagePath[] = "/api/v1/stage";
constexpr char kStagePrefix[] = "/api/v1/stage/";
constexpr char kStageCancelSuffix[] = "/cancel";
constexpr char kReleasePrefix[] = "/api/v1/release/";
constexpr char kArchiveInfoPath[] = "/api/v1/archiveinfo";
constexpr long long kDefaultMaxRequestSize = 4 * 1024 * 1024;

class TapeApiHandler final : public XrdHttpExtHandler
{
  public:
    TapeApiHandler(const std::string &root, long long maxRequestSize,
                   const std::string &siteName)
      : m_store(root), m_maxRequestSize(maxRequestSize),
        m_siteName(siteName) {}

    bool MatchesPath(const char *verb, const char *path) override;
    int ProcessReq(XrdHttpExtReq &req) override;
    int Init(const char *cfgfile) override;
    const std::string &InitializationError() const { return m_initError; }

  private:
    static int SendJson(XrdHttpExtReq &req, int code,
                        const std::string &body,
                        const std::string &additionalHeaders = {});
    static int SendError(XrdHttpExtReq &req, int code,
                         const std::string &message);
    static int SendStatus(XrdHttpExtReq &req,
                          const XrdHttpTapeApiStore::Status &status,
                          const std::string &body = {});
    bool ReadBody(XrdHttpExtReq &req, std::string &body,
                  int &errorCode, std::string &error);
    static bool ParseJsonBody(const std::string &body, Json &json,
                              std::string &error);
    static bool ParsePaths(const std::string &body, Json &paths,
                           std::string &error);
    static bool ExtractRequestId(const std::string &resource,
                                 const std::string &prefix,
                                 std::string &requestId);
    int Discovery(XrdHttpExtReq &req);
    int Stage(XrdHttpExtReq &req, const std::string &body);
    int StageStatus(XrdHttpExtReq &req, const std::string &requestId);
    int StageCancel(XrdHttpExtReq &req, const std::string &requestId,
                    const std::string &body);
    int StageDelete(XrdHttpExtReq &req, const std::string &requestId);
    int Release(XrdHttpExtReq &req, const std::string &requestId,
                const std::string &body);
    int ArchiveInfo(XrdHttpExtReq &req, const std::string &body);

    XrdHttpTapeApiStore m_store;
    long long m_maxRequestSize;
    std::string m_siteName;
    std::string m_initError;
};

int TapeApiHandler::SendJson(XrdHttpExtReq &req, int code,
                             const std::string &body,
                             const std::string &additionalHeaders)
{
  std::string headers = "Content-Type: application/json";
  if(!additionalHeaders.empty())
  {
    headers += "\r\n";
    headers += additionalHeaders;
  }
  // A null description makes XrdHttp fill in the standard reason phrase.
  return req.SendSimpleResp(code, nullptr, headers.c_str(),
                            body.c_str(), body.size());
}

int TapeApiHandler::SendError(XrdHttpExtReq &req, int code,
                              const std::string &message)
{
  const std::string body = Json({{"status", code}, {"title", message}}).dump();
  return req.SendSimpleResp(code, nullptr,
    "Content-Type: application/problem+json", body.c_str(), body.size());
}

int TapeApiHandler::SendStatus(
  XrdHttpExtReq &req, const XrdHttpTapeApiStore::Status &status,
  const std::string &body)
{
  return status ? SendJson(req, status.code, body)
                : SendError(req, status.code, status.message);
}

bool TapeApiHandler::ReadBody(XrdHttpExtReq &req, std::string &body,
                              int &errorCode, std::string &error)
{
  body.clear();
  if(req.length == 0) return true;
  if(req.length < 0)
  {
    errorCode = 400;
    error = "invalid negative request length";
    return false;
  }
  if(req.length > m_maxRequestSize)
  {
    errorCode = 413;
    error = "request too large";
    return false;
  }

  // BuffgetData returns a borrowed view into the protocol buffer; it does not
  // transfer ownership to the handler.
  char *buffer = nullptr;
  const int bytesRead = req.BuffgetData(
    static_cast<int>(req.length), &buffer, true);
  if(bytesRead != req.length || buffer == nullptr)
  {
    errorCode = 400;
    error = "missing or invalid request body";
    return false;
  }
  body.assign(buffer, buffer + bytesRead);
  return true;
}

bool TapeApiHandler::ParseJsonBody(const std::string &body, Json &json,
                                   std::string &error)
{
  try
  {
    json = Json::parse(body);
    return true;
  }
  catch(const std::exception &ex)
  {
    error = "malformed JSON request: " + std::string(ex.what());
    return false;
  }
}

bool TapeApiHandler::ParsePaths(const std::string &body, Json &paths,
                                std::string &error)
{
  Json json;
  if(!ParseJsonBody(body, json, error)) return false;
  if(!json.is_object() || !json.contains("paths")
     || !json["paths"].is_array() || json["paths"].empty())
  {
    error = "request must contain a non-empty paths array";
    return false;
  }
  for(const auto &item : json["paths"])
  {
    if(!item.is_string() || item.get<std::string>().empty())
    {
      error = "paths entries must be non-empty strings";
      return false;
    }
  }
  paths = std::move(json["paths"]);
  return true;
}

bool TapeApiHandler::ExtractRequestId(const std::string &resource,
                                      const std::string &prefix,
                                      std::string &requestId)
{
  if(resource.compare(0, prefix.size(), prefix) != 0) return false;
  requestId = resource.substr(prefix.size());
  return !requestId.empty() && requestId.find('/') == std::string::npos;
}

bool TapeApiHandler::MatchesPath(const char * /*verb*/, const char *path)
{
  if(!path) return false;

  const std::string resource(path);
  return resource == kDiscoveryPath
         || resource == kStagePath
         || resource.compare(0, sizeof(kStagePrefix) - 1, kStagePrefix) == 0
         || resource.compare(0, sizeof(kReleasePrefix) - 1,
                             kReleasePrefix) == 0
         || resource == kArchiveInfoPath;
}

int TapeApiHandler::ProcessReq(XrdHttpExtReq &req)
{
  const std::string resource = req.resource;

  std::string body;
  if(req.verb == "POST")
  {
    int errorCode = 400;
    std::string error;
    if(!ReadBody(req, body, errorCode, error))
    {
      return SendError(req, errorCode, error);
    }
  }

  if(resource == kDiscoveryPath) return Discovery(req);
  if(resource == kStagePath) return Stage(req, body);
  if(resource == kArchiveInfoPath) return ArchiveInfo(req, body);

  std::string requestId;
  if(resource.compare(0, sizeof(kStagePrefix) - 1, kStagePrefix) == 0)
  {
    if(resource.size() > sizeof(kStageCancelSuffix) - 1
       && resource.compare(resource.size() - (sizeof(kStageCancelSuffix) - 1),
                           sizeof(kStageCancelSuffix) - 1,
                           kStageCancelSuffix) == 0)
    {
      const std::string stageResource = resource.substr(
        0, resource.size() - (sizeof(kStageCancelSuffix) - 1));
      if(ExtractRequestId(stageResource, kStagePrefix, requestId))
      {
        return StageCancel(req, requestId, body);
      }
    }
    else if(ExtractRequestId(resource, kStagePrefix, requestId))
    {
      return req.verb == "DELETE" ? StageDelete(req, requestId)
                                   : StageStatus(req, requestId);
    }
    return SendError(req, 404, "unknown stage request");
  }

  if(ExtractRequestId(resource, kReleasePrefix, requestId))
  {
    return Release(req, requestId, body);
  }
  return SendError(req, 404, "unexpected Tape REST API path");
}

int TapeApiHandler::Init(const char * /*cfgfile*/)
{
  const auto status = m_store.Initialize();
  m_initError = status.message;
  return status ? 0 : 1;
}

int TapeApiHandler::Discovery(XrdHttpExtReq &req)
{
  if(req.verb != "GET") return SendError(req, 405, "expected GET");

  const auto hostHeader =
    XrdOucTUtils::caseInsensitiveFind(req.headers, "host");
  const std::string host =
    hostHeader == req.headers.end() ? "" : hostHeader->second;
  if(host.empty()) return SendError(req, 400, "missing Host header");

  Json body;
  body["sitename"] = m_siteName;
  body["endpoints"] = Json::array({
    {{"uri", "https://" + host + "/api/v1"}, {"version", "v1"}}
  });
  return SendJson(req, 200, body.dump());
}

int TapeApiHandler::Stage(XrdHttpExtReq &req, const std::string &body)
{
  if(req.verb != "POST") return SendError(req, 405, "expected POST");
  Json json;
  std::string error;
  if(!ParseJsonBody(body, json, error)) return SendError(req, 400, error);
  if(!json.is_object() || !json.contains("files")
     || !json["files"].is_array() || json["files"].empty())
  {
    return SendError(req, 400,
      "stage request must contain a non-empty files array");
  }
  for(const auto &file : json["files"])
  {
    if(!file.is_object() || !file.contains("path")
       || !file["path"].is_string()
       || file["path"].get<std::string>().empty())
    {
      return SendError(req, 400,
        "stage files must contain a non-empty path");
    }
  }

  std::string requestId;
  const auto status = m_store.CreateStage(json["files"], requestId);
  if(!status) return SendError(req, status.code, status.message);
  const std::string response = Json({{"requestId", requestId}}).dump();
  return SendJson(req, 201, response,
                  "Location: /api/v1/stage/" + requestId);
}

int TapeApiHandler::StageStatus(XrdHttpExtReq &req,
                                const std::string &requestId)
{
  if(req.verb != "GET") return SendError(req, 405, "expected GET");
  Json response;
  const auto status = m_store.GetStage(requestId, response);
  return SendStatus(req, status, response.dump());
}

int TapeApiHandler::StageCancel(XrdHttpExtReq &req,
                                const std::string &requestId,
                                const std::string &body)
{
  if(req.verb != "POST") return SendError(req, 405, "expected POST");
  Json paths;
  std::string error;
  if(!ParsePaths(body, paths, error)) return SendError(req, 400, error);
  return SendStatus(req, m_store.CancelStage(requestId, paths));
}

int TapeApiHandler::StageDelete(XrdHttpExtReq &req,
                                const std::string &requestId)
{
  if(req.verb != "DELETE") return SendError(req, 405, "expected DELETE");
  return SendStatus(req, m_store.DeleteStage(requestId));
}

int TapeApiHandler::Release(XrdHttpExtReq &req,
                            const std::string &requestId,
                            const std::string &body)
{
  if(req.verb != "POST") return SendError(req, 405, "expected POST");
  Json paths;
  std::string error;
  if(!ParsePaths(body, paths, error)) return SendError(req, 400, error);
  return SendStatus(req, m_store.Release(requestId, paths));
}

int TapeApiHandler::ArchiveInfo(XrdHttpExtReq &req,
                                const std::string &body)
{
  if(req.verb != "POST") return SendError(req, 405, "expected POST");
  Json paths;
  std::string error;
  if(!ParsePaths(body, paths, error)) return SendError(req, 400, error);
  Json response;
  const auto status = m_store.ArchiveInfo(paths, response);
  return SendStatus(req, status, response.dump());
}
}

XrdVERSIONINFO(XrdHttpGetExtHandler, TapeApi);

extern "C"
{
XrdHttpExtHandler *XrdHttpGetExtHandler(
  XrdSysError *eDest, const char *confg, const char *parms,
  XrdOucEnv * /*myEnv*/)
{
  if(!parms || !*parms)
  {
    eDest->Emsg("TapeApiInitialize",
                "Tape API handler requires a local state directory");
    return nullptr;
  }

  std::string parameters(parms);
  XrdOucTokenizer options(parameters.data());
  options.GetLine();
  const char *root = options.GetToken();
  const char *maxRequestSizeOption = options.GetToken();
  if(!root || !*root)
  {
    eDest->Emsg("TapeApiInitialize",
                "Tape API handler requires a local state directory");
    return nullptr;
  }

  long long maxRequestSize = kDefaultMaxRequestSize;
  if(maxRequestSizeOption
     && XrdOuca2x::a2sz(*eDest, "Tape API maximum request size",
                        maxRequestSizeOption, &maxRequestSize, 1, INT_MAX))
  {
    return nullptr;
  }
  if(options.GetToken())
  {
    eDest->Emsg("TapeApiInitialize",
                "Tape API handler received unexpected parameters");
    return nullptr;
  }

  const char *siteName = std::getenv("XRDSITE");
  if(!siteName || !*siteName)
  {
    eDest->Emsg("TapeApiInitialize",
                "all.sitename must be configured for the Tape API handler");
    return nullptr;
  }

  auto *handler = new TapeApiHandler(root, maxRequestSize, siteName);
  if(handler->Init(confg) != 0)
  {
    eDest->Emsg("TapeApiInitialize", handler->InitializationError().c_str());
    delete handler;
    return nullptr;
  }
  return handler;
}
}
