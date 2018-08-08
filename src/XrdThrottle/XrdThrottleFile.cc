
#include "XrdSfs/XrdSfsAio.hh"
#include "XrdSec/XrdSecEntity.hh"

#include "XrdThrottle.hh"

using namespace XrdThrottle;

#define DO_LOADSHED if (m_throttle.CheckLoadShed(m_loadshed)) \
{ \
   unsigned port; \
   std::string host; \
   m_throttle.PerformLoadShed(m_loadshed, host, port); \
   m_eroute.Emsg("File", "Performing load-shed for client", m_user.c_str()); \
   error.setErrInfo(port, host.c_str()); \
   return SFS_REDIRECT; \
}

#define DO_THROTTLE(amount) \
DO_LOADSHED \
m_throttle.Apply(amount, 1, m_uid); \
XrdThrottleTimer xtimer = m_throttle.StartIOTimer();

class ErrorSentry
{
public:
    ErrorSentry(XrdOucErrInfo &dst_err, XrdOucErrInfo &src_err, bool forOpen=false)
        : m_dst_err(dst_err), m_src_err(src_err)
    {
        unsigned long long cbArg;
        XrdOucEICB *cbVal = dst_err.getErrCB(cbArg);

        if (forOpen)
        {
            src_err.setUCap(dst_err.getUCap());
        }
        src_err.setErrCB(cbVal, cbArg);
    }

    ~ErrorSentry()
    {
        if (m_src_err.getErrInfo())
        {
            m_dst_err = m_src_err;
        }
        else
        {
            m_dst_err.Reset();
        }
    }

private:
    XrdOucErrInfo &m_dst_err;
    XrdOucErrInfo &m_src_err;
};

File::File(const char                     *user,
                 int                       monid,
                 unique_sfs_ptr            sfs,
                 XrdThrottleManager       &throttle,
                 XrdSysError              &eroute)
   : XrdSfsFile(user, monid),
#if __cplusplus >= 201103L
     m_sfs(std::move(sfs)), // Guaranteed to be non-null by FileSystem::newFile
#else
     m_sfs(sfs),
#endif
     m_uid(0),
     m_user(user),
     m_throttle(throttle),
     m_eroute(eroute)
{}

File::~File()
{}

int
File::open(const char                *fileName,
                 XrdSfsFileOpenMode   openMode,
                 mode_t               createMode,
           const XrdSecEntity        *client,
           const char                *opaque)
{
   m_uid = XrdThrottleManager::GetUid(client->name);
   m_throttle.PrepLoadShed(opaque, m_loadshed);
   ErrorSentry sentry(error, m_sfs->error, true);
   return m_sfs->open(fileName, openMode, createMode, client, opaque);
}

int
File::close()
{
   ErrorSentry sentry(error, m_sfs->error);
   return m_sfs->close();
}

int
File::fctl(const int               cmd,
           const char             *args,
                 XrdOucErrInfo    &out_error)
{
   ErrorSentry sentry(error, m_sfs->error);
   // Disable sendfile
   if (cmd == SFS_FCTL_GETFD)
   {
      error.setErrInfo(ENOTSUP, "Sendfile not supported by throttle plugin.");
      return SFS_ERROR;
   }
   // If out_error is aliased to this->error, then the ErrorSentry destructor will clobber
   // the out_error with the contents of m_sfs->error, resulting in an incorrect state.
   // Instead, we pass m_sfs->error as the argument to fctl.  This way, the underlying
   // m_sfs also sees the aliased behavior, more closely mimicking the case where the
   // chained SFS doesn't exist.
   else return m_sfs->fctl(cmd, args, &error == &out_error ? m_sfs->error : out_error);
}

const char *
File::FName()
{
   ErrorSentry sentry(error, m_sfs->error);
   return m_sfs->FName();
}

int
File::getMmap(void **Addr, off_t &Size)
{  // We cannot monitor mmap-based reads, so we disable them.
   error.setErrInfo(ENOTSUP, "Mmap not supported by throttle plugin.");
   return SFS_ERROR;
}

int
File::read(XrdSfsFileOffset   fileOffset,
           XrdSfsXferSize     amount)
{
   DO_THROTTLE(amount)
   ErrorSentry sentry(error, m_sfs->error);
   return m_sfs->read(fileOffset, amount);
}

XrdSfsXferSize
File::read(XrdSfsFileOffset   fileOffset,
           char              *buffer,
           XrdSfsXferSize     buffer_size)
{
   DO_THROTTLE(buffer_size);
   ErrorSentry sentry(error, m_sfs->error);
   return m_sfs->read(fileOffset, buffer, buffer_size);
}

int
File::read(XrdSfsAio *aioparm)
{  // We disable all AIO-based reads.
   aioparm->Result = this->read((XrdSfsFileOffset)aioparm->sfsAio.aio_offset,
                                          (char *)aioparm->sfsAio.aio_buf,
                                  (XrdSfsXferSize)aioparm->sfsAio.aio_nbytes);
   aioparm->doneRead();
   ErrorSentry sentry(error, m_sfs->error);
   return SFS_OK;
}

XrdSfsXferSize
File::write(      XrdSfsFileOffset   fileOffset,
            const char              *buffer,
                  XrdSfsXferSize     buffer_size)
{
   DO_THROTTLE(buffer_size);
   ErrorSentry sentry(error, m_sfs->error);
   return m_sfs->write(fileOffset, buffer, buffer_size);
}

int
File::write(XrdSfsAio *aioparm)
{
   aioparm->Result = this->write((XrdSfsFileOffset)aioparm->sfsAio.aio_offset,
                                           (char *)aioparm->sfsAio.aio_buf,
                                   (XrdSfsXferSize)aioparm->sfsAio.aio_nbytes);
   aioparm->doneRead();
   ErrorSentry sentry(error, m_sfs->error);
   return SFS_OK;
}

int
File::sync()
{
   ErrorSentry sentry(error, m_sfs->error);
   return m_sfs->sync();
}

int
File::sync(XrdSfsAio *aiop)
{
   aiop->Result = this->sync();
   aiop->doneWrite();
   ErrorSentry sentry(error, m_sfs->error);
   return m_sfs->sync(aiop);
}

int
File::stat(struct stat *buf)
{
   ErrorSentry sentry(error, m_sfs->error);
   return m_sfs->stat(buf);
}

int
File::truncate(XrdSfsFileOffset   fileOffset)
{
   ErrorSentry sentry(error, m_sfs->error);
   return m_sfs->truncate(fileOffset);
}

int
File::getCXinfo(char cxtype[4], int &cxrsz)
{
   ErrorSentry sentry(error, m_sfs->error);
   return m_sfs->getCXinfo(cxtype, cxrsz);
}

int
File::SendData(XrdSfsDio         *sfDio,
               XrdSfsFileOffset   offset,
               XrdSfsXferSize     size)
{
   ErrorSentry sentry(error, m_sfs->error);
   DO_THROTTLE(size);
   return m_sfs->SendData(sfDio, offset, size);
}

