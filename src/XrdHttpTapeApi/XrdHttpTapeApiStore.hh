#ifndef __XRDHTTPTAPEAPISTORE_HH__
#define __XRDHTTPTAPEAPISTORE_HH__
/******************************************************************************/
/*                                                                            */
/*              X r d H t t p T a p e A p i S t o r e . h h                 */
/*                                                                            */
/* (c) 2026 by the XRootD Collaboration                                       */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/******************************************************************************/

#include "XrdOuc/XrdOucJson.hh"

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

class XrdHttpTapeApiStore
{
  public:
    struct Status
    {
      int code = 200;
      std::string message;

      explicit operator bool() const { return message.empty(); }
    };

    explicit XrdHttpTapeApiStore(std::filesystem::path root);

    Status Initialize();
    Status CreateStage(const nlohmann::json &files, std::string &requestId);
    Status GetStage(const std::string &requestId, nlohmann::json &response);
    Status CancelStage(const std::string &requestId,
                       const nlohmann::json &paths);
    Status DeleteStage(const std::string &requestId);
    Status Release(const std::string &requestId, const nlohmann::json &paths);
    Status ArchiveInfo(const nlohmann::json &paths, nlohmann::json &response);

  private:
    Status ResolvePath(const std::filesystem::path &root,
                       const std::string &path,
                       std::string &normalized,
                       std::filesystem::path &resolved) const;
    Status ReadRequest(const std::string &requestId,
                       nlohmann::json &request) const;
    Status WriteRequest(const nlohmann::json &request) const;
    Status ValidateRequestPaths(const nlohmann::json &request,
                                const nlohmann::json &paths,
                                std::vector<std::string> &normalized) const;

    static bool IsRequestId(const std::string &requestId);
    static std::uint64_t Now();
    static std::string GenerateRequestId();
    static nlohmann::json StatusResponse(const nlohmann::json &request);

    std::filesystem::path m_root;
    std::filesystem::path m_archiveRoot;
    std::filesystem::path m_diskRoot;
    std::filesystem::path m_requestsRoot;
    mutable std::mutex m_mutex;
};

#endif // __XRDHTTPTAPEAPISTORE_HH__
