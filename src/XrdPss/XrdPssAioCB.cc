/******************************************************************************/
/*                                                                            */
/*                        X r d P s s A i o C B . c c                         */
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

#include <cerrno>

#include "XrdPss/XrdPssAioCB.hh"
#include "XrdSfs/XrdSfsAio.hh"

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

XrdSysMutex  XrdPssAioCB::myMutex;
XrdPssAioCB *XrdPssAioCB::freeCB   =   0;
int          XrdPssAioCB::numFree  =   0;
int          XrdPssAioCB::maxFree  = 100;
  
/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/

XrdPssAioCB *XrdPssAioCB::Alloc(XrdSfsAio *aiop, bool isWr, bool pgrw)
{
   XrdPssAioCB *newCB;

// Try to allocate an prexisting object otherwise get a new one
//
   myMutex.Lock();
   if ((newCB = freeCB)) {freeCB = newCB->next; numFree--;}
      else newCB = new XrdPssAioCB;
   myMutex.UnLock();

// Initialize the callback and return it
//
   newCB->theAIOP = aiop;
   newCB->isWrite = isWr;
   newCB->isPGrw  = pgrw;
   return newCB;
}

/******************************************************************************/
/*                              C o m p l e t e                               */
/******************************************************************************/
  
#include <iostream>
void XrdPssAioCB::Complete(ssize_t result)
{

// Set correct result
//
// std::cerr <<"AIO fin " <<(isWrite ? " write ":" read ")
//           <<theAIOP->sfsAio.aio_nbytes <<'@' <<theAIOP->sfsAio.aio_offset
//           <<" result " <<result <<std::endl;
   theAIOP->Result = (result < 0 ? -errno : result);

// Perform post processing for pgRead or pgWrite if successful
//
   if (isPGrw && result >= 0)
      {if (isWrite)
          {
          } else {
           if (csVec.size() && theAIOP->cksVec)
           memcpy(theAIOP->cksVec, csVec.data(), csVec.size()*sizeof(uint32_t));
          }
      }

// Invoke the callback
//
   if (isWrite) theAIOP->doneWrite();
      else      theAIOP->doneRead();

// Now recycle ourselves
//
   Recycle();
}

/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/
  
void XrdPssAioCB::Recycle()
{
// Perform recycling
//
   myMutex.Lock();
   if (numFree >= maxFree) delete this;
      else {next   = freeCB;
            freeCB = this;
            numFree++;
            csVec.clear();
           }
   myMutex.UnLock();
}
