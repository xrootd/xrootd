/******************************************************************************/
/*                                                                            */
/*                    X r d O u c C a c h e D r a m . c c                     */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

#include <errno.h>

#include "XrdOuc/XrdOucCacheDram.hh"
#include "XrdOuc/XrdOucCacheReal.hh"
  
/******************************************************************************/
/*                                C r e a t e                                 */
/******************************************************************************/
  
XrdOucCache *XrdOucCacheDram::Create(XrdOucCache::Parms &ParmV,
                                     XrdOucCacheIO::aprParms *aprP)
{
   XrdOucCacheReal *cP;
   int rc;

// We simply create a new instance of a real cache and return it. We do it this
// way so that in the future new types of caches can be created using the same
// interface.
//
   cP = new XrdOucCacheReal(rc, ParmV, aprP);
   if (rc) {delete cP; cP = 0; errno = rc;}
   return (XrdOucCache *)cP;
}
