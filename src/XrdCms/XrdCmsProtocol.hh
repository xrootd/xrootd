#ifndef __CMS_PROTOCOL_H__
#define __CMS_PROTOCOL_H__
/******************************************************************************/
/*                                                                            */
/*                     X r d C m s P r o t o c o l . h h                      */
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

#include "Xrd/XrdProtocol.hh"
#include "XrdCms/XrdCmsParser.hh"
#include "XrdCms/XrdCmsTypes.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdLink;
class XrdCmsManager;
class XrdCmsNode;
class XrdCmsRRData;
class XrdCmsRouting;

class XrdCmsProtocol : public XrdProtocol
{
friend class XrdCmsJob;
public:

static XrdCmsProtocol *Alloc(const char *theRole = "", XrdCmsManager *mP=0,
                             const char *theMan  = 0,  int thePort=0);

       void            DoIt();

       int             Execute(XrdCmsRRData &Data);

       XrdProtocol    *Match(XrdLink *lp);   // Upon    accept

       int             Process(XrdLink *lp); // Initial entry

       void            Recycle(XrdLink *lp, int consec, const char *reason);

       void            Ref(int rcnt);

       int             Stats(char *buff, int blen, int do_sync=0);

              XrdCmsProtocol() : XrdProtocol("cms protocol handler") {Init();}
             ~XrdCmsProtocol() {}

private:

XrdCmsRouting  *Admit();
XrdCmsRouting  *Admit_DataServer(int);
XrdCmsRouting  *Admit_Redirector(int);
XrdCmsRouting  *Admit_Supervisor(int);
SMask_t         AddPath(XrdCmsNode *nP, const char *pType, const char *Path);
int             Authenticate();
void            ConfigCheck(unsigned char *theConfig);
enum Bearing    {isDown, isLateral, isUp};
const char     *Dispatch(Bearing cDir, int maxWait, int maxTries);
void            Init(const char *iRole="?", XrdCmsManager *uMan=0,
                     const char *iMan="?",  int iPort=0);
XrdCmsRouting  *Login_Failed(const char *Reason);
void            Pander(const char *manager, int mport);
void            Reissue(XrdCmsRRData &Data);
void            Reply_Delay(XrdCmsRRData &Data, kXR_unt32 theDelay);
void            Reply_Error(XrdCmsRRData &Data, int ecode, const char *etext);
bool            SendPing();
void            Sync();

static XrdSysMutex     ProtMutex;
static XrdCmsProtocol *ProtStack;
static XrdCmsParser    ProtArgs;
       XrdCmsProtocol *ProtLink;

       XrdCmsRouting  *Routing;   // Request routing for this instance

static const int       maxReqSize = 16384;
       XrdSysMutex     refMutex;
       XrdSysSemaphore *refWait;
       XrdLink        *Link;
static int             readWait;
const  char           *myRole;
       XrdCmsNode     *myNode;
       XrdCmsManager  *Manager;
const  char           *myMan;
       int             myManPort;
       int             refCount;
       short           RSlot;      // True only for redirectors
       char            loggedIn;   // True if login succeeded
       bool            isNBSQ;     // True if nbsq is active
};
#endif
