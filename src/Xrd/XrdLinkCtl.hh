#ifndef __XRD_LINKCTL_H__
#define __XRD_LINKCTL_H__
/******************************************************************************/
/*                                                                            */
/*                         X r d L i n k C t l . h h                          */
/*                                                                            */
/* (c) 2018 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "Xrd/XrdLinkXeq.hh"

#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                      C l a s s   D e f i n i t i o n                       */
/******************************************************************************/
  
class XrdLinkMatch;

class XrdLinkCtl : protected XrdLinkXeq
{
public:

//-----------------------------------------------------------------------------
//! Allocate a new link object.
//!
//! @param  peer    The connection information for the endpoint.
//! @param  opts    Processing options:
//!                 XRDLINK_NOCLOSE - do not close the FD upon recycling.
//!                 XRDLINK_RDLOCK  - obtain a lock prior to reading data.
//!
//! @return !0      The pointer to the new object.
//!         =0      A new link object could not be allocated.
//-----------------------------------------------------------------------------

#define XRDLINK_RDLOCK  0x0001
#define XRDLINK_NOCLOSE 0x0002

static XrdLink *Alloc(XrdNetAddr &peer, int opts=0);

//-----------------------------------------------------------------------------
//! Translate a file descriptor number to the corresponding link object.
//!
//! @param  fd      The file descriptor number.
//!
//! @return !0      Pointer to the link object.
//!         =0      The file descriptor is not associated with a link.
//-----------------------------------------------------------------------------

static XrdLink  *fd2link(int fd)
                 {if (fd < 0) fd = -fd;
                  return (fd <= LTLast && LinkBat[fd] ? LinkTab[fd] : 0);
                 }

//-----------------------------------------------------------------------------
//! Translate a file descriptor number and an instance to a link object.
//!
//! @param  fd      The file descriptor number.
//! @param  inst    The file descriptor number instance number.
//!
//! @return !0      Pointer to the link object.
//!         =0      The file descriptor instance is not associated with a link.
//-----------------------------------------------------------------------------

static XrdLink  *fd2link(int fd, unsigned int inst)
                 {if (fd < 0) fd = -fd;
                  if (fd <= LTLast && LinkBat[fd] && LinkTab[fd]
                  && LinkTab[fd]->Instance == inst) return LinkTab[fd];
                  return (XrdLink *)0;
                 }

//-----------------------------------------------------------------------------
//! Translate a file descriptor number to the corresponding PollInfo object.
//!
//! @param  fd      The file descriptor number.
//!
//! @return !0      Pointer to the PollInfo object.
//!         =0      The file descriptor is not associated with a link.
//-----------------------------------------------------------------------------

static XrdPollInfo *fd2PollInfo(int fd)
                    {if (fd < 0) fd = -fd;
                     if (fd <= LTLast && LinkBat[fd])
                        return &(LinkTab[fd]->PollInfo);
                     return 0;
                    }

//-----------------------------------------------------------------------------
//! Find the next link matching certain attributes.
//!
//! @param  cur     Is an internal tracking value that allows repeated calls.
//!                 It must be set to a value of 0 or less on the initial call
//!                 and not touched therafter unless a null pointer is returned.
//! @param  who     If the object use to check if teh link matches the wanted
//!                 criterea (typically, client name and host name). If the
//!                 ppointer is nil, the next link is always returned.
//!
//! @return !0      Pointer to the link object that matches the criterea. The
//!                 link's reference counter is increased to prevent it from
//!                 being reused. A subsequent call will reduce the number.
//!         =0      No more links exist with the specified criterea.
//-----------------------------------------------------------------------------

static XrdLink  *Find(int &curr, XrdLinkMatch *who=0);

//-----------------------------------------------------------------------------
//! Find the next client name matching certain attributes.
//!
//! @param  cur     Is an internal tracking value that allows repeated calls.
//!                 It must be set to a value of 0 or less on the initial call
//!                 and not touched therafter unless zero is returned.
//! @param  bname   Pointer to a buffer where the name is to be returned.
//! @param  blen    The length of the buffer.
//! @param  who     If the object use to check if the link matches the wanted
//!                 criterea (typically, client name and host name). If the
//!                 pointer is nil, a match always occurs.
//!
//! @return !0      The length of the name placed in the buffer.
//!         =0      No more links exist with the specified criterea.
//-----------------------------------------------------------------------------

static int      getName(int &curr, char *bname, int blen, XrdLinkMatch *who=0);

//-----------------------------------------------------------------------------
//! Look for idle links and close hem down.
//-----------------------------------------------------------------------------

static void     idleScan();

//-----------------------------------------------------------------------------
//! Set kill constants.
//!
//! @param  wksec   Seconds to wait for kill to happed,
//! @param  kwsec   The minimum number of seconds to wait after killing a
//!                 connection for it to end.
//-----------------------------------------------------------------------------

static void     setKWT(int wkSec, int kwSec);

//-----------------------------------------------------------------------------
//! Setup link processing.
//!
//! @param  maaxfds The maximum number of connections to handle.
//! @param  idlewt  The time interval to check for idle connections.
//!
//! @return !0      Successful.
//!         =0      Setup failed.
//-----------------------------------------------------------------------------

static int      Setup(int maxfds, int idlewt);

//-----------------------------------------------------------------------------
//! Synchronize statustics for ll links.
//-----------------------------------------------------------------------------

static void     SyncAll();

//-----------------------------------------------------------------------------
//! Unhook a link from the active table of links.
//-----------------------------------------------------------------------------

static void     Unhook(int fd);

//-----------------------------------------------------------------------------
//! Link destruction control constants
//-----------------------------------------------------------------------------

static short    killWait;  // Kill then wait;
static short    waitKill;  // Wait then kill

//-----------------------------------------------------------------------------
//! Constructor
//-----------------------------------------------------------------------------

                XrdLinkCtl() {}

private:
               ~XrdLinkCtl() {}  // Is never deleted!

static XrdSysMutex   LTMutex;    // For the LinkTab only LTMutex->IOMutex allowed
static XrdLinkCtl  **LinkTab;
static char         *LinkBat;
static unsigned int  LinkAlloc;
static int           LTLast;
static int           maxFD;
static const char   *TraceID;
};
#endif
