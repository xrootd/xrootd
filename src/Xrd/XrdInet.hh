#ifndef __XRD_INET_H__
#define __XRD_INET_H__
/******************************************************************************/
/*                                                                            */
/*                            X r d I n e t . h h                             */
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

#include <unistd.h>

#include "XrdNet/XrdNet.hh"
#include "XrdNet/XrdNetIF.hh"

// The XrdInet class defines a generic network where we can define common
// initial tcp/ip and udp operations. It is based on the generalized network
// support framework. However, Accept and Connect have been augmented to
// provide for more scalable communications handling.
//
class XrdOucTrace;
class XrdSysError;
class XrdSysSemaphore;
class XrdNetSecurity;
class XrdLink;

class XrdInet : public XrdNet
{
public:

XrdLink    *Accept(int opts=0, int timeout=-1, XrdSysSemaphore *theSem=0);

XrdLink    *Connect(const char *host, int port, int opts=0, int timeout=-1);

void        Secure(XrdNetSecurity *secp);

            XrdInet(XrdSysError *erp, XrdOucTrace *tP, XrdNetSecurity *secp=0)
                      : XrdNet(erp,0), Patrol(secp), XrdTrace(tP) {}
           ~XrdInet() {}
static
XrdNetIF    netIF;

private:

XrdNetSecurity    *Patrol;
XrdOucTrace       *XrdTrace;
static const char *TraceID;
};
#endif
