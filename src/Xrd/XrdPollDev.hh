#ifndef __XRD_POLLDEV_H__
#define __XRD_POLLDEV_H__
/******************************************************************************/
/*                                                                            */
/*                         x r d _ P o l l D e v . h                          */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*      All Rights Reserved. See XrdVersion.cc for complete License Terms     */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//        $Id$

#include <poll.h>

#include "Xrd/XrdPoll.hh"
  
class XrdPollDev : public XrdPoll
{
public:

       void Disable(XrdLink *lp, const char *etxt=0);

       int   Enable(XrdLink *lp);

       void Start(XrdOucSemaphore *syncp, int &rc);

            XrdPollDev(struct pollfd *ptab, int numfd, int pfd)
                       {PollTab = ptab; PollMax = numfd; PollDfd = pfd;}
           ~XrdPollDev();

protected:
       void Exclude(XrdLink *lp);
       int  Include(XrdLink *lp) {return 1;}

private:

void doRequests(int maxreq);
void LogEvent(struct pollfd *pp);

struct pollfd *PollTab;
       int     PollDfd;
       int     PollMax;
};
#endif
