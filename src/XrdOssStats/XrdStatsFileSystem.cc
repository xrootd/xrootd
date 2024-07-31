
#include "XrdOuc/XrdOucGatherConf.hh"
#include "XrdStatsConfig.hh"
#include "XrdStatsDirectory.hh"
#include "XrdStatsFile.hh"
#include "XrdStatsFileSystem.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdXrootd/XrdXrootdGStream.hh"

#include <inttypes.h>
#include <stdexcept>
#include <thread>

StatsFileSystem::StatsFileSystem(XrdOss *oss, XrdSysLogger *lp, const char *configfn, XrdOucEnv *envP) :
    m_oss(oss),
    m_env(envP),
    m_log(lp, "fsstat_"),
    m_slow_duration(std::chrono::seconds(1))
{
    if (!oss) {
        throw std::runtime_error("The storage statistics plugin must be chained with another filesystem.");
    }
    m_log.Say("------ Initializing the storage statistics plugin.");
    if (!Config(configfn)) {
        throw std::runtime_error("Failed to configure the storage statistics plugin.");
    }
    pthread_t tid;
    int rc;
    if ((rc = XrdSysThread::Run(&tid, StatsFileSystem::AggregateBootstrap, static_cast<void *>(this), 0, "FS Stats Compute Thread"))) {
      m_log.Emsg("StatsFileSystem", rc, "create stats compute thread");
      throw std::runtime_error("Failed to create the statistics computing thread.");
    }
    if (envP) {
       m_gstream = reinterpret_cast<XrdXrootdGStream*>(envP->GetPtr("oss.gStream*"));
       m_log.Say("Config", "Stats monitoring has", m_gstream ? "" : " NOT", " been configured via xrootd.mongstream directive");
    }
}

StatsFileSystem::~StatsFileSystem() {}

void *
StatsFileSystem::AggregateBootstrap(void *me) {
    auto myself = static_cast<StatsFileSystem*>(me);
    while (1) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        myself->AggregateStats();
    }
    return nullptr;
}

bool
StatsFileSystem::Config(const char *configfn)
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

XrdOssDF *StatsFileSystem::newDir(const char *user)
{
    // Call the underlying OSS newDir
    std::unique_ptr<XrdOssDF> wrapped(m_oss->newDir(user));
    return new StatsDirectory(user, std::move(wrapped), m_log, *this);
}

XrdOssDF *StatsFileSystem::newFile(const char *user)
{
    // Call the underlying OSS newFile
    std::unique_ptr<XrdOssDF> wrapped(m_oss->newFile(user));
    return new StatsFile(std::move(wrapped), m_log, *this);
}

int StatsFileSystem::Chmod(const char * path, mode_t mode, XrdOucEnv *env)
{
    OpTimer op(m_ops.m_chmod_ops, m_slow_ops.m_chmod_ops, m_times.m_chmod, m_slow_times.m_chmod, m_slow_duration);
    return m_oss->Chmod(path, mode, env);
}

void StatsFileSystem::Connect(XrdOucEnv &env)
{
    m_oss->Connect(env);
}

int       StatsFileSystem::Create(const char *tid, const char *path, mode_t mode, XrdOucEnv &env,
                        int opts)
{
    return m_oss->Create(tid, path, mode, env, opts);
}

void      StatsFileSystem::Disc(XrdOucEnv &env)
{
    m_oss->Disc(env);
}

void      StatsFileSystem::EnvInfo(XrdOucEnv *env)
{
    m_oss->EnvInfo(env);
}

uint64_t  StatsFileSystem::Features()
{
    return m_oss->Features();
}

int       StatsFileSystem::FSctl(int cmd, int alen, const char *args, char **resp)
{
    return m_oss->FSctl(cmd, alen, args, resp);
}

int       StatsFileSystem::Init(XrdSysLogger *lp, const char *cfn)
{
    return 0;
}

int       StatsFileSystem::Init(XrdSysLogger *lp, const char *cfn, XrdOucEnv *env)
{
    return Init(lp, cfn);
}

int       StatsFileSystem::Mkdir(const char *path, mode_t mode, int mkpath,
                    XrdOucEnv  *env)
{
    return m_oss->Mkdir(path, mode, mkpath, env);
}

int       StatsFileSystem::Reloc(const char *tident, const char *path,
                    const char *cgName, const char *anchor)
{
    return m_oss->Reloc(tident, path, cgName, anchor);
}

int       StatsFileSystem::Remdir(const char *path, int Opts, XrdOucEnv *env)
{
    return m_oss->Remdir(path, Opts, env);
}

int       StatsFileSystem::Rename(const char *oPath, const char *nPath,
                        XrdOucEnv  *oEnvP, XrdOucEnv *nEnvP)
{
    OpTimer op(m_ops.m_rename_ops, m_slow_ops.m_rename_ops, m_times.m_rename, m_slow_times.m_rename, m_slow_duration);
    return m_oss->Rename(oPath, nPath, oEnvP, nEnvP);
}

int       StatsFileSystem::Stat(const char *path, struct stat *buff,
                    int opts, XrdOucEnv *env)
{
    OpTimer op(m_ops.m_stat_ops, m_slow_ops.m_stat_ops, m_times.m_stat, m_slow_times.m_stat, m_slow_duration);
    return m_oss->Stat(path, buff, opts, env);
}

int       StatsFileSystem::Stats(char *buff, int blen)
{
    return m_oss->Stats(buff, blen);
}

int       StatsFileSystem::StatFS(const char *path, char *buff, int &blen,
                        XrdOucEnv  *env)
{
    OpTimer op(m_ops.m_stat_ops, m_slow_ops.m_stat_ops, m_times.m_stat, m_slow_times.m_stat, m_slow_duration);
    return m_oss->StatFS(path, buff, blen, env);
}

int       StatsFileSystem::StatLS(XrdOucEnv &env, const char *path,
                        char *buff, int &blen)
{
    OpTimer op(m_ops.m_stat_ops, m_slow_ops.m_stat_ops, m_times.m_stat, m_slow_times.m_stat, m_slow_duration);
    return m_oss->StatLS(env, path, buff, blen);
}

int       StatsFileSystem::StatPF(const char *path, struct stat *buff, int opts)
{
    OpTimer op(m_ops.m_stat_ops, m_slow_ops.m_stat_ops, m_times.m_stat, m_slow_times.m_stat, m_slow_duration);
    return m_oss->StatPF(path, buff, opts);
}

int       StatsFileSystem::StatPF(const char *path, struct stat *buff)
{
    OpTimer op(m_ops.m_stat_ops, m_slow_ops.m_stat_ops, m_times.m_stat, m_slow_times.m_stat, m_slow_duration);
    return m_oss->StatPF(path, buff, 0);
}

int       StatsFileSystem::StatVS(XrdOssVSInfo *vsP, const char *sname, int updt)
{
    OpTimer op(m_ops.m_stat_ops, m_slow_ops.m_stat_ops, m_times.m_stat, m_slow_times.m_stat, m_slow_duration);
    return m_oss->StatVS(vsP, sname, updt);
}

int       StatsFileSystem::StatXA(const char *path, char *buff, int &blen,
                        XrdOucEnv *env)
{
    OpTimer op(m_ops.m_stat_ops, m_slow_ops.m_stat_ops, m_times.m_stat, m_slow_times.m_stat, m_slow_duration);
    return m_oss->StatXA(path, buff, blen, env);
}

int       StatsFileSystem::StatXP(const char *path, unsigned long long &attr,
                        XrdOucEnv  *env)
{
    OpTimer op(m_ops.m_stat_ops, m_slow_ops.m_stat_ops, m_times.m_stat, m_slow_times.m_stat, m_slow_duration);
    return m_oss->StatXP(path, attr, env);
}

int       StatsFileSystem::Truncate(const char *path, unsigned long long fsize,
                        XrdOucEnv *env)
{
    OpTimer op(m_ops.m_truncate_ops, m_slow_ops.m_truncate_ops, m_times.m_truncate, m_slow_times.m_truncate, m_slow_duration);
    return m_oss->Truncate(path, fsize, env);
}

int       StatsFileSystem::Unlink(const char *path, int Opts, XrdOucEnv *env)
{
    OpTimer op(m_ops.m_unlink_ops, m_slow_ops.m_unlink_ops, m_times.m_unlink, m_slow_times.m_unlink, m_slow_duration);
    return m_oss->Unlink(path, Opts, env);
}

int       StatsFileSystem::Lfn2Pfn(const char *Path, char *buff, int blen)
{
    return m_oss->Lfn2Pfn(Path, buff, blen);
}

const char       *StatsFileSystem::Lfn2Pfn(const char *Path, char *buff, int blen, int &rc)
{
    return m_oss->Lfn2Pfn(Path, buff, blen, rc);
}

void StatsFileSystem::AggregateStats()
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
        m_ops.m_read_ops.load(), m_ops.m_write_ops.load(), m_ops.m_stat_ops.load(),
        m_ops.m_pgread_ops.load(), m_ops.m_pgwrite_ops.load(), m_ops.m_readv_ops.load(),
        m_ops.m_readv_segs.load(), m_ops.m_dirlist_ops.load(), m_ops.m_dirlist_entries.load(),
        m_ops.m_truncate_ops.load(), m_ops.m_unlink_ops.load(), m_ops.m_chmod_ops.load(),
        m_ops.m_open_ops.load(), m_ops.m_rename_ops.load(),
        m_slow_ops.m_read_ops.load(), m_slow_ops.m_write_ops.load(), m_slow_ops.m_stat_ops.load(),
        m_slow_ops.m_pgread_ops.load(), m_slow_ops.m_pgwrite_ops.load(), m_slow_ops.m_readv_ops.load(),
        m_slow_ops.m_readv_segs.load(), m_slow_ops.m_dirlist_ops.load(), m_slow_ops.m_dirlist_entries.load(),
        m_slow_ops.m_truncate_ops.load(), m_slow_ops.m_unlink_ops.load(), m_slow_ops.m_chmod_ops.load(),
        m_slow_ops.m_open_ops.load(), m_slow_ops.m_rename_ops.load(),
        static_cast<float>(m_times.m_open.load())/1e9, static_cast<float>(m_times.m_read.load())/1e9, static_cast<float>(m_times.m_readv.load())/1e9,
        static_cast<float>(m_times.m_pgread.load())/1e9, static_cast<float>(m_times.m_write.load())/1e9, static_cast<float>(m_times.m_pgwrite.load())/1e9,
        static_cast<float>(m_times.m_dirlist.load())/1e9, static_cast<float>(m_times.m_stat.load())/1e9, static_cast<float>(m_times.m_truncate.load())/1e9,
        static_cast<float>(m_times.m_unlink.load())/1e9, static_cast<float>(m_times.m_rename.load())/1e9, static_cast<float>(m_times.m_chmod.load())/1e9,
        static_cast<float>(m_slow_times.m_open.load())/1e9, static_cast<float>(m_slow_times.m_read.load())/1e9, static_cast<float>(m_slow_times.m_readv.load())/1e9,
        static_cast<float>(m_slow_times.m_pgread.load())/1e9, static_cast<float>(m_slow_times.m_write.load())/1e9, static_cast<float>(m_slow_times.m_pgwrite.load())/1e9,
        static_cast<float>(m_slow_times.m_dirlist.load())/1e9, static_cast<float>(m_slow_times.m_stat.load())/1e9, static_cast<float>(m_slow_times.m_truncate.load())/1e9,
        static_cast<float>(m_slow_times.m_unlink.load())/1e9, static_cast<float>(m_slow_times.m_rename.load())/1e9, static_cast<float>(m_slow_times.m_chmod.load())/1e9

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

StatsFileSystem::OpTimer::OpTimer(std::atomic<uint64_t> &op_count, std::atomic<uint64_t> &slow_op_count, std::atomic<uint64_t> &timing, std::atomic<uint64_t> &slow_timing, std::chrono::steady_clock::duration duration)
    : m_op_count(op_count),
    m_slow_op_count(slow_op_count),
    m_timing(timing),
    m_slow_timing(slow_timing),
    m_start(std::chrono::steady_clock::now()),
    m_slow_duration(duration)
{}

StatsFileSystem::OpTimer::~OpTimer()
{
    auto dur = std::chrono::steady_clock::now() - m_start;
    m_op_count++;
    m_timing += std::chrono::nanoseconds(dur).count();
    if (dur > m_slow_duration) {
        m_slow_op_count++;
        m_slow_timing += std::chrono::nanoseconds(dur).count();
    }
}
