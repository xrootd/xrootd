/******************************************************************************/
/*                                                                            */
/*                          X r d O u c a 2 x . c c                           */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//           $Id$

const char *XrdOuca2xCVSID = "$Id$";

#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

#include "XrdOuc/XrdOuca2x.hh"

/******************************************************************************/
/*                                   a 2 i                                    */
/******************************************************************************/

int XrdOuca2x::a2i(XrdOucError &Eroute, const char *emsg, char *item,
                                             int *val, int minv, int maxv)
{  int rc;
   if (rc = a2i(Eroute, emsg, item, val, minv)) return rc;
   if (*val > maxv) return Eroute.Emsg("a2i", EINVAL, emsg, item);
   return 0;
}

int XrdOuca2x::a2i(XrdOucError &Eroute, const char *emsg, char *item,
                                             int *val, int minv)
{
    if (!item || !*item)
       {Eroute.Emsg("a2i", emsg, (char *)"not specified"); return -1;}

    errno = 0;
    *val  = strtol(item, (char **)NULL, 10);
    if (errno || *val < minv) 
       return Eroute.Emsg("a2i", EINVAL, emsg, item);
    return 0;
}
 
/******************************************************************************/
/*                                  a 2 l l                                   */
/******************************************************************************/

long long XrdOuca2x::a2ll(XrdOucError &Eroute, const char *emsg, char *item,
                                long long *val, long long minv, long long maxv)
{  int rc;
   if (rc = a2ll(Eroute, emsg, item, val, minv)) return rc;
   if (*val > maxv) return Eroute.Emsg("a2ll", EINVAL, emsg, item);
   return 0;
}
  
long long XrdOuca2x::a2ll(XrdOucError &Eroute, const char *emsg, char *item,
                                long long *val, long long minv)
{
    if (!item || !*item)
       {Eroute.Emsg("a2ll", emsg, (char *)"not specified"); return -1;}

    errno = 0;
    *val  = strtoll(item, (char **)NULL, 10);
    if (errno || *val < minv) 
       return Eroute.Emsg("a2ll", EINVAL, emsg, item);
    return 0;
}

/******************************************************************************/
/*                                  a 2 f m                                   */
/******************************************************************************/

int XrdOuca2x::a2fm(XrdOucError &Eroute, const char *emsg, char *item,
                                              int *val, int minv, int maxv)
{  int rc, num;
   if (rc = a2fm(Eroute, emsg, item, &num, minv)) return rc;
   if ((*val | maxv) != maxv) return Eroute.Emsg("a2fm", EINVAL, emsg, item);

   *val = 0;
   if (num & 0100) *val |= S_IXUSR; // execute permission: owner
   if (num & 0200) *val |= S_IWUSR; // write permission:   owner
   if (num & 0400) *val |= S_IRUSR; // read permission:    owner
   if (num & 0010) *val |= S_IXGRP; // execute permission: group
   if (num & 0020) *val |= S_IWGRP; // write permission:   group
   if (num & 0040) *val |= S_IRGRP; // read permission:    group
   if (num & 0001) *val |= S_IXOTH; // execute permission: other
   if (num & 0002) *val |= S_IWOTH; // write permission:   other
   if (num & 0004) *val |= S_IROTH; // read permission:    other
   return 0;
}

int XrdOuca2x::a2fm(XrdOucError &Eroute, const char *emsg, char *item,
                                              int *val, int minv)
{
    if (!item || !*item)
       {Eroute.Emsg("a2fm", emsg, (char *)"not specified"); return -1;}

    errno = 0;
    *val  = strtol(item, (char **)NULL, 8);
    if (errno || !(*val & minv))
       return Eroute.Emsg("a2fm", EINVAL, emsg, item);
    return 0;
}
 
/******************************************************************************/
/*                                  a 2 s z                                   */
/******************************************************************************/

long long XrdOuca2x::a2sz(XrdOucError &Eroute, const char *emsg, char *item,
                                long long *val, long long minv, long long maxv)
{  int rc;
   if (rc = a2sz(Eroute, emsg, item, val, minv)) return rc;
   if (*val > maxv) return Eroute.Emsg("a2sz", EINVAL, emsg, item);
   return 0;
}
  
long long XrdOuca2x::a2sz(XrdOucError &Eroute, const char *emsg, char *item,
                                long long *val, long long minv)
{   int i = strlen(item)-1;
    long long qmult = 1;

    if (!item || !*item)
       {Eroute.Emsg("a2sz", emsg, (char *)"not specified"); return -1;}

    errno = 0;
    if (item[i] == 'k' || item[i] == 'K') qmult = 1024;
    if (item[i] == 'm' || item[i] == 'M') qmult = 1024*1024;
    if (item[i] == 'g' || item[i] == 'g') qmult = 1024*1024*1024;
    if (qmult > 1) item[i] = '\0';
    *val  = strtoll(item, (char **)NULL, 10) * qmult;
    if (errno || *val < minv) 
       return Eroute.Emsg("a2sz", EINVAL, emsg, item);
    return 0;
}
 
/******************************************************************************/
/*                                  a 2 t m                                   */
/******************************************************************************/

int XrdOuca2x::a2tm(XrdOucError &Eroute, const char *emsg, char *item, int *val,
                          int minv, int maxv)
{  int rc;
   if (rc = a2tm(Eroute, emsg, item, val, minv)) return rc;
   if (*val > maxv) return Eroute.Emsg("a2tm", EINVAL, emsg, item);
   return 0;
}
  
int XrdOuca2x::a2tm(XrdOucError &Eroute, const char *emsg, char *item,
                          int *val, int minv)
{   int i = strlen(item)-1;
    int qmult = 0;

    if (!item || !*item)
       {Eroute.Emsg("a2tm", emsg, (char *)"not specified"); return -1;}

    errno = 0;
    if (item[i] == 's' || item[i] == 's') qmult = 1;
    if (item[i] == 'm' || item[i] == 'M') qmult = 60;
    if (item[i] == 'h' || item[i] == 'H') qmult = 60*60;
    if (item[i] == 'd' || item[i] == 'D') qmult = 60*60*24;
    if (qmult > 0) item[i] = '\0';
       else qmult = 1;
    *val  = strtoll(item, (char **)NULL, 10) * qmult;
    if (errno || *val < minv) 
       return Eroute.Emsg("a2sz", EINVAL, emsg, item);
    return 0;
}
