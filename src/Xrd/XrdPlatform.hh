#ifndef __XRD_PLATFORM_H__
#define __XRD_PLATFORM_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d P l a t f o r m . h h                         */
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

#define F_DUP2FD F_DUPFD

#define STATFS      statfs
#define STATFS_BUFF struct statfs

#define FS_BLKFACT  4

#define SHMDT_t const void *

typedef off_t offset_t;

#define GTZ_NULL (struct timezone *)0

#define GETHOSTBYNAME(hname, rbuff, cbuff, cblen,  rpnt, pretc) \
     (gethostbyname_r(hname, rbuff, cbuff, cblen, &rpnt, pretc) == 0)

#define GETHOSTBYADDR(haddr,hlen,htype,rbuff,cbuff,cblen, rpnt,pretc) \
     (gethostbyaddr_r(haddr,hlen,htype,rbuff,cbuff,cblen,&rpnt,pretc) == 0)

#define GETSERVBYNAME(name, stype, psrv, buff, blen,  rpnt) \
     (getservbyname_r(name, stype, psrv, buff, blen, &rpnt) == 0)

#ifndef strlcpy
#define strlcpy(dst, src, sz) \
        {char *dp = dst; char *sp = (char *)src; \
         int i=sz-1; if (i > 0) while (*sp && i--) *dp++=*sp++; *dp='\0';}
#endif

#else

#define STATFS      statvfs
#define STATFS_BUFF struct statvfs

#define FS_BLKFACT  1

#define SHMDT_t char *

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
#define ntohll _x_
#endif
#ifndef h2nll
#define h2nll(_x_, _y_) memcpy((void *)&_y_,(const void *)&_x_,sizeof(long long))
#endif
#ifndef ntohll
#define ntohll _x_
#endif
#ifndef n2hll
#define n2hll(_x_, _y_) memcpy((void *)&_y_,(const void *)&_x_,sizeof(long long))
#endif

#else
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
