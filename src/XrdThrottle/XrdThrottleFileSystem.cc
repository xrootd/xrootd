
#include "XrdOfs/XrdOfs.hh"

#include "XrdThrottle/XrdThrottle.hh"

using namespace XrdThrottle;

/*
 * A whole ton of pass-through functions which chain to the underlying SFS.
 */

XrdSfsDirectory *
FileSystem::newDir(char *user,
                   int   monid)
{
   return (XrdSfsDirectory *)new XrdOfsDirectory(user, monid);
}

XrdSfsFile *
FileSystem::newFile(char *user,
                    int   monid)
{
   XrdSfsFile * chain_file = m_sfs.newFile(user, monid);
   if (chain_file)
   {
      std::auto_ptr<XrdSfsFile> chain_file_ptr(chain_file);
      // We should really be giving out shared_ptrs to m_throttle, but alas, no boost.
      return static_cast<XrdSfsFile*>(new File(user, monid, chain_file_ptr, m_throttle, m_eroute));
   }
   return NULL;
}

int
FileSystem::chmod(const char             *Name,
                        XrdSfsMode        Mode,
                        XrdOucErrInfo    &out_error,
                  const XrdSecEntity     *client,
                  const char             *opaque)
{
   return m_sfs.chmod(Name, Mode, out_error, client, opaque);
}

int
FileSystem::exists(const char                *fileName,
                         XrdSfsFileExistence &exists_flag,
                         XrdOucErrInfo       &out_error,
                   const XrdSecEntity        *client,
                   const char                *opaque)
{
   return m_sfs.exists(fileName, exists_flag, out_error, client, opaque);
}

int
FileSystem::fsctl(const int               cmd,
                  const char             *args,
                        XrdOucErrInfo    &out_error,
                  const XrdSecEntity     *client)
{
   return m_sfs.fsctl(cmd, args, out_error, client);
}

int
FileSystem::getStats(char *buff,
                     int   blen)
{
   return m_sfs.getStats(buff, blen);
}

const char *
FileSystem::getVersion()
{
   return m_sfs.getVersion();
}

int
FileSystem::mkdir(const char             *dirName,
                        XrdSfsMode        Mode,
                        XrdOucErrInfo    &out_error,
                  const XrdSecEntity     *client,
                  const char             *opaque)
{
   return m_sfs.mkdir(dirName, Mode, out_error, client, opaque);
}

int
FileSystem::prepare(      XrdSfsPrep       &pargs,
                          XrdOucErrInfo    &out_error,
                    const XrdSecEntity     *client)
{
   return m_sfs.prepare(pargs, out_error, client);
}

int
FileSystem::rem(const char             *path,
                      XrdOucErrInfo    &out_error,
                const XrdSecEntity     *client,
                const char             *info)
{
   return m_sfs.rem(path, out_error, client, info);
}

int
FileSystem::remdir(const char             *dirName,
                         XrdOucErrInfo    &out_error,
                   const XrdSecEntity     *client,
                   const char             *info)
{
   return m_sfs.remdir(dirName, out_error, client, info);
}

int
FileSystem::rename(const char             *oldFileName,
                   const char             *newFileName,
                         XrdOucErrInfo    &out_error,
                   const XrdSecEntity     *client,
                   const char             *infoO,
                   const char             *infoN)
{
   return m_sfs.rename(oldFileName, newFileName, out_error, client, infoO, infoN);
}

int
FileSystem::stat(const char             *Name,
                       struct stat      *buf,
                       XrdOucErrInfo    &out_error,
                 const XrdSecEntity     *client,
                 const char             *opaque)
{
   return m_sfs.stat(Name, buf, out_error, client, opaque);
}

int
FileSystem::stat(const char             *Name,
                       mode_t           &mode,
                       XrdOucErrInfo    &out_error,
                 const XrdSecEntity     *client,
                 const char             *opaque)
{
   return m_sfs.stat(Name, mode, out_error, client, opaque);
}

int
FileSystem::truncate(const char             *Name,
                           XrdSfsFileOffset fileOffset,
                           XrdOucErrInfo    &out_error,
                     const XrdSecEntity     *client,
                     const char             *opaque)
{
   return m_sfs.truncate(Name, fileOffset, out_error, client, opaque);
}

