/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d M o n F M a p . h h                    */
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
#include <string.h>
#include <unistd.h>

#include "XrdSys/XrdSysPlatform.hh"

#include "XrdXrootd/XrdXrootdFileStats.hh"
#include "XrdXrootd/XrdXrootdMonFMap.hh"

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/
  
long XrdXrootdMonFMap::invVal =  1;
long XrdXrootdMonFMap::valVal = ~1;

/******************************************************************************/
/*                                  F r e e                                   */
/******************************************************************************/
  
bool XrdXrootdMonFMap::Free(int slotNum)
{
// Validate the data before freeing the slot
//
   if (!fMap || slotNum < 0 || slotNum >= fmSize || fMap[slotNum].cVal & invVal)
      return false;

// Plase this entry on the free list
//
   fMap[slotNum].cPtr  = free.cPtr;
   fMap[slotNum].cVal |= invVal;
   free.cPtr           = &fMap[slotNum];
   return true;
}

/******************************************************************************/
/* Private:                         I n i t                                   */
/******************************************************************************/
  
bool XrdXrootdMonFMap::Init()
{
   static const int bytes = fmSize * sizeof(cvPtr);
   static       int pagsz = getpagesize();
   void *mPtr;
   int  alignment, i;

// Allocate memory
//
   alignment = (bytes < pagsz ? 1024 : pagsz);
   if (posix_memalign(&mPtr, alignment, bytes)) return false;
   fMap = (cvPtr *)mPtr;

// Chain all the entries together
//
   for (i = 1; i < fmSize; i++)
       {fMap[i-1].cPtr = &fMap[i];
        fMap[i-1].cVal |= invVal;
       }
   fMap[fmSize-1].cVal = invVal;
   free.cPtr           = &fMap[0];
   return true;
}

/******************************************************************************/
/*                                I n s e r t                                 */
/******************************************************************************/
  
int XrdXrootdMonFMap::Insert(XrdXrootdFileStats *fsP)
{
   cvPtr *mEnt;

// Check if we have a free slot available
//
   if (!free.cVal) {if (fMap || !Init()) return -1;}

// Return the free slot (Init() gaurantees one is available)
//
   mEnt       =  free.cPtr;
   free.cPtr  =  free.cPtr->cPtr;
   free.cVal &=  valVal;
   mEnt->vPtr =  fsP;
   return mEnt - fMap;
}

/******************************************************************************/
/*                                  N e x t                                   */
/******************************************************************************/
  
XrdXrootdFileStats *XrdXrootdMonFMap::Next(int &slotNum)
{

// Return next valid pointer
//
   for (; slotNum < fmSize-1; slotNum++)
        {if (!(fMap[slotNum].cVal & invVal)) return fMap[slotNum++].vPtr;}

// At the end of the map
//
   return 0;
}
