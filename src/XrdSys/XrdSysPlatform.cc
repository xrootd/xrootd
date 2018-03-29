/******************************************************************************/
/*                                                                            */
/*                     X r d S y s P l a t f o r m . c c                      */
/*                                                                            */
/* (c) 2006 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
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

#include <stdio.h>
#include <string.h>
#ifndef WIN32
#include <unistd.h>
#include <netinet/in.h>
#endif
#include <sys/types.h>

#if defined(_LITTLE_ENDIAN) || defined(__LITTLE_ENDIAN__) || \
    defined(__IEEE_LITTLE_ENDIAN) || \
   (defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN)
#if !defined(__GNUC__) || defined(__APPLE__)
extern "C"
{
unsigned long long Swap_n2hll(unsigned long long x)
{
    unsigned long long ret_val;
    *( (unsigned int  *)(&ret_val) + 1) = ntohl(*( (unsigned int  *)(&x)));
    *(((unsigned int  *)(&ret_val)))    = ntohl(*(((unsigned int  *)(&x))+1));
    return ret_val;
}
}
#endif

#endif

#ifndef HAVE_STRLCPY
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
