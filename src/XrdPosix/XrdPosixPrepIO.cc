/******************************************************************************/
/*                                                                            */
/*                     X r d P o s i x P r e p I O . c c                      */
/*                                                                            */
/* (c) 2016 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdPosix/XrdPosixObjGuard.hh"
#include "XrdPosix/XrdPosixPrepIO.hh"
#include "XrdPosix/XrdPosixStats.hh"
#include "XrdPosix/XrdPosixTrace.hh"

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace XrdPosixGlobals
{
extern XrdOucCache   *theCache;
extern XrdPosixStats  Stats;
};
  
/******************************************************************************/
/*                               D i s a b l e                                */
/******************************************************************************/
  
void XrdPosixPrepIO::Disable()
{
   EPNAME("PrepIODisable");
   XrdPosixObjGuard objGuard(fileP);

   DEBUG("Disabling deferred open "<<fileP->Origin());

   openRC = -ESHUTDOWN;
}
  
/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
bool XrdPosixPrepIO::Init(XrdOucCacheIOCB *iocbP)
{
   EPNAME("PrepIOInit");
   XrdPosixObjGuard objGuard(fileP);
   XrdCl::XRootDStatus Status;
   static int maxCalls = 64;

// Count the number of entries here. We want to catch someone ignoring the
// Update() call and using this object as it is very ineffecient.
//
   if (iCalls++ >= maxCalls)
      {maxCalls = maxCalls*2;
       DMSG("Init", iCalls <<" unexpected PrepIO calls!");
      }

// Do not try to open the file if there was previous error
//
   if (openRC) return false;

// Check if the file is already opened. This caller may be vestigial
//
   if (fileP->clFile.IsOpen()) return true;

// Count number of opens after the open was deferred (successful or not)
//
   if (XrdPosixGlobals::theCache)
       XrdPosixGlobals::theCache->Statistics.Count(
         (XrdPosixGlobals::theCache->Statistics.X.OpenDefers));

// Open the file. It is too difficult to do an async open here as there is a
// possible pending async request and doing both is not easy at all.
//
   Status = fileP->clFile.Open((std::string)fileP->Origin(), clFlags, clMode);
   XrdPosixGlobals::Stats.Count((XrdPosixGlobals::Stats.X.Opens));

// Make sure all went well. If so, do a Stat() call on the underlying file
//
   if (Status.IsOK()) fileP->Stat(Status);
      else {openRC = XrdPosixMap::Result(Status, false);
            if (DEBUGON && errno != ENOENT && errno != ELOOP)
               {std::string eTxt = Status.ToString();
                DEBUG(eTxt<<" deferred open "<<fileP->Origin());
               }
            XrdPosixGlobals::Stats.Count(XrdPosixGlobals::Stats.X.OpenErrs);
            return false;
           }

// Inform the cache that we have now have a new I/O object
//
   fileP->XCio->Update(*fileP);
   return true;
}
