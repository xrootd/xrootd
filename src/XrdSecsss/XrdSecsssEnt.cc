/******************************************************************************/
/*                                                                            */
/*                       X r d S e c s s s E n t . c c                        */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdOuc/XrdOucPup.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdSec/XrdSecEntityAttr.hh"
#include "XrdSecsss/XrdSecsssCon.hh"
#include "XrdSecsss/XrdSecsssEnt.hh"
#include "XrdSecsss/XrdSecsssKT.hh"
#include "XrdSecsss/XrdSecsssMap.hh"
#include "XrdSecsss/XrdSecsssRR.hh"

using namespace XrdSecsssMap;

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
namespace
{
class copyAttrs : public XrdSecEntityAttrCB
{
public:

XrdSecEntityAttrCB::Action Attr(const char *key, const char *val)
                               {if (!key) return XrdSecEntityAttrCB::Stop;
                                if (calcSz)
                                   {bL += strlen(key) + strlen(val) + 8;
                                   } else {
                                    *bP++ = XrdSecsssRR_Data::theAKey;
                                    XrdOucPup::Pack(&bP,key);
                                    *bP++ = XrdSecsssRR_Data::theAVal;
                                    XrdOucPup::Pack(&bP,val);
                                   }
                                return XrdSecEntityAttrCB::Next;
                               }
char *bP;
int   bL;
bool  calcSz;

      copyAttrs() : bP(0), bL(0), calcSz(false) {}
     ~copyAttrs() {}
};
}

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

char *XrdSecsssEnt::myHostName = 0;
int   XrdSecsssEnt::myHostNLen = 0;
  
/******************************************************************************/
/*                            A d d C o n t a c t                             */
/******************************************************************************/

void XrdSecsssEnt::AddContact(const std::string &hostID)
{
// If we are tracking connections then add this one to the set. We use the
// fact that a set can only have one instance of a member and ignores dups.
//
   if (conTrack) Contacts.insert(hostID);
}
  
/******************************************************************************/
/*                                D e l e t e                                 */
/******************************************************************************/

void XrdSecsssEnt::Delete()
{
// Invoke the cleanup call back if there is something to clean up
//
   if (conTrack && Contacts.size()) conTrack->Cleanup(Contacts, *eP);

// Now we can delete ourselves
//
   delete this;
}
  
/******************************************************************************/
/*                               R R _ D a t a                                */
/******************************************************************************/
  
int XrdSecsssEnt::RR_Data(char *&dP, const char *hostIP, int dataOpts)
{
   char *bP;
   int cpyLen, totLen = XrdSecsssRR_Data_HdrLen;
   int n = 0;

// If we have not yet serialized the data, do so now.
//
   if (!eData && !Serialize()) return 0;

// Compute the length we will need for the buffer (it must be exact)
//
   if (dataOpts & v2Client)
      {cpyLen = tLen;
       if (dataOpts & addCreds) cpyLen += credLen;
      } else cpyLen = iLen;
   totLen += cpyLen;

// Add in the hostIP if specified and hostname if available
//
   n = strlen(hostIP) + 4;
   if (hostIP) totLen += n;
   totLen += myHostNLen;

// Allocate a buffer
//
   if (!(dP = (char *)malloc(totLen))) return 0;
   bP = dP + XrdSecsssRR_Data_HdrLen;

// The first thing in the serialized packet is the host IP
//
   if (hostIP)
      {*bP++ = XrdSecsssRR_Data::theHost;
       XrdOucPup::Pack(&bP, hostIP);
      }

// The next thing is to addthe hostname for backward compatibility
//
   if (myHostName)
      {memcpy(bP, myHostName, myHostNLen); bP += myHostNLen;}

// Copy the remaining data
//
   memcpy(bP, eData, cpyLen);

// Return length of the whole thing
//
   return totLen;
}

/******************************************************************************/
/* Private:                    S e r i a l i z e                              */
/******************************************************************************/
  
bool XrdSecsssEnt::Serialize()
{
   copyAttrs theAttr;
   char *bP, rBuff[XrdSecsssRR_Data::MinDSz], uName[256], gName[256];
   int n, rLen = 0;
   bool incCreds = false;

// Make sure we have an entity here
//
   if (!eP) return false;

// Perform some initialization here
//
   *uName = 0;
   *gName = 0;

// Calculate the length needed for the entity (4 bytes overhead for each item)
// These items ae always sent to all servers.
//
   iLen = (eP->name         ? strlen(eP->name)         + 4 : 0)
        + (eP->vorg         ? strlen(eP->vorg)         + 4 : 0)
        + (eP->role         ? strlen(eP->role)         + 4 : 0)
        + (eP->grps         ? strlen(eP->grps)         + 4 : 0)
        + (eP->caps         ? strlen(eP->caps)         + 4 : 0)
        + (eP->endorsements ? strlen(eP->endorsements) + 4 : 0);

// The above is always sent to V1 servers and it can't be too short
//
   n = iLen + XrdSecsssRR_Data_HdrLen;
   if (n < XrdSecsssRR_Data::MinDSz)
      {rLen = XrdSecsssRR_Data::MinDSz - n;
       XrdSecsssKT::genKey(rBuff, rLen);
       if (!rBuff[0]) rBuff[0] = 0xff;
       iLen += rLen + 4;
      }

// Now, compute the size of all extra stuff we will be sending. Note that
// V1 servers will ignore them but we might exceed their maximum buffer size
// if we do, so we never send this information.
//
   tLen = iLen;
   theAttr.calcSz = true;
   eP->eaAPI->List(theAttr);
   theAttr.calcSz = false;
   tLen += theAttr.bL;

// Add in the length authnetication protocol (original)
//
   tLen += strlen(eP->prot) + 4;

// Add in the length of the traceID that we will send
//
   if (eP->tident) tLen += strlen(eP->tident) + 4;

// If the underlying protocol is not sss then it may replace the username
// and group name which determines the uid and gid. Otherwise, those are
// extracted from the keyfile by the server. This only applies if a uid
// or gid was enetered in the SecEntity object in the first place.
//
   if (*(eP->prot) && strcmp("sss", eP->prot))
      {if (eP->uid && !XrdOucUtils::UserName( eP->uid, uName, sizeof(uName)))
          tLen += strlen(uName) + 4;
          else *uName = 0;
       if (eP->gid &&  XrdOucUtils::GroupName(eP->gid, gName, sizeof(gName)))
          tLen += strlen(gName) + 4;
          else *gName = 0;
      }

// At the end we attach the credentials if they are not too large. The data
// must be at the end because it can optionally be pruned when returned.
//
   if (eP->credslen && eP->credslen <= XrdSecsssRR_Data::MaxCSz)
      {tLen += eP->credslen + 3;
       incCreds = true;
      }

// If no identity information, return failure otherwise allocate a struct
//
   if (!tLen || !(eData = (char *)malloc(tLen))) return 0;

// Now stick each entry into the iData field
//
   bP = eData;
   if (eP->name)
      {*bP++ = XrdSecsssRR_Data::theName; XrdOucPup::Pack(&bP,eP->name);}
   if (eP->vorg)
      {*bP++ = XrdSecsssRR_Data::theVorg; XrdOucPup::Pack(&bP,eP->vorg);}
   if (eP->role)
      {*bP++ = XrdSecsssRR_Data::theRole; XrdOucPup::Pack(&bP,eP->role);}
   if (eP->grps)
      {*bP++ = XrdSecsssRR_Data::theGrps; XrdOucPup::Pack(&bP,eP->grps);}
   if (eP->caps)
      {*bP++ = XrdSecsssRR_Data::theCaps; XrdOucPup::Pack(&bP,eP->caps);}
   if (eP->endorsements)
      {*bP++ = XrdSecsssRR_Data::theEndo; XrdOucPup::Pack(&bP,eP->endorsements);}
   if (rLen)
      {*bP++ = XrdSecsssRR_Data::theRand; XrdOucPup::Pack(&bP, rBuff, rLen);}
   iLen = bP - eData;

// Add the underlying security protocol
//
   if (*(eP->prot))
      {*bP++ = XrdSecsssRR_Data::theAuth; XrdOucPup::Pack(&bP,eP->prot);}

// Add the trace ident
//
   if (eP->tident)
      {*bP++ = XrdSecsssRR_Data::theTID;  XrdOucPup::Pack(&bP,eP->tident);}

// Add the user name if present
//
   if (*uName)
      {*bP++ = XrdSecsssRR_Data::theUser;  XrdOucPup::Pack(&bP,uName);}

// Add the group name if present
//
   if (*gName)
      {*bP++ = XrdSecsssRR_Data::theGrup;  XrdOucPup::Pack(&bP,gName);}

// Add additional attributes. The buffer is gauranteed to be big enough.
//
   if (theAttr.bL > 0)
      {theAttr.bP = bP;
       eP->eaAPI->List(theAttr);
       bP = theAttr.bP;
      }

// Compue total length
//
   tLen = bP - eData;

// Attach the credentials if we must
//
   if (incCreds)
      {*bP++ = XrdSecsssRR_Data::theCred;
       credLen = XrdOucPup::Pack(&bP, eP->creds, eP->credslen) + 1;
      } else credLen = 0;

// All done
//
   return true;
}

/******************************************************************************/
/*                           s e t H o s t N a m e                            */
/******************************************************************************/
  
void XrdSecsssEnt::setHostName(const char *hnP)
{
   char *bP;
   int n = strlen(hnP);

// The host name always goes into the serialized data. So, we prepack it here.
//
   if (n)
      {if (myHostName) free(myHostName);
       myHostName = bP = (char *)malloc(n+4);
       *bP++ = XrdSecsssRR_Data::theHost;
       myHostNLen = XrdOucPup::Pack(&bP, hnP) + 1;
      }
}
