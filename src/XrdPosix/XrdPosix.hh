#ifndef __XRDPOSIX_H__
#define __XRDPOSIX_H__
/******************************************************************************/
/*                                                                            */
/*                           X r d P o s i x . h h                            */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
// The following defines substitute our names for the common system names. We
// would have liked to use wrappers but each platform uses a different mechanism
// to accomplish this. So, redefinition is the most portable way of doing this.
//
#define close(a)         XrdPosix_Close(a)

#define lseek(a,b,c)     XrdPosix_Lseek(a,b,c)

#define fstat(a,b)       XrdPosix_Fstat(a,b)

#define fsync(a)         XrdPosix_Fsync(a)

#define open             XrdPosix_Open
  
#define pread(a,b,c,d)   XrdPosix_Pread(a,b,c,d)

#define read(a,b,c)      XrdPosix_Read(a,b,c)
  
#define readv(a,b,c)     XrdPosix_Readv(a,b,c)

#define stat(a,b)        XrdPosix_Stat(a,b)

#define pwrite(a,b,c,d)  XrdPosix_Pwrite(a,b,c,d)

#define write(a,b,c)     XrdPosix_Write(a,b,c)

#define writev(a,b,c)    XrdPosix_Writev(a,b,c)

// Now define the external interfaces (not C++ but OS compatabile)
//
extern int     XrdPosix_Close(int fildes);

extern off_t   XrdPosix_Lseek(int fildes, off_t offset, int whence);

extern int     XrdPosix_Fstat(int fildes, struct stat *buf);

extern int     XrdPosix_Fsync(int fildes);

extern int     XrdPosix_Open(const char *path, int oflag, ...);
  
extern ssize_t XrdPosix_Pread(int fildes, void *buf, size_t nbyte, off_t offset);

extern ssize_t XrdPosix_Read(int fildes, void *buf, size_t nbyte);
  
extern ssize_t XrdPosix_Readv(int fildes, const struct iovec *iov, int iovcnt);

extern int     XrdPosix_Stat(const char *path, struct stat *buf);

extern ssize_t XrdPosix_Pwrite(int fildes, const void *buf, size_t nbyte, off_t offset);

extern ssize_t XrdPosix_Write(int fildes, const void *buf, size_t nbyte);

extern ssize_t XrdPosix_Writev(int fildes, const struct iovec *iov, int iovcnt);
#endif
