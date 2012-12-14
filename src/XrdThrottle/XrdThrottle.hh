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

class FileSystem;

class File : public XrdSfsFile {

friend class FileSystem;

public:

   int
   open(const char                *fileName,
              XrdSfsFileOpenMode   openMode,
              mode_t               createMode,
        const XrdSecEntity        *client,
        const char                *opaque = 0);

   int
   close();

   int
   fctl(const int               cmd,
        const char             *args,
              XrdOucErrInfo    &out_error);

   const char *
   FName();

   int
   getMmap(void **Addr, off_t &Size);

   int
   read(XrdSfsFileOffset   fileOffset,   // Preread only
        XrdSfsXferSize     amount);

   XrdSfsXferSize
   read(XrdSfsFileOffset   fileOffset,
        char              *buffer,
        XrdSfsXferSize     buffer_size);

   int
   read(XrdSfsAio *aioparm);

   XrdSfsXferSize
   write(XrdSfsFileOffset   fileOffset,
         const char        *buffer,
         XrdSfsXferSize     buffer_size);

   int
   write(XrdSfsAio *aioparm);

   int
   sync();

   int
   sync(XrdSfsAio *aiop);

   int
   stat(struct stat *buf);

   int
   truncate(XrdSfsFileOffset   fileOffset);

   int
   getCXinfo(char cxtype[4], int &cxrsz);

private:
   File(const char *user, int monid, std::auto_ptr<XrdSfsFile>);

   virtual
   ~File();

   std::auto_ptr<XrdSfsFile> m_sfs;
   int m_uid; // A unique identifier for this user; has no meaning except for the fairshare.

};

class FileSystem : public XrdSfsFileSystem
{

friend XrdSfsFileSystem * XrdSfsGetFileSystem_Internal(XrdSfsFileSystem *, XrdSysLogger *, const char *);

public:

   XrdSfsDirectory *
   newDir(char *user=0, int monid=0);

   XrdSfsFile *
   newFile(char *user=0, int monid=0);

   int
   chmod(const char             *Name,
               XrdSfsMode        Mode,
               XrdOucErrInfo    &out_error,
         const XrdSecEntity     *client,
         const char             *opaque = 0);

   int
   exists(const char                *fileName,
                XrdSfsFileExistence &exists_flag,
                XrdOucErrInfo       &out_error,
          const XrdSecEntity        *client,
          const char                *opaque = 0);

   int
   fsctl(const int               cmd,
         const char             *args,
               XrdOucErrInfo    &out_error,
         const XrdSecEntity     *client);

   int
   getStats(char *buff, int blen);

   const char *
   getVersion();

   int
   mkdir(const char             *dirName,
               XrdSfsMode        Mode,
               XrdOucErrInfo    &out_error,
         const XrdSecEntity     *client,
         const char             *opaque = 0);

   int
   prepare(      XrdSfsPrep       &pargs,
                 XrdOucErrInfo    &out_error,
           const XrdSecEntity     *client = 0);

   int
   rem(const char             *path,
             XrdOucErrInfo    &out_error,
       const XrdSecEntity     *client,
       const char             *info = 0);

   int
   remdir(const char             *dirName,
                XrdOucErrInfo    &out_error,
          const XrdSecEntity     *client,
          const char             *info = 0);

   int
   rename(const char             *oldFileName,
          const char             *newFileName,
                XrdOucErrInfo    &out_error,
          const XrdSecEntity     *client,
          const char             *infoO = 0,
          const char             *infoN = 0);

   int
   stat(const char             *Name,
              struct stat      *buf,
              XrdOucErrInfo    &out_error,
        const XrdSecEntity     *client,
        const char             *opaque = 0);

   int
   stat(const char             *Name,
              mode_t           &mode,
              XrdOucErrInfo    &out_error,
        const XrdSecEntity     *client,
        const char             *opaque = 0);

   int
   truncate(const char             *Name,
                  XrdSfsFileOffset fileOffset,
                  XrdOucErrInfo    &out_error,
            const XrdSecEntity     *client = 0,
            const char             *opaque = 0);

   virtual int
   Configure(XrdSysError &);

private:
   static void
   Initialize(      FileSystem      *&fs,
                    XrdSfsFileSystem *native_fs,
                    XrdSysLogger     *lp,
              const char             *config_file);

   FileSystem();

   virtual
  ~FileSystem();

   int
   xthrottle(XrdOucStream &Config);

   int
   xloadshed(XrdOucStream &Config);

   int
   xtrace(XrdOucStream &Config);

   static FileSystem  *m_instance;
   XrdSysError         m_eroute;
   XrdOucTrace         m_trace;
   std::string         m_config_file;
   XrdSfsFileSystem   &m_sfs;
   XrdSfsFileSystem   *m_sfs_ptr;
   bool                m_initialized;
   XrdThrottleManager  m_throttle;
   XrdVersionInfo     *myVersion;

};

}

#endif

