
#include <memory>
#include <string>
#include <vector>
#include <sys/time.h>

#include "XrdSys/XrdSysPthread.hh"

#include "XrdHttp/XrdHttpExtHandler.hh"
#include "XrdHttp/XrdHttpUtils.hh"

#include "XrdTls/XrdTlsTempCA.hh"
#include "PMarkManager.hh"

#include <curl/curl.h>

class XrdOucErrInfo;
class XrdOucStream;
class XrdSfsFile;
class XrdSfsFileSystem;
class XrdXrootdTpcMon;
typedef void CURL;

namespace TPC {
class State;

enum LogMask {
    Debug   = 0x01,
    Info    = 0x02,
    Warning = 0x04,
    Error   = 0x08,
    All     = 0xff
};


struct CurlDeleter {
    void operator()(CURL *curl);
};
using ManagedCurlHandle = std::unique_ptr<CURL, CurlDeleter>;


class TPCHandler : public XrdHttpExtHandler {
public:
    TPCHandler(XrdSysError *log, const char *config, XrdOucEnv *myEnv);
    virtual ~TPCHandler();

    virtual bool MatchesPath(const char *verb, const char *path);
    virtual int ProcessReq(XrdHttpExtReq &req);
    // Abstract method in the base class, but does not seem to be used
    virtual int Init(const char *cfgfile) {return 0;}

private:

    static int sockopt_setcloexec_callback(void * clientp, curl_socket_t curlfd, curlsocktype purpose);
    static int opensocket_callback(void *clientp,
                                   curlsocktype purpose,
                                   struct curl_sockaddr *address);

    static int closesocket_callback(void *clientp, curl_socket_t fd);

    struct TPCLogRecord {

        TPCLogRecord(XrdNetPMark * pmark) : bytes_transferred( -1 ), status( -1 ),
                         tpc_status(-1), streams( 1 ), isIPv6(false), pmarkManager(pmark)
        {
         gettimeofday(&begT, 0); // Set effective start time
        }
       ~TPCLogRecord();

        std::string log_prefix;
        std::string local;
        std::string remote;
        std::string name;
        std::string clID;
        static XrdXrootdTpcMon* tpcMonitor;
        timeval     begT;
        off_t bytes_transferred;
        int status;
        int tpc_status;
        unsigned int streams;
        bool isIPv6;
        PMarkManager pmarkManager;
    };

    int ProcessOptionsReq(XrdHttpExtReq &req);

    static std::string GetAuthz(XrdHttpExtReq &req);

    // Configure curl handle's CA settings.  The CA files present here should
    // be valid for the lifetime of the process.
    void ConfigureCurlCA(CURL *curl);

    // Redirect the transfer according to the contents of an XrdOucErrInfo object.
    int RedirectTransfer(CURL *curl, const std::string &redirect_resource, XrdHttpExtReq &req,
        XrdOucErrInfo &error, TPCLogRecord &);

    int OpenWaitStall(XrdSfsFile &fh, const std::string &resource, int mode,
                      int openMode, const XrdSecEntity &sec,
                      const std::string &authz);

#ifdef XRD_CHUNK_RESP
    int DetermineXferSize(CURL *curl, XrdHttpExtReq &req, TPC::State &state,
                          bool &success, TPCLogRecord &, bool shouldReturnErrorToClient = true);

    int GetContentLengthTPCPull(CURL *curl, XrdHttpExtReq &req, uint64_t & contentLength, bool & success, TPCLogRecord &rec);

    // Send a 'performance marker' back to the TPC client, informing it of our
    // progress.  The TPC client will use this information to determine whether
    // the transfer is making sufficient progress and/or other monitoring info
    // (such as whether the transfer is happening over IPv4, IPv6, or both).
    int SendPerfMarker(XrdHttpExtReq &req, TPCLogRecord &rec, TPC::State &state);
    int SendPerfMarker(XrdHttpExtReq &req, TPCLogRecord &rec, std::vector<State*> &state,
        off_t bytes_transferred);

    // Perform the libcurl transfer, periodically sending back chunked updates.
    int RunCurlWithUpdates(CURL *curl, XrdHttpExtReq &req, TPC::State &state,
                           TPCLogRecord &rec);

    // Experimental multi-stream version of RunCurlWithUpdates
    int RunCurlWithStreams(XrdHttpExtReq &req, TPC::State &state,
                           size_t streams, TPCLogRecord &rec);
    int RunCurlWithStreamsImpl(XrdHttpExtReq &req, TPC::State &state,
                           size_t streams, std::vector<TPC::State*> &streams_handles,
                           std::vector<ManagedCurlHandle> &curl_handles,
                           TPCLogRecord &rec);
#else
    int RunCurlBasic(CURL *curl, XrdHttpExtReq &req, TPC::State &state,
                     TPCLogRecord &rec);
#endif

    int ProcessPushReq(const std::string & resource, XrdHttpExtReq &req);
    int ProcessPullReq(const std::string &resource, XrdHttpExtReq &req);

    bool ConfigureFSLib(XrdOucStream &Config, std::string &path1, bool &path1_alt,
                        std::string &path2, bool &path2_alt);
    bool Configure(const char *configfn, XrdOucEnv *myEnv);
    bool ConfigureLogger(XrdOucStream &Config);

    // Generate a consistently-formatted log message.
    void logTransferEvent(LogMask lvl, const TPCLogRecord &record,
        const std::string &event, const std::string &message="");

    static int m_marker_period;
    static size_t m_block_size;
    static size_t m_small_block_size;
    bool m_desthttps;
    int m_timeout; // the 'timeout interval'; if no bytes have been received during this time period, abort the transfer.
    int m_first_timeout; // the 'first timeout interval'; the amount of time we're willing to wait to get the first byte.
                         // Unless explicitly specified, this is 2x the timeout interval.
    std::string m_cadir;  // The directory to use for CAs.
    std::string m_cafile; // The file to use for CAs in libcurl
    static XrdSysMutex m_monid_mutex;
    static uint64_t m_monid;
    XrdSysError m_log;
    XrdSfsFileSystem *m_sfs;
    std::shared_ptr<XrdTlsTempCA> m_ca_file;

    // 16 blocks in flight at 16 MB each, meaning that there will be up to 256MB
    // in flight; this is equal to the bandwidth delay product of a 200ms transcontinental
    // connection at 10Gbps.
#ifdef USE_PIPELINING
    static const int m_pipelining_multiplier = 16;
#else
    static const int m_pipelining_multiplier = 1;
#endif

    bool usingEC; // indicate if XrdEC is used
};
}
