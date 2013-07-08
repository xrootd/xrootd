/******************************************************************************/
/*                                                                            */
/*                 X r d X r o o t d T r a n s S e n d . c c                  */
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

#include "Xrd/XrdLink.hh"
#include "XrdXrootd/XrdXrootdTransSend.hh"

/******************************************************************************/
/*                                  S e n d                                   */
/******************************************************************************/

int XrdXrootdTransSend::Send(const struct iovec *headP, int headN,
                             const struct iovec *tailP, int tailN)
{
   XrdLink::sfVec *sfVec;
   int i, k = 0, numV = headN + tailN + 1;

// Allocate a new sfVec to accomodate all the items
//
   if (sfFD >= 0) sfVec = new XrdLink::sfVec[numV];
      else        sfVec = new XrdLink::sfVec[numV-sfFD];

// Copy the headers
//
   if (headP) for (i = 0; i < headN; i++, k++)
      {sfVec[k].buffer = (char *)headP[i].iov_base;
       sfVec[k].sendsz = headP[i].iov_len;
       sfVec[k].fdnum  = -1;
      }

// Insert the sendfile request
//
   if (sfFD >= 0)
      {sfVec[k].offset = sfOff;
       sfVec[k].sendsz = sfLen;
       sfVec[k].fdnum  = sfFD;
       k++;
      } else {
       for (i = 1; i < -sfFD; i++)
           {sfVec[k  ].offset = sfVP[i].offset;
            sfVec[k  ].sendsz = sfVP[i].sendsz;
            sfVec[k++].fdnum  = sfVP[i].fdnum;
           }
      }

// Copy the trailer
//
   if (tailP) for (i = 0; i < tailN; i++, k++)
      {sfVec[k].buffer = (char *)tailP[i].iov_base;
       sfVec[k].sendsz = tailP[i].iov_len;
       sfVec[k].fdnum  = -1;
      }

// Issue sendfile request
//
   k = linkP->Send(sfVec, numV);

// Deallocate the vector and return the result
//
   delete [] sfVec;
   return (k < 0 ? -1 : 0);
}
