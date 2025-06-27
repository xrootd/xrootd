#ifndef __XRDNETREFRESH_H__
#define __XRDNETREFRESH_H__
/******************************************************************************/
/*                                                                            */
/*                      X r d N e t R e f r e s h . h h                       */
/*                                                                            */
/* (c) 2025 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

class XrdNetPeer;
class XrdScheduler;
class XrdSysError;

class XrdNetRefresh : public XrdJob
{
public:

virtual void DoIt() override;

static  bool Register(XrdNetPeer& Peer);

static  void Start(XrdSysLogger* logP, XrdScheduler *sP);

static  void UnRegister(int fd);

             XrdNetRefresh() : XrdJob("NetRefresh") {}
virtual     ~XrdNetRefresh() {}

private:

static  bool RegFail(const char* why);
static  bool SetDest(int fd, XrdNetSockAddr& netAddr, const char* hName,
                     bool newFam);
static  void Update();
};
#endif
