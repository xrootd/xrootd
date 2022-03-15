#ifndef __XRDNETPMARKCFG__
#define __XRDNETPMARKCFG__
/******************************************************************************/
/*                                                                            */
/*                     X r d N e t P M a r k C f g . h h                      */
/*                                                                            */
/* (c) 2021 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

class XrdNetMsg;
class XrdNetPMark;
class XrdOucMapP2N;
class XrdOucStream;
class XrdScheduler;
class XrdSysError;
class XrdSysTrace;

#include "XrdNet/XrdNetPMark.hh"

class XrdNetPMarkCfg : public XrdNetPMark
{
public:

        XrdNetPMark::Handle *Begin(XrdSecEntity &Client,
                                   const char   *path=0,
                                   const char   *cgi=0,
                                   const char   *app=0) override;

        XrdNetPMark::Handle *Begin(XrdNetAddrInfo      &addr,
                                   XrdNetPMark::Handle &handle,
                                   const char          *tident) override;

static  XrdNetPMark *Config(XrdSysError *eLog, XrdScheduler *sched,
                            XrdSysTrace *trc,  bool &fatal);

static  int          Parse(XrdSysError *eLog, XrdOucStream &Config);

                     XrdNetPMarkCfg() {}


private:
           ~XrdNetPMarkCfg() {}

static bool ConfigDefs();
static bool ConfigPV2E(char *info);
static bool ConfigRU2A(char *info);
static void Display();
static
const char *Extract(const char *sVec, char *buff, int blen);
static bool FetchFile();
static bool getCodes(XrdSecEntity &client, const char *path,
                     const char *cgi, int &ecode, int &acode);
static bool LoadFile();
static bool LoadJson(char *buff);
};
#endif
