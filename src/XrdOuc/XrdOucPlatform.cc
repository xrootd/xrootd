/******************************************************************************/
/*                                                                            */
/*                     X r d O u c P l a t f o r m . c c                      */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//        $Id$

const char *XrdOucPlatformCVSID = "$Id$";

#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>

#ifdef __linux__

#ifdef __ICC__
extern "C"
{
unsigned long long Swap_n2hll(unsigned long long x)
{
 unsigned long long ret_val;

    *( (unsigned long *)(&ret_val) + 1) = ntohl(*( (unsigned long *)(&x)));
    *(((unsigned long *)(&ret_val)))    = ntohl(*(((unsigned long *)(&x))+1));
    return ret_val;
}
}
#endif

#endif

#ifndef HAS_STRLCPY
extern "C"
{
size_t strlcpy(char *dst, const char *src, size_t sz)
{
    size_t slen = strlen(src);
    size_t tlen =sz-1;

    if (slen <= tlen) strcpy(dst, src);
       else if (tlen > 0) {strncpy(dst, src, tlen); dst[tlen] = '\0';}
               else if (tlen == 0) dst[0] = '\0';

    return slen;
}
}
#endif

