
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
   XrdSfsFile * chain_file = m_sfs_ptr->newFile(user, monid);
   if (chain_file)
   {
      unique_sfs_ptr chain_file_ptr(chain_file);
      // We should really be giving out shared_ptrs to m_throttle, but alas, no boost.
#if __cplusplus >= 201103L
      return static_cast<XrdSfsFile*>(new File(user, monid, std::move(chain_file_ptr), m_throttle, m_eroute));
#else
      return static_cast<XrdSfsFile*>(new File(user, monid, chain_file_ptr, m_throttle, m_eroute));
#endif
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
   return m_sfs_ptr->chmod(Name, Mode, out_error, client, opaque);
}

int
FileSystem::exists(const char                *fileName,
                         XrdSfsFileExistence &exists_flag,
                         XrdOucErrInfo       &out_error,
                   const XrdSecEntity        *client,
                   const char                *opaque)
{
   return m_sfs_ptr->exists(fileName, exists_flag, out_error, client, opaque);
}

int
FileSystem::fsctl(const int               cmd,
                  const char             *args,
                        XrdOucErrInfo    &out_error,
                  const XrdSecEntity     *client)
{
   return m_sfs_ptr->fsctl(cmd, args, out_error, client);
}

int
FileSystem::getStats(char *buff,
                     int   blen)
{
   return m_sfs_ptr->getStats(buff, blen);
}

const char *
FileSystem::getVersion()
{
   return XrdVERSION;
}

int
FileSystem::mkdir(const char             *dirName,
                        XrdSfsMode        Mode,
                        XrdOucErrInfo    &out_error,
                  const XrdSecEntity     *client,
                  const char             *opaque)
{
   return m_sfs_ptr->mkdir(dirName, Mode, out_error, client, opaque);
}

int
FileSystem::prepare(      XrdSfsPrep       &pargs,
                          XrdOucErrInfo    &out_error,
                    const XrdSecEntity     *client)
{
   return m_sfs_ptr->prepare(pargs, out_error, client);
}

int
FileSystem::rem(const char             *path,
                      XrdOucErrInfo    &out_error,
                const XrdSecEntity     *client,
                const char             *info)
{
   return m_sfs_ptr->rem(path, out_error, client, info);
}

int
FileSystem::remdir(const char             *dirName,
                         XrdOucErrInfo    &out_error,
                   const XrdSecEntity     *client,
                   const char             *info)
{
   return m_sfs_ptr->remdir(dirName, out_error, client, info);
}

int
FileSystem::rename(const char             *oldFileName,
                   const char             *newFileName,
                         XrdOucErrInfo    &out_error,
                   const XrdSecEntity     *client,
                   const char             *infoO,
                   const char             *infoN)
{
   return m_sfs_ptr->rename(oldFileName, newFileName, out_error, client, infoO, infoN);
}

int
FileSystem::stat(const char             *Name,
                       struct stat      *buf,
                       XrdOucErrInfo    &out_error,
                 const XrdSecEntity     *client,
                 const char             *opaque)
{
   return m_sfs_ptr->stat(Name, buf, out_error, client, opaque);
}

int
FileSystem::stat(const char             *Name,
                       mode_t           &mode,
                       XrdOucErrInfo    &out_error,
                 const XrdSecEntity     *client,
                 const char             *opaque)
{
   return m_sfs_ptr->stat(Name, mode, out_error, client, opaque);
}

int
FileSystem::truncate(const char             *Name,
                           XrdSfsFileOffset fileOffset,
                           XrdOucErrInfo    &out_error,
                     const XrdSecEntity     *client,
                     const char             *opaque)
{
   return m_sfs_ptr->truncate(Name, fileOffset, out_error, client, opaque);
}

