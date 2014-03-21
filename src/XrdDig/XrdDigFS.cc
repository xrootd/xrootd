/******************************************************************************/
/*                                                                            */
/*                           X r d D i g F S . c c                            */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*               DE-AC02-76-SFO0515 with the Deprtment of Energy              */
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

#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "XrdVersion.hh"

#include "XrdOuc/XrdOucStream.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdSfs/XrdSfsAio.hh"

#include "XrdDig/XrdDigConfig.hh"
#include "XrdDig/XrdDigFS.hh"

#ifdef AIX
#include <sys/mode.h>
#endif

/******************************************************************************/
/*                         L o c a l   D e f i n e s                          */
/******************************************************************************/

#ifdef __linux__
#define IS_PROC(x) !strncmp(x+SFS_LCLPLEN, "proc", 4) \
                   &&  (*(x+SFS_LCLPLEN+4) == '\0'||*(x+SFS_LCLPLEN+4) == '/')
#endif

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
namespace XrdDig
{
          XrdSysError *eDest;

   extern XrdDigConfig Config;
};

using namespace XrdDig;
  
/******************************************************************************/
/*            U n i x   F i l e   S y s t e m   I n t e r f a c e             */
/******************************************************************************/

class XrdDigUFS
{
public:

static int Close(int fd) {return close(fd);}

static int Open(const char *path, int oflag)  {return open(path, oflag);}

static int Statfd(int fd, struct stat *buf) {return  fstat(fd, buf);}

static int Statfn(const char *fn, struct stat *buf) {return stat(fn, buf);}

static int Statlk(const char *fn, struct stat *buf) {return lstat(fn, buf);}
};

/******************************************************************************/
/*                           X r d D i g G e t F S                            */
/******************************************************************************/
  
XrdSfsFileSystem *XrdDigGetFS(XrdSfsFileSystem *native_fs,
                              XrdSysLogger     *lp,
                              const char       *cFN,
                              const char       *parms)
{
 static XrdSysError  Eroute(lp, "XrdDig");
 static XrdDigFS     myFS;
 bool   isOK;

 Eroute.Say("Copr.  2013 Stanford University/SLAC dig file system");
 eDest = &Eroute;

// Configure
//
   eDest->Say("++++++ DigFS initialization started.");
   isOK = Config.Configure(cFN, parms);
   eDest->Say("------ DigFS initialization ",(isOK ? "completed." : "failed."));

// All done
//
   return (isOK ? &myFS : 0);
}

/******************************************************************************/
/*           D i r e c t o r y   O b j e c t   I n t e r f a c e s            */
/******************************************************************************/
/******************************************************************************/
/*                                  o p e n                                   */
/******************************************************************************/
  
int XrdDigDirectory::open(const char              *dir_path, // In
                          const XrdSecClientName  *client,   // In
                          const char              *info)     // In
/*
  Function: Open the directory `path' and prepare for reading.

  Input:    path      - The fully qualified name of the directory to open.
            cred      - Authentication credentials, if any.
            info      - Opaque information, if any.

  Output:   Returns SFS_OK upon success, otherwise SFS_ERROR.
*/
{
   static const char *epname = "opendir";
   int retc;

// Verify that this object is not already associated with an open directory
//
   if (dh || isBase) return XrdDigFS::Emsg(epname, error, EADDRINUSE,
                                           "open directory", dir_path);

// Check if we are trying to open the root to list it
//
   if (!strcmp(dir_path, SFS_LCLPRFX) || !strcmp(dir_path, SFS_LCLPRFY))
      {isBase = true;
       if ((dirFD = Config.GenAccess(client, dirent_full.aEnt, aESZ)) < 0)
          return XrdDigFS::Emsg(epname,error,EACCES,"open directory",dir_path);
       ateof = dirFD == 0;
       return SFS_OK;
      }

// Authorize this open and get actual file name to open
//
   if ( (retc = XrdDigFS::Validate(dir_path))
   ||  !(fname = Config.GenPath(retc, client, "opendir",
                                dir_path+SFS_LCLPLEN, XrdDigConfig::isDir)))
      return XrdDigFS::Emsg(epname,error,retc,"open directory",dir_path);

// Set up values for this directory object
//
   ateof = false;

// Open the directory and get it's id
//
   if (!(dh = opendir(fname)))
      {if (fname) {free(fname); fname = 0;}
       return XrdDigFS::Emsg(epname,error,errno,"open directory",dir_path);
      }

// Check if this is a reference to /proc (Linux only)
//
#ifdef __linux__
   if (IS_PROC(dir_path))
      {noTag =  *(dir_path+SFS_LCLPLEN+4) == 0
             || !strcmp(dir_path+SFS_LCLPLEN+4, "/");
       isProc = true;
       dirFD  = dirfd(dh);
      }
#endif

// All done
//
   return SFS_OK;
}

/******************************************************************************/
/*                             n e x t E n t r y                              */
/******************************************************************************/

const char *XrdDigDirectory::nextEntry()
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
   static const int wMask = ~(S_IWUSR | S_IWGRP | S_IWOTH);
   struct dirent *rp;
   int retc;

// Check for base listing
//
   if (isBase)
      {if (dirFD > 0) return dirent_full.aEnt[--dirFD];
       ateof = true;
       return (const char *)0;
      }

// Lock the direcrtory and do any required tracing
//
   if (!dh) 
      {XrdDigFS::Emsg(epname,error,EBADF,"read directory",fname);
       return (const char *)0;
      }

// Check if we are at EOF (once there we stay there)
//
   if (ateof) return (const char *)0;

// Read the next directory entry
//
do{errno = 0;
   if ((retc = readdir_r(dh, d_pnt, &rp)))
      {if (retc && errno != 0)
          XrdDigFS::Emsg(epname,error,retc,"read directory",fname);
       d_pnt->d_name[0] = '\0';
       return (const char *)0;
      }

// Check if we have reached end of file
//
   if (retc || !rp || !d_pnt->d_name[0])
      {ateof = true;
       error.clear();
       return (const char *)0;
      }

// If autostat wanted, do so here
//
   if (sBuff)
      {
#ifdef HAVE_FSTATAT
       int sFlags = (isProc ?  AT_SYMLINK_NOFOLLOW : 0);
       if (fstatat(dirFD, d_pnt->d_name, sBuff, sFlags)) continue;
       sBuff->st_mode = (sBuff->st_mode & wMask) | S_IRUSR;
#else
       char dPath[2048];
       snprintf(dPath, sizeof(dPath), "%s%s", fname, d_pnt->d_name);
       if (stat(dPath, sBuff)) continue;
       sBuff->st_mode = (sBuff->st_mode & wMask) | S_IRUSR;
#endif
      }

// We want to extend the directory entry information with symlink information
// if this is a symlink. This is only done for /proc (Linux only)
//
#ifdef __linux__
   if (isProc)
      {struct stat Stat, *sP = (sBuff ? sBuff : &Stat);
       char *dP;
       int n, rc;
       rc = (sBuff ? 0:fstatat(dirFD,d_pnt->d_name,&Stat,AT_SYMLINK_NOFOLLOW));
       if (!rc && !noTag && S_ISLNK(sP->st_mode))
          {n = strlen(d_pnt->d_name);
           dP = d_pnt->d_name + n + 4;
           n = sizeof(dirent_full.nbf) - (n + 8);
           if ((n = readlinkat(dirFD,d_pnt->d_name,dP,n)) < 0) strcpy(dP,"?");
              else *(dP+n) = 0;
           strncpy(dP-4, " -> ", 4);
          }
      }
#endif

// Return the actual entry
//
   return (const char *)(d_pnt->d_name);
  } while(1);
   return 0; // Keep compiler happy
}

/******************************************************************************/
/*                                 c l o s e                                  */
/******************************************************************************/
  
int XrdDigDirectory::close()
/*
  Function: Close the directory object.

  Input:    cred       - Authentication credentials, if any.

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   static const char *epname = "closedir";

// Release the handle
//
   sBuff = 0;
   if (dh && closedir(dh))
      {XrdDigFS::Emsg(epname, error, errno, "close directory", fname);
       return SFS_ERROR;
      }

// Do some clean-up
//
   if (fname) {free(fname); fname = 0;}
   dh = (DIR *)0; 
   isProc = isBase = false;
   return SFS_OK;
}

/******************************************************************************/
/*                F i l e   O b j e c t   I n t e r f a c e s                 */
/******************************************************************************/
/******************************************************************************/
/*                                  o p e n                                   */
/******************************************************************************/

int XrdDigFile::open(const char               *path,      // In
                           XrdSfsFileOpenMode  open_mode, // In
                           mode_t              Mode,      // In
                     const XrdSecClientName   *client,    // In
                     const char               *info)      // In
/*
  Function: Open the file `path' in the mode indicated by `open_mode'.  

  Input:    path      - The fully qualified name of the file to open.
            open_mode - One of the following flag values:
                        SFS_O_RDONLY - Open file for reading (only allowed)
            Mode      - Ignored.
            client    - Authentication credentials, if any.
            info      - Opaque information to be used as seen fit.

  Output:   Returns SFS_OK upon success, otherwise SFS_ERROR is returned.
*/
{
   static const char *epname = "open";
   int retc;
   struct stat buf;

// Verify that this object is not already associated with an open file
//
   if (oh >= 0)
      return XrdDigFS::Emsg(epname,error,EADDRINUSE,"open file",path);

// Allow only opens in readonly mode
//
   open_mode &= (SFS_O_RDONLY | SFS_O_WRONLY | SFS_O_RDWR | SFS_O_CREAT);
   if (open_mode && open_mode != SFS_O_RDONLY)
      return XrdDigFS::Emsg(epname,error,EROFS,"open file",path);

// Authorize this open and get actual file name to open
//
   if ( (retc = XrdDigFS::Validate(path))
   ||  !(fname = Config.GenPath(retc, client, "open",
                                path+SFS_LCLPLEN, XrdDigConfig::isFile)))
      return XrdDigFS::Emsg(epname,error,retc,"open file",path);

// Prohibit opening of a symlink in /proc (linux only)
//
#ifdef __linux__
   if (IS_PROC(path))
      {struct stat Stat;
       if (XrdDigUFS::Statlk(fname, &Stat)) retc = errno;
          else if (!S_ISREG(Stat.st_mode))  retc = EPERM;
                  else retc = 0;
       if (retc)
          {free(fname);
           return XrdDigFS::Emsg(epname, error, retc, "open proc file", path);
          }
       isProc = true;
      }
#endif


// Open the file and make sure it is a file
//
   if ((oh = XrdDigUFS::Open(fname, O_RDONLY)) >= 0)
      {do {retc = XrdDigUFS::Statfd(oh, &buf);} while(retc && errno == EINTR);
       if (!retc && !(buf.st_mode & S_IFREG))
          {XrdDigUFS::Close(oh);
           oh = (buf.st_mode & S_IFDIR ? -EISDIR : -ENOTBLK);
          }
      } else oh = -errno;

// All done.
//
   if (oh >= 0) return SFS_OK;
   if (fname) {free(fname); fname = 0;}
   return XrdDigFS::Emsg(epname,error,oh,"open file",path);
}

/******************************************************************************/
/*                                 c l o s e                                  */
/******************************************************************************/

int XrdDigFile::close()
/*
  Function: Close the file object.

  Input:    None

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   static const char *epname = "close";

// Release the handle and return
//
    if (oh >= 0  && XrdDigUFS::Close(oh))
       return XrdDigFS::Emsg(epname, error, errno, "close", fname);
    oh = -1;
    if (fname) {free(fname); fname = 0;}
    return SFS_OK;
}

/******************************************************************************/
/*                                  f c t l                                   */
/******************************************************************************/

int      XrdDigFile::fctl(const int      cmd,
                          const char    *args,
                          XrdOucErrInfo &out_error)
{
// See if we can do this
//
   if (cmd == SFS_FCTL_GETFD)
      {out_error.setErrCode(isProc ? -1 : oh);
       return SFS_OK;
      }

// We don't support this
//
   out_error.setErrInfo(EEXIST, "fctl operation not supported");
   return SFS_ERROR;
}
  
/******************************************************************************/
/*                                  r e a d                                   */
/******************************************************************************/

XrdSfsXferSize XrdDigFile::read(XrdSfsFileOffset  offset,    // In
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
      return XrdDigFS::Emsg(epname, error, EFBIG, "read", fname);
#endif

// Read the actual number of bytes
//
   do { nbytes = pread(oh, (void *)buff, (size_t)blen, (off_t)offset); }
        while(nbytes < 0 && errno == EINTR);

   if (nbytes  < 0)
      return XrdDigFS::Emsg(epname, error, errno, "read", fname);

// Return number of bytes read
//
   return nbytes;
}

/******************************************************************************/
/*                                  r e a d v                                 */
/******************************************************************************/

XrdSfsXferSize XrdDigFile::readv(XrdOucIOVec *readV,     // In
                                 int          readCount) // In
/*
  Function: Perform all the reads specified in the readV vector.

  Input:    readV     - A description of the reads to perform; includes the
                        absolute offset, the size of the read, and the buffer
                        to place the data into.
            readCount - The size of the readV vector.

  Output:   Returns the number of bytes read upon success and SFS_ERROR o/w.
            If the number of bytes read is less than requested, it is considered
            an error.
*/
{
   static const char *epname = "readv";
   XrdSfsXferSize nbytes = 0;
   ssize_t curCount;
   int i;

   for (i=0; i<int(readCount); i++)
     {do {curCount = pread(oh, (void *)readV[i].data, (size_t)readV[i].size, (off_t)readV[i].offset);}
           while(curCount < 0 && errno == EINTR);

      if (curCount != readV[i].size)
         {if (curCount > 0) errno = ESPIPE;
          return XrdDigFS::Emsg(epname, error, errno, "readv", fname);
         }
      nbytes += curCount;
     }

   return nbytes;
}

/******************************************************************************/
/*                              r e a d   A I O                               */
/******************************************************************************/
  
int XrdDigFile::read(XrdSfsAio *aiop)
{

// Execute this request in a synchronous fashion
//
   aiop->Result = this->read((XrdSfsFileOffset)aiop->sfsAio.aio_offset,
                                       (char *)aiop->sfsAio.aio_buf,
                               (XrdSfsXferSize)aiop->sfsAio.aio_nbytes);
   aiop->doneRead();
   return 0;
}
  
/******************************************************************************/
/*                                  s t a t                                   */
/******************************************************************************/

int XrdDigFile::stat(struct stat     *buf)         // Out
/*
  Function: Return file status information

  Input:    buf         - The stat structiure to hold the results

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   static const char *epname = "stat";
   static const int wMask = ~(S_IWUSR | S_IWGRP | S_IWOTH);

// Execute the function
//
   if (XrdDigUFS::Statfd(oh, buf))
      return XrdDigFS::Emsg(epname, error, errno, "stat", fname);

// Fixup size when stat is issued into /proc (linux only would set isProc)
//
   if (isProc && !buf->st_size && S_ISREG(buf->st_mode)) buf->st_size = 1048576;

// Turn off write bits in the mode
//
   buf->st_mode &= wMask;

// All went well
//
   return SFS_OK;
}

/******************************************************************************/
/*         F i l e   S y s t e m   O b j e c t   I n t e r f a c e s          */
/******************************************************************************/
/******************************************************************************/
/*                                  E m s g                                   */
/******************************************************************************/

int XrdDigFS::Emsg(const char    *pfx,    // Message prefix value
                   XrdOucErrInfo &einfo,  // Place to put text & error code
                   int            ecode,  // The error code
                   const char    *op,     // Operation being performed
                   const char    *target) // The target (e.g., fname)
{
    char *etext, buffer[MAXPATHLEN+80], unkbuff[64];

// Get the reason for the error
//
   if (ecode < 0) ecode = -ecode;
   if (!(etext = strerror(ecode)))
      {sprintf(unkbuff, "reason unknown (%d)", ecode); etext = unkbuff;}

// Format the error message
//
   snprintf(buffer,sizeof(buffer),"Unable to %s %s; %s", op, target, etext);

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

/******************************************************************************/
/*                                e x i s t s                                 */
/******************************************************************************/

int XrdDigFS::exists(const char                *path,        // In
                           XrdSfsFileExistence &file_exists, // Out
                           XrdOucErrInfo       &error,       // Out
                     const XrdSecClientName    *client,      // In
                     const char                *info)        // In
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
            info        - Opaque information, if any.

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.

  Notes:    When failure occurs, 'file_exists' is not modified.
*/
{
   static const char *epname = "exists";
   struct stat fstat;

// Now try to find the file or directory
//
   if (!XrdDigUFS::Statfn(path, &fstat))
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
   return XrdDigFS::Emsg(epname, error, errno, "locate", path);
}

/******************************************************************************/
/*                                 f s c t l                                  */
/******************************************************************************/

int XrdDigFS::fsctl(const int               cmd,
                    const char             *args,
                          XrdOucErrInfo    &eInfo,
                    const XrdSecClientName *client)
/*
  Function: Perform filesystem operations:

  Input:    cmd       - Operation command (currently supported):
                        SFS_FSCTL_LOCATE - locate file (always local)
            arg       - Command dependent argument:
                      - Locate: The path whose location is wanted
            eInfo     - Error/Response information structure.
            client    - Authentication credentials, if any.

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{

// Process the LOCATE request. We only support "*/path" type of requests and
// if valid, we return ourselves as the location. Security is not applied here.
//
   if ((cmd & SFS_FSCTL_CMD) == SFS_FSCTL_LOCATE)
      {if ((*args == '*' && !(SFS_LCLROOT(args+1)))
       ||  (*args == '/' && !(SFS_LCLROOT(args  ))))
          {eInfo.setErrInfo(EINVAL, "Invalid locate path");
           return SFS_ERROR;
          }

       Config.GetLocResp(eInfo, (cmd & SFS_O_HNAME));
       return SFS_DATA;
      }

// We don't support anything else
//
   eInfo.setErrInfo(ENOTSUP, "Operation not supported.");
   return SFS_ERROR;
}
  
/******************************************************************************/
/*                            g e t V e r s i o n                             */
/******************************************************************************/

const char *XrdDigFS::getVersion() {return XrdVERSION;}
  
/******************************************************************************/
/* Private:                       R e j e c t                                 */
/******************************************************************************/
  
int XrdDigFS::Reject(const char *op, const char *trg, XrdOucErrInfo &eInfo)
{
   return XrdDigFS::Emsg("Inspect", eInfo, EROFS, op, trg);
}

/******************************************************************************/
/*                                  s t a t                                   */
/******************************************************************************/

int XrdDigFS::stat(const char              *path,        // In
                         struct stat       *buf,         // Out
                         XrdOucErrInfo     &error,       // Out
                   const XrdSecClientName  *client,      // In
                   const char              *info)        // In
/*
  Function: Get info on 'path'.

  Input:    path        - Is the fully qualified name of the file to be tested.
            buf         - The stat structiure to hold the results
            error       - Error information object holding the details.
            client      - Authentication credentials, if any.
            info        - Opaque information, if any.

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   static const char *epname = "stat";
   static const int wMask = ~(S_IWUSR | S_IWGRP | S_IWOTH);
   char *fname;
   int   retc;

// Check if we are trying to stat the root
//
   if (!strcmp(path, SFS_LCLPRFX) || !strcmp(path, SFS_LCLPRFY))
      {const char *auth;
       if (Config.GenAccess(client, &auth, 1) < 0)
          return XrdDigFS::Emsg(epname,error,EACCES,"stat directory",path);
       XrdDigConfig::StatRoot(buf);
       return SFS_OK;
      }

// Authorize and get the correct fname to stat
//
   if ((retc = Validate(path))
   ||  !(fname = Config.GenPath(retc, client, "stat", path+SFS_LCLPLEN)))
      return XrdDigFS::Emsg(epname,error,retc,"stat",path);

// Fixup filename when stat is issued into /proc (linux only)
//
#ifdef __linux__
   char *myLink;
   if ((myLink = strstr(fname, " -> "))) *myLink = 0;
#endif

// Execute the function
//
   if (XrdDigUFS::Statfn(fname, buf))
      {retc = errno;
       free(fname);
       return XrdDigFS::Emsg(epname, error, retc, "stat", path);
      }

// Turn off write bits in the mode
//
   buf->st_mode &= wMask;

// All went well
//
   free(fname);
   return SFS_OK;
}

/******************************************************************************/
/*                              V a l i d a t e                               */
/******************************************************************************/
  
int XrdDigFS::Validate(const char *path)
{
// Make sure this is our path and is legal
//
   return (SFS_LCLPATH(path) && *(path+SFS_LCLPLEN) ? 0 : EPERM);
}
