#ifndef __XRD_POLLPOLL_H__
#define __XRD_POLLPOLL_H__
/******************************************************************************/
/*                                                                            */
/*                        x r d _ P o l l P o l l . h                         */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*      All Rights Reserved. See XrdVersion.cc for complete License Terms     */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

#include <poll.h>

#include "Xrd/XrdPoll.hh"

class XrdLink;
  
class XrdPollPoll : XrdPoll
{
public:

       void Detach(XrdLink *lp);

       void Disable(XrdLink *lp, const char *etxt=0);

       int   Enable(XrdLink *lp);

       void Start(XrdOucSemaphore *syncp, int &rc);

       int  XSignal;

            XrdPollPoll(struct pollfd *pp, int numfd);
           ~XrdPollPoll();

protected:
       void doDetach(struct pollfd *dent);
       void Exclude(XrdLink *lp);
       int  Include(XrdLink *lp);

private:

void  doRequests(int maxreq);
void  dqLink(XrdLink *lp);
void  LogEvent(int req, int pollfd, int cmdfd);
void  Recover(int numleft);
void  Restart(int ecode);

struct     pollfd      PipePoll;

struct     pollfd     *PollTab;
           int         PollTNum;
           int         PollENum;
           XrdLink    *PollQ;
XrdOucMutex PollMutex;                 // Protects above data

           int        maxent;
};
#endif
