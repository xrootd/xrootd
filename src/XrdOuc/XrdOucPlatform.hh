#ifndef __XRDOUC_PLATFORM_H__
#define __XRDOUC_PLATFORM_H__
/******************************************************************************/
/*                                                                            */
/*                     X r d O u c P l a t f o r m . h h                      */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//        $Id$

#ifdef __linux__

#include <memory.h>
#include <string.h>
#include <sys/types.h>
#include <asm/param.h>
#include <byteswap.h>

#define S_IAMB      0x1FF   /* access mode bits */

#define F_DUP2FD F_DUPFD

#define STATFS      statfs
#define STATFS_BUFF struct statfs

#define FS_BLKFACT  4

#define SHMDT_t const void *

#define FLOCK_t struct flock

typedef off_t offset_t;

#define GTZ_NULL (struct timezone *)0

#define GETHOSTBYNAME(hname, rbuff, cbuff, cblen,  rpnt, pretc) \
     (gethostbyname_r(hname, rbuff, cbuff, cblen, &rpnt, pretc) == 0)

#define GETHOSTBYADDR(haddr,hlen,htype,rbuff,cbuff,cblen, rpnt,pretc) \
     (gethostbyaddr_r(haddr,hlen,htype,rbuff,cbuff,cblen,&rpnt,pretc) == 0)

#define GETSERVBYNAME(name, stype, psrv, buff, blen,  rpnt) \
     (getservbyname_r(name, stype, psrv, buff, blen, &rpnt) == 0)

#else

#define STATFS      statvfs
#define STATFS_BUFF struct statvfs

#define FS_BLKFACT  1

#define SHMDT_t char *

#define FLOCK_t flock_t

#define GTZ_NULL (void *)0

#define GETHOSTBYNAME(hname, rbuff, cbuff, cblen, rpnt, pretc) \
(rpnt=gethostbyname_r(hname, rbuff, cbuff, cblen,       pretc))


#define GETHOSTBYADDR(haddr, hlen, htype, rbuff, cbuff, cblen, rpnt, pretc) \
(rpnt=gethostbyaddr_r(haddr, hlen, htype, rbuff, cbuff, cblen,       pretc))

#define GETSERVBYNAME(name, stype, psrv, buff, blen, rpnt) \
(rpnt=getservbyname_r(name, stype, psrv, buff, blen))

#endif

// Only sparc platforms have structure alignment problems w/ optimization
#if defined(__sparc) || __BYTE_ORDER==__BIG_ENDIAN

#ifndef htonll
#define htonll(_x_)  _x_
#endif
#ifndef h2nll
#define h2nll(_x_, _y_) memcpy((void *)&_y_,(const void *)&_x_,sizeof(long long))
#endif
#ifndef ntohll
#define ntohll(_x_)  _x_
#endif
#ifndef n2hll
#define n2hll(_x_, _y_) memcpy((void *)&_y_,(const void *)&_x_,sizeof(long long))
#endif

#else
// Unfortunately, icc does *not* support GNUC's byte swap routines
#ifdef __ICC__
#ifndef __bswap_64
extern "C"
{extern unsigned long long Swap_n2hll(unsigned long long x);}
#define __bswap_64(x) Swap_n2hll(x)
#endif
#endif

// When we ever return a long long we will have to fix this as well, perhaps.
#ifndef htonll
#define htonll(_x_) __bswap_64(_x_)
#endif
#ifndef h2nll
#define h2nll(_x_, _y_) memcpy((void *)&_y_,(const void *)&_x_,sizeof(long long));\
                        _y_ = htonll(_y_)
#endif
#ifndef ntohll
#define ntohll(_x_) __bswap_64(_x_)
#endif
#ifndef n2hll
#define n2hll(_x_, _y_) memcpy((void *)&_y_,(const void *)&_x_,sizeof(long long));\
                        _y_ = ntohll(_y_)
#endif

#endif

#endif

#ifndef HAS_STRLCPY
extern "C"
{extern size_t strlcpy(char *dst, const char *src, size_t size);}
#endif

#ifdef __macos__
#define memalign(pgsz,amt) valloc(amt)
#endif
