#ifndef __XRD_POLLINFO_H__
#define __XRD_POLLINFO_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d P o l l I n f o . h h                         */
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

class  XrdLink;
class  XrdPoll;
struct pollfd;

class XrdPollInfo
{
public:

XrdPollInfo   *Next;        // Chain of links waiting for a PollPoll event
XrdLink       &Link;        // Link associated with this object (always the same)
struct pollfd *PollEnt;     // Used only by PollPoll
XrdPoll       *Poller;      // -> Poller object associated with this object
int            FD;          // Associated target file descriptor number
bool           inQ;         // True -> in a PollPoll event queue
bool           isEnabled;   // True -> interrupts are enabled
char           rsv[2];      // Reserved for future flags

void           Zorch() {Next      = 0;     PollEnt  = 0;
                        Poller    = 0;     FD       = -1;
                        isEnabled = false; inQ      = false;
                        rsv[0]    = 0;     rsv[1]   = 0;
                       }

               XrdPollInfo(XrdLink &lnk) : Link(lnk) {Zorch();}
              ~XrdPollInfo() {}
};
#endif
