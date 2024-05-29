
#pragma once

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdStatsFileSystem.hh"
#include "XrdSys/XrdSysError.hh"

#include <memory>

class StatsDirectory : public XrdOssDF {
public:
    StatsDirectory(const char *user, std::unique_ptr<XrdOssDF> ossDF, XrdSysError &log, StatsFileSystem &oss) :
        XrdOssDF(user),
        m_wrappedDir(std::move(ossDF)),
        m_log(log),
        m_oss(oss)
    {
    }

    virtual ~StatsDirectory() {}

    virtual int
    Opendir(const char *path,
            XrdOucEnv &env) override 
    {
        StatsFileSystem::OpTimer op(m_oss.m_ops.m_dirlist_ops, m_oss.m_slow_ops.m_dirlist_ops, m_oss.m_times.m_dirlist, m_oss.m_slow_times.m_dirlist, m_oss.m_slow_duration);
        return m_wrappedDir->Opendir(path, env);
    }

    int Readdir(char *buff, int blen) override
    {
        StatsFileSystem::OpTimer op(m_oss.m_ops.m_dirlist_entries, m_oss.m_slow_ops.m_dirlist_entries, m_oss.m_times.m_dirlist, m_oss.m_slow_times.m_dirlist, m_oss.m_slow_duration);
        return m_wrappedDir->Readdir(buff, blen);
    }

    int StatRet(struct stat *statStruct) override
    {
        return m_wrappedDir->StatRet(statStruct);
    }

    int Close(long long *retsz=0) override
    {
        return m_wrappedDir->Close(retsz);
    }


private:
    std::unique_ptr<XrdOssDF> m_wrappedDir;
    XrdSysError m_log;
    StatsFileSystem &m_oss;
};
