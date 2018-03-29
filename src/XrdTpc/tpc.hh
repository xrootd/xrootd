
#include <string>
#include <memory>
#include <atomic>

#include "XrdHttp/XrdHttpExtHandler.hh"

class XrdOucErrInfo;
class XrdOucStream;
class XrdSfsFile;
class XrdSfsFileSystem;
typedef void CURL;

namespace TPC {
class State;

class TPCHandler : public XrdHttpExtHandler {
public:
    TPCHandler(XrdSysError *log, const char *config, XrdOucEnv *myEnv);
    virtual ~TPCHandler();

    virtual bool MatchesPath(const char *verb, const char *path) override;
    virtual int ProcessReq(XrdHttpExtReq &req) override;
    // Abstract method in the base class, but does not seem to be used
    virtual int Init(const char *cfgfile) override {return 0;}

private:
    int ProcessOptionsReq(XrdHttpExtReq &req);

    static std::string GetAuthz(XrdHttpExtReq &req);

    int RedirectTransfer(XrdHttpExtReq &req, XrdOucErrInfo &error);

    int OpenWaitStall(XrdSfsFile &fh, const std::string &resource, int mode,
                      int openMode, const XrdSecEntity &sec,
                      const std::string &authz);

#ifdef XRD_CHUNK_RESP
    int DetermineXferSize(CURL *curl, XrdHttpExtReq &req, TPC::State &state,
                          bool &success);

    int SendPerfMarker(XrdHttpExtReq &req, off_t bytes_transferred);

    // Perform the libcurl transfer, periodically sending back chunked updates.
    int RunCurlWithUpdates(CURL *curl, XrdHttpExtReq &req, TPC::State &state,
                           const char *log_prefix);

    // Experimental multi-stream version of RunCurlWithUpdates
    int RunCurlWithStreams(XrdHttpExtReq &req, TPC::State &state,
                           const char *log_prefix, size_t streams);
#else
    int RunCurlBasic(CURL *curl, XrdHttpExtReq &req, TPC::State &state,
                     const char *log_prefix);
#endif

    int ProcessPushReq(const std::string & resource, XrdHttpExtReq &req);
    int ProcessPullReq(const std::string &resource, XrdHttpExtReq &req);

    bool ConfigureFSLib(XrdOucStream &Config, std::string &path1, bool &path1_alt,
                        std::string &path2, bool &path2_alt);
    bool Configure(const char *configfn, XrdOucEnv *myEnv);

    static constexpr int m_marker_period = 5;
    static constexpr size_t m_block_size = 16*1024*1024;
    bool m_desthttps{false};
    std::string m_cadir;
    static std::atomic<uint64_t> m_monid;
    XrdSysError &m_log;
    std::unique_ptr<XrdSfsFileSystem> m_sfs;
    void *m_handle_base{nullptr};
    void *m_handle_chained{nullptr};
};
}
