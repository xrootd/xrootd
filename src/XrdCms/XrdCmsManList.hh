#ifndef __CMS_MANLIST__H
#define __CMS_MANLIST__H
/******************************************************************************/
/*                                                                            */
/*                      X r d C m s M a n L i s t . h h                       */
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

#include "XrdSys/XrdSysPthread.hh"

class XrdCmsManRef;
class XrdOucTList;
class XrdNetAddr;

class XrdCmsManList
{
public:

// Add() adds an alternate manager to the list of managers (duplicates not added)
//       Previous entries for netAddr are removed before addition.
//
void     Add(const XrdNetAddr *netAddr, char *redList, int manport, int lvl);

// Del() removes all entries added under refp
//
void     Del(const XrdNetAddr *refP) {Del(getRef(refP));}

void     Del(int ref);

// Get a reference number for a manager
//
int      getRef(const XrdNetAddr *refP);

// haveAlts() returns true if alternates exist, false otherwise
//
int      haveAlts() {return allMans != 0;}

// Next() returns the next manager in the list and its level or 0 if none are left.
//        The next call to Next() will return the first manager in the list.
//
int      Next(int &port, char *buff, int bsz);

         XrdCmsManList() {allMans = nextMan = 0;}
        ~XrdCmsManList();

private:
void Add(int ref, char *manp, int manport, int lvl);

XrdSysMutex   refMutex;
XrdOucTList  *refList;

XrdSysMutex   mlMutex;
XrdCmsManRef *nextMan;
XrdCmsManRef *allMans;
};
#endif
