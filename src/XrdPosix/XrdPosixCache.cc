/******************************************************************************/
/*                                                                            */
/*                      X r d P o s i x C a c h e . c c                       */
/*                                                                            */
/* (c) 2018 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdOuc/XrdOucCache.hh"
#include "XrdOuc/XrdOucCacheStats.hh"
#include "XrdPosix/XrdPosixCache.hh"

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
namespace XrdPosixGlobals
{
extern XrdOucCache *theCache;
}

using namespace XrdPosixGlobals;

/******************************************************************************/
/*                             C a c h e P a t h                              */
/******************************************************************************/

int XrdPosixCache::CachePath(const char *url, char *buff, int blen)
{
   return theCache->LocalFilePath(url, buff, blen, XrdOucCache::ForPath);
}

/******************************************************************************/
/*                            C a c h e Q u e r y                             */
/******************************************************************************/

int XrdPosixCache::CacheQuery(const char *url, bool hold)
{

   int rc = theCache->LocalFilePath(url, 0, 0,
                                    (hold ? XrdOucCache::ForAccess
                                          : XrdOucCache::ForInfo)
                                   );
   if (!rc) return 1;
   if (rc == -EREMOTE) return 0;
   return -1;
}
  
/******************************************************************************/
/*                                 R m d i r                                  */
/******************************************************************************/
  
int XrdPosixCache::Rmdir(const char* path)
                        {return theCache->Rmdir(path);}

/******************************************************************************/
/*                                R e n a m e                                 */
/******************************************************************************/
  
int XrdPosixCache::Rename(const char* oldPath, const char* newPath)
                         {return theCache->Rename(oldPath, newPath);}

/******************************************************************************/
/*                                  S t a t                                   */
/******************************************************************************/
  
int XrdPosixCache::Stat(const char *path, struct stat &sbuff)
                       {return theCache->Stat(path, sbuff);}

/******************************************************************************/
/*                            S t a t i s t i c s                             */
/******************************************************************************/

void XrdPosixCache::Statistics(XrdOucCacheStats &Stats)
                        {return theCache->Statistics.Get(Stats);}

/******************************************************************************/
/*                              T r u n c a t e                               */
/******************************************************************************/
  
int XrdPosixCache::Truncate(const char* path, off_t size)
                           {return theCache->Truncate(path, size);}

/******************************************************************************/
/*                                U n l i n k                                 */
/******************************************************************************/
  
int XrdPosixCache::Unlink(const char* path)
                         {return theCache->Unlink(path);}
