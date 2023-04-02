/******************************************************************************/
/*                                                                            */
/*                        X r d P r o m e t h e u s . c c                     */
/*                                                                            */
/******************************************************************************/

#include "XrdPrometheus.hh"
#include "XrdDictMgr.hh"

#include <prometheus/counter.h>
#include <prometheus/text_serializer.h>

#include "XrdAcc/XrdAccAccess.hh"
#include "XrdHttp/XrdHttpExtHandler.hh"
#include "XrdHttp/XrdHttpUtils.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include <XrdOuc/XrdOucString.hh>
#include "Xrd/XrdStats.hh"
#include "XrdVersion.hh"
#include "XrdXml/XrdXmlReader.hh"
#include "XrdXrootd/XrdXrootdMonitor.hh"

#include <sstream>

XrdVERSIONINFO(XrdHttpGetExtHandler,  XrdPrometheus);


namespace {

class FrameworkStats : public XrdStats::CallBack
{
public:
    FrameworkStats() {}

    virtual ~FrameworkStats() {}

    void Info(const char *buff, int bsz) override
    {
        m_stats_buffer = std::string(buff, bsz);
    }

    std::string &getStatsBuffer() {return m_stats_buffer;}

private:
    std::string m_stats_buffer;
};


bool
ReadOutLL(XrdHttpExtReq &req, XrdXmlReader &reader, const char *elem, long long &result, int &send_result)
{
    const char *elem_search[3] = {"stats", elem, nullptr};
    if (!reader.GetElement(elem_search, false)) {
        std::stringstream ss;
        ss << "Summary statistics missing '" << elem << "' element";
        int ecode;
        auto text_err = reader.GetError(ecode);
        if (text_err) {
            ss << ". Error: " << text_err;
        }
        send_result = req.SendSimpleResp(500, nullptr, nullptr, ss.str().c_str(), 0);
        return false;
    } 
    std::unique_ptr<char, decltype(&free)> out_text(reader.GetText(elem), &free);
    if (!out_text) {
       std::stringstream ss;
       ss << "Summary statistics missing '" << elem << "' text";
       int ecode;
       auto text_err = reader.GetError(ecode);
       if (text_err) {
           ss << ". Error: " << text_err;
       }
       send_result = req.SendSimpleResp(500, nullptr, nullptr, ss.str().c_str(), 0);
       return false;
    }
    try {
        result = std::stoll(out_text.get());
    } catch (...) {
        std::string msg = std::string("Summary statistics '") + elem + "' text not an integer";
        send_result = req.SendSimpleResp(500, nullptr, nullptr, msg.c_str(), msg.size());
        return false;
    }
    return true;
}


void
CleanupAttrResults(const char **attr, char **results)
{
    for (auto idx = 0; attr[idx]; idx++) {
        if (results[idx]) {
            free(results[idx]);
            results[idx] = nullptr;
        }
    }
}


bool
AnyMissing(const char **attr, char **results)
{
    for (auto idx = 0; attr[idx]; idx++) {
        if (!results[idx]) {return true;}
        idx += 1;
    }
    return false;
}


void
IncrementTo(prometheus::Counter &ctr, double new_val)
{
    auto cur_val = ctr.Value();
    auto inc_val = new_val - cur_val;
    if (inc_val > 0) {ctr.Increment(inc_val);}
}


class Callback final : public XrdXrootdMonitor::Callback {
public:
    Callback(XrdPrometheus::Handler *parent) : m_parent(parent) {}

    Callback(Callback &&) = default;

    virtual void Send(void *buff, int blen) const override
    {
        m_parent->Send(buff, blen);
    }

private:
    const XrdPrometheus::Handler *m_parent;
};


bool
GetSIDRest(const std::string &info, UserRecord::sid_t &sid, std::string &rest)
{
    auto newline_pos = info.find('\n');
    if (newline_pos == std::string::npos) {
        return false;
    }
    rest = info.substr(newline_pos + 1);
    auto sid_pos = info.find_last_of(':', newline_pos);
    if (sid_pos == std::string::npos) {
        return false;
    }
    try {
        sid = std::stol(info.substr(sid_pos + 1, newline_pos - sid_pos - 1));
    } catch (...) {
        return false;
    }
    return true;
}

} //end namespace


using namespace XrdPrometheus;


Handler::Handler(XrdSysError *log, const char *config, XrdStats *stats, XrdAccAuthorize *chain) :
        m_log(*log),
        m_stats(*stats),
        m_chain(chain),
        m_bytes_family(prometheus::BuildCounter()
                       .Name("xrootd_server_bytes")
                       .Help("Number of bytes read into the server")
                       .Register(m_registry)),
        m_bytes_in_ctr(m_bytes_family.Add({{"direction", "rx"}})),
        m_bytes_out_ctr(m_bytes_family.Add({{"direction", "tx"}})),
        m_connections_family(prometheus::BuildCounter()
                             .Name("xrootd_server_connection_count")
                             .Help("Aggregate number of server connections")
                             .Register(m_registry)),
        m_connections_ctr(m_connections_family.Add({})),
        m_threads_family(prometheus::BuildGauge()
                       .Name("xrootd_sched_thread_count")
                       .Help("Number of scheduler threads")
                       .Register(m_registry)),
        m_threads_idle(m_threads_family.Add({{"state", "idle"}})),
        m_threads_running(m_threads_family.Add({{"state", "running"}})),
        m_transfer_ops_family(prometheus::BuildCounter()
                       .Name("xrootd_transfer_operations_count")
                       .Help("Number of transfer operations performed")
                       .Register(m_registry)),
        m_transfer_readv_segments_family(prometheus::BuildCounter()
                       .Name("xrootd_transfer_readv_segments_count")
                       .Help("Number of segments in readv operations")
                       .Register(m_registry)),
        m_transfer_bytes_family(prometheus::BuildCounter()
                       .Name("xrootd_transfer_bytes")
                       .Help("Bytes of transfers")
                       .Register(m_registry)),
        m_metadata(prometheus::BuildGauge()
                   .Name("xrootd_server_metadata")
                   .Help("XRootD server metadata")
                   .Register(m_registry))
{
    if (!stats) {
        throw std::runtime_error("Prometheus handler given null stats object");
    }
    if (!log) {
        throw std::runtime_error("Prometheus handler given null logging object");
    }
    m_log.SetPrefix("Prometheus");
    if (!Config(config))
    {
        throw std::runtime_error("Prometheus handler config failed.");
    }

    XrdXrootdMonitor::RegisterCallback(std::make_unique<Callback>(this));


    m_log.Log(LogMask::Info, "Initialization", "Successfully enabled Prometheus handler");
}


bool Handler::MatchesPath(const char *verb, const char *path)
{
    return !strcmp(verb, "GET") && (!strcmp(path, "/metrics") || !strcmp(path, "/metrics/"));
}


int Handler::ProcessReq(XrdHttpExtReq &req)
{
    if (m_chain) {
        auto iter = req.headers.find("Authorization");
        std::unique_ptr<XrdOucEnv> env;
        if (iter != req.headers.end()) {
            env.reset(new XrdOucEnv());
            std::unique_ptr<char, decltype(&free)> quoted_authz(quote(iter->second.c_str()), &free);
            env->Put("authz", quoted_authz.get());
        }
        if (!m_chain->Access(&req.GetSecEntity(), req.resource.c_str(), AOP_Read, env.get())) {
            return req.SendSimpleResp(403, nullptr, nullptr, "Insufficient privileges to access metrics", 0);
        }
    }

    FrameworkStats fstats;
    m_stats.Stats(&fstats, XRD_STATS_ALL);
    //auto buff = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<xml>\n" + fstats.getStatsBuffer() + "\n</xml>";
    auto buff = fstats.getStatsBuffer();

    auto reader = XrdXmlReader::GetReaderFromBuffer(buff.c_str());
    if (!reader)
    {
        return req.SendSimpleResp(500, nullptr, nullptr, "Failed to parse the summary statistics buffer", 0);
    }

    const char *names[3] = {"", "statistics", nullptr};
    auto count = reader->GetElement(names, false);

    if (count == 0) {
        return req.SendSimpleResp(500, nullptr, nullptr, "Summary statistics missing the 'statistics' tag", 0);
    }

    const char *attrs[5] = {"ver", "src", "site", "tos", nullptr};
    char *result[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    if (!reader->GetAttributes(attrs, result) || AnyMissing(attrs, result)) {
        return req.SendSimpleResp(500, nullptr, nullptr, "Summary statistics missing attributes", 0);
    }
    long long tos;
    try {
        tos = std::stoll(result[3]);
    } catch (...) {
        return req.SendSimpleResp(500, nullptr, nullptr, "Summary statistics 'tos' not an integer", 0);
    }
    auto &gauge = m_metadata.Add({{"instance", result[1]}, {"version", result[0]}, {"site", result[2]}});
    gauge.Set(static_cast<double>(tos));
    CleanupAttrResults(attrs, result);


    const char *stats[3] = {"statistics", "stats", nullptr};
    count = reader->GetElement(stats, false);
    while (count) {
        attrs[0] = "id";
        attrs[1] = nullptr;
        result[0] = nullptr;
        std::unique_ptr<char, decltype(&free)> id_text(nullptr, &free);
        if (!reader->GetAttributes(attrs, result)) {
            return req.SendSimpleResp(500, nullptr, nullptr, "Summary stat missing 'id' attribute", 0);
        }
        std::string stat_type(result[0]);
        free(result[0]);

        int send_result;
        if (stat_type == "link") {

            long long bytes_in, bytes_out, tot;
            if (!ReadOutLL(req, *reader, "tot", tot, send_result)) {return send_result;}
            if (!ReadOutLL(req, *reader, "in", bytes_in, send_result)) {return send_result;}
            if (!ReadOutLL(req, *reader, "out", bytes_out, send_result)) {return send_result;}
            reader->GetElement(names, false); // Reset to top level

            {
                std::lock_guard<std::mutex> lock(m_stats_mutex);
                IncrementTo(m_bytes_in_ctr, static_cast<double>(bytes_in));
                IncrementTo(m_bytes_out_ctr, static_cast<double>(bytes_out));
                IncrementTo(m_connections_ctr, static_cast<double>(tot));
            }
        } else if (stat_type == "sched")
        {
            long long idle, threads;
            if (!ReadOutLL(req, *reader, "threads", threads, send_result)) {return send_result;}
            if (!ReadOutLL(req, *reader, "idle", idle, send_result)) {return send_result;}
            reader->GetElement(names, false);

            m_threads_idle.Set(idle);
            m_threads_running.Set(threads - idle);
        }

        count = reader->GetElement(stats, false);
    }

    auto metrics = m_registry.Collect();
    const prometheus::TextSerializer serializer;
    std::string text_metrics = serializer.Serialize(metrics);

    return req.SendSimpleResp(200, nullptr, nullptr, text_metrics.c_str(), text_metrics.size());
}


std::string
Handler::ComputePrefix(const std::string &path) const
{
    if (m_monitor_paths.empty()) {return "/";}

    PathList segments;
    XrdOucString path_tmp(path.c_str()), segment;
    int from = 0;
    while ((from = path_tmp.tokenize(segment, from, '/')) != -1) {
        if (segment.length() == 0) {continue;}
        segments.emplace_back(segment.c_str());
    }

    size_t maxlen = 0;
    for (const auto &path : m_monitor_paths) {
        size_t curlen = 0;
        for (const auto &segment : path) {
            if (segments.size() <= curlen) {break;}
            if ((segments[curlen] != segment) &&
                (segment != "*"))
            {
                break;
            }
            curlen ++;
        }
        if (curlen > maxlen) {maxlen = curlen;}
    }
    if (maxlen == 0) {return "/";}

    std::string result;
    for (size_t idx = 0; idx < maxlen; idx++) {
        result += "/" + segments[idx];
    }
    return result;
}

void
Handler::Send(void *buff, size_t blen) const
{
    m_log.Log(LogMask::Debug, "MonPacket", "Received a new monitor packet");

    auto mHdr = static_cast<XrdXrootdMonHeader*>(buff);

    switch (mHdr->code) {
    case 'd': // User has opened a file
    {
        m_log.Log(LogMask::Debug, "MonPacket", "Received a file-open packet");
        auto mMap = static_cast<XrdXrootdMonMap*>(buff);
        FileRecord::id_t fileid = ntohl(mMap->dictid);
        UserRecord::sid_t userid;
        std::string rest;
        if (GetSIDRest(mMap->info, userid, rest)) {
            auto path = ComputePrefix(rest);
            {
                std::lock_guard<std::mutex> guard(m_transfers_mutex);
                const_cast<Handler*>(this)->m_transfers.insert({fileid, {userid, path}});
            }
        }
        break;
    }
    case 'f': // f-stream records.  Could be open / close / disconnect.
    {
        m_log.Log(LogMask::Debug, "MonPacket", "Received a f-stream packet");
        auto buffc = static_cast<char*>(buff);
        std::ptrdiff_t offset = sizeof(XrdXrootdMonHeader) + sizeof(XrdXrootdMonFileTOD);
        size_t bytesRemain = ntohs(mHdr->plen) - offset;
        while (bytesRemain) {
            auto curPtr = static_cast<void*>(buffc + offset);
            auto mFileHdr = static_cast<XrdXrootdMonFileHdr*>(curPtr);
            bytesRemain -= ntohs(mFileHdr->recSize);
            offset += ntohs(mFileHdr->recSize);

            switch (mFileHdr->recType) {
            case XrdXrootdMonFileHdr::isClose:
            {
                m_log.Log(LogMask::Debug, "MonPacket", "Received a f-stream file-close packet");
                auto mCLS = static_cast<XrdXrootdMonFileCLS*>(curPtr);
                FileRecord::id_t fileid = ntohl(mCLS->Hdr.fileID);
                prometheus::Labels labels;
                std::string path;
                FileRecord record;
                {
                    std::lock_guard<std::mutex> guard(m_transfers_mutex);
                    decltype(m_transfers) &transfers = const_cast<Handler*>(this)->m_transfers;
                    auto iter = transfers.find(fileid);
                    if (iter != transfers.end()) {
                        record = iter->second;
                        transfers.erase(iter);
                    }
                }
                labels["path"] = record.path;
                std::string ap, dn, role, org;
                {
                    std::lock_guard<std::mutex> guard(m_sessions_mutex);
                    decltype(m_sessions) &sessions = const_cast<Handler*>(this)->m_sessions;
                    auto iter = sessions.find(record.userid);
                    if (iter != sessions.end()) {
                        ap = iter->second.authenticationProtocol;
                        dn = iter->second.dn;
                        role = iter->second.role;
                        org = iter->second.org;
                    }
                }
                labels["auth_protocol"] = ap;
                labels["username"] = dn;
                labels["role"] = role;
                labels["org"] = org;
                if (mCLS->Hdr.recFlag & XrdXrootdMonFileHdr::hasOPS) {
                    auto &ctr_rsegs = m_transfer_readv_segments_family.Add(labels);
                    ctr_rsegs.Increment(ntohll(mCLS->Ops.rsegs) - record.readv_segs);
                    labels["type"] = "read";
                    auto &ctr_r = m_transfer_ops_family.Add(labels);
                    ctr_r.Increment(ntohl(mCLS->Ops.read) - record.read_ops);
                    labels["type"] = "readv";
                    auto &ctr_rv = m_transfer_ops_family.Add(labels);
                    ctr_rv.Increment(ntohl(mCLS->Ops.readv) - record.readv_ops);
                    labels["type"] = "write";
                    auto &ctr_w = m_transfer_ops_family.Add(labels);
                    ctr_w.Increment(ntohl(mCLS->Ops.write) - record.write_ops);
                }
                labels["type"] = "read";
                auto &ctr_r = m_transfer_bytes_family.Add(labels);
                ctr_r.Increment(ntohll(mCLS->Xfr.read) - record.read_bytes);
                labels["type"] = "readv";
                auto &ctr_rv = m_transfer_bytes_family.Add(labels);
                ctr_rv.Increment(ntohll(mCLS->Xfr.readv) - record.readv_bytes);
                labels["type"] = "write";
                auto &ctr_wr = m_transfer_bytes_family.Add(labels);
                ctr_wr.Increment(ntohll(mCLS->Xfr.write) - record.write_bytes);
                break;
            }
            case XrdXrootdMonFileHdr::isDisc:
            {
                m_log.Log(LogMask::Debug, "MonPacket", "Received a f-stream disconnect packet");
                auto mDSC = static_cast<XrdXrootdMonFileDSC*>(curPtr);
                UserRecord::id_t userid = ntohl(mDSC->Hdr.userID);
                {
                    std::lock_guard<std::mutex> guard(m_sessions_mutex);
                    decltype(m_sessions) &sessions = const_cast<Handler*>(this)->m_sessions;
                    sessions.erase(userid);
                }
                break;
            }
            case XrdXrootdMonFileHdr::isOpen:
            {
                m_log.Log(LogMask::Debug, "MonPacket", "Received a f-stream file-open packet");
                auto mOPN = static_cast<XrdXrootdMonFileOPN*>(curPtr);
                FileRecord::id_t fileid = ntohl(mOPN->Hdr.fileID);
                std::string path;
                if (mOPN->Hdr.recFlag & XrdXrootdMonFileHdr::hasLFN) {
                    path = ComputePrefix(mOPN->ufn.lfn);
                    if (m_log.getMsgMask() & LogMask::Debug)
                    {
                        std::stringstream ss;
                        ss << "User LFN " << mOPN->ufn.lfn << " matches prefix " << path;
                        m_log.Log(LogMask::Debug, "MonPacket", ss.str().c_str());
                    }
                }
                UserRecord::id_t userid = ntohll(mOPN->ufn.user);
                {
                    std::lock_guard<std::mutex> guard(m_transfers_mutex);
                    const_cast<Handler*>(this)->m_transfers.insert({fileid, {userid, path}});
                }
                break;
            }
            // Timestamp record -- no apparent need for this in Prometheus?
            case XrdXrootdMonFileHdr::isTime:
            {
                m_log.Log(LogMask::Debug, "MonPacket", "Received a f-stream time packet");
                break;
            }
            case XrdXrootdMonFileHdr::isXfr:
            {
                m_log.Log(LogMask::Debug, "MonPacket", "Received a f-stream time packet");
                auto mXFR = static_cast<XrdXrootdMonFileXFR*>(curPtr);
                FileRecord::id_t fileid = ntohl(mXFR->Hdr.fileID);
                std::string path;
                FileRecord record;
                {
                    std::lock_guard<std::mutex> guard(m_transfers_mutex);
                    decltype(m_transfers) &transfers = const_cast<Handler*>(this)->m_transfers;
                    auto iter = transfers.find(fileid);
                    if (iter == transfers.end()) {
                        break;
                    }
                    auto &record = iter->second;
                    record.read_bytes = ntohll(mXFR->Xfr.read);
                    record.readv_bytes = ntohll(mXFR->Xfr.readv);
                    record.write_bytes = ntohll(mXFR->Xfr.write);
                }
                break;
            }
            default:
            {
                if (m_log.getMsgMask() & LogMask::Debug)
                {
                    std::stringstream ss;
                    ss << "Received an unhandled monitoring packet of type " << mHdr->code;
                    m_log.Log(LogMask::Debug, "MonPacket", ss.str().c_str());
                }
            }
            }
        }
        break;
    }
    case 'g':
    {
        m_log.Log(LogMask::Debug, "MonPacket", "Received a g-stream packet");
        break;
    }
    case 'u':
    {
        m_log.Log(LogMask::Debug, "MonPacket", "Received a user login packet");
        auto mMap = static_cast<XrdXrootdMonMap*>(buff);
        UserRecord::id_t userid;
        std::string auth;
        if (GetSIDRest(mMap->info, userid, auth)) {
            XrdOucEnv env(auth.c_str(), auth.size());
            UserRecord record;
            auto dn = env.Get("n");
            if (dn) record.dn = std::string(dn);
            auto ap = env.Get("p");
            if (ap) record.authenticationProtocol = std::string(ap);
            auto on = env.Get("o");
            if (on) record.org = std::string(on);
            auto rn = env.Get("r");
            if (rn) record.role = std::string(rn);

            std::lock_guard<std::mutex> guard(m_sessions_mutex);
            decltype(m_sessions) &sessions = const_cast<Handler*>(this)->m_sessions;
            sessions.insert({userid, record});
        }
        break;
    }
    default:
    {
        if (m_log.getMsgMask() & LogMask::Debug)
        {
            std::stringstream ss;
            ss << "Received an unhandled monitoring packet of type " << mHdr->code;
            m_log.Log(LogMask::Debug, "MonPacket", ss.str().c_str());
        }
        break;
    }
    }
    return;
}


extern "C" {

XrdHttpExtHandler *XrdHttpGetExtHandler(XrdSysError *log, const char * config,
    const char * parms, XrdOucEnv * env)
{
    void *authz_raw = env->GetPtr("XrdAccAuthorize*");
    XrdAccAuthorize *def_authz = static_cast<XrdAccAuthorize *>(authz_raw);

    void *stats_raw = env->GetPtr("XrdStats*");
    if (!stats_raw) {
        log->Emsg("Config", "Prometheus handler couldn't find the summary stats information");
        return nullptr;
    }
    XrdStats *stats = static_cast<XrdStats *>(stats_raw);

    if (!XrdXmlReader::Init()) {
        log->Emsg("Config", "Failed to initialize the XML library");
        return nullptr;
    }

    if (parms && strlen(parms) > 0) {
        log->Emsg("Config", "Prometheus handler does not take any arguments", parms);
        return nullptr;
    }

    log->Emsg("Config", "Initializing Prometheus handler");
    try
    {
        return new Handler(log, config, stats, def_authz);
    }
    catch (std::runtime_error &e)
    {
        log->Emsg("Config", "Initialization of Prometheus handler failed", e.what());
        return NULL;
    }
}

}
