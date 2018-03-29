#ifndef _CMS_CLIENTCONFIG_H
#define _CMS_CLIENTCONFIG_H
/******************************************************************************/
/*                                                                            */
/*                 X r d C m s C l i e n t C o n f i g . h h                  */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdOuc/XrdOucTList.hh"
#include "XrdOuc/XrdOuca2x.hh"
  
class XrdOucStream;
class XrdSysError;

#define ODC_FAILOVER 'f'
#define ODC_ROUNDROB 'r'
  
class XrdCmsClientConfig
{
public:

enum configHow  {configMeta   = 1, configNorm  = 2, configProxy  = 4};
enum configWhat {configMan    = 1, configSuper = 2, configServer = 4};

int           Configure(const char *cfn, configWhat What, configHow How);

int           ConWait;      // Seconds to wait for a manager connection
int           RepWait;      // Seconds to wait for manager replies
int           RepWaitMS;    // RepWait*1000 for poll()
int           RepDelay;     // Seconds to delay before retrying manager
int           RepNone;      // Max number of consecutive non-responses
int           PrepWait;     // Millisecond wait between prepare requests
int           FwdWait;      // Millisecond wait between foward  requests
int           haveMeta;     // Have a meta manager (only if we are a manager)

char         *CMSPath;      // Path to the local cmsd for target nodes
const char   *myHost;
const char   *myName;
      char   *myVNID;
      char   *cidTag;

XrdOucTList  *ManList;      // List of managers for remote redirection
XrdOucTList  *PanList;      // List of managers for proxy  redirection
unsigned char SMode;        // Manager selection mode
unsigned char SModeP;       // Manager selection mode (proxy)

enum {FailOver = 'f', RoundRob = 'r'};

      XrdCmsClientConfig() : ConWait(10), RepWait(3),  RepWaitMS(3000),
                             RepDelay(5), RepNone(8),  PrepWait(33),
                             FwdWait(0),  haveMeta(0), CMSPath(0),
                             myHost(0),   myName(0),   myVNID(0),
                             cidTag(0),   ManList(0),  PanList(0),
                             SMode(FailOver), SModeP(FailOver),
                             VNID_Lib(0),  VNID_Parms(0),
                             isMeta(0), isMan(0) {}
     ~XrdCmsClientConfig();

private:
char *VNID_Lib;
char *VNID_Parms;

int isMeta;   // We are  a meta manager
int isMan;    // We are  a      manager

int ConfigProc(const char *cfn);
bool ConfigSID(const char *cFile, XrdOucTList *tpl, char sfx);
int ConfigXeq(char *var, XrdOucStream &Config);
int xapath(XrdOucStream &Config);
int xcidt(XrdOucStream  &Config);
int xconw(XrdOucStream  &Config);
int xmang(XrdOucStream  &Config);
int xreqs(XrdOucStream  &Config);
int xtrac(XrdOucStream  &Config);
int xvnid(XrdOucStream  &Config);
};
#endif
