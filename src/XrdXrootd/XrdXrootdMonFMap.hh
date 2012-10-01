#ifndef __XRDXROOTDMONFMAP__
#define __XRDXROOTDMONFMAP__
/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d M o n F M a p . h h                    */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

class XrdXrootdFileStats;

class XrdXrootdMonFMap
{
public:

struct cvPtr {union {long cVal; cvPtr *cPtr; XrdXrootdFileStats *vPtr;};};

cvPtr              *fMap;
cvPtr               free;

static const int    mapNum = 128;
static const int    fmSize = 512;
static const int    fmHold = 31;
static const int    fmMask = 0x01ff;
static const int    fmShft = 9;

bool                Free(int slotNum);

int                 Insert(XrdXrootdFileStats  *fsP);

XrdXrootdFileStats *Next(int &slotNum);

                    XrdXrootdMonFMap() : fMap(0) {free.cVal = 0;}
                   ~XrdXrootdMonFMap() {}
private:

bool                Init();

static long         invVal;
static long         valVal;
};
#endif
