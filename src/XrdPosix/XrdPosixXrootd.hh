#ifndef __XRDPOSIX_H__
#define __XRDPOSIX_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d P o s i x X r o o t d                         */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>

#include "XrdOuc/XrdOucPthread.hh"

const int XrdPosixFD = 65536;

class XrdPosixFile;

class XrdPosixXrootd
{
public:

static int     Close(int fildes);

static off_t   Lseek(int fildes, off_t offset, int whence);

static int     Fstat(int fildes, struct stat *buf);

static int     Fsync(int fildes);

static int     Open(const char *path, int oflag, int mode=0);
  
static ssize_t Pread(int fildes, void *buf, size_t nbyte, off_t offset);
  
static ssize_t Read(int fildes, void *buf, size_t nbyte);

static ssize_t Readv(int fildes, const struct iovec *iov, int iovcnt);

static int     Stat(const char *path, struct stat *buf);

static ssize_t Pwrite(int fildes, const void *buf, size_t nbyte, off_t offset);

static ssize_t Write(int fildes, const void *buf, size_t nbyte);

static ssize_t Write(int fildes, void *buf, size_t nbyte, off_t offset);

static ssize_t Writev(int fildes, const struct iovec *iov, int iovcnt);

               XrdPosixXrootd(int maxfd=64);
              ~XrdPosixXrootd();

private:

static XrdPosixFile         *findFP(int fildes, int glk=0);
static int                   mapError(int rc);
static int                   mapFlags(int flags);

static XrdOucMutex    myMutex;
static const  int     FDMask;
static const  int     FDOffs;
static const  int     FDLeft;
static XrdPosixFile **myFiles;
static int            lastFD;
static int            highFD;
};
#endif
