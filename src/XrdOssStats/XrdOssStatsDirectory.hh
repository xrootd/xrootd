
#ifndef __XRDSTATS_DIRECTORY_H
#define __XRDSTATS_DIRECTORY_H

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOss/XrdOssWrapper.hh"
#include "XrdOssStatsFileSystem.hh"
#include "XrdSys/XrdSysError.hh"

#include <memory>

namespace XrdOssStats {

class Directory : public XrdOssWrapDF {
public:
    Directory(std::unique_ptr<XrdOssDF> ossDF, XrdSysError &log, FileSystem &oss) :
        XrdOssWrapDF(*ossDF),
        m_wrappedDir(std::move(ossDF)),
        m_log(log),
        m_oss(oss)
    {
    }

    virtual ~Directory() {}

    virtual int
    Opendir(const char *path,
            XrdOucEnv &env) override 
    {
        FileSystem::OpTimer op(m_oss.m_ops.m_dirlist_ops, m_oss.m_slow_ops.m_dirlist_ops, m_oss.m_times.m_dirlist, m_oss.m_slow_times.m_dirlist, m_oss.m_slow_duration);
        return wrapDF.Opendir(path, env);
    }

    int Readdir(char *buff, int blen) override
    {
        FileSystem::OpTimer op(m_oss.m_ops.m_dirlist_entries, m_oss.m_slow_ops.m_dirlist_entries, m_oss.m_times.m_dirlist, m_oss.m_slow_times.m_dirlist, m_oss.m_slow_duration);
        return wrapDF.Readdir(buff, blen);
    }


private:
    std::unique_ptr<XrdOssDF> m_wrappedDir;
    XrdSysError m_log;
    FileSystem &m_oss;
};

} // namespace XrdOssStats

#endif // __XRDSTATS_DIRECTORY_H
