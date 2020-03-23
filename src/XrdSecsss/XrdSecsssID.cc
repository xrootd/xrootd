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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <sys/types.h>

#include "XrdSecsss/XrdSecsssEnt.hh"
#include "XrdSecsss/XrdSecsssID.hh"
#include "XrdSecsss/XrdSecsssRR.hh"

#include "XrdOuc/XrdOucHash.hh"
#include "XrdOuc/XrdOucPup.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                               D e f i n e s                                */
/******************************************************************************/
  
#define XRDSECSSSENDO "XrdSecsssENDORSEMENT"

/******************************************************************************/
/*                               S t a t i c s                                */
/******************************************************************************/
  
namespace
{
XrdSysMutex               sssMutex;
XrdSecsssID              *IDMapper = 0;
XrdOucHash<XrdSecsssEnt>  Registry;
}

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdSecsssID::XrdSecsssID(authType aType, XrdSecEntity *idP, bool *isOK)
                        : defaultID(0),
                          myAuth(XrdSecsssID::idStatic), isStatic(true)
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
   XrdSecsssEnt *fP;
   int n;

// Lock the hash table and find the entry
//
   sssMutex.Lock();
   if (!(fP = Registry.Find(lid)))
      {if (!(fP = defaultID))
          {sssMutex.UnLock(); return 0;}
      }

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

bool XrdSecsssID::Register(const char *lid, XrdSecEntity *eP,
                           bool doRep, bool defer)
{
   XrdSecsssEnt *idP;
   int    hOpt = (doRep ? Hash_replace : Hash_default);
   bool   isOK;

// If this is an invalid call, return failure
//
   if (!defaultID) return false;

// Check if we are simply deleting an entry
//
   if (!eP)
      {sssMutex.Lock(); Registry.Del(lid); sssMutex.UnLock(); return true;}

// Generate an ID and add it to registry
//
   idP = new XrdSecsssEnt(eP, defer);
   sssMutex.Lock(); 
   isOK = (Registry.Add(lid, idP, 0, XrdOucHash_Options(hOpt)) ? false : true);
   sssMutex.UnLock();
   if (!isOK) delete idP;
   return isOK;
}
