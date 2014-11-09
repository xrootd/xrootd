/******************************************************************************/
/*                                                                            */
/*                          X r d O u c S i d . c c                           */
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
/******************************************************************************/

#include "XrdOuc/XrdOucSid.hh"

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOucSid::XrdOucSid(int numSid, bool mtproof, XrdOucSid *glblSid)
{

// Set obvious values
//
   sidLock   = mtproof;
   globalSid = glblSid;

// Calculate actual values
//
   sidSize = (numSid / 8) + ((numSid % 8 ?  1 : 0) * 8);
   sidMax  = sidSize * 8;
   sidFree = 0;

// Allocate a sid table for the number we want
//
   sidVec = (unsigned char *)malloc(sidSize);
   memset(sidVec, 255, sidSize);
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdOucSid::~XrdOucSid()
{
   if (sidVec) free(sidVec);
}

/******************************************************************************/
/*                                O b t a i n                                 */
/******************************************************************************/
  
bool XrdOucSid::Obtain(XrdOucSid::theSid *sidP)
{
//                             0000 0001 0010 0011 0100 0101 0110 0111
   static const          //    1000 1001 1010 1011 1100 1101 1110 1111
   unsigned     char mask[] = {0x00,0x11,0x22,0x11,0x44,0x11,0x22,0x11,
                               0x88,0x11,0x22,0x11,0x44,0x11,0x22,0x11};
   bool aOK = true;

// Lock if need be
//
   if (sidLock) sidMutex.Lock();

// Realign the vector
//
   while(sidFree < sidSize && sidVec[sidFree] == 0x00) sidFree++;

// Allocate a local free sid if we actually have one free. Otherwise, try to
// allocate one from the global sid table.
//
   if (sidFree < sidSize)
      {int sidMask, sidNum, sidByte = sidVec[sidFree] & 0xff;
       if (sidByte & 0x0f)
          {sidMask = mask[sidByte & 0x0f] & 0x0f;
           sidNum  = (sidMask >  4 ? 3 :  sidMask>>1);
          } else {
           sidMask = mask[sidByte >> 4]   & 0xf0;
           sidNum  = (sidMask > 64 ? 7 : (sidMask>>5) + 4);
          }
       sidVec[sidFree] &= ~sidMask;
       sidP->sidS = (sidFree * 8) + sidNum;
      } else if (globalSid)
                {aOK = globalSid->Obtain(sidP);
                 sidP->sidS += sidMax;
                } else aOK = false;

// Unlock if locked and return result
//
   if (sidLock) sidMutex.UnLock();
   return aOK;
}

/******************************************************************************/
/*                               R e l e a s e                                */
/******************************************************************************/
  
bool XrdOucSid::Release(XrdOucSid::theSid *sidP)
{
   static const             //    0    1    2    3    4    5    6    7
   unsigned     char mask[] = {0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80};
   bool   aOK = true;

// Lock if need be
//
   if (sidLock) sidMutex.Lock();
  
// If this is a local sid, then handle it locally. Otherwise, try releasing
// using the global sid pool.
//
   if (sidP->sidS < sidMax)
      {int sidIdx = (sidP->sidS)>>3;
       sidVec[sidIdx] |= mask[(sidP->sidS)%8];
       if (sidIdx < sidFree) sidFree = sidIdx;
      } else if (globalSid)
                {short gSid = sidP->sidS - sidMax;
                 aOK = globalSid->Release((theSid *)&gSid);
                } else aOK = false;

// Unlock if locked and return result
//
   if (sidLock) sidMutex.UnLock();
   return aOK;
}

/******************************************************************************/
/*                                 R e s e t                                  */
/******************************************************************************/
  
void XrdOucSid::Reset()
{
   if (sidLock) sidMutex.Lock();
   if (sidVec) memset(sidVec, 0xff, sidSize);
   if (sidLock) sidMutex.UnLock();
}
