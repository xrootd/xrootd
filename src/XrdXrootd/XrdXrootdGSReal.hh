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

class XrdNetMsg;
class XrdSysError;

class XrdXrootdGSReal : public XrdJob, public XrdXrootdGStream,
                        public XrdXrootdMonitor::Hello
{
public:

void      DoIt(); // XrdJob override

void      Flush();

uint32_t  GetDictID(const char *text, bool isPath=false);

bool      HasHdr();

void      Ident();

bool      Insert(const char *data, int dlen);

bool      Insert(int dlen);

char     *Reserve(int dlen);

int       SetAutoFlush(int afsec);

int       Space();

//-----------------------------------------------------------------------------
//! Constructor
//!
//! @param  gsParms   the stream parameters as defined by GSParms.
//! @param  aOK       reference to a boolean which will contain true on success
//!                   or will be set to false, otherwise.
//-----------------------------------------------------------------------------

   static const int fmtNone = 0;       //! Do not include info
   static const int fmtBin  = 1;       //! Format as binary info
   static const int fmtCgi  = 2;       //! Format as CGI    info
   static const int fmtJson = 3;       //! Format as JSON   info

   static const int hdrNone = 0;       //!< Do not include header
   static const int hdrNorm = 1;       //!< Include standard header
   static const int hdrSite = 2;       //!< Include site
   static const int hdrHost = 3;       //!< Include site, host
   static const int hdrInst = 4;       //!< Include site, host, port, inst
   static const int hdrFull = 5;       //!< Include site, host, port, inst, pgm

   static const int optNoID = 0x01;    //!< Don't send ident records

   struct GSParms {const char *pin;    //!< the plugin name.
                   const char *dest;   //!< Destination for records
                   int         Mode;   //!< the monitor type for send routing.
                   int         maxL;   //!< Maximum packet length (default 32K)
                   int         flsT;   //!< Flush time (default from monitor)
                   kXR_char    Type;   //!< the specific G-Stream identifier
                   char        Opt;    //!< Options
                   char        Fmt;    //!< How to handle the records
                   char        Hdr;    //!< Hdr type
                  };

          XrdXrootdGSReal(const GSParms &gsParms, bool &aOK);

//-----------------------------------------------------------------------------
//! Destructor. Normally, this object is never deleted.
//-----------------------------------------------------------------------------

         ~XrdXrootdGSReal() {}

private:


void AutoFlush();
void Expel(int dlen);
int  hdrBIN(const GSParms &gs);
int  hdrCGI(const GSParms &gs, char *buff, int blen);
int  hdrJSN(const GSParms &gs, char *buff, int blen);

struct HdrInfo
      {char *pseq;
       char *tbeg;
       char *tend;
      }      hInfo;

char                  *dictHdr;
char                  *idntHdr0;
char                  *idntHdr1;
int                    idntHsz1;
int                    pSeq;
int                    pSeqID;     // For ident records
int                    pSeqDID;    // For dict  records
XrdSysRecMutex         gMutex;
XrdNetMsg             *udpDest;
XrdXrootdMonGS        *binHdr;
char                  *udpBuffer;
char                  *udpBFirst;
char                  *udpBNext;
char                  *udpBEnd;
int                    tBeg;
int                    tEnd;
int                    rsvbytes;
int                    monType;
int                    afTime;
bool                   afRunning;
bool                   isCGI;

XrdXrootdMonitor::User gMon;
};
#endif
