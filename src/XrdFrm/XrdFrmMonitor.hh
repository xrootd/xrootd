#ifndef __XRDFRMMONITOR__
#define __XRDFRMMONITOR__
/******************************************************************************/
/*                                                                            */
/*                      X r d F r m M o n i t o r . h h                       */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <inttypes.h>
#include <time.h>
#include <sys/types.h>

#include "XrdXrootd/XrdXrootdMonData.hh"
#include "XProtocol/XPtypes.hh"

#define XROOTD_MON_INFO     1
#define XROOTD_MON_STAGE    2
#define XROOTD_MON_MIGR     4
#define XROOTD_MON_PURGE    8

class XrdNetMsg;

class XrdFrmMonitor
{
public:

static void              Defaults(char *dest1, int m1, char *dest2, int m2,
                                  int   iTime);

static void              Ident();

static int               Init(const char *iHost, const char *iProg,
                              const char *iName);

static kXR_unt32         Map(char code, const char *uname, const char *path);

static char              monMIGR;
static char              monPURGE;
static char              monSTAGE;

                         XrdFrmMonitor();
                        ~XrdFrmMonitor(); 

private:
static void              fillHeader(XrdXrootdMonHeader *hdr,
                                    const char id, int size);
static int               Send(int mmode, void *buff, int size);

static char              *Dest1;
static int                monMode1;
static XrdNetMsg     *InetDest1;
static char              *Dest2;
static int                monMode2;
static XrdNetMsg     *InetDest2;
static kXR_int32          startTime;
static int                isEnabled;
static char              *idRec;
static int                idLen;
static int                sidSize;
static char              *sidName;
static int                idTime;
};
#endif
