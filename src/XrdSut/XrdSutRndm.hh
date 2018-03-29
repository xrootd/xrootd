#ifndef __SUT_RNDM_H__
#define __SUT_RNDM_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d S u t R n d m . h h                           */
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

#ifndef __SUT_AUX_H__
#include "XrdSut/XrdSutAux.hh"
#endif

/******************************************************************************/
/*                                                                            */
/*  Provider of random bunches of bits                                        */
/*                                                                            */
/******************************************************************************/

class XrdOucString;

class XrdSutRndm {

public:
   static bool   fgInit;

   XrdSutRndm() { if (!fgInit) fgInit = XrdSutRndm::Init(); }
   virtual ~XrdSutRndm() { }

   // Initializer
   static bool   Init(bool force = 0);

   // Buffer provider
   static char  *GetBuffer(int len, int opt = -1);
   // String provider
   static int    GetString(int opt, int len, XrdOucString &s);
   static int    GetString(const char *copt, int len, XrdOucString &s);
   // Integer providers
   static unsigned int GetUInt();
   // Random Tag
   static int    GetRndmTag(XrdOucString &rtag);
}
;

#endif

