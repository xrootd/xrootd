/******************************************************************************/
/*                                                                            */
/*                       X r d C m s R R D a t a . c c                        */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <unistd.h>

#include "XrdCms/XrdCmsRRData.hh"

#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPthread.hh"

using namespace XrdCms;

/******************************************************************************/
/*                               g e t B u f f                                */
/******************************************************************************/
  
int XrdCmsRRData::getBuff(size_t bsz)
{  
   static size_t PageSize  = sysconf(_SC_PAGESIZE);
          size_t Alignment = PageSize;

   if (bsz < Alignment)
      {if (bsz <= 8) Alignment = bsz = 8;
          else {do {Alignment = Alignment >> 1;} while(bsz < Alignment);
                Alignment = Alignment << 1; bsz = Alignment;
               }
      }

   if (Buff) free(Buff);
   if (posix_memalign((void **)&Buff, Alignment, bsz))
      {Buff = 0; return 0;}

   Blen = bsz;
   return 1;
}

/******************************************************************************/
/*                             O b j e c t i f y                              */
/******************************************************************************/
  
XrdCmsRRData *XrdCmsRRData::Objectify(XrdCmsRRData *op)
{
   static XrdSysMutex   myMutex;
   static XrdCmsRRData *Free;

// Obtain a new object or recycle an old one
//
   myMutex.Lock();
   if (op) {op->Next = Free; Free = op; op = 0;}
      else {if ((op = Free)) Free = op->Next;
               else {op = new XrdCmsRRData; op->Buff = 0; op->Blen = 0;}
            op->Ident = 0; op->Next = 0;
           }

   myMutex.UnLock();

   return op;
}
