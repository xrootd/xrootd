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
#include "XrdOuc/XrdOucJson.hh"
#include "XrdOuc/XrdOucTUtils.hh"
#include "XrdVersion.hh"

#include <climits>
#include <exception>
#include <string>

namespace
{
using Json = nlohmann::json;

constexpr char kStagePrefix[] = "/api/v1/stage/";
constexpr char kReleasePrefix[] = "/api/v1/release/";

class TapeApiHandler final : public XrdHttpExtHandler
{
  public:
    bool MatchesPath(const char *verb, const char *path) override;
    int ProcessReq(XrdHttpExtReq &req) override;
    int Init(const char *cfgfile) override;

  private:
    static int SendJson(XrdHttpExtReq &req, int code,
                        const std::string &body);
    static int SendError(XrdHttpExtReq &req, int code,
                         const std::string &message);
    static bool ReadBody(XrdHttpExtReq &req, std::string &body,
                         int &errorCode, std::string &error);
    static bool ParsePathArray(const std::string &body,
                               const char *field,
                               std::string &error);
    static int Discovery(XrdHttpExtReq &req);
    static int Stage(XrdHttpExtReq &req, const std::string &body);
    static int StageStatus(XrdHttpExtReq &req);
    static int StageCancel(XrdHttpExtReq &req, const std::string &body);
    static int StageDelete(XrdHttpExtReq &req);
    static int Release(XrdHttpExtReq &req, const std::string &body);
    static int ArchiveInfo(XrdHttpExtReq &req, const std::string &body);
};

int TapeApiHandler::SendJson(XrdHttpExtReq &req, int code,
                             const std::string &body)
{
  // A null description makes XrdHttp fill in the standard reason phrase.
  return req.SendSimpleResp(code, nullptr, "Content-Type: application/json",
                            body.c_str(), body.size());
}

int TapeApiHandler::SendError(XrdHttpExtReq &req, int code,
                              const std::string &message)
{
  const std::string body = Json({{"status", code}, {"title", message}}).dump();
  return req.SendSimpleResp(code, nullptr,
    "Content-Type: application/problem+json", body.c_str(), body.size());
}

bool TapeApiHandler::ReadBody(XrdHttpExtReq &req, std::string &body,
                              int &errorCode, std::string &error)
{
  body.clear();
  if(req.length <= 0) return true;
  if(req.length > INT_MAX)
  {
    errorCode = 413;
    error = "request too large";
    return false;
  }

  char *data = nullptr;
  const int length = static_cast<int>(req.length);
  if(req.BuffgetData(length, &data, true) != length || data == nullptr)
  {
    errorCode = 400;
    error = "missing or invalid request body";
    return false;
  }
  body.assign(data, static_cast<std::size_t>(length));
  return true;
}

bool TapeApiHandler::ParsePathArray(const std::string &body,
                                    const char *field,
                                    std::string &error)
{
  try
  {
    const Json json = Json::parse(body);
    if(!json.is_object() || !json.contains(field) || !json[field].is_array()
       || json[field].empty())
    {
      error = std::string("request must contain a non-empty ") + field
              + " array";
      return false;
    }
    for(const auto &item : json[field])
    {
      if(!item.is_string() || item.get<std::string>().empty())
      {
        error = std::string(field) + " entries must be non-empty strings";
        return false;
      }
    }
    return true;
  }
  catch(const std::exception &ex)
  {
    error = "malformed JSON request: " + std::string(ex.what());
    return false;
  }
}

bool TapeApiHandler::MatchesPath(const char * /*verb*/, const char *path)
{
  if(!path) return false;

  const std::string resource(path);
  return resource == "/.well-known/wlcg-tape-rest-api"
         || resource == "/api/v1/stage"
         || resource.compare(0, sizeof(kStagePrefix) - 1, kStagePrefix) == 0
         || resource.compare(0, sizeof(kReleasePrefix) - 1,
                             kReleasePrefix) == 0
         || resource == "/api/v1/archiveinfo";
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

  if(resource == "/.well-known/wlcg-tape-rest-api") return Discovery(req);
  if(resource == "/api/v1/stage") return Stage(req, body);
  if(resource == "/api/v1/stage/request-1")
  {
    return req.verb == "DELETE" ? StageDelete(req) : StageStatus(req);
  }
  if(resource == "/api/v1/stage/request-1/cancel")
  {
    return StageCancel(req, body);
  }
  if(resource == "/api/v1/release/request-1") return Release(req, body);
  if(resource == "/api/v1/archiveinfo") return ArchiveInfo(req, body);

  if(resource.compare(0, sizeof(kStagePrefix) - 1, kStagePrefix) == 0
     && resource.find("/cancel") == std::string::npos)
  {
    return req.verb == "DELETE" ? StageDelete(req)
                                 : SendError(req, 404, "unknown stage request");
  }

  return SendError(req, 404, "unexpected Tape REST API path");
}

int TapeApiHandler::Init(const char * /*cfgfile*/)
{
  return 0;
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
  body["sitename"] = "xrootd-ci";
  body["endpoints"] = Json::array({
    {{"uri", "https://" + host + "/api/v1"}, {"version", "v1"}}
  });
  return SendJson(req, 200, body.dump());
}

int TapeApiHandler::Stage(XrdHttpExtReq &req, const std::string &body)
{
  if(req.verb != "POST") return SendError(req, 405, "expected POST");
  try
  {
    const Json json = Json::parse(body);
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
  }
  catch(const std::exception &ex)
  {
    return SendError(req, 400,
      "malformed JSON request: " + std::string(ex.what()));
  }
  return SendJson(req, 201, R"({"requestId":"request-1"})");
}

int TapeApiHandler::StageStatus(XrdHttpExtReq &req)
{
  if(req.verb != "GET") return SendError(req, 405, "expected GET");

  return SendJson(req, 200,
    R"({"id":"request-1","createdAt":1,"startedAt":2,)"
    R"("completedAt":3,"files":[{"path":"/store/file",)"
    R"("state":"COMPLETED","startedAt":2,"finishedAt":3}]})");
}

int TapeApiHandler::StageCancel(XrdHttpExtReq &req,
                                const std::string &body)
{
  if(req.verb != "POST") return SendError(req, 405, "expected POST");
  std::string error;
  if(!ParsePathArray(body, "paths", error))
  {
    return SendError(req, 400, error);
  }
  return SendJson(req, 200, "{}");
}

int TapeApiHandler::StageDelete(XrdHttpExtReq &req)
{
  if(req.verb != "DELETE") return SendError(req, 405, "expected DELETE");
  return SendJson(req, 200, "{}");
}

int TapeApiHandler::Release(XrdHttpExtReq &req, const std::string &body)
{
  if(req.verb != "POST") return SendError(req, 405, "expected POST");
  std::string error;
  if(!ParsePathArray(body, "paths", error))
  {
    return SendError(req, 400, error);
  }
  return SendJson(req, 200, "{}");
}

int TapeApiHandler::ArchiveInfo(XrdHttpExtReq &req,
                                const std::string &body)
{
  if(req.verb != "POST") return SendError(req, 405, "expected POST");
  std::string error;
  if(!ParsePathArray(body, "paths", error))
  {
    return SendError(req, 400, error);
  }
  return SendJson(req, 200,
    R"([{"path":"/store/file","locality":"DISK_AND_TAPE"}])");
}
}

XrdVERSIONINFO(XrdHttpGetExtHandler, TapeApi);

extern "C"
{
XrdHttpExtHandler *XrdHttpGetExtHandler(
  XrdSysError * /*eDest*/, const char * /*confg*/, const char * /*parms*/,
  XrdOucEnv * /*myEnv*/)
{
  return new TapeApiHandler();
}
}
