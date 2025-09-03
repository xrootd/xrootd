/******************************************************************************/
/*                                                                            */
/* (c) 2025 by the Morgridge Institute for Research                           */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#include "XrdOuc/XrdOucEnv.hh"
#include <XrdOuc/XrdOucGatherConf.hh>
#include "XrdOss/XrdOss.hh"
#include "XrdOss/XrdOssWrapper.hh"
#include "XrdSfs/XrdSfsAio.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdThrottle/XrdThrottleConfig.hh"
#include "XrdThrottle/XrdThrottleManager.hh"
#include "XrdThrottle/XrdThrottleTrace.hh"
#include "XrdVersion.hh"

#include <functional>

namespace {

class File final : public XrdOssWrapDF {
public:
    File(std::unique_ptr<XrdOssDF> wrapDF, XrdThrottleManager &throttle, XrdSysError *lP, XrdOucTrace *tP)
        : XrdOssWrapDF(*wrapDF), m_log(lP), m_throttle(throttle), m_trace(tP), m_wrapped(std::move(wrapDF)) {}

virtual ~File() {}

virtual int Open(const char *path, int Oflag, mode_t Mode,
    XrdOucEnv &env) override {

    std::tie(m_user, m_uid) = m_throttle.GetUserInfo(env.secEnv());

    std::string open_error_message;
    if (!m_throttle.OpenFile(m_user, open_error_message)) {
        TRACE(DEBUG, open_error_message);
        return -EMFILE;
    }

    auto rval = wrapDF.Open(path, Oflag, Mode, env);

    if (rval < 0) {
        m_throttle.CloseFile(m_user);
    }

    return rval;
}

virtual int Close(long long *retsz) override {
   m_throttle.CloseFile(m_user);
   return wrapDF.Close(retsz);
}

virtual int getFD() override {return -1;}

virtual off_t getMmap(void **addr) override {*addr = 0; return 0;}

virtual ssize_t pgRead (void* buffer, off_t offset, size_t rdlen,
    uint32_t* csvec, uint64_t opts) override {

    return DoThrottle(rdlen, 1,
        static_cast<ssize_t (XrdOssDF::*)(void*, off_t, size_t, uint32_t*, uint64_t)>(&XrdOssDF::pgRead),
        buffer, offset, rdlen, csvec, opts);
}

virtual int pgRead(XrdSfsAio *aioparm, uint64_t opts) override
{  // We disable all AIO-based reads.
   aioparm->Result = pgRead((char *)aioparm->sfsAio.aio_buf,
                                    aioparm->sfsAio.aio_offset,          
                                    aioparm->sfsAio.aio_nbytes,
                                    aioparm->cksVec, opts);
   aioparm->doneRead();
   return 0;
}

virtual ssize_t pgWrite(void* buffer, off_t offset, size_t wrlen,
    uint32_t* csvec, uint64_t opts) override {

    return DoThrottle(wrlen, 1,
        static_cast<ssize_t (XrdOssDF::*)(void*, off_t, size_t, uint32_t*, uint64_t)>(&XrdOssDF::pgWrite),
        buffer, offset, wrlen, csvec, opts);
}

virtual int pgWrite(XrdSfsAio *aioparm, uint64_t opts) override
{  // We disable all AIO-based writes.
   aioparm->Result = this->pgWrite((char *)aioparm->sfsAio.aio_buf,
                                           aioparm->sfsAio.aio_offset, 
                                           aioparm->sfsAio.aio_nbytes,
                                           aioparm->cksVec, opts);
   aioparm->doneWrite();
   return 0;
}

virtual ssize_t Read(off_t offset, size_t size) override {
    return DoThrottle(size, 1,
        static_cast<ssize_t (XrdOssDF::*)(off_t, size_t)>(&XrdOssDF::Read),
        offset, size);
}
virtual ssize_t Read(void* buffer, off_t offset, size_t size) override {
    return DoThrottle(size, 1,
        static_cast<ssize_t (XrdOssDF::*)(void*, off_t, size_t)>(&XrdOssDF::Read),
        buffer, offset, size);
}

virtual int Read(XrdSfsAio *aiop) override {
    aiop->Result = this->Read((char *)aiop->sfsAio.aio_buf,
                                   aiop->sfsAio.aio_offset,
                                   aiop->sfsAio.aio_nbytes);
    aiop->doneRead();
    return 0;
}

virtual ssize_t ReadV(XrdOucIOVec *readV, int rdvcnt) override {
    off_t sum = 0;
    for (int i = 0; i < rdvcnt; ++i) {
        sum += readV[i].size;
    }
    return DoThrottle(sum, rdvcnt, &XrdOssDF::ReadV, readV, rdvcnt);
}


virtual ssize_t Write(const void* buffer, off_t offset, size_t size) override {
    return DoThrottle(size, 1,
        static_cast<ssize_t (XrdOssDF::*)(const void*, off_t, size_t)>(&XrdOssDF::Write),
        buffer, offset, size);
}

virtual int Write(XrdSfsAio *aiop) override {
    aiop->Result = this->Write((char *)aiop->sfsAio.aio_buf,
                                    aiop->sfsAio.aio_offset,
                                    aiop->sfsAio.aio_nbytes);
    aiop->doneWrite();
    return 0;
}

private:

    template <class Fn, class... Args>
    int DoThrottle(size_t rdlen, size_t ops, Fn &&fn, Args &&... args) {
        m_throttle.Apply(rdlen, ops, m_uid);
        bool ok = true;
        XrdThrottleTimer timer = m_throttle.StartIOTimer(m_uid, ok);
        if (!ok) {
            TRACE(DEBUG, "Throttling in progress");
            return -EMFILE;
        }
        return std::invoke(fn, wrapDF, std::forward<Args>(args)...);
    }

    XrdSysError *m_log{nullptr};
    XrdThrottleManager &m_throttle;
    XrdOucTrace *m_trace{nullptr};
    std::unique_ptr<XrdOssDF> m_wrapped;
    std::string m_user;
    uint16_t m_uid;

    static constexpr char TraceID[] = "XrdThrottleFile";
};

class FileSystem final : public XrdOssWrapper {
public:
    FileSystem(XrdOss *oss, XrdSysLogger *log, XrdOucEnv *envP)
        : XrdOssWrapper(*oss),
          m_env(envP),
          m_oss(oss),
          m_log(new XrdSysError(log)),
          m_trace(new XrdOucTrace(m_log.get())),
          m_throttle(m_log.get(), m_trace.get())
    {

        m_throttle.Init();
        if (envP)
        {
            auto gstream = reinterpret_cast<XrdXrootdGStream*>(envP->GetPtr("Throttle.gStream*"));
            m_log->Say("Config", "Throttle g-stream has", gstream ? "" : " NOT", " been configured via xrootd.mongstream directive");
            m_throttle.SetMonitor(gstream);
        }
    }

    int Configure(const std::string &config_filename) {
        XrdThrottle::Configuration config(*m_log, m_env);
        if (config.Configure(config_filename)) {
            m_log->Emsg("Config", "Unable to load configuration file", config_filename.c_str());
            return 1;
        }
        m_throttle.FromConfig(config);
        return 0;
    }

    virtual ~FileSystem() {}

    virtual XrdOssDF *newFile(const char *user = 0) override {
        std::unique_ptr<XrdOssDF> wrapped(wrapPI.newFile(user));
        return new File(std::move(wrapped), m_throttle, m_log.get(), m_trace.get());
    }

private:
    XrdOucEnv *m_env{nullptr};
    std::unique_ptr<XrdOss> m_oss;
    std::unique_ptr<XrdSysError> m_log{nullptr};
    std::unique_ptr<XrdOucTrace> m_trace{nullptr};
    XrdThrottleManager m_throttle;
};

} // namespace

extern "C" {

XrdOss *XrdOssAddStorageSystem2(XrdOss *curr_oss, XrdSysLogger *logger,
                                const char *config_fn, const char *parms,
                                XrdOucEnv *envP) {
    std::unique_ptr<FileSystem> fs(new FileSystem(curr_oss, logger, envP));
    if (fs->Configure(config_fn)) {
        XrdSysError(logger, "XrdThrottle").Say("Config", "Unable to load configuration file", config_fn);
        return nullptr;
    }
    // Note the throttle is set up as an OSS.
    // This will prevent the throttle from being layered on top of the OFS; to keep backward
    // compatibility with old configurations, we do not cause the server to fail.
    //
    // Originally, XrdThrottle was used as an OFS because the loadshed code required the ability
    // to redirect the client to a different server.  This is rarely (never?) used in practice.
    // By putting the throttle in the OSS, we benefit from the fact the OFS has first run the
    // authorization code and has made a user name available for fairshare of the throttle.
    envP->PutInt("XrdOssThrottle", 1);
    return fs.release();
}

XrdVERSIONINFO(XrdOssAddStorageSystem2, throttle);
    
} // extern "C"

