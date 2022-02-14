/******************************************************************************/
/* XrdFfsWcache.cc simple write cache that captures consecutive small writes  */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/* Author: Wei Yang (SLAC National Accelerator Laboratory, 2009)              */
/*         Contract DE-AC02-76-SFO0515 with the Department of Energy          */
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

/* 
   When direct_io is not used, kernel will break large write to 4Kbyte  
   writes. This significantly reduces the writting performance. This 
   simple cache mechanism is to improve the performace on small writes. 

   Note that fuse 2.8.0 pre2 or above and kernel 2.6.27 or above provide
   a big_writes option to allow > 4KByte writing. It will make this 
   smiple write caching obsolete. 
*/

#if defined(__linux__)
/* For pread()/pwrite() */
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif
#endif

#include <cstring>
#include <cstdlib>
#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>
#include <cerrno>

#include <pthread.h>

#include "XrdFfs/XrdFfsWcache.hh"
#ifndef NOXRD
    #include "XrdFfs/XrdFfsPosix.hh"
#endif

#ifndef O_DIRECT
#define O_DIRECT 0
#endif

#ifdef __cplusplus
  extern "C" {
#endif

ssize_t XrdFfsRcacheBufsize;
ssize_t XrdFfsWcacheBufsize = 131072;

struct XrdFfsWcacheFilebuf {
    off_t offset;
    size_t len;
    char *buf;
    size_t bufsize;
    pthread_mutex_t *mlock;
};

struct XrdFfsWcacheFilebuf *XrdFfsWcacheFbufs;

/* #include "xrdposix.h" */

int XrdFfsPosix_baseFD, XrdFfsWcacheNFILES;
void XrdFfsWcache_init(int basefd, int maxfd)
{
    int fd;
/* We are now using virtual file descriptors (from Xrootd Posix interface) in XrdFfsXrootdfs.cc so we need to set 
 * base (lowest) file descriptor, and max number of file descriptors..
 *
    struct rlimit rlp;

    getrlimit(RLIMIT_NOFILE, &rlp);
    XrdFfsWcacheNFILES = rlp.rlim_cur;
    XrdFfsWcacheNFILES = (XrdFfsWcacheNFILES == (int)RLIM_INFINITY? 4096 : XrdFfsWcacheNFILES);
 */

   XrdFfsPosix_baseFD = basefd;
   XrdFfsWcacheNFILES = maxfd; 
   
/*    printf("%d %d\n", XrdFfsWcacheNFILES, sizeof(struct XrdFfsWcacheFilebuf)); */
    XrdFfsWcacheFbufs = (struct XrdFfsWcacheFilebuf*)malloc(sizeof(struct XrdFfsWcacheFilebuf) * XrdFfsWcacheNFILES);
    for (fd = 0; fd < XrdFfsWcacheNFILES; fd++)
    {
        XrdFfsWcacheFbufs[fd].offset = 0;
        XrdFfsWcacheFbufs[fd].len = 0;
        XrdFfsWcacheFbufs[fd].buf = NULL;
        XrdFfsWcacheFbufs[fd].mlock = NULL;
    }
    if (!getenv("XRDCL_EC"))
    {
        XrdFfsRcacheBufsize = 1024 * 128;
    }
    else
    {
        char *savptr;
        int nbdat = atoi(strtok_r(getenv("XRDCL_EC"), ",", &savptr));
        strtok_r(NULL, ",", &savptr);
        int chsz = atoi(strtok_r(NULL, ",", &savptr));
        XrdFfsRcacheBufsize = nbdat * chsz; 
    }
    if (getenv("XROOTDFS_WCACHESZ"))
        XrdFfsRcacheBufsize = atoi(getenv("XROOTDFS_WCACHESZ"));
}

int XrdFfsWcache_create(int fd, int flags)
/* Create a write cache buffer for a given file descriptor
 *
 * fd:      file descriptor
 *
 * returns: 1 - ok
 *          0 - error, error code in errno
 */
{
    XrdFfsWcache_destroy(fd);
    fd -= XrdFfsPosix_baseFD;

    XrdFfsWcacheFbufs[fd].offset = 0;
    XrdFfsWcacheFbufs[fd].len = 0;
    // "flag & O_RDONLY" is not equivalant to ! (flags & O_RDWR) && ! (flags & O_WRONLY)
    if ( ! (flags & O_RDWR) &&     
         ! (flags & O_WRONLY) &&
         (flags & O_DIRECT) )  // Limit the usage scenario of the read cache 
    {
        XrdFfsWcacheFbufs[fd].buf = (char*)malloc(XrdFfsRcacheBufsize);
        XrdFfsWcacheFbufs[fd].bufsize = XrdFfsRcacheBufsize;
    }
    else
    {
        XrdFfsWcacheFbufs[fd].buf = (char*)malloc(XrdFfsWcacheBufsize);
        XrdFfsWcacheFbufs[fd].bufsize = XrdFfsWcacheBufsize;
    }
    if (XrdFfsWcacheFbufs[fd].buf == NULL)
    {
        errno = ENOMEM;
        return 0;
    }
    XrdFfsWcacheFbufs[fd].mlock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    if (XrdFfsWcacheFbufs[fd].mlock == NULL)
    {
        errno = ENOMEM;
        return 0;
    }
    errno = pthread_mutex_init(XrdFfsWcacheFbufs[fd].mlock, NULL);
    if (errno)
        return 0;
    return 1;
}

void XrdFfsWcache_destroy(int fd)
{
/*  XrdFfsWcache_flush(fd); */
    fd -= XrdFfsPosix_baseFD;

    XrdFfsWcacheFbufs[fd].offset = 0;
    XrdFfsWcacheFbufs[fd].len = 0;
    if (XrdFfsWcacheFbufs[fd].buf != NULL) 
        free(XrdFfsWcacheFbufs[fd].buf);
    XrdFfsWcacheFbufs[fd].buf = NULL;
    if (XrdFfsWcacheFbufs[fd].mlock != NULL)
    {
        pthread_mutex_destroy(XrdFfsWcacheFbufs[fd].mlock);
        free(XrdFfsWcacheFbufs[fd].mlock);
    }
    XrdFfsWcacheFbufs[fd].mlock = NULL;
}

ssize_t XrdFfsWcache_flush(int fd)
{
    ssize_t rc;
    fd -= XrdFfsPosix_baseFD;

    if (XrdFfsWcacheFbufs[fd].len == 0 || XrdFfsWcacheFbufs[fd].buf == NULL )
        return 0;

    rc = XrdFfsPosix_pwrite(fd + XrdFfsPosix_baseFD, 
                            XrdFfsWcacheFbufs[fd].buf, XrdFfsWcacheFbufs[fd].len, XrdFfsWcacheFbufs[fd].offset);
    if (rc > 0)
    {
        XrdFfsWcacheFbufs[fd].offset = 0;
        XrdFfsWcacheFbufs[fd].len = 0;
    }
    return rc;
}

/*
struct fd_n_offset {
    int fd;
    off_t offset;
    fd_n_offset(int myfd, off_t myoffset) : fd(myfd), offset(myoffset) {}
};

void *XrdFfsWcache_updateReadCache(void *x)
{
    struct fd_n_offset *a = (struct fd_n_offset*) x;
    size_t bufsize = XrdFfsWcacheFbufs[a->fd].bufsize;

    pthread_mutex_lock(XrdFfsWcacheFbufs[a->fd].mlock);
    XrdFfsWcacheFbufs[a->fd].offset = (a->offset / bufsize) * bufsize;
    XrdFfsWcacheFbufs[a->fd].len = XrdFfsPosix_pread(a->fd + XrdFfsPosix_baseFD,
                                                     XrdFfsWcacheFbufs[a->fd].buf,
                                                     bufsize,
                                                     XrdFfsWcacheFbufs[a->fd].offset);
    pthread_mutex_unlock(XrdFfsWcacheFbufs[a->fd].mlock);   
    return NULL;
}
*/

// this is a read cache
ssize_t XrdFfsWcache_pread(int fd, char *buf, size_t len, off_t offset)
{
    ssize_t rc;
    fd -= XrdFfsPosix_baseFD;
    if (fd < 0)
    {
        errno = EBADF;
        return -1;
    }

    char *bufptr;
    size_t bufsize = XrdFfsWcacheFbufs[fd].bufsize;

    pthread_mutex_lock(XrdFfsWcacheFbufs[fd].mlock);

    // identity which block to cache
    if (XrdFfsWcacheFbufs[fd].len == 0 || 
        (offset / bufsize != XrdFfsWcacheFbufs[fd].offset / bufsize))
    {
        XrdFfsWcacheFbufs[fd].offset = (offset / bufsize) * bufsize;
        XrdFfsWcacheFbufs[fd].len = XrdFfsPosix_pread(fd + XrdFfsPosix_baseFD, 
                                                      XrdFfsWcacheFbufs[fd].buf,
                                                      bufsize,
                                                      XrdFfsWcacheFbufs[fd].offset);
    }  // when XrdFfsWcacheFbufs[fd].len < bufsize, the block is partially cached.


    // fetch data from the cache, up to the block's upper boundary.
    if (XrdFfsWcacheFbufs[fd].offset <= offset && 
        offset < XrdFfsWcacheFbufs[fd].offset + (off_t)XrdFfsWcacheFbufs[fd].len) 
    {  // read from cache, 
//----------------------------------------------------------
// FUSE doesn't like this block of the code, unless direct_io is enabled, or
// O_DIRECT flags is used. Otherwise, FUSES will stop reading prematurely
// when two processes read the same file at the same time.  
       bufptr = &XrdFfsWcacheFbufs[fd].buf[offset - XrdFfsWcacheFbufs[fd].offset]; 
       rc = (len < XrdFfsWcacheFbufs[fd].len - (offset - XrdFfsWcacheFbufs[fd].offset))?
             len : XrdFfsWcacheFbufs[fd].len - (offset - XrdFfsWcacheFbufs[fd].offset);
       memcpy(buf, bufptr, rc);
//----------------------------------------------------------
    } 
    else
    { // offset fall into the uncached part of the partically cached block
       rc = XrdFfsPosix_pread(fd + XrdFfsPosix_baseFD, buf, len, offset);
    }
    pthread_mutex_unlock(XrdFfsWcacheFbufs[fd].mlock);
/*    
    // prefetch the next block
    if ( (offset + rc) ==
         (XrdFfsWcacheFbufs[fd].offset + bufsize) )
    {
        pthread_t thread;
        pthread_attr_t attr;
        //size_t stacksize = 4*1024*1024;
    
        pthread_attr_init(&attr);
        //pthread_attr_setstacksize(&attr, stacksize);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        struct fd_n_offset nextblock(fd, (offset + bufsize));
        if (! pthread_create(&thread, &attr, XrdFfsWcache_updateReadCache, &nextblock)) 
            pthread_detach(thread);
        pthread_attr_destroy(&attr);
    }
*/
    return rc;
}

ssize_t XrdFfsWcache_pwrite(int fd, char *buf, size_t len, off_t offset)
{
    ssize_t rc;
    char *bufptr;
    fd -= XrdFfsPosix_baseFD;
    if (fd < 0)
    {
        errno = EBADF;
        return -1;
    }

/* do not use caching under these cases */
    if (len > (size_t)(XrdFfsWcacheBufsize/2) || fd >= XrdFfsWcacheNFILES)
    {
        rc = XrdFfsPosix_pwrite(fd + XrdFfsPosix_baseFD, buf, len, offset);
        return rc;
    }

    pthread_mutex_lock(XrdFfsWcacheFbufs[fd].mlock);
    rc = XrdFfsWcacheFbufs[fd].len;
/* 
   in the following two cases, a XrdFfsWcache_flush is required:
   1. current offset isnn't pointing to the tail of data in buffer
   2. adding new data will exceed the current buffer 
*/ 
    if (offset != (off_t)(XrdFfsWcacheFbufs[fd].offset + XrdFfsWcacheFbufs[fd].len) ||
        (off_t)(offset + len) > (XrdFfsWcacheFbufs[fd].offset + XrdFfsWcacheBufsize))
        rc = XrdFfsWcache_flush(fd + XrdFfsPosix_baseFD);

    errno = 0;
    if (rc < 0) 
    {
        errno = ENOSPC;
        pthread_mutex_unlock(XrdFfsWcacheFbufs[fd].mlock);
        return -1;
    }

    bufptr = &XrdFfsWcacheFbufs[fd].buf[XrdFfsWcacheFbufs[fd].len];
    memcpy(bufptr, buf, len);
    if (XrdFfsWcacheFbufs[fd].len == 0)
        XrdFfsWcacheFbufs[fd].offset = offset;
    XrdFfsWcacheFbufs[fd].len += len;

    pthread_mutex_unlock(XrdFfsWcacheFbufs[fd].mlock);
    return (ssize_t)len;
}

#ifdef __cplusplus
  }
#endif
