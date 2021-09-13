/******************************************************************************/
/*                                                                            */
/*                        X r d S e c s s s I D . c c                         */
/*                                                                            */
/* (c) 2008 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <iostream>
#include <map>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pwd.h>
#include <sys/types.h>

#include "XrdOuc/XrdOucPup.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysPthread.hh"

#include "XrdSec/XrdSecEntity.hh"
#include "XrdSecsss/XrdSecsssEnt.hh"
#include "XrdSecsss/XrdSecsssID.hh"
#include "XrdSecsss/XrdSecsssMap.hh"
#include "XrdSecsss/XrdSecsssRR.hh"

/******************************************************************************/
/*                               D e f i n e s                                */
/******************************************************************************/
  
#define XRDSECSSSENDO "XrdSecsssENDORSEMENT"

/******************************************************************************/
/*                               S t a t i c s                                */
/******************************************************************************/
  
namespace XrdSecsssMap
{
XrdSysMutex   sssMutex;
XrdSecsssID  *IDMapper = 0;
XrdSecsssCon *conTrack = 0;

typedef std::map<std::string, XrdSecsssEnt*> EntityMap;

EntityMap     Registry;
}

using namespace XrdSecsssMap;

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdSecsssID::XrdSecsssID(authType aType, const XrdSecEntity *idP,
                         XrdSecsssCon *Tracker, bool *isOK)
                        : defaultID(0),
                          myAuth(XrdSecsssID::idStatic), isStatic(true),
                          trackOK(false)
{

// Check if we have initialized already. If so, indicate warning
//
   sssMutex.Lock();
   if (IDMapper)
      {sssMutex.UnLock();
       if (isOK) *isOK = false;
          else std::cerr <<"SecsssID: Already instantiated; new instance"
                           " ineffective!\n" <<std::flush;
       return;
      }

// Verify the authType
//
   switch(aType)
         {case idDynamic: isStatic = false;
          case idStatic:  break;
          case idStaticM: break;
          case idMapped:  isStatic = false;
                          break;
          case idMappedM: isStatic = false;
                          break;
          default:        idP = 0;
                          aType = idStatic;
                          isStatic = true;
                          break;
         }
   myAuth = aType;

// Generate a default identity
//
   if (idP) defaultID = new XrdSecsssEnt(idP);
      else  defaultID = genID(isStatic);

// Establish a pointer to this object.
//
   IDMapper = this;

// Decide whether or not we will track connections
//
   if (Tracker && (aType == idMapped || aType == idMappedM)) conTrack = Tracker;

// All done with initialization
//
   if (isOK) *isOK = true;
   sssMutex.UnLock();
}

/******************************************************************************/
/* Private:                   D e s t r u c t o r                             */
/******************************************************************************/

XrdSecsssID::~XrdSecsssID() {if (defaultID) free(defaultID);}
  
/******************************************************************************/
/* Private:                         F i n d                                   */
/******************************************************************************/
  
int XrdSecsssID::Find(const char *lid,  char *&dP,
                      const char *myIP, int dataOpts)
{
   EntityMap::iterator it;
   XrdSecsssEnt *fP;
   int n;

// Lock the registry and find the entry
//
   sssMutex.Lock();
   it = Registry.find(lid);
   if (it == Registry.end())
      {if (!(fP = defaultID))
          {sssMutex.UnLock(); return 0;}
      } else fP = it->second;

// Return the data
//
   n = fP->RR_Data(dP, myIP, dataOpts);
   sssMutex.UnLock();
   return n;
}

/******************************************************************************/
/* Private:                        g e n I D                                  */
/******************************************************************************/
  
XrdSecsssEnt *XrdSecsssID::genID(bool Secure)
{
   XrdSecEntity   myID("sss");
   static const int pgSz = 256;
   char pBuff[pgSz], gBuff[pgSz];

// Use either our own uid/gid or a generic
//
   myID.name = (Secure || XrdOucUtils:: UserName(geteuid(), pBuff, pgSz))
             ? (char *)"nobody"  : pBuff;
   myID.grps = (Secure || XrdOucUtils::GroupName(getegid(), gBuff, pgSz) == 0)
             ? (char *)"nogroup" : gBuff;

   if (getenv(XRDSECSSSENDO)) 
     {myID.endorsements = getenv(XRDSECSSSENDO); }

// Just return the sssID
//
   return new XrdSecsssEnt(&myID);
}
  
/******************************************************************************/
/* Private:                       g e t O b j                                 */
/******************************************************************************/
  
XrdSecsssID *XrdSecsssID::getObj(authType &aType, XrdSecsssEnt *&idP)
{
   bool sType = false;

// Prevent changes
//
   sssMutex.Lock();

// Pick up the settings (we might not have any)
//
   if (!IDMapper)
      {aType = idStatic;
       sType = true;
       idP   = 0;
      } else {
       aType = IDMapper->myAuth;
       idP   = IDMapper->defaultID;
      }
   if (!idP) idP = genID(sType);

// Return result
//
   XrdSecsssID *theMapper = IDMapper;
   sssMutex.UnLock();
   return theMapper;
}

/******************************************************************************/
/*                              R e g i s t e r                               */
/******************************************************************************/

bool XrdSecsssID::Register(const char *lid, const XrdSecEntity *eP,
                           bool doRep, bool defer)
{
   EntityMap::iterator it;
   XrdSecsssEnt *idP;

// If this is an invalid call, return failure
//
   if (isStatic) return false;

// Check if we are simply deleting an entry
//
   if (!eP)
      {sssMutex.Lock();
       it = Registry.find(std::string(lid));
       if (it == Registry.end()) sssMutex.UnLock();
          else {idP = it->second;
                Registry.erase(it);
                sssMutex.UnLock();
                idP->Delete();
               }
       return true;
      }

// Generate an ID entry and add it to registry (we are optimistic here)
// Note: We wish we could use emplace() but that isn't suported until gcc 4.8.0
//
   std::pair<EntityMap::iterator, bool>  ret;
   std::pair<std::string, XrdSecsssEnt*> psp;
   idP = new XrdSecsssEnt(eP, defer);
   psp = {std::string(lid), idP};
   sssMutex.Lock(); 
   ret = Registry.insert(psp);
   if (ret.second)
      {sssMutex.UnLock();
       return true;
      }

// We were not successful, replace the element if we are allowed to do so.
//
   if (doRep)
      {XrdSecsssEnt *oldP = ret.first->second;
       ret.first->second = idP;
       sssMutex.UnLock();
       oldP->Delete();
       return true;
      }

// Sigh, the element exists but we cannot replace it.
//
   sssMutex.UnLock();
   idP->Delete();
   return false;
}
