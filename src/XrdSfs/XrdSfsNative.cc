/******************************************************************************/
/*                                                                            */
/*                       X r d X f s N a t i v e . c c                        */
/*                                                                            */
/* (C) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*      All Rights Reserved. See XrdVersion.cc for complete License Terms     */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*               DE-AC03-76-SFO0515 with the Deprtment of Energy              */
/******************************************************************************/

//          $Id$

const char *XrdSfsNativeCVSID = "$Id$";

#include "Experiment/Experiment.hh"

#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <iostream.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucLogger.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include "XrdSfs/XrdSfsNative.hh"
#include "XrdSec/XrdSecInterface.hh"

#ifdef AIX
#include <sys/mode.h>
#endif

/******************************************************************************/
/*       O S   D i r e c t o r y   H a n d l i n g   I n t e r f a c e        */
/******************************************************************************/

#ifndef S_IAMB
#define S_IAMB  0x1FF
#endif

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
XrdOucError    *XrdSfsNative::eDest;

XrdSfsAIO      *XrdSfsAIOFirst = 0;
XrdSfsAIO      *XrdSfsAIOLast  = 0;

XrdOucMutex     XrdSfsAIOMutex;
XrdOucSemaphore XrdSfsAIOQueue;

/******************************************************************************/
/*            U n i x   F i l e   S y s t e m   I n t e r f a c e             */
/******************************************************************************/

class XrdSfsUFS
{
public:

static int Chmod(const char *fn, mode_t mode) {return chmod(fn, mode);}

static int Close(int fd) {return close(fd);}

static int Mkdir(const char *fn, mode_t mode) {return mkdir(fn, mode);}

static int Open(const char *path, int oflag, mode_t omode)
               {return open(path, oflag, omode);}

static int Rem(const char *fn) {return unlink(fn);}

static int Remdir(const char *fn) {return rmdir(fn);}

static int Rename(const char *ofn, const char *nfn) {return rename(ofn, nfn);}

static int Statfd(int fd, struct stat *buf) {return  fstat(fd, buf);}

static int Statfn(const char *fn, struct stat *buf) {return stat(fn, buf);}
};
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdSfsNative::XrdSfsNative(XrdOucError *ep)
{
  eDest = ep;
}
  
/******************************************************************************/
/*                         G e t F i l e S y s t e m                          */
/******************************************************************************/
  
XrdSfsFileSystem *XrdSfsGetFileSystem(XrdSfsFileSystem *native_fs, XrdOucLogger *lp)
{
 static XrdOucError  Eroute(lp, "XrdSfs");
 static XrdSfsNative myFS(&Eroute);

 Eroute.Say(0, (char *)"(c) 2003 Stanford University/SLAC "
                       "sfs (Standard File System) v 9.0n");

 return &myFS;
}

/******************************************************************************/
/*           D i r e c t o r y   O b j e c t   I n t e r f a c e s            */
/******************************************************************************/
/******************************************************************************/
/*                                  o p e n                                   */
/******************************************************************************/
  
int XrdSfsNativeDirectory::open(const char              *dir_path, // In
                                const XrdSecClientName  *client)   // In
/*
  Function: Open the directory `path' and prepare for reading.

  Input:    path      - The fully qualified name of the directory to open.
            cred      - Authentication credentials, if any.

  Output:   Returns SFS_OK upon success, otherwise SFS_ERROR.
*/
{
   static const char *epname = "opendir";

// Verify that this object is not already associated with an open directory
//
     if (dh) return
        XrdSfsNative::Emsg(epname, error, EADDRINUSE, 
                             "opening directory", dir_path);

// Set up values for this directory object
//
   ateof = 0;
   fname = strdup(dir_path);

// Open the directory and get it's id
//
     if (!(dh = opendir(dir_path))) return
        XrdSfsNative::Emsg(epname,error,errno,"opening directory",dir_path);

// All done
//
   return SFS_OK;
}

/******************************************************************************/
/*                             n e x t E n t r y                              */
/******************************************************************************/

const char *XrdSfsNativeDirectory::nextEntry()
/*
  Function: Read the next directory entry.

  Input:    None.

  Output:   Upon success, returns the contents of the next directory entry as
            a null terminated string. Returns a null pointer upon EOF or an
            error. To differentiate the two cases, getErrorInfo will return
            0 upon EOF and an actual error code (i.e., not 0) on error.
*/
{
    static const char *epname = "nextEntry";
    struct dirent *rp;
    int retc;

// Lock the direcrtory and do any required tracing
//
   if (!dh) 
      {XrdSfsNative::Emsg(epname,error,EBADFD,"reading directory",fname);
       return (const char *)0;
      }

// Check if we are at EOF (once there we stay there)
//
   if (ateof) return (const char *)0;

// Read the next directory entry
//
   errno = 0;
   if (retc = readdir_r(dh, d_pnt, &rp))
      {if (retc && errno != 0)
          XrdSfsNative::Emsg(epname,error,retc,"reading directory",fname);
       d_pnt->d_name[0] = '\0';
       return (const char *)0;
      }

// Check if we have reached end of file
//
   if (retc || !rp || !d_pnt->d_name[0])
      {ateof = 1;
       error.clear();
       return (const char *)0;
      }

// Return the actual entry
//
   return (const char *)(d_pnt->d_name);
}

/******************************************************************************/
/*                                 c l o s e                                  */
/******************************************************************************/
  
int XrdSfsNativeDirectory::close()
/*
  Function: Close the directory object.

  Input:    cred       - Authentication credentials, if any.

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   static const char *epname = "closedir";

// Release the handle
//
    if (dh && closedir(dh))
       {XrdSfsNative::Emsg(epname, error, errno, "closing directory", fname);
        return SFS_ERROR;
       }

// Do some clean-up
//
   if (fname) free(fname);
   dh = (DIR *)0; 
   return SFS_OK;
}

/******************************************************************************/
/*                F i l e   O b j e c t   I n t e r f a c e s                 */
/******************************************************************************/
/******************************************************************************/
/*                                  o p e n                                   */
/******************************************************************************/

int XrdSfsNativeFile::open(const char          *path,      // In
                           XrdSfsFileOpenMode   open_mode, // In
                           mode_t               Mode,      // In
                     const XrdSecClientName    *client,    // In
                     const char                *info)      // In
/*
  Function: Open the file `path' in the mode indicated by `open_mode'.  

  Input:    path      - The fully qualified name of the file to open.
            open_mode - One of the following flag values:
                        SFS_O_RDONLY - Open file for reading.
                        SFS_O_WRONLY - Open file for writing.
                        SFS_O_RDWR   - Open file for update
                        SFS_O_CREAT  - Create the file open in RDWR mode
                        SFS_O_TRUNC  - Trunc  the file open in RDWR mode
            file_mode - The Posix access mode bits to be assigned to the file.
                        These bits correspond to the standard Unix permission
                        bits (e.g., 744 == "rwxr--r--"). This parameter is
                        ignored unless open_mode = SFS_O_CREAT.
            client    - Authentication credentials, if any.
            info      - Opaque information to be used as seen fit.

  Output:   Returns OOSS_OK upon success, otherwise SFS_ERROR is returned.
*/
{
   static const char *epname = "open";
   char *opname;
   mode_t acc_mode = Mode & S_IAMB;
   int open_flag = 0;

// Verify that this object is not already associated with an open file
//
   if (oh >= 0)
      return XrdSfsNative::Emsg(epname,error,EADDRINUSE,"opening file",path);
   fname = strdup(path);

// Set the actual open mode
//
   switch(open_mode & (SFS_O_RDONLY | SFS_O_WRONLY | SFS_O_RDWR))
   {
   case SFS_O_RDONLY: open_flag = O_RDONLY; break;
   case SFS_O_WRONLY: open_flag = O_WRONLY; break;
   case SFS_O_RDWR:   open_flag = O_RDWR;   break;
   default:           open_flag = O_RDONLY; break;
   }

// Prepare to create or open the file, as needed
//
   if (open_mode & SFS_O_CREAT)
      {open_flag  = O_RDWR | O_CREAT | O_EXCL;
       opname = (char *)"creating";
      } else if (open_mode & SFS_O_TRUNC)
                {open_flag  = O_RDWR | O_CREAT;
                 opname = (char *)"truncating";
                } else opname = (char *)"opening";

// Open the file.
//
   if ((oh = XrdSfsUFS::Open(path, open_flag, acc_mode)) < 0)
      return XrdSfsNative::Emsg(epname,error,errno,(const char *)opname,path);

// All done.
//
   return SFS_OK;
}

/******************************************************************************/
/*                                 c l o s e                                  */
/******************************************************************************/

int XrdSfsNativeFile::close()
/*
  Function: Close the file object.

  Input:    None

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   static const char *epname = "close";

// Release the handle and return
//
    if (oh >= 0  && XrdSfsUFS::Close(oh))
       return XrdSfsNative::Emsg(epname, error, errno, "closing", fname);
    oh = -1;
    if (fname) {free(fname); fname = 0;}
    return SFS_OK;
}

/******************************************************************************/
/*                                  r e a d                                   */
/******************************************************************************/

XrdSfsXferSize XrdSfsNativeFile::read(XrdSfsFileOffset  offset,    // In
                                      char             *buff,      // Out
                                      XrdSfsXferSize    blen)      // In
/*
  Function: Read `blen' bytes at `offset' into 'buff' and return the actual
            number of bytes read.

  Input:    offset    - The absolute byte offset at which to start the read.
            buff      - Address of the buffer in which to place the data.
            blen      - The size of the buffer. This is the maximum number
                        of bytes that will be read from 'fd'.

  Output:   Returns the number of bytes read upon success and SFS_ERROR o/w.
*/
{
   static const char *epname = "read";
   XrdSfsXferSize nbytes;

// Make sure the offset is not too large
//
#if _FILE_OFFSET_BITS!=64
   if (offset >  0x000000007fffffff)
      return XrdSfsNative::Emsg(epname, error, EFBIG, "reading", fname);
#endif

// Read the actual number of bytes
//
   do { nbytes = pread(oh, (void *)buff, (size_t)blen, (off64_t)offset); }
        while(nbytes < 0 && errno == EINTR);

   if (nbytes  < 0)
      return XrdSfsNative::Emsg(epname, error, errno, "reading", fname);

// Return number of bytes read
//
   return nbytes;
}
  
/******************************************************************************/
/*                              r e a d   A I O                               */
/******************************************************************************/
  
// In native mode, this reverts to synchronous I/O
//
int XrdSfsNativeFile::read(XrdSfsAIO *aioparm)
{
    if (aioparm->buffer)
       aioparm->errcode = ((aioparm->result =
                read(aioparm->offset, aioparm->buffer, aioparm->size)) < 0 ?
                aioparm->errcode = errno : 0);
       else {aioparm->result = aioparm->size; aioparm->errcode = 0;}
    aioparm->next = 0;

    XrdSfsAIOMutex.Lock();
    if (XrdSfsAIOLast)
       {XrdSfsAIOLast->next = aioparm; XrdSfsAIOLast = aioparm;}
       else XrdSfsAIOFirst = XrdSfsAIOLast = aioparm;
    XrdSfsAIOQueue.Post();
    XrdSfsAIOMutex.UnLock();

    return 0;
}

/******************************************************************************/
/*                                 w r i t e                                  */
/******************************************************************************/

XrdSfsXferSize XrdSfsNativeFile::write(XrdSfsFileOffset   offset,    // In
                                       const char        *buff,      // In
                                       XrdSfsXferSize     blen)      // In
/*
  Function: Write `blen' bytes at `offset' from 'buff' and return the actual
            number of bytes written.

  Input:    offset    - The absolute byte offset at which to start the write.
            buff      - Address of the buffer from which to get the data.
            blen      - The size of the buffer. This is the maximum number
                        of bytes that will be written to 'fd'.

  Output:   Returns the number of bytes written upon success and SFS_ERROR o/w.

  Notes:    An error return may be delayed until the next write(), close(), or
            sync() call.
*/
{
   static const char *epname = "write";
   XrdSfsXferSize nbytes;

// Make sure the offset is not too large
//
#if _FILE_OFFSET_BITS!=64
   if (offset >  0x000000007fffffff)
      return XrdSfsNative::Emsg(epname, error, EFBIG, "writing", fname);
#endif

// Write the requested bytes
//
   do { nbytes = pwrite(oh, (void *)buff, (size_t)blen, (off64_t)offset); }
        while(nbytes < 0 && errno == EINTR);

   if (nbytes  < 0)
      return XrdSfsNative::Emsg(epname, error, errno, "writing", fname);

// Return number of bytes written
//
   return nbytes;
}

/******************************************************************************/
/*                             w r i t e   A I O                              */
/******************************************************************************/
  
// In native mode, this reverts to synchronous I/O
//
int XrdSfsNativeFile::write(XrdSfsAIO *aioparm)
{
    aioparm->errcode = ((aioparm->result =
             write(aioparm->offset, aioparm->buffer, aioparm->size)) < 0 ?
             aioparm->errcode = errno : 0);
    aioparm->next = 0;

    XrdSfsAIOMutex.Lock();
    if (XrdSfsAIOLast)
       {XrdSfsAIOLast->next = aioparm; XrdSfsAIOLast = aioparm;}
       else XrdSfsAIOFirst = XrdSfsAIOLast = aioparm;
    XrdSfsAIOQueue.Post();
    XrdSfsAIOMutex.UnLock();

    return 0;
}
  
/******************************************************************************/
/*                               w a i t a i o                                */
/******************************************************************************/

XrdSfsAIO *XrdSfsNativeFile::waitaio()
{
  XrdSfsAIO *aiop = 0;

  do {XrdSfsAIOQueue.Wait();
      XrdSfsAIOMutex.Lock();
      if (aiop = XrdSfsAIOFirst)
         if (aiop == XrdSfsAIOLast) XrdSfsAIOFirst = XrdSfsAIOLast = 0;
            else XrdSfsAIOFirst = aiop->next;
      XrdSfsAIOMutex.UnLock();
     } while(!aiop);

  return aiop;
}
  
/******************************************************************************/
/*                                  s t a t                                   */
/******************************************************************************/

int XrdSfsNativeFile::stat(struct stat     *buf)         // Out
/*
  Function: Return file status information

  Input:    buf         - The stat structiure to hold the results

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   static const char *epname = "stat";

// Execute the function
//
   if (XrdSfsUFS::Statfd(oh, buf))
      return XrdSfsNative::Emsg(epname, error, errno, "stating", fname);

// All went well
//
   return SFS_OK;
}

/******************************************************************************/
/*                                  s y n c                                   */
/******************************************************************************/

int XrdSfsNativeFile::sync()
/*
  Function: Commit all unwritten bytes to physical media.

  Input:    None

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   static const char *epname = "sync";

// Perform the function
//
   if (fsync(oh))
      return XrdSfsNative::Emsg(epname,error,errno,"synchronizing",fname);

// All done
//
   return SFS_OK;
}

/******************************************************************************/
/*                              t r u n c a t e                               */
/******************************************************************************/

int XrdSfsNativeFile::truncate(XrdSfsFileOffset  flen)  // In
/*
  Function: Set the length of the file object to 'flen' bytes.

  Input:    flen      - The new size of the file.

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.

  Notes:    If 'flen' is smaller than the current size of the file, the file
            is made smaller and the data past 'flen' is discarded. If 'flen'
            is larger than the current size of the file, a hole is created
            (i.e., the file is logically extended by filling the extra bytes 
            with zeroes).
*/
{
   static const char *epname = "trunc";

// Make sure the offset is not too larg
//
#if _FILE_OFFSET_BITS!=64
   if (flen >  0x000000007fffffff)
      return XrdSfsNative::Emsg(epname, error, EFBIG, "truncating", fname);
#endif

// Perform the function
//
   if (ftruncate(oh, flen))
      return XrdSfsNative::Emsg(epname, error, errno, "truncating", fname);

// All done
//
   return SFS_OK;
}

/******************************************************************************/
/*         F i l e   S y s t e m   O b j e c t   I n t e r f a c e s          */
/******************************************************************************/
/******************************************************************************/
/*                    C r e a t e   F i l e   O b j e c t                     */
/******************************************************************************/
  
XrdSfsFile *XrdSfsNative::openFile(const char             *fileName,   // In
                                   XrdSfsFileOpenMode      openMode,   // In
                                   mode_t                  createMode, // In
                                   XrdOucErrInfo          &out_error,  // Out
                                   const XrdSecClientName *client,     // In
                                   const char             *opaque)     // In
/*
  Function: Create a file object and open it.

  Input:    See open() for full details.

  Output:   Return a pointer to a file object upon success; o/w a NULL pointer.
*/
{
   XrdSfsNativeFile *fp = new XrdSfsNativeFile;

// If we were able to create the file object open it, else indicate error
//
   if (!fp) out_error.setErrInfo(ENOMEM, "insufficient memory");
      else if (fp->open(fileName, openMode, createMode, client, opaque))
              {out_error = fp->error; delete fp; fp = 0;}

// Return a pointer to the file object or a NULL
//
   return fp;
}

/******************************************************************************/
/*               C r e a t e   D i r e c t o r y   O b j e c t                */
/******************************************************************************/
  
XrdSfsDirectory *XrdSfsNative::openDir(const char             *directoryPath,
                                       XrdOucErrInfo          &out_error,
                                       const XrdSecClientName *client)
/*
  Function: Create a directory object and open it.

  Input:    See open() for full details.

  Output:   A pointer to a directior object upon success; o/w a NULL pointer.
*/
{
   static const char *epname = "openDir";
   XrdSfsNativeDirectory *dp = new XrdSfsNativeDirectory;

// If we were able to create the directory object open it, else indicate error
//
   if (!dp) out_error.setErrInfo(ENOMEM, "insufficient memory");
      else if (dp->open(directoryPath, client))
              {out_error = dp->error; delete dp; dp = 0;}

// Return a pointer to the file object or a NULL
//
   return dp;
}

/******************************************************************************/
/*               M i s c e l l a n e o u s   F u n c t i o n s                */
/******************************************************************************/
/******************************************************************************/
/*                                 c h m o d                                  */
/******************************************************************************/

int XrdSfsNative::chmod(const char             *path,    // In
                              XrdSfsMode        Mode,    // In
                              XrdOucErrInfo    &error,   // Out
                        const XrdSecClientName *client)  // In
/*
  Function: Change the mode on a file or directory.

  Input:    path      - Is the fully qualified name of the file to be removed.
            einfo     - Error information object to hold error details.
            client    - Authentication credentials, if any.

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   static const char *epname = "chmod";
   mode_t acc_mode = Mode & S_IAMB;

// Perform the actual deletion
//
   if (XrdSfsUFS::Chmod(path, acc_mode) )
      return XrdSfsNative::Emsg(epname,error,errno,"changing mode on",path);

// All done
//
    return SFS_OK;
}
  
/******************************************************************************/
/*                                e x i s t s                                 */
/******************************************************************************/

int XrdSfsNative::exists(const char                *path,        // In
                               XrdSfsFileExistence &file_exists, // Out
                               XrdOucErrInfo       &error,       // Out
                         const XrdSecClientName    *client)      // In
/*
  Function: Determine if file 'path' actually exists.

  Input:    path        - Is the fully qualified name of the file to be tested.
            file_exists - Is the address of the variable to hold the status of
                          'path' when success is returned. The values may be:
                          XrdSfsFileExistsIsDirectory - file not found but path is valid.
                          XrdSfsFileExistsIsFile      - file found.
                          XrdSfsFileExistsIsNo        - neither file nor directory.
            einfo       - Error information object holding the details.
            client      - Authentication credentials, if any.

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.

  Notes:    When failure occurs, 'file_exists' is not modified.
*/
{
   static const char *epname = "exists";
   struct stat fstat;

// Now try to find the file or directory
//
   if (!XrdSfsUFS::Statfn(path, &fstat) )
      {     if (S_ISDIR(fstat.st_mode)) file_exists=XrdSfsFileExistIsDirectory;
       else if (S_ISREG(fstat.st_mode)) file_exists=XrdSfsFileExistIsFile;
       else                             file_exists=XrdSfsFileExistNo;
       return SFS_OK;
      }
   if (errno == ENOENT)
      {file_exists=XrdSfsFileExistNo;
       return SFS_OK;
      }

// An error occured, return the error info
//
   return XrdSfsNative::Emsg(epname, error, errno, "locating", path);
}

/******************************************************************************/
/*                                 m k d i r                                  */
/******************************************************************************/

int XrdSfsNative::mkdir(const char             *path,    // In
                              XrdSfsMode        Mode,    // In
                              XrdOucErrInfo    &error,   // Out
                        const XrdSecClientName *client)  // In
/*
  Function: Create a directory entry.

  Input:    path      - Is the fully qualified name of the file to be removed.
            einfo     - Error information object to hold error details.
            client    - Authentication credentials, if any.

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   static const char *epname = "mkdir";
   mode_t acc_mode = Mode & S_IAMB;

// Perform the actual deletion
//
   if (XrdSfsUFS::Mkdir(path, acc_mode) )
      return XrdSfsNative::Emsg(epname,error,errno,"creating directory",path);

// All done
//
    return SFS_OK;
}

/******************************************************************************/
/*                                   r e m                                    */
/******************************************************************************/
  
int XrdSfsNative::rem(const char             *path,    // In
                            XrdOucErrInfo    &error,   // Out
                      const XrdSecClientName *client)  // In
/*
  Function: Delete a file from the namespace.

  Input:    path      - Is the fully qualified name of the file to be removed.
            einfo     - Error information object to hold error details.
            client    - Authentication credentials, if any.

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   static const char *epname = "rem";

// Perform the actual deletion
//
    if (XrdSfsUFS::Rem(path) )
       return XrdSfsNative::Emsg(epname, error, errno, "removing", path);

// All done
//
    return SFS_OK;
}

/******************************************************************************/
/*                                r e m d i r                                 */
/******************************************************************************/

int XrdSfsNative::remdir(const char             *path,    // In
                               XrdOucErrInfo    &error,   // Out
                         const XrdSecClientName *client)  // In
/*
  Function: Delete a directory from the namespace.

  Input:    path      - Is the fully qualified name of the dir to be removed.
            einfo     - Error information object to hold error details.
            client    - Authentication credentials, if any.

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   static const char *epname = "remdir";

// Perform the actual deletion
//
    if (XrdSfsUFS::Remdir(path) )
       return XrdSfsNative::Emsg(epname, error, errno, "removing", path);

// All done
//
    return SFS_OK;
}

/******************************************************************************/
/*                                r e n a m e                                 */
/******************************************************************************/

int XrdSfsNative::rename(const char             *old_name,  // In
                         const char             *new_name,  // In
                               XrdOucErrInfo    &error,     //Out
                         const XrdSecClientName *client)    // In
/*
  Function: Renames a file/directory with name 'old_name' to 'new_name'.

  Input:    old_name  - Is the fully qualified name of the file to be renamed.
            new_name  - Is the fully qualified name that the file is to have.
            error     - Error information structure, if an error occurs.
            client    - Authentication credentials, if any.

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   static const char *epname = "rename";

// Perform actual rename operation
//
   if (XrdSfsUFS::Rename(old_name, new_name) )
      return XrdSfsNative::Emsg(epname, error, errno, "renaming", old_name);

// All done
//
   return SFS_OK;
}
  
/******************************************************************************/
/*                                  s t a t                                   */
/******************************************************************************/

int XrdSfsNative::stat(const char              *path,        // In
                             struct stat       *buf,         // Out
                             XrdOucErrInfo     &error,       // Out
                       const XrdSecClientName  *client)      // In
/*
  Function: Get info on 'path'.

  Input:    path        - Is the fully qualified name of the file to be tested.
            buf         - The stat structiure to hold the results
            error       - Error information object holding the details.
            client      - Authentication credentials, if any.

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   static const char *epname = "stat";

// Execute the function
//
   if (XrdSfsUFS::Statfn(path, buf) )
      return XrdSfsNative::Emsg(epname, error, errno, "stating", path);

// All went well
//
   return SFS_OK;
}

/******************************************************************************/
/*                                  E m s g                                   */
/******************************************************************************/

int XrdSfsNative::Emsg(const char    *pfx,    // Message prefix value
                       XrdOucErrInfo &einfo,  // Place to put text & error code
                       int            ecode,  // The error code
                       const char    *op,     // Operation being performed
                       const char    *target) // The target (e.g., fname)
{
    char *etext, buffer[SFS_MAX_ERROR_LEN];

// Get the reason for the error
//
   if (ecode < 0) ecode = -ecode;
   if (!(etext = strerror(ecode))) etext = (char *)"reason unknown";

// Format the error message
//
    snprintf(buffer,sizeof(buffer),"Error %d (%s) %s %s.", ecode,
             etext, op, target);

// Print it out if debugging is enabled
//
#ifndef NODEBUG
   eDest->Emsg(pfx, buffer);
#endif

// Place the error message in the error object and return
//
    einfo.setErrInfo(ecode, buffer);

    return SFS_ERROR;
}
