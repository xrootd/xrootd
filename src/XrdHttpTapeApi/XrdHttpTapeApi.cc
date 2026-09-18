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
#include "XrdOfs/XrdOfsPrepProtocol.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include <arpa/inet.h>
#include <set>
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
constexpr unsigned kEvictOperation = 0x10000 | kXR_evict;
constexpr long long kDefaultMaxRequestSize = 4 * 1024 * 1024;

class TapeApiHandler final : public XrdHttpExtHandlerBridge
{
  public:
    TapeApiHandler(long long maxRequestSize, const std::string &siteName)
      : m_maxRequestSize(maxRequestSize),
        m_siteName(siteName) {}

    bool MatchesPath(const char *verb, const char *path) override;
    int ProcessReq(XrdHttpExtReq &req) override;
    int Init(const char * /*cfgfile*/) override { return 0; }
    bool RequiresBridge(const char *, const char *path) const override {
      return path && std::string(path) != kDiscoveryPath;
    }
    const std::string &InitializationError() const { return m_initError; }

  private:
    static int SendJson(XrdHttpExtReq &req, int code,
                        const std::string &body,
                        const std::string &additionalHeaders = {});
    static int SendError(XrdHttpExtReq &req, int code,
                         const std::string &message);
    static int Native(XrdHttpExtReq &req, unsigned operation, const std::string &id,
                      const Json &files, bool stage = false, bool archive = false);
    static std::string Payload(XrdHttpExtReq &req, const Json &files,
                               const std::string &requestId = {});
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

std::string TapeApiHandler::Payload(XrdHttpExtReq &req, const Json &files,
                                     const std::string &requestId)
{
  using namespace XrdOfsPrepProtocol;
  if (files.size() > MaxFiles) Fail(E2BIG, "prepare batch exceeds 48 files");
  std::string authorization;
  const auto full = req.headers.find("xrd-http-fullresource");
  if (full != req.headers.end()) {
    const auto marker = full->second.find('?');
    if (marker != std::string::npos) {
      XrdOucEnv query(full->second.c_str() + marker + 1);
      const char *authz = query.Get("authz");
      if (authz && *authz) authorization = std::string("authz=") + authz;
    }
  }
  std::set<std::string> seen;
  std::string payload;
  for (const auto &file : files) {
    const auto path = Path(file.is_string() ? file.get<std::string>()
                                          : file.at("path").get<std::string>());
    if (!seen.insert(path).second) Fail(EINVAL, "duplicate prepare path");
    std::string cgi;
    if (!file.is_string()) cgi = "xrd.prepare.file=" + Hex(Metadata(file).dump());
    if (!requestId.empty()) {
      if (!cgi.empty()) cgi += '&';
      cgi += "xrd.prepare.request=" + requestId;
    }
    if (!authorization.empty()) {
      if (!cgi.empty()) cgi += '&';
      cgi += authorization;
    }
    payload += path;
    if (!cgi.empty()) payload += "?" + cgi;
    payload += '\n';
  }
  return payload;
}

int TapeApiHandler::Native(XrdHttpExtReq &req, unsigned operation,
                           const std::string &id, const Json &files,
                           bool stage, bool archive)
{
  using namespace XrdOfsPrepProtocol;
  ClientRequest native{};
  std::string payload;
  if (operation == kXR_query) {
    native.header.requestid = htons(kXR_query);
    native.query.infotype = htons(kXR_QPrep);
    payload = id + "\n" + Payload(req, files);
  } else {
    native.header.requestid = htons(kXR_prepare);
    if (stage) native.prepare.options = kXR_stage;
    else if (operation == kXR_cancel) {
      native.prepare.options = kXR_cancel;
      payload = id + "\n";
    } else native.prepare.optionX = htons(kXR_evict);
    payload += Payload(req, files, operation == kEvictOperation && !stage ? id : "");
  }
  native.header.dlen = htonl(payload.size());
  const int rc = req.RunNative(native, payload,
    [stage, archive, operation](XrdHttpExtReq &request, const XrdHttpExtReq::NativeResponse &result) {
      if (result.kind == XrdHttpExtReq::NativeResponse::Redirect)
        return SendError(request, 503, "prepare redirect requires a configured Tape REST owning endpoint");
      if (result.kind == XrdHttpExtReq::NativeResponse::Error) {
        int status = 500;
        switch (result.code) {
          case kXR_NotAuthorized: status = 403; break;
          case kXR_NotFound: status = 404; break;
          case kXR_ArgInvalid: case kXR_ArgMissing: status = 400; break;
          case kXR_ArgTooLong: status = 413; break;
          case kXR_Unsupported: status = 501; break;
          case kXR_overQuota: status = 429; break;
          case kXR_inProgress: case kXR_noserver: case kXR_Cancelled: status = 503; break;
          default: break;
        }
        return SendError(request, status, result.data);
      }
      std::string body = result.data;
      while (!body.empty() && body.back() == '\0') body.pop_back();
      if (stage) {
        if (!IsId(body)) return SendError(request, 502, "backend returned an invalid prepare request ID");
        return SendJson(request, 201, Json({{"requestId", body}}).dump(),
                        "Location: /api/v1/stage/" + body);
      }
      if (operation == kXR_query) {
        try {
          auto json = Json::parse(body);
          if ((archive && !json.is_array()) || (!archive && !json.is_object()))
            return SendError(request, 502, "invalid structured prepare response");
          return SendJson(request, 200, json.dump());
        } catch (const Json::exception &) {
          return SendError(request, 502, "invalid structured prepare response");
        }
      }
      return SendJson(request, 200, "");
    });
  return rc == XrdHttpExtReq::Pending ? rc : SendError(req, 503, "native prepare bridge unavailable");
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
  try {
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
  } catch (const std::system_error &ex) {
    return SendError(req, ex.code().value() == E2BIG ? 413 : 400, ex.what());
  } catch (const Json::exception &) {
    return SendError(req, 400, "invalid prepare request JSON");
  } catch (const std::exception &) {
    return SendError(req, 500, "could not process prepare request");
  }
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
    {{"uri", req.headers.at("xrd-http-prot") + "://" + host + "/api/v1"}, {"version", "v1"}}
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

  return Native(req, kXR_stage, "", json["files"], true);
}

int TapeApiHandler::StageStatus(XrdHttpExtReq &req,
                                const std::string &requestId)
{
  if(req.verb != "GET") return SendError(req, 405, "expected GET");
  if (!XrdOfsPrepProtocol::IsId(requestId)) return SendError(req, 404, "unknown prepare request");
  return Native(req, kXR_query, requestId, Json::array());
}

int TapeApiHandler::StageCancel(XrdHttpExtReq &req,
                                const std::string &requestId,
                                const std::string &body)
{
  if(req.verb != "POST") return SendError(req, 405, "expected POST");
  Json paths;
  std::string error;
  if(!ParsePaths(body, paths, error)) return SendError(req, 400, error);
  if (!XrdOfsPrepProtocol::IsId(requestId)) return SendError(req, 404, "unknown prepare request");
  return Native(req, kXR_cancel, requestId, paths);
}

int TapeApiHandler::StageDelete(XrdHttpExtReq &req,
                                const std::string &requestId)
{
  if(req.verb != "DELETE") return SendError(req, 405, "expected DELETE");
  if (!XrdOfsPrepProtocol::IsId(requestId)) return SendError(req, 404, "unknown prepare request");
  return Native(req, kXR_cancel, std::string(XrdOfsPrepProtocol::DeletePrefix) + requestId,
                Json::array());
}

int TapeApiHandler::Release(XrdHttpExtReq &req,
                            const std::string &requestId,
                            const std::string &body)
{
  if(req.verb != "POST") return SendError(req, 405, "expected POST");
  Json paths;
  std::string error;
  if(!ParsePaths(body, paths, error)) return SendError(req, 400, error);
  if (!XrdOfsPrepProtocol::IsId(requestId)) return SendError(req, 404, "unknown prepare request");
  return Native(req, kEvictOperation, requestId, paths);
}

int TapeApiHandler::ArchiveInfo(XrdHttpExtReq &req,
                                const std::string &body)
{
  if(req.verb != "POST") return SendError(req, 405, "expected POST");
  Json paths;
  std::string error;
  if(!ParsePaths(body, paths, error)) return SendError(req, 400, error);
  return Native(req, kXR_query, XrdOfsPrepProtocol::ArchiveQuery, paths, false, true);
}
}

XrdVERSIONINFO(XrdHttpGetExtHandler, TapeApi);

extern "C"
{
XrdHttpExtHandler *XrdHttpGetExtHandler(
  XrdSysError *eDest, const char *confg, const char *parms,
  XrdOucEnv *myEnv)
{
  const char *profile = myEnv ? myEnv->Get("xrd.prepare.profile") : nullptr;
  if (!profile || std::string(profile) != "v1") {
    eDest->Emsg("TapeApiInitialize", "a prepare backend implementing durable profile v1 is required");
    return nullptr;
  }
  long long maxRequestSize = kDefaultMaxRequestSize;
  if (parms && *parms && XrdOuca2x::a2sz(*eDest, "Tape API maximum request size",
                                         parms, &maxRequestSize, 1, 4 * 1024 * 1024))
    return nullptr;

  const char *siteName = std::getenv("XRDSITE");
  if(!siteName || !*siteName)
  {
    eDest->Emsg("TapeApiInitialize",
                "all.sitename must be configured for the Tape API handler");
    return nullptr;
  }

  auto *handler = new TapeApiHandler(maxRequestSize, siteName);
  if(handler->Init(confg) != 0)
  {
    eDest->Emsg("TapeApiInitialize", handler->InitializationError().c_str());
    delete handler;
    return nullptr;
  }
  return handler;
}
}
