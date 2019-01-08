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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <sys/types.h>

#include "XrdSecsss/XrdSecsssID.hh"
#include "XrdSecsss/XrdSecsssRR.hh"

#include "XrdOuc/XrdOucPup.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysHeaders.hh"

/******************************************************************************/
/*                               D e f i n e s                                */
/******************************************************************************/
  
#define XRDSECSSSID "XrdSecsssID"
#define XRDSECSSSENDO "XrdSecsssENDORSEMENT"

XrdSysMutex         XrdSecsssID::InitMutex;

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdSecsssID::XrdSecsssID(authType aType, XrdSecEntity *idP) : defaultID(0)
{
   static char buff[64];
   union {unsigned long val; XrdSecsssID *myP;} p2i;

// Check if we have initialized already. If so, indicate warning
//
   InitMutex.Lock();
   if (getenv(XRDSECSSSID))
      {InitMutex.UnLock();
       cerr <<"SecsssID: Already instantiated; new instance ineffective!" <<endl;
       return;
      }

// Verify the authType
//
   switch(aType)
         {case idDynamic: break;
          case idStatic:  break;
          case idStaticM: break;
          default:        idP = 0; aType = idStatic; break;
         }
   myAuth = aType;

// Generate a default identity
//
   if (!idP || !(defaultID = genID(idP)))
      defaultID = genID(aType != idDynamic);

// Establish a pointer to this object so that the shared library can use it
// We only do this once!
//
   p2i.myP = this;
   sprintf(buff, XRDSECSSSID"=%lx", p2i.val);
   putenv(buff);

// All done with initialization
//
   InitMutex.UnLock();
}

/******************************************************************************/
/*                                  F i n d                                   */
/******************************************************************************/
  
int XrdSecsssID::Find(const char *lid, char *Buff, int Blen)
{
   sssID *fP;
   int rc;

// Lock the hash table and find the entry
//
   myMutex.Lock();
   if (!(fP = Registry.Find(lid))) fP = defaultID;
   if (!fP || fP->iLen > Blen) {myMutex.UnLock(); return 0;}

// Return the data
//
   memcpy(Buff, fP->iData, fP->iLen);
   rc = fP->iLen;
   myMutex.UnLock();
   return rc;
}
  
/******************************************************************************/
/*                                g e t O b j                                 */
/******************************************************************************/
  
XrdSecsssID *XrdSecsssID::getObj(authType &aType, char **dID, int &dIDsz)
{
   int freeIDP = 0;
   sssID *idP;
   char *eP, *xP;
   union {long long llval; long lval; XrdSecsssID *idP;} i2p;

// Prevent changes
//
   InitMutex.Lock();

// Convert to pointer
//
   aType = idStatic;
   if ((eP = getenv(XRDSECSSSID)) && *eP)
      {if (sizeof(XrdSecsssID *) > 4) i2p.llval = strtoll(eP, &xP, 16);
          else                        i2p.lval  = strtol (eP, &xP, 16);
       if (*xP)                       i2p.idP   = 0;
          else aType = i2p.idP->myAuth;
      } else i2p.idP = 0;

// Establish the default ID
//
   if (!i2p.idP || !(idP = i2p.idP->defaultID))
      {idP = genID(aType == idDynamic); freeIDP = 1;}

// Copy out the default id to the caller
//
   dIDsz = idP->iLen;
  *dID = (char *)malloc(dIDsz);
   memcpy(*dID, idP->iData, dIDsz);

// Return result
//
   InitMutex.UnLock();
   if (freeIDP) free(idP);
   return i2p.idP;
}

/******************************************************************************/
/*                              R e g i s t e r                               */
/******************************************************************************/

int XrdSecsssID::Register(const char *lid, XrdSecEntity *eP, int doRep)
{
   sssID *idP;
   int    rc;
   int    hOpt = (doRep ? Hash_replace : Hash_default) | Hash_dofree;

// Check if we are simply deleting an entry
//
   if (!eP)
      {myMutex.Lock(); Registry.Del(lid); myMutex.UnLock(); return 1;}

// Generate an ID and add it to registry
//
   if (!(idP = genID(eP))) return 0;
   myMutex.Lock(); 
   rc = (Registry.Add(lid, idP, 0, XrdOucHash_Options(hOpt)) ? 0 : 1);
   myMutex.UnLock();
   return rc;
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                                 g e n I D                                  */
/******************************************************************************/
  
XrdSecsssID::sssID *XrdSecsssID::genID(int Secure)
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
   return genID(&myID);
}

/******************************************************************************/

XrdSecsssID::sssID *XrdSecsssID::genID(XrdSecEntity *eP)
{
   sssID *idP;
   char *bP;
   int tLen;

// Calculate the length needed for the entity (4 bytes overhead for each item)
//
   tLen = (eP->name         ? strlen(eP->name)         + 4 : 0)
        + (eP->vorg         ? strlen(eP->vorg)         + 4 : 0)
        + (eP->role         ? strlen(eP->role)         + 4 : 0)
        + (eP->grps         ? strlen(eP->grps)         + 4 : 0)
        + (eP->endorsements ? strlen(eP->endorsements) + 4 : 0);

// If no identity information, return failure otherwise allocate a struct
//
   if (!tLen || !(idP = (sssID *)malloc(tLen + sizeof(sssID)))) return 0;

// Now stick each entry into the iData field
//
   bP = idP->iData;
   if (eP->name)
      {*bP++ = XrdSecsssRR_Data::theName; XrdOucPup::Pack(&bP,eP->name);}
   if (eP->vorg)
      {*bP++ = XrdSecsssRR_Data::theVorg; XrdOucPup::Pack(&bP,eP->vorg);}
   if (eP->role)
      {*bP++ = XrdSecsssRR_Data::theRole; XrdOucPup::Pack(&bP,eP->role);}
   if (eP->grps)
      {*bP++ = XrdSecsssRR_Data::theGrps; XrdOucPup::Pack(&bP,eP->grps);}
   if (eP->endorsements)
      {*bP++ = XrdSecsssRR_Data::theEndo; XrdOucPup::Pack(&bP,eP->endorsements);}
   idP->iLen = bP - (idP->iData);

// All done
//
   return idP;
}
