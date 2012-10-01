#ifndef __SUT_BUCKET_H__
#define __SUT_BUCKET_H__
/******************************************************************************/
/*                                                                            */
/*                      X r d S u t B u c k e t . h h                         */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Gerri Ganis for CERN                                         */
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

#ifndef __SUT_STRING_H__
#include "XrdSut/XrdSutAux.hh"
#endif

class XrdOucString;

/******************************************************************************/
/*                                                                            */
/*  Unit for information exchange                                             */
/*                                                                            */
/******************************************************************************/

class XrdSutBucket
{
public:
   kXR_int32   type;
   kXR_int32   size;
   char       *buffer;

   XrdSutBucket(char *bp=0, int sz=0, int ty=0);
   XrdSutBucket(XrdOucString &s, int ty=0);
   XrdSutBucket(XrdSutBucket &b);
   virtual ~XrdSutBucket() {if (membuf) delete[] membuf;}

   void Update(char *nb = 0, int ns = 0, int ty = 0); // Uses 'nb'
   int Update(XrdOucString &s, int ty = 0);
   int SetBuf(const char *nb = 0, int ns = 0);         // Duplicates 'nb'

   void Dump(int opt = 1);
   void ToString(XrdOucString &s);

   // Equality operator
   int operator==(const XrdSutBucket &b);

   // Inequality operator
   int operator!=(const XrdSutBucket &b) { return !(*this == b); }

private:
   char *membuf;
};

#endif

