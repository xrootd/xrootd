#ifndef __XRDOUCREQID__
#define __XRDOUCREQID__
/******************************************************************************/
/*                                                                            */
/*                        X r d O u c R e q I D . h h                         */
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

#include <stdlib.h>
#ifndef WIN32
#include <strings.h>
#endif

#include "XrdSys/XrdSysPthread.hh"

union XrdNetSockAddr;
  
class XrdOucReqID
{
public:

       char *ID(char *buff, int blen); // blen >= 48

       char *isMine(char *reqid, int &hport, char *hname, int hlen);

       char *PFX() {return reqPFX;}

static int   Index(int KeyMax, const char *KeyVal, int KeyLen=0);

             XrdOucReqID();
             XrdOucReqID(const XrdNetSockAddr *myAddr, int myPort=-1);

            ~XrdOucReqID() {} // Statics go away at exit

private:

XrdSysMutex  myMutex;
int          reqPFXlen;
int          reqIntern;
char        *reqPFX;
char        *reqFMT;
int          reqNum;
};
#endif
