
#ifndef __XRDOSSSTATS_FILESYSTEM_H
#define __XRDOSSSTATS_FILESYSTEM_H

#include "XrdOss/XrdOssWrapper.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysRAtomic.hh"

#include <chrono>
#include <memory>
#include <string>

class XrdXrootdGStream;

namespace XrdOssStats {

// The "stats" filesystem is a wrapper that collects information
// about the performance of the underlying storage.
//
// It allows one to accumulate time spent in I/O, the number of operations,
// and information about "slow" operations
class FileSystem : public XrdOssWrapper {
    friend class File;
    friend class Directory;

public:
    // Note: FileSystem takes ownership of the underlying oss
    FileSystem(XrdOss *oss, XrdSysLogger *log, const char *configName, XrdOucEnv *envP);
    virtual ~FileSystem();

    bool
    Config(const char *configfn);

    // Indicate whether the initialization of the object was successful
    //
    // If `false` and `errMsg` is set, then the failure is fatal and the
    // xrootd server should not startup.
    // If `false` and `errMsg` is empty, then it's OK to bypass this object
    // and just use the wrapped OSS pointer directly.
    bool
    InitSuccessful(std::string &errMsg);

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

    // Indicates whether the class was able to initialize.
    // On initialization failure, if there is no failure message
    // set, then we assume it is OK to proceed with the wrapped OSS.
    // If m_failure is set, then we assume the initialization failure
    // was fatal and it's better to halt startup than proceed.
    bool m_ready{false};
    std::string m_failure;
    std::unique_ptr<XrdOss> m_oss;
    XrdOucEnv *m_env;
    XrdSysError m_log;

    class OpTimer {
        public:
            OpTimer(RAtomic_uint64_t &op_count, RAtomic_uint64_t &slow_op_count, RAtomic_uint64_t &timing, RAtomic_uint64_t &slow_timing, std::chrono::steady_clock::duration duration);
            ~OpTimer();

        private:
            RAtomic_uint64_t &m_op_count;
            RAtomic_uint64_t &m_slow_op_count;
            RAtomic_uint64_t &m_timing;
            RAtomic_uint64_t &m_slow_timing;
            std::chrono::steady_clock::time_point m_start;
            std::chrono::steady_clock::duration m_slow_duration;
    };

    struct OpRecord {
        RAtomic_uint64_t m_read_ops{0};
        RAtomic_uint64_t m_write_ops{0};
        RAtomic_uint64_t m_stat_ops{0};
        RAtomic_uint64_t m_pgread_ops{0};
        RAtomic_uint64_t m_pgwrite_ops{0};
        RAtomic_uint64_t m_readv_ops{0};
        RAtomic_uint64_t m_readv_segs{0};
        RAtomic_uint64_t m_dirlist_ops{0};
        RAtomic_uint64_t m_dirlist_entries{0};
        RAtomic_uint64_t m_truncate_ops{0};
        RAtomic_uint64_t m_unlink_ops{0};
        RAtomic_uint64_t m_chmod_ops{0};
        RAtomic_uint64_t m_open_ops{0};
        RAtomic_uint64_t m_rename_ops{0};
    };

    struct OpTiming {
        RAtomic_uint64_t m_open{0};
        RAtomic_uint64_t m_read{0};
        RAtomic_uint64_t m_readv{0};
        RAtomic_uint64_t m_pgread{0};
        RAtomic_uint64_t m_write{0};
        RAtomic_uint64_t m_pgwrite{0};
        RAtomic_uint64_t m_dirlist{0};
        RAtomic_uint64_t m_stat{0};
        RAtomic_uint64_t m_truncate{0};
        RAtomic_uint64_t m_unlink{0};
        RAtomic_uint64_t m_rename{0};
        RAtomic_uint64_t m_chmod{0};
    };

    OpRecord m_ops;
    OpTiming m_times;
    OpRecord m_slow_ops;
    OpTiming m_slow_times;
    std::chrono::steady_clock::duration m_slow_duration;
};

} // XrdOssStats

#endif // __XRDOSSSTATS_FILESYSTEM_H
