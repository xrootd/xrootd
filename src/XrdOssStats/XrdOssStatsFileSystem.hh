
#pragma once

#include "XrdOss/XrdOssWrapper.hh"
#include "XrdSys/XrdSysError.hh"

#include <atomic>
#include <chrono>
#include <memory>

class XrdXrootdGStream;

// The "stats" filesystem is a wrapper that collects information
// about the performance of the underlying storage.
//
// It allows one to accumulate time spent in I/O, the number of operations,
// and information about "slow" operations
class StatsFileSystem : public XrdOssWrapper {
    friend class StatsFile;
    friend class StatsDirectory;

public:
    // Note: StatsFileSystem takes ownerhip of the underlying oss
    StatsFileSystem(XrdOss *oss, XrdSysLogger *log, const char *configName, XrdOucEnv *envP);
    virtual ~StatsFileSystem();

    bool
    Config(const char *configfn);

    XrdOssDF *newDir(const char *user=0) override;
    XrdOssDF *newFile(const char *user=0) override;
    int Chmod(const char * path, mode_t mode, XrdOucEnv *env=0) override;
    int       Rename(const char *oPath, const char *nPath,
                         XrdOucEnv  *oEnvP=0, XrdOucEnv *nEnvP=0) override;
    int       Stat(const char *path, struct stat *buff,
                       int opts=0, XrdOucEnv *env=0) override;
    int       StatFS(const char *path, char *buff, int &blen,
                         XrdOucEnv  *env=0) override;
    int       StatLS(XrdOucEnv &env, const char *path,
                         char *buff, int &blen) override;
    int       StatPF(const char *path, struct stat *buff, int opts) override;
    int       StatPF(const char *path, struct stat *buff) override;
    int       StatVS(XrdOssVSInfo *vsP, const char *sname=0, int updt=0) override;
    int       StatXA(const char *path, char *buff, int &blen,
                         XrdOucEnv *env=0) override;
    int       StatXP(const char *path, unsigned long long &attr,
                         XrdOucEnv  *env=0) override;
    int       Truncate(const char *path, unsigned long long fsize,
                           XrdOucEnv *env=0) override;
    int       Unlink(const char *path, int Opts=0, XrdOucEnv *env=0) override;

private:
    static void * AggregateBootstrap(void *instance);
    void AggregateStats();

    XrdXrootdGStream* m_gstream{nullptr};

    std::unique_ptr<XrdOss> m_oss;
    XrdOucEnv *m_env;
    XrdSysError m_log;

    class OpTimer {
        public:
            OpTimer(std::atomic<uint64_t> &op_count, std::atomic<uint64_t> &slow_op_count, std::atomic<uint64_t> &timing, std::atomic<uint64_t> &slow_timing, std::chrono::steady_clock::duration duration);
            ~OpTimer();

        private:
            std::atomic<uint64_t> &m_op_count;
            std::atomic<uint64_t> &m_slow_op_count;
            std::atomic<uint64_t> &m_timing;
            std::atomic<uint64_t> &m_slow_timing;
            std::chrono::steady_clock::time_point m_start;
            std::chrono::steady_clock::duration m_slow_duration;
    };

    struct OpRecord {
        std::atomic<uint64_t> m_read_ops{0};
        std::atomic<uint64_t> m_write_ops{0};
        std::atomic<uint64_t> m_stat_ops{0};
        std::atomic<uint64_t> m_pgread_ops{0};
        std::atomic<uint64_t> m_pgwrite_ops{0};
        std::atomic<uint64_t> m_readv_ops{0};
        std::atomic<uint64_t> m_readv_segs{0};
        std::atomic<uint64_t> m_dirlist_ops{0};
        std::atomic<uint64_t> m_dirlist_entries{0};
        std::atomic<uint64_t> m_truncate_ops{0};
        std::atomic<uint64_t> m_unlink_ops{0};
        std::atomic<uint64_t> m_chmod_ops{0};
        std::atomic<uint64_t> m_open_ops{0};
        std::atomic<uint64_t> m_rename_ops{0};
    };

    struct OpTiming {
        std::atomic<uint64_t> m_open{0};
        std::atomic<uint64_t> m_read{0};
        std::atomic<uint64_t> m_readv{0};
        std::atomic<uint64_t> m_pgread{0};
        std::atomic<uint64_t> m_write{0};
        std::atomic<uint64_t> m_pgwrite{0};
        std::atomic<uint64_t> m_dirlist{0};
        std::atomic<uint64_t> m_stat{0};
        std::atomic<uint64_t> m_truncate{0};
        std::atomic<uint64_t> m_unlink{0};
        std::atomic<uint64_t> m_rename{0};
        std::atomic<uint64_t> m_chmod{0};
    };

    OpRecord m_ops;
    OpTiming m_times;
    OpRecord m_slow_ops;
    OpTiming m_slow_times;
    std::chrono::steady_clock::duration m_slow_duration;
};
