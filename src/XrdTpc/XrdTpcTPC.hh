
#include <memory>
#include <string>
#include <vector>

#include "XrdSys/XrdSysPthread.hh"

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

    virtual bool MatchesPath(const char *verb, const char *path);
    virtual int ProcessReq(XrdHttpExtReq &req);
    // Abstract method in the base class, but does not seem to be used
    virtual int Init(const char *cfgfile) {return 0;}

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
    int RunCurlWithStreamsImpl(XrdHttpExtReq &req, TPC::State &state,
                           const char *log_prefix, size_t streams,
                           std::vector<TPC::State*> streams_handles);
#else
    int RunCurlBasic(CURL *curl, XrdHttpExtReq &req, TPC::State &state,
                     const char *log_prefix);
#endif

    int ProcessPushReq(const std::string & resource, XrdHttpExtReq &req);
    int ProcessPullReq(const std::string &resource, XrdHttpExtReq &req);

    bool ConfigureFSLib(XrdOucStream &Config, std::string &path1, bool &path1_alt,
                        std::string &path2, bool &path2_alt);
    bool Configure(const char *configfn, XrdOucEnv *myEnv);

    static int m_marker_period;
    static size_t m_block_size;
    bool m_desthttps;
    std::string m_cadir;
    static XrdSysMutex m_monid_mutex;
    static uint64_t m_monid;
    XrdSysError &m_log;
    std::unique_ptr<XrdSfsFileSystem> m_sfs;
    void *m_handle_base;
    void *m_handle_chained;

    // 16 blocks in flight at 16 MB each, meaning that there will be up to 256MB
    // in flight; this is equal to the bandwidth delay product of a 200ms transcontinental
    // connection at 10Gbps.
#ifdef USE_PIPELINING
    static const int m_pipelining_multiplier = 16;
#else
    static const int m_pipelining_multiplier = 1;
#endif
};
}
