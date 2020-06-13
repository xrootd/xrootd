#ifndef __SFS_XIO_IMPL_H__
#define __SFS_XIO_IMPL_H__
/******************************************************************************/
/*                                                                            */
/*                      X r d S f s X i o I m p l . h h                       */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdSfs/XrdSfsXio.hh"

//-----------------------------------------------------------------------------
//! This class is used to allow a class that inherits XrdSfsXio to specify
//! the implementation to be used for the static methods. It is passed to the
//! XrdSfsXio constructor. The static methost in XrdSfsXio use the method
//! pointers in the passed object to effect the desired action. This class
//! is meant to be a private interface for inherited objects. Note that the
//! reason some methods in XrdSfsXio need to be static because we wish to allow
//! the user of XrdSfsXio to call them irrespective of any instance.
//-----------------------------------------------------------------------------
  
class XrdSfsXioImpl
{
public:

typedef char* (*Buffer_t)(XrdSfsXioHandle, int *);
typedef void  (*Reclaim_t )(XrdSfsXioHandle);

//-----------------------------------------------------------------------------
//! Implementation of XrdSfsXio::Buffer(...).
//! Get the address and size of the buffer associated with a handle.
//-----------------------------------------------------------------------------

Buffer_t          Buffer;

//-----------------------------------------------------------------------------
//! Implementation of XrdSfsXio::Reclaim(...).
//-----------------------------------------------------------------------------

Reclaim_t         Reclaim;

//-----------------------------------------------------------------------------
//! Constructor
//!
//! @param buff_func  Pointer to the Buffer()  implementation.
//! @param recl_func  Pointer to the Reclaim() implementation.
//-----------------------------------------------------------------------------


             XrdSfsXioImpl(Buffer_t buff_func, Reclaim_t recl_func)
                          {Buffer  = buff_func;
                           Reclaim = recl_func;
                          }

//-----------------------------------------------------------------------------
//! Destructor
//-----------------------------------------------------------------------------

            ~XrdSfsXioImpl() {}
};
#endif
