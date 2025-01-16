
#include "XrdOuc/XrdOucGatherConf.hh"
#include "XrdOssStatsConfig.hh"
#include "XrdOssStatsDirectory.hh"
#include "XrdOssStatsFile.hh"
#include "XrdOssStatsFileSystem.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdXrootd/XrdXrootdGStream.hh"

#include <inttypes.h>
#include <stdexcept>
#include <thread>

using namespace XrdOssStats;
using namespace XrdOssStats::detail;

FileSystem::FileSystem(XrdOss *oss, XrdSysLogger *lp, const char *configfn, XrdOucEnv *envP) :
    XrdOssWrapper(*oss),
    m_oss(oss),
    m_env(envP),
    m_log(lp, "fsstat_"),
    m_slow_duration(std::chrono::seconds(1))
{
    m_log.Say("------ Initializing the storage statistics plugin.");
    if (!Config(configfn)) {
        m_failure = "Failed to configure the storage statistics plugin.";
        return;
    }

    // While the plugin _does_ print its activity to the debugging facility (if enabled), its relatively useless
    // unless the g-stream is available.  Hence, if it's _not_ available, we stop the OSS initialization but do
    // not cause the server startup to fail.
    if (envP) {
        m_gstream = reinterpret_cast<XrdXrootdGStream*>(envP->GetPtr("oss.gStream*"));
        if (m_gstream) {
            m_log.Say("Config", "Stats monitoring has been configured via xrootd.mongstream directive");
        } else {
            m_log.Say("Config", "XrdOssStats plugin is loaded but it requires the oss monitoring g-stream to also be enabled to be useful; try adding `xrootd.mongstream oss ...` to your configuration");
            return;
        }
    } else {
        m_failure = "XrdOssStats plugin invoked without a configured environment; likely an internal error";
        return;
    }

    pthread_t tid;
    int rc;
    if ((rc = XrdSysThread::Run(&tid, FileSystem::AggregateBootstrap, static_cast<void *>(this), 0, "FS Stats Compute Thread"))) {
        m_log.Emsg("FileSystem", rc, "create stats compute thread");
        m_failure = "Failed to create the statistics computing thread.";
        return;
    }

    m_ready = true;
}

FileSystem::~FileSystem() {}

bool
FileSystem::InitSuccessful(std::string &errMsg) {
    if (m_ready) return true;

    errMsg = m_failure;
    if (errMsg.empty()) {
        m_oss.release();
    }
    return false;
}

void *
FileSystem::AggregateBootstrap(void *me) {
    auto myself = static_cast<FileSystem*>(me);
    while (1) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        myself->AggregateStats();
    }
    return nullptr;
}

bool
FileSystem::Config(const char *configfn)
{
    m_log.setMsgMask(LogMask::Error | LogMask::Warning);

    XrdOucGatherConf statsConf("fsstats.trace fsstats.slowop", &m_log);
    int result;
    if ((result = statsConf.Gather(configfn, XrdOucGatherConf::trim_lines)) < 0) {
        m_log.Emsg("Config", -result, "parsing config file", configfn);
        return false;
    }

    char *val;
    while (statsConf.GetLine()) {
        val = statsConf.GetToken(); // Ignore -- we asked for a single value
        if (!strcmp(val, "trace")) {
            m_log.setMsgMask(0);
            if (!(val = statsConf.GetToken())) {
                m_log.Emsg("Config", "fsstats.trace requires an argument.  Usage: fsstats.trace [all|err|warning|info|debug|none]");
                return false;
            }
            do {
                if (!strcmp(val, "all")) {m_log.setMsgMask(m_log.getMsgMask() | LogMask::All);}
                else if (!strcmp(val, "error")) {m_log.setMsgMask(m_log.getMsgMask() | LogMask::Error);}
                else if (!strcmp(val, "warning")) {m_log.setMsgMask(m_log.getMsgMask() | LogMask::Error | LogMask::Warning);}
                else if (!strcmp(val, "info")) {m_log.setMsgMask(m_log.getMsgMask() | LogMask::Error | LogMask::Warning | LogMask::Info);}
                else if (!strcmp(val, "debug")) {m_log.setMsgMask(m_log.getMsgMask() | LogMask::Error | LogMask::Warning | LogMask::Info | LogMask::Debug);}
                else if (!strcmp(val, "none")) {m_log.setMsgMask(0);}
            } while ((val = statsConf.GetToken()));
        } else if (!strcmp(val, "slowop")) {
            if (!(val = statsConf.GetToken())) {
                m_log.Emsg("Config", "fsstats.slowop requires an argument.  Usage: fsstats.slowop [duration]");
                return false;
            }
            std::string errmsg;
            if (!ParseDuration(val, m_slow_duration, errmsg)) {
                m_log.Emsg("Config", "fsstats.slowop couldn't parse duration", val, errmsg.c_str());
                return false;
            }
        }
    }
    m_log.Emsg("Config", "Logging levels enabled", LogMaskToString(m_log.getMsgMask()).c_str());

    return true;
}

XrdOssDF *FileSystem::newDir(const char *user)
{
    // Call the underlying OSS newDir
    std::unique_ptr<XrdOssDF> wrapped(wrapPI.newDir(user));
    return new Directory(std::move(wrapped), m_log, *this);
}

XrdOssDF *FileSystem::newFile(const char *user)
{
    // Call the underlying OSS newFile
    std::unique_ptr<XrdOssDF> wrapped(wrapPI.newFile(user));
    return new File(std::move(wrapped), m_log, *this);
}

int FileSystem::Chmod(const char * path, mode_t mode, XrdOucEnv *env)
{
    OpTimer op(m_ops.m_chmod_ops, m_slow_ops.m_chmod_ops, m_times.m_chmod, m_slow_times.m_chmod, m_slow_duration);
    return wrapPI.Chmod(path, mode, env);
}

int       FileSystem::Rename(const char *oPath, const char *nPath,
                        XrdOucEnv  *oEnvP, XrdOucEnv *nEnvP)
{
    OpTimer op(m_ops.m_rename_ops, m_slow_ops.m_rename_ops, m_times.m_rename, m_slow_times.m_rename, m_slow_duration);
    return wrapPI.Rename(oPath, nPath, oEnvP, nEnvP);
}

int       FileSystem::Stat(const char *path, struct stat *buff,
                    int opts, XrdOucEnv *env)
{
    OpTimer op(m_ops.m_stat_ops, m_slow_ops.m_stat_ops, m_times.m_stat, m_slow_times.m_stat, m_slow_duration);
    return wrapPI.Stat(path, buff, opts, env);
}

int       FileSystem::StatFS(const char *path, char *buff, int &blen,
                        XrdOucEnv  *env)
{
    OpTimer op(m_ops.m_stat_ops, m_slow_ops.m_stat_ops, m_times.m_stat, m_slow_times.m_stat, m_slow_duration);
    return wrapPI.StatFS(path, buff, blen, env);
}

int       FileSystem::StatLS(XrdOucEnv &env, const char *path,
                        char *buff, int &blen)
{
    OpTimer op(m_ops.m_stat_ops, m_slow_ops.m_stat_ops, m_times.m_stat, m_slow_times.m_stat, m_slow_duration);
    return wrapPI.StatLS(env, path, buff, blen);
}

int       FileSystem::StatPF(const char *path, struct stat *buff, int opts)
{
    OpTimer op(m_ops.m_stat_ops, m_slow_ops.m_stat_ops, m_times.m_stat, m_slow_times.m_stat, m_slow_duration);
    return wrapPI.StatPF(path, buff, opts);
}

int       FileSystem::StatPF(const char *path, struct stat *buff)
{
    OpTimer op(m_ops.m_stat_ops, m_slow_ops.m_stat_ops, m_times.m_stat, m_slow_times.m_stat, m_slow_duration);
    return wrapPI.StatPF(path, buff, 0);
}

int       FileSystem::StatVS(XrdOssVSInfo *vsP, const char *sname, int updt)
{
    OpTimer op(m_ops.m_stat_ops, m_slow_ops.m_stat_ops, m_times.m_stat, m_slow_times.m_stat, m_slow_duration);
    return wrapPI.StatVS(vsP, sname, updt);
}

int       FileSystem::StatXA(const char *path, char *buff, int &blen,
                        XrdOucEnv *env)
{
    OpTimer op(m_ops.m_stat_ops, m_slow_ops.m_stat_ops, m_times.m_stat, m_slow_times.m_stat, m_slow_duration);
    return wrapPI.StatXA(path, buff, blen, env);
}

int       FileSystem::StatXP(const char *path, unsigned long long &attr,
                        XrdOucEnv  *env)
{
    OpTimer op(m_ops.m_stat_ops, m_slow_ops.m_stat_ops, m_times.m_stat, m_slow_times.m_stat, m_slow_duration);
    return wrapPI.StatXP(path, attr, env);
}

int       FileSystem::Truncate(const char *path, unsigned long long fsize,
                        XrdOucEnv *env)
{
    OpTimer op(m_ops.m_truncate_ops, m_slow_ops.m_truncate_ops, m_times.m_truncate, m_slow_times.m_truncate, m_slow_duration);
    return wrapPI.Truncate(path, fsize, env);
}

int       FileSystem::Unlink(const char *path, int Opts, XrdOucEnv *env)
{
    OpTimer op(m_ops.m_unlink_ops, m_slow_ops.m_unlink_ops, m_times.m_unlink, m_slow_times.m_unlink, m_slow_duration);
    return wrapPI.Unlink(path, Opts, env);
}

void FileSystem::AggregateStats()
{
    char buf[1500];
    auto len = snprintf(buf, 1500,
        "{"
        "\"event\":\"oss_stats\"," \
        "\"reads\":%" PRIu64 ",\"writes\":%" PRIu64 ",\"stats\":%" PRIu64 "," \
        "\"pgreads\":%" PRIu64 ",\"pgwrites\":%" PRIu64 ",\"readvs\":%" PRIu64 "," \
        "\"readv_segs\":%" PRIu64 ",\"dirlists\":%" PRIu64 ",\"dirlist_ents\":%" PRIu64 ","
        "\"truncates\":%" PRIu64 ",\"unlinks\":%" PRIu64 ",\"chmods\":%" PRIu64 ","
        "\"opens\":%" PRIu64 ",\"renames\":%" PRIu64 ","
        "\"slow_reads\":%" PRIu64 ",\"slow_writes\":%" PRIu64 ",\"slow_stats\":%" PRIu64 ","
        "\"slow_pgreads\":%" PRIu64 ",\"slow_pgwrites\":%" PRIu64 ",\"slow_readvs\":%" PRIu64 ","
        "\"slow_readv_segs\":%" PRIu64 ",\"slow_dirlists\":%" PRIu64 ",\"slow_dirlist_ents\":%" PRIu64 ","
        "\"slow_truncates\":%" PRIu64 ",\"slow_unlinks\":%" PRIu64 ",\"slow_chmods\":%" PRIu64 ","
        "\"slow_opens\":%" PRIu64 ",\"slow_renames\":%" PRIu64 ","
        "\"open_t\":%.4f,\"read_t\":%.4f,\"readv_t\":%.4f,"
        "\"pgread_t\":%.4f,\"write_t\":%.4f,\"pgwrite_t\":%.4f,"
        "\"dirlist_t\":%.4f,\"stat_t\":%.4f,\"truncate_t\":%.4f,"
        "\"unlink_t\":%.4f,\"rename_t\":%.4f,\"chmod_t\":%.4f,"
        "\"slow_open_t\":%.4f,\"slow_read_t\":%.4f,\"slow_readv_t\":%.4f,"
        "\"slow_pgread_t\":%.4f,\"slow_write_t\":%.4f,\"slow_pgwrite_t\":%.4f,"
        "\"slow_dirlist_t\":%.4f,\"slow_stat_t\":%.4f,\"slow_truncate_t\":%.4f,"
        "\"slow_unlink_t\":%.4f,\"slow_rename_t\":%.4f,\"slow_chmod_t\":%.4f"
        "}",
        static_cast<uint64_t>(m_ops.m_read_ops), static_cast<uint64_t>(m_ops.m_write_ops), static_cast<uint64_t>(m_ops.m_stat_ops),
        static_cast<uint64_t>(m_ops.m_pgread_ops), static_cast<uint64_t>(m_ops.m_pgwrite_ops), static_cast<uint64_t>(m_ops.m_readv_ops),
        static_cast<uint64_t>(m_ops.m_readv_segs), static_cast<uint64_t>(m_ops.m_dirlist_ops), static_cast<uint64_t>(m_ops.m_dirlist_entries),
        static_cast<uint64_t>(m_ops.m_truncate_ops), static_cast<uint64_t>(m_ops.m_unlink_ops), static_cast<uint64_t>(m_ops.m_chmod_ops),
        static_cast<uint64_t>(m_ops.m_open_ops), static_cast<uint64_t>(m_ops.m_rename_ops),
        static_cast<uint64_t>(m_slow_ops.m_read_ops), static_cast<uint64_t>(m_slow_ops.m_write_ops), static_cast<uint64_t>(m_slow_ops.m_stat_ops),
        static_cast<uint64_t>(m_slow_ops.m_pgread_ops), static_cast<uint64_t>(m_slow_ops.m_pgwrite_ops), static_cast<uint64_t>(m_slow_ops.m_readv_ops),
        static_cast<uint64_t>(m_slow_ops.m_readv_segs), static_cast<uint64_t>(m_slow_ops.m_dirlist_ops), static_cast<uint64_t>(m_slow_ops.m_dirlist_entries),
        static_cast<uint64_t>(m_slow_ops.m_truncate_ops), static_cast<uint64_t>(m_slow_ops.m_unlink_ops), static_cast<uint64_t>(m_slow_ops.m_chmod_ops),
        static_cast<uint64_t>(m_slow_ops.m_open_ops), static_cast<uint64_t>(m_slow_ops.m_rename_ops),
        static_cast<float>(m_times.m_open)/1e9, static_cast<float>(m_times.m_read)/1e9, static_cast<float>(m_times.m_readv)/1e9,
        static_cast<float>(m_times.m_pgread)/1e9, static_cast<float>(m_times.m_write)/1e9, static_cast<float>(m_times.m_pgwrite)/1e9,
        static_cast<float>(m_times.m_dirlist)/1e9, static_cast<float>(m_times.m_stat)/1e9, static_cast<float>(m_times.m_truncate)/1e9,
        static_cast<float>(m_times.m_unlink)/1e9, static_cast<float>(m_times.m_rename)/1e9, static_cast<float>(m_times.m_chmod)/1e9,
        static_cast<float>(m_slow_times.m_open)/1e9, static_cast<float>(m_slow_times.m_read)/1e9, static_cast<float>(m_slow_times.m_readv)/1e9,
        static_cast<float>(m_slow_times.m_pgread)/1e9, static_cast<float>(m_slow_times.m_write)/1e9, static_cast<float>(m_slow_times.m_pgwrite)/1e9,
        static_cast<float>(m_slow_times.m_dirlist)/1e9, static_cast<float>(m_slow_times.m_stat)/1e9, static_cast<float>(m_slow_times.m_truncate)/1e9,
        static_cast<float>(m_slow_times.m_unlink)/1e9, static_cast<float>(m_slow_times.m_rename)/1e9, static_cast<float>(m_slow_times.m_chmod)/1e9

    );
    if (len >= 1500) {
        m_log.Log(LogMask::Error, "Aggregate", "Failed to generate g-stream statistics packet");
        return;
    }
    m_log.Log(LogMask::Debug, "Aggregate", buf);
    if (m_gstream && !m_gstream->Insert(buf, len + 1)) {
        m_log.Log(LogMask::Error, "Aggregate", "Failed to send g-stream statistics packet");
        return;
    }
}

FileSystem::OpTimer::OpTimer(RAtomic_uint64_t &op_count, RAtomic_uint64_t &slow_op_count, RAtomic_uint64_t &timing, RAtomic_uint64_t &slow_timing, std::chrono::steady_clock::duration duration)
    : m_op_count(op_count),
    m_slow_op_count(slow_op_count),
    m_timing(timing),
    m_slow_timing(slow_timing),
    m_start(std::chrono::steady_clock::now()),
    m_slow_duration(duration)
{}

FileSystem::OpTimer::~OpTimer()
{
    auto dur = std::chrono::steady_clock::now() - m_start;
    m_op_count++;
    m_timing += std::chrono::nanoseconds(dur).count();
    if (dur > m_slow_duration) {
        m_slow_op_count++;
        m_slow_timing += std::chrono::nanoseconds(dur).count();
    }
}
