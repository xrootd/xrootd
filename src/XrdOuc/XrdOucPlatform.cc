/******************************************************************************/
/*                                                                            */
/*                     X r d O u c P l a t f o r m . c c                      */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*      All Rights Reserved. See XrdVersion.cc for complete License Terms     */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//        $Id$

const char *XrdOucPlatformCVSID = "$Id$";

#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "Experiment/Experiment.hh"

#ifdef __linux__

extern "C"
{
int strlcpy(char *dst, const char *src, size_t sz)
{
    int slen = strlen(src);
    int tlen =sz-1;

    if (slen <= tlen) strcpy(dst, src);
       else if (tlen > 0) {strncpy(dst, src, tlen); dst[tlen] = '\0';}
               else if (tlen == 0) dst[0] = '\0';

    return slen;
}
}
#endif
