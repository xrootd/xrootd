/******************************************************************************/
/*                                                                            */
/*                      X r d S s i R e q u e s t . h h                       */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <stdlib.h>
#include <string.h>

#include "XrdSsi/XrdSsiRespInfo.hh"
#include "XrdSsi/XrdSsiRequest.hh"

/******************************************************************************/
/*                              C o p y D a t a                               */
/******************************************************************************/
  
bool XrdSsiRequest::CopyData(char *buff, int blen)
{
   bool last;

// Make sure the buffer length is valid
//
   if (blen <= 0)
      {eInfo.Set("Buffer length invalid", EINVAL);
       return false;
      }

// Check if we have any data here
//
   reqMutex.Lock();
   if (Resp.blen > 0)
      {if (Resp.blen > blen) last = false;
          else {blen = Resp.blen; last = true;}
       memcpy(buff, Resp.buff, blen);
       Resp.buff += blen; Resp.blen -= blen;
      } else {blen = 0; last = true;}
   reqMutex.UnLock();

// Invoke the callback
//
   ProcessResponseData(buff, blen, last);
   return true;
}
