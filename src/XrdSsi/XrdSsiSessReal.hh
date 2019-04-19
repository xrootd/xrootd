#ifndef __XRDSSISESSREAL_HH__
#define __XRDSSISESSREAL_HH__
/******************************************************************************/
/*                                                                            */
/*                     X r d S s i S e s s R e a l . h h                      */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <string.h>

#include "XrdCl/XrdClFile.hh"

#include "XrdSsi/XrdSsiAtomics.hh"
#include "XrdSsi/XrdSsiEvent.hh"

#include "XrdSys/XrdSysPthread.hh"

class XrdSsiServReal;
class XrdSsiTaskReal;

class XrdSsiSessReal : public XrdSsiEvent
{
public:

XrdSsiSessReal  *nextSess;

const char      *GetKey() {return resKey;}

uint32_t         GetSID() {return sessID;}

        void     InitSession(XrdSsiServReal *servP,
                             const char     *sName,
                             int             uent,
                             bool            hold,
                             bool            newSID=false);

        void     Lock() {sessMutex.Lock();}

XrdSsiMutex     *MutexP() {return &sessMutex;}

        bool     Provision(XrdSsiRequest *reqP, const char *epURL);

        bool     Run(XrdSsiRequest *reqP);

        void     SetKey(const char *key)
                       {if (resKey) free(resKey);
                        resKey =  (key ? strdup(key) : 0);
                       }

        void     TaskFinished(XrdSsiTaskReal *tP);

        void     UnHold(bool cleanup=true);

        void     UnLock() {sessMutex.UnLock();}

        void     Unprovision();

        bool     XeqEvent(XrdCl::XRootDStatus *status,
                          XrdCl::AnyObject   **respP);

                 XrdSsiSessReal(XrdSsiServReal *servP,
                                const char     *sName,
                                int             uent,
                                bool            hold=false)
                               : sessMutex(XrdSsiMutex::Recursive),
                                 resKey(0), sessName(0), sessNode(0)
                                 {InitSession(servP, sName, uent, hold, true);}

                ~XrdSsiSessReal();

XrdCl::File         epFile;

private:
XrdSsiTaskReal  *NewTask(XrdSsiRequest *reqP);
void             RelTask(XrdSsiTaskReal *tP);
void             Shutdown(XrdCl::XRootDStatus &epStatus, bool onClose);

XrdSsiMutex      sessMutex;
XrdSsiServReal  *myService;
XrdSsiTaskReal  *attBase;
XrdSsiTaskReal  *freeTask;
XrdSsiRequest   *requestP;
char            *resKey;
char            *sessName;
char            *sessNode;
uint32_t         sessID;
uint32_t         nextTID;
uint32_t         alocLeft;
int16_t          uEnt;     // User index for scaling
bool             isHeld;
bool             inOpen;
bool             noReuse;
};
#endif
