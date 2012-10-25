#ifndef __XRD_POLLDEV_H__
#define __XRD_POLLDEV_H__
/******************************************************************************/
/*                                                                            */
/*                         X r d P o l l D e v . h h                          */
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

#include <sys/epoll.h>

#include "Xrd/XrdPoll.hh"

#ifndef EPOLLRDHUP
#define EPOLLRDHUP 0
#endif
  
class XrdPollE : public XrdPoll
{
public:

       void Disable(XrdLink *lp, const char *etxt=0);

       int   Enable(XrdLink *lp);

       void Start(XrdSysSemaphore *syncp, int &rc);

            XrdPollE(struct epoll_event *ptab, int numfd, int pfd)
                       {PollTab = ptab; PollMax = numfd; PollDfd = pfd;}
           ~XrdPollE();

protected:
       void  Exclude(XrdLink *lp);
       int   Include(XrdLink *lp);
const  char *x2Text(unsigned int evf, char *buff);

private:
void remFD(XrdLink *lp, unsigned int events);

#ifdef EPOLLONESHOT
   static const int ePollOneShot = EPOLLONESHOT;
#else
   static const int ePollOneShot = 0;
#endif
   static const int ePollEvents = EPOLLIN  | EPOLLHUP | EPOLLPRI | EPOLLERR |
                                  EPOLLRDHUP | ePollOneShot;

struct epoll_event *PollTab;
       int          PollDfd;
       int          PollMax;
};
#endif
