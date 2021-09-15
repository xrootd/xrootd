/******************************************************************************/
/*                                                                            */
/*                        X r d O u c C a c h e . c c                         */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include "XrdOuc/XrdOucPgrwUtils.hh"
#include "XrdSys/XrdSysPageSize.hh"

/******************************************************************************/
/*                                p g R e a d                                 */
/******************************************************************************/

int XrdOucCacheIO::pgRead(char                  *buff,
                          long long              offs,
                          int                    rdlen,
                          std::vector<uint32_t> &csvec,
                          uint64_t               opts,
                          int                   *csfix)
{
   int bytes;

// Read the data into the buffer
//
   bytes = Read(buff, offs, rdlen);

// Calculate checksums if so wanted
//
   if (bytes > 0 && (opts & forceCS))
       XrdOucPgrwUtils::csCalc((const char *)buff, (ssize_t)offs,
                               (size_t)bytes, csvec);

// All done
//
   return bytes;
}

/******************************************************************************/
/*                               p g W r i t e                                */
/******************************************************************************/

int XrdOucCacheIO::pgWrite(char                  *buff,
                           long long              offs,
                           int                    wrlen,
                           std::vector<uint32_t> &csvec,
                           uint64_t               opts,
                           int                   *csfix)
{
// Now just return the result of a plain write
//
   return Write(buff, offs, wrlen);
}

/******************************************************************************/
/*                                 R e a d V                                  */
/******************************************************************************/
  
int XrdOucCacheIO::ReadV(const XrdOucIOVec *readV, int rnum)
{
   int nbytes = 0, curCount = 0;

   for (int i = 0; i < rnum; i++)
       {curCount = Read(readV[i].data, readV[i].offset, readV[i].size);
        if (curCount != readV[i].size)
           return (curCount < 0 ? curCount : -ESPIPE);
        nbytes += curCount;
       }
   return nbytes;
}

/******************************************************************************/
/*                                W r i t e V                                 */
/******************************************************************************/
  
int XrdOucCacheIO::WriteV(const XrdOucIOVec *writV, int wnum)
{
   int nbytes = 0, curCount = 0;

   for (int i = 0; i < wnum; i++)
       {curCount = Write(writV[i].data, writV[i].offset, writV[i].size);
        if (curCount != writV[i].size)
           {if (curCount < 0) return curCount;
            return -ESPIPE;
           }
        nbytes += curCount;
       }
   return nbytes;
}
