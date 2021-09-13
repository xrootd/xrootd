#ifndef __FRCUTILS__HH
#define __FRCUTILS__HH
/******************************************************************************/
/*                                                                            */
/*                        X r d F r c U t i l s . h h                         */
/*                                                                            */
/* (c) 2009 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdlib>
#include <ctime>

#include "XrdFrc/XrdFrcRequest.hh"

class  XrdFrcXAttrPin;

class XrdFrcUtils
{
public:

static       char  Ask(char dflt, const char *Msg1, const char *Msg2="",
                                  const char *Msg3="");

static       int   chkURL(const char *Url);

static       char *makePath(const char *iName, const char *Path, int Mode);

static       char *makeQDir(const char *Path, int Mode);

static       int   MapM2O(const char *Nop, const char *Pop);

static       int   MapR2Q(char Opc, int *Flags=0);

static       int   MapV2I(const char *Opc, XrdFrcRequest::Item &ICode);

static       int   Unique(const char *lkfn, const char *myProg);

static       int   updtCpy(const char *Pfn, int Adj);

static       int   Utime(const char *Path, time_t tVal);

                   XrdFrcUtils() {}
                  ~XrdFrcUtils() {}
private:
};
#endif
