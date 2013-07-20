/******************************************************************************/
/*                                                                            */
/*                      X r d O u c E r r I n f o . c c                       */
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
/*                                                                            */
/******************************************************************************/

#include <stdlib.h>

#include "XrdOuc/XrdOucErrInfo.hh"

/******************************************************************************/
/*                               c p y D a t a                                */
/******************************************************************************/
  
bool XrdOucErrInfo::cpyData(const char *dataP, int dlen)
{

// Get the length of the data if not supplied
//
   if (dlen < 0) dlen = strlen(dataP)+1;
   if (dlen > maxDataLen) return false;

// Check if we can fit this in the current buffer. If it's an appendage then
// we already know that it's at least Max_Error_Len in size.
//
   if (dlen <= (int)XrdOucEI::Max_Error_Len || dlen <= dataBLen)
      {memcpy(dataBuff, dataP, dlen);
       dataBLen = dlen;
       return true;
      }

// Free the appendage if it exists
//
   if (dataBuff != ErrInfo.message) free(dataBuff);

// Allocate the new buffer
//
   if (!(dataBuff = (char *)malloc(dlen)))
      {dataBuff = ErrInfo.message;
       dataBLen = -1;
       return false;
      }

// Copy the data and return
//
   memcpy(dataBuff, dataP, dlen);
   dataBLen = dlen;
   return true;
}

/******************************************************************************/
/*                                 R e s e t                                  */
/******************************************************************************/
  
void XrdOucErrInfo::Reset()
{

// Free the appendage if it exists
//
   if (dataBuff != ErrInfo.message) free(dataBuff);

// Reset things to the beginning
//
   dataBuff = ErrInfo.message;
   dataBLen = -1;
   *ErrInfo.message = 0;
    ErrInfo.code    = 0;
}

/******************************************************************************/
/*                               s e t D a t a                                */
/******************************************************************************/
  
bool XrdOucErrInfo::setData(char *dataP, int dlen)
{

// Validate the length
//
   if (dlen < 0 || dlen > maxDataLen) return false;

// Free the appendage if it exists
//
   if (dataBuff != ErrInfo.message) free(dataBuff);

// Set the reference
//
   dataBuff = dataP;
   dataBLen = dlen;
   return true;
}
