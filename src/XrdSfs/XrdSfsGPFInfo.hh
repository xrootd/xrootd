#ifndef __XRDSFSGPFINFO_H__
#define __XRDSFSGPFINFO_H__
/******************************************************************************/
/*                                                                            */
/*                      X r d S f s G P F I n f o . h h                       */
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

#include <cstdint>
#include <string>

//! Class XrdSfsGPFInfo is used to control the execution of the GetFile()
//! and putFile() methods in XrdSfsInterface. An nstance of this class is
//! passed to the correspondng method specifying what has to be done and how
//! results are to be communicated back.

/******************************************************************************/
/*                   C l a s s   X r d S f s G P F I n f o                    */
/******************************************************************************/
  
//------------------------------------------------------------------------------
//! The XrdXrdSfsGPFInfo class contains the get/putFile() parameters and
//! contains callback methods that indicate when the operation completes as
//! well as for progress status updates.
//------------------------------------------------------------------------------

class XrdSfsGPFInfo
{
public:

std::string  cksType;      //!< Checksum    type  or empty if none wanted
std::string  cksValue;     //!< Checksum    value as ASCII hexdecimal string
std::string  src;          //!< Source      getFile: URL  putFile: path
std::string  srcCgi;       //!< Source      cgi or empty if none
std::string  dst;          //!< Destination getFile: path putFile: URL
std::string  dstCgi;       //!< Destination cgi or empty if none.
const char  *tident;       //!< Trace identifier
void        *rsvd1;        //!< Reserved for future use
uint16_t     options;      //!< Processing options
uint16_t     rsvd2;        //!< Reserved for future use.
uint8_t      pingsec;      //!< Seconds between calls to Update() (0 -> no calls)
uint8_t      sources;      //!< Number of parallel sources (0 -> default)
uint8_t      streams;      //!< Number of parallel streams (0 -> default)
uint8_t      rsvd3;        //!< Reserved for future use.

//------------------------------------------------------------------------------
//! Possible options.
//------------------------------------------------------------------------------

static const uint16_t Keep     = 0x0001;  //!< Do not remove file upon failure.
static const uint16_t Replace  = 0x0002;  //<! Replace identically named file.
static const uint16_t Delegate = 0x0004;  //!< Use delegated credentials.
static const uint16_t mkPath   = 0x0008;  //!< Create destination path.
static const uint16_t useTLS   = 0x0010;  //!< Use TLS for the data path

//------------------------------------------------------------------------------
//! Indicate that an accepted get/putFile requtest has completed. This must
//! be called at completion. Upon return this object is deleted.
//!
//! @param  eMsg   - A text string describing the problem if in error. If no
//!                  error was encounteredm a nil pointer should be passed.
//! @param  eNum   - The errno value corresponding to the error type. A value
//!                  zero indicates that the copy successfully completed.
//!
//! @return true   - Completion sent to client.
//! @return false  - Client is no longer connected, completion not sent.
//------------------------------------------------------------------------------

virtual bool Completed(const char *eMsg=0, int eNum=0) = 0;

//------------------------------------------------------------------------------
//! Supply status information to the client. This is normally done every
//! XrdSfsGPFInfo::pingsec seconds.
//!
//! @param  bytes  - The number of bytes processed (i.e. xfer or checked).
//! @param  stage  - One of XStage indicating execution stage.
//! @param  percent- Percentage of execution stage completed.
//!
//! @return true   - Status sent to client.
//! @return false  - Client is no longer connected, status not sent.
//------------------------------------------------------------------------------

enum XStage {isPending = 0, //!< Copy operation is pending
             isCopying = 1, //!< Copy operation in progress
             isVerCSum = 2  //!< Copy operation is verifying checksum
            };

virtual bool Update(uint64_t bytes, XStage stage, uint8_t percent) = 0;

/******************************************************************************/
/*              C o n s t r u c t o r   &   D e s t r u c t o r               */
/******************************************************************************/
  
         XrdSfsGPFInfo() : tident(""), rsvd1(0),   options(0), rsvd2(0),
                           pingsec(0), sources(0), streams(0), rsvd3(0) {}

virtual ~XrdSfsGPFInfo() {}
};
#endif
