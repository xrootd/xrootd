#ifndef __OOUC_A2X__
#define __OOUC_A2X__
/******************************************************************************/
/*                                                                            */
/*                            o o u c _ a 2 x . h                             */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*      All Rights Reserved. See XrdVersion.cc for complete License Terms     */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//          $Id$

#include "XrdOuc/XrdOucError.hh"

// This class is a holding area for various conversion utility routines
//

class XrdOuca2x
{
public:
static int         a2i( XrdOucError &, const char *emsg, char *item, int *val, int minv);
static int         a2i( XrdOucError &, const char *emsg, char *item, int *val, int minv, int maxv);
static long long   a2ll(XrdOucError &, const char *emsg, char *item, long long *val, long long minv);
static long long   a2ll(XrdOucError &, const char *emsg, char *item, long long *val, long long minv, long long maxv);
static int         a2fm(XrdOucError &, const char *emsg, char *item, int *val, int minv);
static int         a2fm(XrdOucError &, const char *emsg, char *item, int *val, int minv, int maxv);
static long long   a2sz(XrdOucError &, const char *emsg, char *item, long long *val, long long minv);
static long long   a2sz(XrdOucError &, const char *emsg, char *item, long long *val, long long minv, long long maxv);
static int         a2tm(XrdOucError &, const char *emsg, char *item, int *val, int minv);
static int         a2tm(XrdOucError &, const char *emsg, char *item, int *val, int minv, int maxv);
};
#endif
