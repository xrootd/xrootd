/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d G S t r e a m . c c                    */
/*                                                                            */
/* (c) 2019 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdXrootd/XrdXrootdGStream.hh"
#include "XrdXrootd/XrdXrootdGSReal.hh"

/******************************************************************************/
/*                                 F l u s h                                  */
/******************************************************************************/
  
void      XrdXrootdGStream::Flush() {gStream.Flush();}
  
/******************************************************************************/
/*                             G e t D i c t I D                              */
/******************************************************************************/
  
uint32_t  XrdXrootdGStream::GetDictID(const char *text, bool isPath)
                           {return gStream.GetDictID(text, isPath);}

/******************************************************************************/
/*                                H a s H d r                                 */
/******************************************************************************/

bool      XrdXrootdGStream::HasHdr()
                           {return gStream.HasHdr();}
  
/******************************************************************************/
/*                                I n s e r t                                 */
/******************************************************************************/

bool      XrdXrootdGStream::Insert(const char *data, int dlen)
                           {return gStream.Insert(data, dlen);}

bool      XrdXrootdGStream::Insert(int dlen) {return gStream.Insert(dlen);}

/******************************************************************************/
/*                               R e s e r v e                                */
/******************************************************************************/
  
char     *XrdXrootdGStream::Reserve(int dlen) {return gStream.Reserve(dlen);}

/******************************************************************************/
/*                          S e t A u t o F l u s h                           */
/******************************************************************************/
  
int       XrdXrootdGStream::SetAutoFlush(int afsec)
                           {if (afsec < 0) afsec = 0;
                               else if (afsec < 60) afsec = 60;
                            return gStream.SetAutoFlush(afsec);
                           }

/******************************************************************************/
/*                                 S p a c e                                  */
/******************************************************************************/

int       XrdXrootdGStream::Space()
                           {return gStream.Space();}
