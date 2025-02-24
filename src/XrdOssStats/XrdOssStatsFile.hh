
#ifndef __XRDOSSSTATS_FILE_H
#define __XRDOSSSTATS_FILE_H

#include "XrdOss/XrdOssWrapper.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdOssStatsFileSystem.hh"

#include <memory>
#include <string>

class XrdSecEntity;

namespace XrdOssStats {

class File : public XrdOssWrapDF {
public:
    File(std::unique_ptr<XrdOssDF> wrapDF, XrdSysError &log, FileSystem &oss) :
      XrdOssWrapDF(*wrapDF),
      m_wrapped(std::move(wrapDF)),
      m_log(log),
      m_oss(oss)
    {}

    virtual ~File();

    int     Open(const char *path, int Oflag, mode_t Mode, XrdOucEnv &env) override
    {
        FileSystem::OpTimer op(m_oss.m_ops.m_open_ops, m_oss.m_slow_ops.m_open_ops, m_oss.m_times.m_open, m_oss.m_slow_times.m_open, m_oss.m_slow_duration);
        return wrapDF.Open(path, Oflag, Mode, env);
    }

    int     Fchmod(mode_t mode) override
    {
        FileSystem::OpTimer op(m_oss.m_ops.m_chmod_ops, m_oss.m_slow_ops.m_chmod_ops, m_oss.m_times.m_chmod, m_oss.m_slow_times.m_chmod, m_oss.m_slow_duration);
        return wrapDF.Fchmod(mode);
    }

    int     Fstat(struct stat *buf) override
    {
        FileSystem::OpTimer op(m_oss.m_ops.m_stat_ops, m_oss.m_slow_ops.m_stat_ops, m_oss.m_times.m_stat, m_oss.m_slow_times.m_stat, m_oss.m_slow_duration);
        return wrapDF.Fstat(buf);
    }

    int     Ftruncate(unsigned long long size) override
    {
        FileSystem::OpTimer op(m_oss.m_ops.m_truncate_ops, m_oss.m_slow_ops.m_truncate_ops, m_oss.m_times.m_truncate, m_oss.m_slow_times.m_truncate, m_oss.m_slow_duration);
        return wrapDF.Ftruncate(size);
    }

    ssize_t pgRead (void* buffer, off_t offset, size_t rdlen,
                        uint32_t* csvec, uint64_t opts) override
    {
        FileSystem::OpTimer op(m_oss.m_ops.m_pgread_ops, m_oss.m_slow_ops.m_pgread_ops, m_oss.m_times.m_pgread, m_oss.m_slow_times.m_pgread, m_oss.m_slow_duration);
        return wrapDF.pgRead(buffer, offset, rdlen, csvec, opts);
    }

    int     pgRead (XrdSfsAio* aioparm, uint64_t opts) override
    {
        FileSystem::OpTimer op(m_oss.m_ops.m_pgread_ops, m_oss.m_slow_ops.m_pgread_ops, m_oss.m_times.m_pgread, m_oss.m_slow_times.m_pgread, m_oss.m_slow_duration);
        return wrapDF.pgRead(aioparm, opts);
    }

    ssize_t pgWrite(void* buffer, off_t offset, size_t wrlen,
                        uint32_t* csvec, uint64_t opts) override
    {
        FileSystem::OpTimer op(m_oss.m_ops.m_pgwrite_ops, m_oss.m_slow_ops.m_pgwrite_ops, m_oss.m_times.m_pgwrite, m_oss.m_slow_times.m_pgwrite, m_oss.m_slow_duration);
        return wrapDF.pgWrite(buffer, offset, wrlen, csvec, opts);
    }

    int     pgWrite(XrdSfsAio* aioparm, uint64_t opts) override
    {
        FileSystem::OpTimer op(m_oss.m_ops.m_pgwrite_ops, m_oss.m_slow_ops.m_pgwrite_ops, m_oss.m_times.m_pgwrite, m_oss.m_slow_times.m_pgwrite, m_oss.m_slow_duration);
        return wrapDF.pgWrite(aioparm, opts);
    }

    ssize_t Read(off_t offset, size_t size) override
    {
        FileSystem::OpTimer op(m_oss.m_ops.m_read_ops, m_oss.m_slow_ops.m_read_ops, m_oss.m_times.m_read, m_oss.m_slow_times.m_read, m_oss.m_slow_duration);
        return wrapDF.Read(offset, size);
    }

    ssize_t Read(void *buffer, off_t offset, size_t size) override
    {
        FileSystem::OpTimer op(m_oss.m_ops.m_read_ops, m_oss.m_slow_ops.m_read_ops, m_oss.m_times.m_read, m_oss.m_slow_times.m_read, m_oss.m_slow_duration);
        return wrapDF.Read(buffer, offset, size);
    }

    int     Read(XrdSfsAio *aiop) override
    {
        FileSystem::OpTimer op(m_oss.m_ops.m_read_ops, m_oss.m_slow_ops.m_read_ops, m_oss.m_times.m_read, m_oss.m_slow_times.m_read, m_oss.m_slow_duration);
        return wrapDF.Read(aiop);
    }

    ssize_t ReadRaw(void *buffer, off_t offset, size_t size) override
    {
        FileSystem::OpTimer op(m_oss.m_ops.m_read_ops, m_oss.m_slow_ops.m_read_ops, m_oss.m_times.m_read, m_oss.m_slow_times.m_read, m_oss.m_slow_duration);
        return wrapDF.ReadRaw(buffer, offset, size);
    }

    ssize_t ReadV(XrdOucIOVec *readV, int rdvcnt) override
    {
        auto start = std::chrono::steady_clock::now();
        auto result = wrapDF.ReadV(readV, rdvcnt);
        auto dur = std::chrono::steady_clock::now() - start;
        m_oss.m_ops.m_readv_ops++;
        m_oss.m_ops.m_readv_segs += rdvcnt;
        auto ns = std::chrono::nanoseconds(dur).count();
        m_oss.m_times.m_readv += ns;
        if (dur > m_oss.m_slow_duration) {
            m_oss.m_slow_ops.m_readv_ops++;
            m_oss.m_slow_ops.m_readv_segs += rdvcnt;
            m_oss.m_times.m_readv += std::chrono::nanoseconds(dur).count();
        }
        return result;
    }

    ssize_t Write(const void *buffer, off_t offset, size_t size) override
    {
        FileSystem::OpTimer op(m_oss.m_ops.m_write_ops, m_oss.m_slow_ops.m_write_ops, m_oss.m_times.m_write, m_oss.m_slow_times.m_write, m_oss.m_slow_duration);
        return wrapDF.Write(buffer, offset, size);
    }

    int     Write(XrdSfsAio *aiop) override
    {
        FileSystem::OpTimer op(m_oss.m_ops.m_write_ops, m_oss.m_slow_ops.m_write_ops, m_oss.m_times.m_write, m_oss.m_slow_times.m_write, m_oss.m_slow_duration);
        return wrapDF.Write(aiop);
    }

    ssize_t WriteV(XrdOucIOVec *writeV, int wrvcnt) override
    {
        FileSystem::OpTimer op(m_oss.m_ops.m_write_ops, m_oss.m_slow_ops.m_write_ops, m_oss.m_times.m_write, m_oss.m_slow_times.m_write, m_oss.m_slow_duration);
        return wrapDF.WriteV(writeV, wrvcnt);
    }

private:
    std::unique_ptr<XrdOssDF> m_wrapped;
    XrdSysError &m_log;
    const XrdSecEntity* m_client;
    FileSystem &m_oss;

};

} // namespace XrdOssStats

#endif // __XRDOSSSTATS_FILES_H
