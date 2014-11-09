#ifndef __XrdProtLoad_H__
#define __XrdProtLoad_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d P r o t L o a d . h h                         */
/*                                                                            */
/* (c) 2006 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "Xrd/XrdProtocol.hh"

// This class load and allows the selection of the appropriate link protocol. 
//
class XrdProtLoad : public XrdProtocol
{
public:

void          DoIt() {}

static void   Init(XrdSysError *eP, XrdOucTrace *tP)
                  {XrdLog = eP; XrdTrace = tP;}

static int    Load(const char *lname, const char *pname, char *parms,
                   XrdProtocol_Config *pi);

static int    Port(const char *lname, const char *pname, char *parms,
                   XrdProtocol_Config *pi);

XrdProtocol  *Match(XrdLink *) {return 0;}

int           Process(XrdLink *lp);

void          Recycle(XrdLink *lp, int ctime, const char *txt);

int           Stats(char *buff, int blen, int do_sync=0) {return 0;}

static int    Statistics(char *buff, int blen, int do_sync=0);

              XrdProtLoad(int port=-1);
             ~XrdProtLoad();

static const int ProtoMax = 8;

private:

static XrdProtocol *getProtocol    (const char *lname, const char *pname,
                                    char *parms, XrdProtocol_Config *pi);
static int          getProtocolPort(const char *lname, const char *pname,
                                    char *parms, XrdProtocol_Config *pi);

static XrdSysError   *XrdLog;
static XrdOucTrace   *XrdTrace;

static char          *ProtName[ProtoMax];   // ->Supported protocol names
static XrdProtocol   *Protocol[ProtoMax];   // ->Supported protocol objects
static int            ProtPort[ProtoMax];   // ->Supported protocol ports
static XrdProtocol   *ProtoWAN[ProtoMax];   // ->Supported protocol objects WAN
static int            ProtoCnt;             // Number in table (at least 1)
static int            ProtWCnt;             // Number in table (WAN may be 0)

       int            myPort;
};
#endif
