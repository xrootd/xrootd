/******************************************************************************/
/*                                                                            */
/*                        X r d P r o m e t h e u s . h h                     */
/*                                                                            */
/******************************************************************************/

#include "XrdDictMgr.hh"

#include <string>

#include "XrdSys/XrdSysError.hh"

#include <prometheus/family.h>
#include <prometheus/registry.h>

#include "XrdHttp/XrdHttpExtHandler.hh"


// Forward dec'ls for classes we need.
class XrdAccAuthorize;
class XrdOucEnv;
class XrdOucGatherConf;
class XrdSysError;
class XrdStats;


namespace XrdPrometheus {

enum LogMask {
    Debug = 0x01,
    Info = 0x02,
    Warning = 0x04,
    Error = 0x08,
    All = 0xff
};

//! HTTP plugin module to produce monitoring information in the OpenMonitoring
//! format used by Prometheus.
class Handler final : public XrdHttpExtHandler {
public:
    Handler(XrdSysError *log, const char *config_filename, XrdStats *stats, XrdAccAuthorize *chain);

    virtual ~Handler() {}

    // Return true if the request's method / path matches something we can provide
    virtual bool MatchesPath(const char *verb, const char *path) override;

    // Handle a HTTP request to completion
    virtual int ProcessReq(XrdHttpExtReq &req) override;

    virtual int Init(const char *cfgfile) override {return 0;}

    void Send(void *buff, size_t blen) const;

private:
    bool Config(const char *config_filename);
    bool ConfigureTrace(XrdOucGatherConf &);
    bool ConfigurePaths(XrdOucGatherConf &);

    /**
     * Given a path name, compute the most specific monitoring prefix.
     */
    std::string ComputePrefix(const std::string &path) const;
    using PathList = std::vector<std::string>;
    std::vector<PathList> m_monitor_paths;

    mutable XrdSysError m_log;
    XrdStats &m_stats;
    XrdAccAuthorize *m_chain{nullptr};

    UserRecordManager m_sessions;
    FileRecordManager m_transfers;

    std::mutex m_stats_mutex;
    mutable std::mutex m_transfers_mutex;
    mutable std::mutex m_sessions_mutex;

    prometheus::Registry m_registry;

    // These are all our various counters we keep track of
    prometheus::Family<prometheus::Counter> &m_bytes_family;
    prometheus::Counter &m_bytes_in_ctr;
    prometheus::Counter &m_bytes_out_ctr;

    prometheus::Family<prometheus::Counter> &m_connections_family;
    prometheus::Counter &m_connections_ctr;

    prometheus::Family<prometheus::Gauge> &m_threads_family;
    prometheus::Gauge &m_threads_idle;
    prometheus::Gauge &m_threads_running;

    prometheus::Family<prometheus::Counter> &m_transfer_ops_family;
    prometheus::Family<prometheus::Counter> &m_transfer_readv_segments_family;
    prometheus::Family<prometheus::Counter> &m_transfer_bytes_family;

    prometheus::Family<prometheus::Gauge> &m_metadata;
};

}
