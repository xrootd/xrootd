/******************************************************************************/
/*                                                                            */
/*                        X r d P o s i x D i r . c c                         */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
/* Modified by Frank Winklmeier to add the full Posix file system definition. */
/******************************************************************************/

#include "XrdPosix/XrdPosixDir.hh"
#include "XrdPosix/XrdPosixMap.hh"

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace XrdPosixGlobals
{
extern XrdCl::DirListFlags::Flags dlFlag;
};
  
/******************************************************************************/
/*                             n e x t E n t r y                              */
/******************************************************************************/

dirent64 *XrdPosixDir::nextEntry(dirent64 *dp)
{
   XrdCl::DirectoryList::ListEntry *dirEnt;
   const char *d_name;
   const int dirhdrln = dp->d_name - (char *)dp;
   size_t d_nlen;

// Reread the directory if we need to (rewind forces this)
//
   if (!myDirVec && !Open()) {eNum = errno; return 0;}

// Check if dir is empty or all entries have been read
//
   if (nxtEnt >= numEnt) {eNum = 0; return 0;}

// Get information about the next entry
//
   dirEnt = myDirVec->At(nxtEnt);
   d_name = dirEnt->GetName().c_str();
   d_nlen = dirEnt->GetName().length();

// Create a directory entry
//
   if (!dp) dp = myDirEnt;
   if (d_nlen > maxDlen) d_nlen = maxDlen;
#ifndef __solaris__
   dp->d_type   = DT_DIR;
#endif
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__GNU__) || (defined(__FreeBSD_kernel__) && defined(__GLIBC__))
   dp->d_fileno = nxtEnt;
   dp->d_namlen = d_nlen;
#else
   dp->d_ino    = nxtEnt+1;
   dp->d_off    = nxtEnt;
#endif
   dp->d_reclen = d_nlen + dirhdrln;
   strncpy(dp->d_name, d_name, d_nlen);
   dp->d_name[d_nlen] = '\0';
   nxtEnt++;
   return dp;
}
  
/******************************************************************************/
/*                                  O p e n                                   */
/******************************************************************************/
  
DIR *XrdPosixDir::Open()
{
   static const size_t dEntSize = sizeof(dirent64) + maxDlen + 1;
   int rc;

// Allocate a local dirent. Note that we get additional padding because on
// some system the dirent structure does not include the name buffer
//
   if (!myDirEnt && !(myDirEnt = (dirent64 *)malloc(dEntSize)))
      {errno = ENOMEM; return (DIR *)0;}

// Get the directory list
//
   rc = XrdPosixMap::Result(DAdmin.Xrd.DirList(DAdmin.Url.GetPathWithParams(),
                                               XrdPosixGlobals::dlFlag,
                                               myDirVec, (uint16_t)0));

// If we failed, return a zero pointer
//
   if (rc) return (DIR *)0;

// Finish up
//
   numEnt = myDirVec->GetSize();
   return (DIR *)&fdNum;
}
