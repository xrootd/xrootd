#ifndef __XRDSYSUTILS_HH__
#define __XRDSYSUTILS_HH__
/******************************************************************************/
/*                                                                            */
/*                        X r d S y s U t i l s . h h                         */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>

class XrdSysUtils
{
public:

// ExecName() returns the full path of the executable invoked.
//
static const char *ExecName();

       XrdSysUtils() {}
      ~XrdSysUtils() {}
};
#endif
