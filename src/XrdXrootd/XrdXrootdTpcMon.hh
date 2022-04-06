#ifndef __XRDXROOTDTPCMON_HH__
#define __XRDXROOTDTPCMON_HH__
/******************************************************************************/
/*                                                                            */
/*                    X r d X r o o t d T p c M o n . h h                     */
/*                                                                            */
/* (c) 2022 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <stddef.h>
#include <time.h>
#include <sys/time.h>

class  XrdSysLogger;
class  XrdXrootdGStream;

class XrdXrootdTpcMon
{
public:

struct TpcInfo
{
const char*      clID;   // Client ID
struct timeval   begT;   // gettimeofday copy started
struct timeval   endT;   // gettimeofday copy ended
const char*      srcURL; // The source URL used
const char*      dstURL; // The destination URL used
size_t           fSize;  // The size of the file
int              endRC;  // Ending return code (0 means success)
unsigned short   opts;   // Additional information:
unsigned char    strm;   // Number of streams used
unsigned char    rsvd;   // Reserved

static const int isaPush = 0x0001; // opts: Push request otherwise a pull
static const int isIPv4  = 0x0002; // opts: Used IPv4 for xfr else IPv6.

void             Init() {clID  = "";
                         begT.tv_sec = 0; begT.tv_usec = 0;
                         endT.tv_sec = 0; endT.tv_usec = 0;
                         srcURL = "";     dstURL = "";
                         fSize  = 0;      endRC  = 0,
                         opts   = 0;      strm = 1;     rsvd = 0;
                        }

                 TpcInfo() {Init();}

                ~TpcInfo() {}
};

//-----------------------------------------------------------------------------
//! Report a TPC event.
//!
//! @param  info   - Reference to the filled in TpcInfo object.
//-----------------------------------------------------------------------------

void        Report(TpcInfo &info);

//-----------------------------------------------------------------------------
//! Constructor
//!
//! @param  proto  - the protocol being used for this object.
//! @param  logP   - Pointer to the logging object.
//! @param  gStrm  - Reference to the gStream to be used for reporting info.
//-----------------------------------------------------------------------------

            XrdXrootdTpcMon(const char *proto, XrdSysLogger* logP,
                            XrdXrootdGStream& gStrm);

private:

//-----------------------------------------------------------------------------
//! Destructor - This object cannot be destroyed.
//-----------------------------------------------------------------------------

           ~XrdXrootdTpcMon() {}

const char *getURL(const char *spec, const char *prot, char *buff, int bsz);
const char* getUTC(struct timeval& tod, char* utcBuff, int utcBLen);

const char*       protocol;
XrdXrootdGStream& gStream;
};
#endif
