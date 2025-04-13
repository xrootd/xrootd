#ifndef __XRDTHROTTLE_H_
#define __XRDTHROTTLE_H_

#include <memory>
#include <string>

#include "XrdVersion.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSfs/XrdSfsInterface.hh"

#include "XrdThrottle/XrdThrottleTrace.hh"
#include "XrdThrottle/XrdThrottleManager.hh"

class XrdSysLogger;
class XrdOucStream;


namespace XrdThrottle {

#if __cplusplus >= 201103L
typedef std::unique_ptr<XrdSfsFile> unique_sfs_ptr;
#else
typedef std::auto_ptr<XrdSfsFile> unique_sfs_ptr;
#endif

class FileSystem;

class File final : public XrdSfsFile {

friend class FileSystem;

public:

   virtual int
   open(const char                *fileName,
              XrdSfsFileOpenMode   openMode,
              mode_t               createMode,
        const XrdSecEntity        *client,
        const char                *opaque = 0) override;

   virtual int
   close() override;

   virtual int
   checkpoint(cpAct act, struct iov *range=0, int n=0) override;

   using XrdSfsFile::fctl;
   virtual int
   fctl(const int               cmd,
        const char             *args,
              XrdOucErrInfo    &out_error) override;

   virtual const char *
   FName() override;

   virtual int
   getMmap(void **Addr, off_t &Size) override;

   virtual XrdSfsXferSize
   pgRead(XrdSfsFileOffset   offset,
          char              *buffer,
          XrdSfsXferSize     rdlen,
          uint32_t          *csvec,
          uint64_t           opts=0) override;

   virtual XrdSfsXferSize
   pgRead(XrdSfsAio *aioparm, uint64_t opts=0) override;

   virtual XrdSfsXferSize
   pgWrite(XrdSfsFileOffset   offset,
           char              *buffer,
           XrdSfsXferSize     rdlen,
           uint32_t          *csvec,
           uint64_t           opts=0) override;

   virtual XrdSfsXferSize
   pgWrite(XrdSfsAio *aioparm, uint64_t opts=0) override;

   virtual int
   read(XrdSfsFileOffset   fileOffset,   // Preread only
        XrdSfsXferSize     amount) override;

   virtual XrdSfsXferSize
   read(XrdSfsFileOffset   fileOffset,
        char              *buffer,
        XrdSfsXferSize     buffer_size) override;

   virtual int
   read(XrdSfsAio *aioparm) override;

   virtual XrdSfsXferSize
   write(XrdSfsFileOffset   fileOffset,
         const char        *buffer,
         XrdSfsXferSize     buffer_size) override;

   virtual int
   write(XrdSfsAio *aioparm) override;

   virtual int
   sync() override;

   virtual int
   sync(XrdSfsAio *aiop) override;

   virtual int
   stat(struct stat *buf) override;

   virtual int
   truncate(XrdSfsFileOffset   fileOffset) override;

   virtual int
   getCXinfo(char cxtype[4], int &cxrsz) override;

   virtual int
   SendData(XrdSfsDio         *sfDio,
            XrdSfsFileOffset   offset,
            XrdSfsXferSize     size) override;

private:
   File(const char *, unique_sfs_ptr, XrdThrottleManager &, XrdSysError &);

   virtual
   ~File();

   bool m_is_open{false};
   unique_sfs_ptr m_sfs;
   int m_uid; // A unique identifier for this user; has no meaning except for the fairshare.
   std::string m_loadshed;
   std::string m_connection_id; // Identity for the connection; may or may authenticated
   std::string m_user;
   XrdThrottleManager &m_throttle;
   XrdSysError &m_eroute;
};

class FileSystem final : public XrdSfsFileSystem
{

friend XrdSfsFileSystem * XrdSfsGetFileSystem_Internal(XrdSfsFileSystem *, XrdSysLogger *, const char *, XrdOucEnv *);

public:

   virtual XrdSfsDirectory *
   newDir(char *user=0, int monid=0) override;

   virtual XrdSfsFile *
   newFile(char *user=0, int monid=0) override;

   virtual int
   chksum(      csFunc         Func,
          const char          *csName,
          const char          *path,
                XrdOucErrInfo &eInfo,
          const XrdSecEntity  *client = 0,
          const char          *opaque = 0) override;

   virtual int
   chmod(const char             *Name,
               XrdSfsMode        Mode,
               XrdOucErrInfo    &out_error,
         const XrdSecEntity     *client,
         const char             *opaque = 0) override;

   virtual void
   Connect(const XrdSecEntity     *client = 0) override;

   virtual void
   Disc(const XrdSecEntity   *client = 0) override;

   virtual void
   EnvInfo(XrdOucEnv *envP) override;

   virtual int
   exists(const char                *fileName,
                XrdSfsFileExistence &exists_flag,
                XrdOucErrInfo       &out_error,
          const XrdSecEntity        *client,
          const char                *opaque = 0) override;

   virtual int
   FAttr(      XrdSfsFACtl      *faReq,
               XrdOucErrInfo    &eInfo,
         const XrdSecEntity     *client = 0) override;

   virtual int
   fsctl(const int               cmd,
         const char             *args,
               XrdOucErrInfo    &out_error,
         const XrdSecEntity     *client) override;

   virtual int
   getChkPSize() override;

   virtual int
   getStats(char *buff, int blen) override;

   virtual const char *
   getVersion() override;

   virtual int
   gpFile(      gpfFunc          &gpAct,
                XrdSfsGPFile     &gpReq,
                XrdOucErrInfo    &eInfo,
          const XrdSecEntity     *client = 0) override;

   virtual int
   mkdir(const char             *dirName,
               XrdSfsMode        Mode,
               XrdOucErrInfo    &out_error,
         const XrdSecEntity     *client,
         const char             *opaque = 0) override;

   virtual int
   prepare(      XrdSfsPrep       &pargs,
                 XrdOucErrInfo    &out_error,
           const XrdSecEntity     *client = 0) override;

   virtual int
   rem(const char             *path,
             XrdOucErrInfo    &out_error,
       const XrdSecEntity     *client,
       const char             *info = 0) override;

   virtual int
   remdir(const char             *dirName,
                XrdOucErrInfo    &out_error,
          const XrdSecEntity     *client,
          const char             *info = 0) override;

   virtual int
   rename(const char             *oldFileName,
          const char             *newFileName,
                XrdOucErrInfo    &out_error,
          const XrdSecEntity     *client,
          const char             *infoO = 0,
          const char             *infoN = 0) override;

   virtual int
   stat(const char             *Name,
              struct stat      *buf,
              XrdOucErrInfo    &out_error,
        const XrdSecEntity     *client,
        const char             *opaque = 0) override;

   virtual int
   stat(const char             *Name,
              mode_t           &mode,
              XrdOucErrInfo    &out_error,
        const XrdSecEntity     *client,
        const char             *opaque = 0) override;

   virtual int
   truncate(const char             *Name,
                  XrdSfsFileOffset fileOffset,
                  XrdOucErrInfo    &out_error,
            const XrdSecEntity     *client = 0,
            const char             *opaque = 0) override;

private:
   static void
   Initialize(      FileSystem      *&fs,
                    XrdSfsFileSystem *native_fs,
                    XrdSysLogger     *lp,
              const char             *config_file,
                    XrdOucEnv        *envP);

   int
   Configure(XrdSysError &, XrdSfsFileSystem *native_fs, XrdOucEnv *envP);

   FileSystem();

   virtual
  ~FileSystem();

   int
   xthrottle(XrdOucStream &Config);

   int
   xloadshed(XrdOucStream &Config);

   int
   xtrace(XrdOucStream &Config);

   int
   xmaxopen(XrdOucStream &Config);

   int
   xmaxconn(XrdOucStream &Config);

   static FileSystem  *m_instance;
   XrdSysError         m_eroute;
   XrdOucTrace         m_trace;
   std::string         m_config_file;
   XrdSfsFileSystem   *m_sfs_ptr;
   bool                m_initialized;
   XrdThrottleManager  m_throttle;
   XrdVersionInfo     *myVersion;

};

}

#endif

