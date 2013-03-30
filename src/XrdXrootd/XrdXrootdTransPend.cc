/******************************************************************************/
/*                                                                            */
/*                    X r d X r o o t d B r i d g e . h h                     */
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

#include <string.h>

#include "XrdXrootd/XrdXrootdTransit.hh"
#include "XrdXrootd/XrdXrootdTransPend.hh"

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

XrdSysMutex         XrdXrootdTransPend::myMutex;

XrdXrootdTransPend *XrdXrootdTransPend::rqstQ = 0;

/******************************************************************************/
/*                                 C l e a r                                  */
/******************************************************************************/
  
void XrdXrootdTransPend::Clear(XrdXrootdTransit *trP)
{
   XrdXrootdTransPend *tpP, *tpN, *tpX;

// Lock this operations
//
   myMutex.Lock();

// Run through the queue deleting all elements owned by the transit object
//
   tpP = 0; tpN = rqstQ;
   while(tpN)
        {if (tpN->bridge == trP)
            {if (tpP) tpP->next = tpN->next;
                else  rqstQ     = tpN->next;
             tpX = tpN; tpN = tpN->next; delete tpX;
            } else {
             tpP = tpN; tpN = tpN->next;
           }
        }

// All done
//
   myMutex.UnLock();
}

/******************************************************************************/
/*                                 Q u e u e                                  */
/******************************************************************************/

void XrdXrootdTransPend::Queue()
{
// Now place it on out pending queue
//
   myMutex.Lock();
   next = rqstQ; rqstQ = this;
   myMutex.UnLock();
}
  
/******************************************************************************/
/*                                R e m o v e                                 */
/******************************************************************************/
  
XrdXrootdTransPend *XrdXrootdTransPend::Remove(XrdLink *lP, short sid)
{
   XrdXrootdTransPend *tpP, *tpN;

// Lock this operations
//
   myMutex.Lock();

// Run through the queue and remove matching element
//
   tpP = 0; tpN = rqstQ;
   while(tpN)
        {if (tpN->link == lP && tpN->Pend.theSid == sid)
            {if (tpP) tpP->next = tpN->next;
                else  rqstQ     = tpN->next;
             break;
            } else {
             tpP = tpN; tpN = tpN->next;
           }
        }

// All done
//
   myMutex.UnLock();
   return tpN;
}
