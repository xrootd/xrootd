/******************************************************************************/
/*                                                                            */
/*                    X r d O u c C a c h e D r a m . c c                     */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
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
