#ifndef __SFS_GPFILE_H__
#define __SFS_GPFILE_H__
/******************************************************************************/
/*                                                                            */
/*                       X r d S f s G P F i l e . h h                        */
/*                                                                            */
/* (c) 2019 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

class XrdSfsGPInfo;

class XrdSfsGPFile
{
public:

uint16_t      opts;    //!< Options as defined below
static const  uint16_t delegate = 0x0008; //!< Use delegation
static const  uint16_t keepErr  = 0x0004; //!< Keep file after request failure
static const  uint16_t mkPath   = 0x0002; //!< Create destination path.
static const  uint16_t replace  = 0x0001; //!< Replace existing file
static const  uint16_t useTLS   = 0x0080; //!< Use TLS for the data path
static const  uint16_t verCKS   = 0x0040; //!< Verify checksum after transfer

uint16_t      rsvd1;
uint8_t       pingsec; //!< Seconds between calls to Update() (0 -> no calls)
uint8_t       sources; //!< Number of parallel sources (0 -> default)
uint8_t       streams; //!< Number of parallel streams (0 -> default)
uint8_t       rsvd2;

union {
XrdSfsGPInfo *gpfInfo; //!< Can be used by the implementation
uint32_t      gpfID;   //!< Can be used by the implementation
      };

const char   *src;     //!< get: full URL,      put: local path
const char   *dst;     //!< get: local path,    put: full URL
const char   *lclCGI;  //!< The CGI, if any, for the local path.
const char   *csType;  //!< Checksum type
const char   *csVal;   //!< Checksum value as a hex string
const char   *tident;  //!< Trace identifier

void         *rsvd3;   //!< Reserved field

//-----------------------------------------------------------------------------
//! Indicate the request has finished.
//!
//! @param rc   - the final return code. A value of zero indicates success.
//!               A non-zero value should be the errno value corresponding
//!               to the reason for the failure.
//! @param emsg - An optional message further explaining the reason for the
//!               failure (highly recommended).
//!
//! @return No value is returned but this object is deleted and no references
//!         to the object should exist after return is made.
//-----------------------------------------------------------------------------

virtual void Finished(int rc, const char *emsg=0) = 0;

//-----------------------------------------------------------------------------
//! Provide request status. Only recursive locks should be held, if any.
//!
//! @param state - One of the enums listed indicating the request state.
//! @param cpct  - Percentage (0 to 100) of completion.
//! @param bytes - Number of bytes processed in the indicated state.
//-----------------------------------------------------------------------------

enum GPFState {gpfPend = 0,  //!< Request is pending
               gpfXfr,       //!< Request is transfering data
               gpfCSV        //!< Request is doing checksum validation
              };

virtual void Status(GPFState state, uint32_t cpct, uint64_t bytes) = 0;

//-----------------------------------------------------------------------------
//! Constructor and Destructor
//-----------------------------------------------------------------------------

             XrdSfsGPFile(const char *tid="")
                         : opts(0),    rsvd1(0),
                           pingsec(0), sources(0), streams(0),  rsvd2(0),
                           gpfInfo(0), src(0),     dst(0),      lclCGI(0),
                           csType(0),  csVal(0),   tident(tid), rsvd3(0) {}
virtual     ~XrdSfsGPFile() {}
};
#endif
