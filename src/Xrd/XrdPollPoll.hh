#ifndef __XRD_POLLPOLL_H__
#define __XRD_POLLPOLL_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d P o l l P o l l . h h                         */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <poll.h>

#include "Xrd/XrdPoll.hh"

class XrdLink;
  
class XrdPollPoll : XrdPoll
{
public:

       void Detach(XrdLink *lp);

       void Disable(XrdLink *lp, const char *etxt=0);

       int  Enable(XrdLink *lp);

       void Start(XrdSysSemaphore *syncp, int &rc);

            XrdPollPoll(struct pollfd *pp, int numfd);
           ~XrdPollPoll();

protected:
       void doDetach(int pti);
       void Exclude(XrdLink *lp);
       int  Include(XrdLink *lp);

private:

void  doRequests(int maxreq);
void  dqLink(XrdLink *lp);
void  LogEvent(int req, int pollfd, int cmdfd);
void  Recover(int numleft);
void  Restart(int ecode);

struct     pollfd     *PollTab;    //<---
           int         PollTNum;   // PollMutex protects these elements
           XrdLink    *PollQ;      //<---
           XrdSysMutex PollMutex;
           int         maxent;
};
#endif
