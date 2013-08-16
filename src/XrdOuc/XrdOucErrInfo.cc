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
/*                                 R e s e t                                  */
/******************************************************************************/
  
void XrdOucErrInfo::Reset()
{

// Free the appendage if it exists
//
   if (dataBuff != ErrInfo.message) buffData->Recycle();
   dataBuff = ErrInfo.message;
   *ErrInfo.message = 0;
    ErrInfo.code    = 0;
}

/******************************************************************************/
/*                               s e t D a t a                                */
/******************************************************************************/
  
void XrdOucErrInfo::setData(XrdOucBuffer *buffP)
{

// Free the appendage if it exists
//
   if (dataBuff != ErrInfo.message) buffData->Recycle();

// Set the reference
//
   buffData = buffP;
}
