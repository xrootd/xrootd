/******************************************************************************/
/*                                                                            */
/*                           X r d P o s i x . c c                            */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

const char *XrdPosixCVSID = "$Id$";
  
#include <stdarg.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>

#include "XrdPosixXrootd.hh"

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
class XrdPosixXrootPath
{
public:

char *URL(const char *path, char *buff, int blen);

      XrdPosixXrootPath();
     ~XrdPosixXrootPath();

private:

struct xpath {struct xpath *next; 
                     char  *path;
                     int    plen;

                     xpath(struct xpath *cur, char *p)
                          : next(cur),
                            path(strdup(p)),
                            plen(strlen(p)) {}
                    ~xpath() {if (path) free(path);}
             };

struct xpath *xplist;

char         *xrootd;
int           xrdlen;
};

/******************************************************************************/
/*         X r d P o s i x X r o o t P a t h   C o n s t r u c t o r          */
/******************************************************************************/

XrdPosixXrootPath::XrdPosixXrootPath()
    : xplist(0),
      xrdlen(0)
{
   char *plist, *base, *colon;

   if (!(xrootd = getenv("XROOTDSERVER")) || !*xrootd) xrootd = 0;
      else xrdlen = strlen(xrootd);

   if (!(plist = getenv("XROOTDPATH")) || !*plist) return;
   base = plist = strdup(plist);

   do {if ((colon = index(plist, (int)':'))) *colon = '\0';
       if (colon != plist) xplist = new struct xpath(xplist, plist);
       plist = colon+1;
      } while(colon);
   free(base);
}

/******************************************************************************/
/*          X r d P o s i x X r o o t P a t h   D e s t r u c t o r           */
/******************************************************************************/
  
XrdPosixXrootPath::~XrdPosixXrootPath()
{
   struct xpath *xpnow;

   while((xpnow = xplist))
        {xplist = xplist->next; delete xpnow;}
}
  
/******************************************************************************/
/*                     X r d P o s i x P a t h : : U R L                      */
/******************************************************************************/
  
char *XrdPosixXrootPath::URL(const char *path, char *buff, int blen)
{
   const char   *rproto = "root://";
   const int     rprlen = strlen(rproto);
   const char   *xproto = "xroot://";
   const int     xprlen = strlen(xproto);
   struct xpath *xpnow = xplist;

// If this starts with 'root", then this is our path
//
   if (!strncmp(rproto, path, rprlen)) return (char *)path;

// If it starts with xroot, then convert it to be root
//
   if (!strncmp(xproto, path, xprlen))
      {if ((int(strlen(path))) > blen) return 0;
       strcpy(buff, path+1);
       return buff;
      }

// If we have no server or no path, then this is not our path
//
   if (!xrootd || !xplist) return 0;

// Check if this path starts with one or our known paths
//
   while(xpnow)
        if (!strncmp(path, xpnow->path, xpnow->plen)) break;
           else xpnow = xpnow->next;

// If we did not match a path, this is not our path. Otherwise build url
//
   if (!xpnow) return 0;
   if (xprlen+xrdlen+int(strlen(path)+1) >= blen) return 0;
   strcpy(buff, rproto);
   strcat(buff, xrootd);
   strcat(buff, "/");
   strcat(buff, path);
   return buff;
}

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
static XrdPosixXrootd    Xroot;

static XrdPosixXrootPath XrootPath;

/******************************************************************************/
/*                        X r d P o s i x _ C l o s e                         */
/******************************************************************************/

int XrdPosix_Close(int fildes)
{

// Return result of the close
//
   return (fildes < XrdPosixFD ? close(fildes) : Xroot.Close(fildes));
}

/******************************************************************************/
/*                     X r d P o s i x _ C l o s e d i r                      */
/******************************************************************************/

int XrdPosix_Closedir(DIR *dirp)
{
   return (Xroot.isXrootdDir(dirp) ? Xroot.Closedir(dirp) : closedir(dirp));
}

/******************************************************************************/
/*                        X r d P o s i x _ L s e e k                         */
/******************************************************************************/
  
off_t XrdPosix_Lseek(int fildes, off_t offset, int whence)
{

// Return the operation of the seek
//
   return (fildes < XrdPosixFD ? lseek(fildes, offset, whence)
                         : Xroot.Lseek(fildes, offset, whence));
}

/******************************************************************************/
/*                        X r d P o s i x _ F s t a t                         */
/******************************************************************************/
  
int XrdPosix_Fstat(int fildes, struct stat *buf)
{

// Return result of the close
//
   return (fildes < XrdPosixFD ? fstat(fildes, buf)
                         : Xroot.Fstat(fildes, buf));
}

/******************************************************************************/
/*                        X r d P o s i x _ F s y n c                         */
/******************************************************************************/
  
int XrdPosix_Fsync(int fildes)
{

// Return the result of the sync
//
   return (fildes < XrdPosixFD ? fsync(fildes) : Xroot.Fsync(fildes));
}

/******************************************************************************/
/*                        X r d P o s i x _ M k d i r                         */
/******************************************************************************/

int XrdPosix_Mkdir(const char *path, mode_t mode)
{
   char *myPath, buff[2048];

// Make sure a path was passed
//
   if (!path) {errno = EFAULT; return -1;}

// Return the results of a mkdir of a Unix file system
//
   if (!(myPath = XrootPath.URL(path, buff, sizeof(buff)))) {
     return mkdir(path, mode);
   }

// Return the results of an mkdir of an xrootd file system
//
   return Xroot.Mkdir(path, mode);
}

/******************************************************************************/
/*                         X r d P o s i x _ O p e n                          */
/******************************************************************************/
  
int XrdPosix_Open(const char *path, int oflag, ...)
{
   char *myPath, buff[2048];
   va_list ap;
   int mode;

// Make sure a path was passed
//
   if (!path) {errno = EFAULT; return -1;}

// Return the results of an open of a Unix file
//
   if (!(myPath = XrootPath.URL(path, buff, sizeof(buff))))
      {if (!(oflag & O_CREAT)) return open(path, oflag);
       va_start(ap, oflag);
       mode = va_arg(ap, int);
       va_end(ap);
       return open(path, oflag, (mode_t)mode);
      }

// Return the results of an open of an xrootd file
//
   if (!(oflag & O_CREAT)) return Xroot.Open(myPath, oflag);
   va_start(ap, oflag);
   mode = va_arg(ap, int);
   va_end(ap);
   return Xroot.Open(myPath, oflag, (mode_t)mode);
}

/******************************************************************************/
/*                       X r d P o s i x _ O p e n d i r                      */
/******************************************************************************/

DIR* XrdPosix_Opendir(const char *path)
{
   char *myPath, buff[2048];

// Make sure a path was passed
//
   if (!path) {errno = EFAULT; return 0;}
   
// Unix opendir
//
   if (!(myPath = XrootPath.URL(path, buff, sizeof(buff)))) {
     return opendir(path);
   }

// Xrootd opendir
//
   return Xroot.Opendir(path);
}


/******************************************************************************/
/*                         X r d P o s i x _ R e a d                          */
/******************************************************************************/
  
ssize_t XrdPosix_Read(int fildes, void *buf, size_t nbyte)
{

// Return the results of the read
//
   return (fildes < XrdPosixFD ? read(fildes, buf, nbyte)
                         : Xroot.Read(fildes, buf, nbyte));
}

/******************************************************************************/
/*                        X r d P o s i x _ P r e a d                         */
/******************************************************************************/
  
ssize_t XrdPosix_Pread(int fildes, void *buf, size_t nbyte, off_t offset)
{

// Return the results of the read
//
   return (fildes < XrdPosixFD ? pread(fildes, buf, nbyte, offset)
                         : Xroot.Pread(fildes, buf, nbyte, offset));
}
 
/******************************************************************************/
/*                        X r d P o s i x _ R e a d v                         */
/******************************************************************************/
  
ssize_t XrdPosix_Readv(int fildes, const struct iovec *iov, int iovcnt)
{

// Return results of the readv
//
   return (fildes < XrdPosixFD ? readv(fildes, iov, iovcnt)
                         : Xroot.Readv(fildes, iov, iovcnt));
}

/******************************************************************************/
/*                      X r d P o s i x _ R e a d d i r                       */
/******************************************************************************/

struct dirent* XrdPosix_Readdir(DIR *dirp)
{
// Return result of readdir
//
   return (Xroot.isXrootdDir(dirp) ? Xroot.Readdir(dirp) : readdir(dirp));
}

/******************************************************************************/
/*                    X r d P o s i x _ R e a d d i r _ r                     */
/******************************************************************************/

int XrdPosix_Readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result)
{
// Return result of readdir
//
   return (Xroot.isXrootdDir(dirp) ? 
           Xroot.Readdir_r(dirp,entry,result) : readdir_r(dirp,entry,result));
}

/******************************************************************************/
/*                    X r d P o s i x _ R e w i n d d i r                     */
/******************************************************************************/

void XrdPosix_Rewinddir(DIR *dirp)
{
// Return result of rewind
//
   return (Xroot.isXrootdDir(dirp) ? Xroot.Rewinddir(dirp) : rewinddir(dirp));
}

/******************************************************************************/
/*                        X r d P o s i x _ R m d i r                         */
/******************************************************************************/

int XrdPosix_Rmdir(const char *path)
{
   char *myPath, buff[2048];

// Make sure a path was passed
//
   if (!path) {errno = EFAULT; return -1;}

// Return the results of a mkdir of a Unix file system
//
   if (!(myPath = XrootPath.URL(path, buff, sizeof(buff)))) {
     return rmdir(path);
   }

// Return the results of an mkdir of an xrootd file system
//
   return Xroot.Rmdir(path);
}

/******************************************************************************/
/*                      X r d P o s i x _ T e l l d i r                       */
/******************************************************************************/

void XrdPosix_Seekdir(DIR *dirp, long loc)
{
// Call seekdir
//
   (Xroot.isXrootdDir(dirp) ? Xroot.Seekdir(dirp, loc) : seekdir(dirp, loc));
}

/******************************************************************************/
/*                         X r d P o s i x _ S t a t                          */
/******************************************************************************/
  
int XrdPosix_Stat(const char *path, struct stat *buf)
{
   char *myPath, buff[2048];

// Make sure a path was passed
//
   if (!path) {errno = EFAULT; return -1;}

// Return the results of an open of a Unix file
//
   return (!(myPath = XrootPath.URL(path, buff, sizeof(buff)))
             ? stat(path, buf) : Xroot.Stat(myPath, buf));
}


/******************************************************************************/
/*                      X r d P o s i x _ T e l l d i r                       */
/******************************************************************************/

long XrdPosix_Telldir(DIR *dirp)
{
// Return result of telldir
//
   return (Xroot.isXrootdDir(dirp) ? Xroot.Telldir(dirp) : telldir(dirp));
}


/******************************************************************************/
/*                      X r d P o s i x _ U n l i n k                         */
/******************************************************************************/

int XrdPosix_Unlink(const char *path)
{   
   char *myPath, buff[2048];

// Make sure a path was passed
//
   if (!path) {errno = EFAULT; return -1;}

// Return the result of a unlink of a Unix file
//
   if (!(myPath = XrootPath.URL(path, buff, sizeof(buff)))) {
     return unlink(path);
   }

// Return the results of an unlink of an xrootd file
//
   return Xroot.Unlink(path);
}


/******************************************************************************/
/*                        X r d P o s i x _ W r i t e                         */
/******************************************************************************/
  
ssize_t XrdPosix_Write(int fildes, const void *buf, size_t nbyte)
{

// Return the results of the write
//
   return (fildes < XrdPosixFD ? write(fildes, buf, nbyte)
                         : Xroot.Write(fildes, buf, nbyte));
}

/******************************************************************************/
/*                       X r d P o s i x _ P w r i t e                        */
/******************************************************************************/
  
ssize_t XrdPosix_Pwrite(int fildes, const void *buf, size_t nbyte, off_t offset)
{

// Return the results of the write
//
   return (fildes < XrdPosixFD ? pwrite(fildes, buf, nbyte, offset)
                         : Xroot.Pwrite(fildes, buf, nbyte, offset));
}
 
/******************************************************************************/
/*                       X r d P o s i x _ W r i t e v                        */
/******************************************************************************/
  
ssize_t XrdPosix_Writev(int fildes, const struct iovec *iov, int iovcnt)
{

// Return results of the writev
//
   return (fildes < XrdPosixFD ? writev(fildes, iov, iovcnt)
                         : Xroot.Writev(fildes, iov, iovcnt));
}
