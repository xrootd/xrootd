#ifndef __XRDXROOTDGSREAL_HH_
#define __XRDXROOTDGSREAL_HH_
/******************************************************************************/
/*                                                                            */
/*                    X r d X r o o t d G S R e a l . h h                     */
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

#include "Xrd/XrdJob.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdXrootd/XrdXrootdGStream.hh"
#include "XrdXrootd/XrdXrootdMonData.hh"
#include "XrdXrootd/XrdXrootdMonitor.hh"
  
//-----------------------------------------------------------------------------
//! This class implements a generic reporter for the XRootD monitoring stream,
//! also known as the G-Stream. It's base class is passed around to various
//! plugins to allow them to add monitoring information into the G-Stream.
//-----------------------------------------------------------------------------

class XrdXrootdGSReal : public XrdJob, public XrdXrootdGStream
{
public:

void      DoIt(); // XrdJob override

void      Flush();

uint32_t  GetDictID(const char *text, bool isPath=false);

bool      Insert(const char *data, int dlen);

bool      Insert(int dlen);

char     *Reserve(int dlen);

int       SetAutoFlush(int afsec);

//-----------------------------------------------------------------------------
//! Constructor
//!
//! @param  gNamePI   the plugin name.
//! @param  gDataID   the G-Stream identifier associated with all of the data
//!                   that will be placed in the stream using this object.
//!                   See XrdXrootdMonData.hh for valid subtypes.
//! @param  mtype     the monitor type for send routing.
//! @param  flint     the autoflush interval.
//-----------------------------------------------------------------------------

          XrdXrootdGSReal(const char *gNamePI, char gDataID,
                          int mtype, int flint);

//-----------------------------------------------------------------------------
//! Destructor. Normally, this object is never deleted.
//-----------------------------------------------------------------------------

         ~XrdXrootdGSReal() {}

private:

void AutoFlush();
void Expel(int dlen);

XrdSysRecMutex         gMutex;
char                  *udpBFirst;
char                  *udpBNext;
char                  *udpBEnd;
int                    rsvbytes;
int                    monType;
int                    afTime;
bool                   afRunning;

XrdXrootdMonitor::User gMon;

struct GStream {XrdXrootdMonGS info;
                char           buff[64536-sizeof(info)];
               } gMsg;
};
#endif
