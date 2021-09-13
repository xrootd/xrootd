/******************************************************************************/
/*                                                                            */
/*                       X r d S e c E n t i t y . h h                        */
/*                                                                            */
/* (c) 2019 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstring>

#include "XrdSec/XrdSecEntity.hh"
#include "XrdSec/XrdSecEntityXtra.hh"
#include "XrdSys/XrdSysError.hh"

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdSecEntity::XrdSecEntity(const char *spName) : eaAPI(new XrdSecEntityXtra)
{
   Init(spName);
}
  
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdSecEntity::~XrdSecEntity()
{
   delete eaAPI->entXtra;
}

/******************************************************************************/
/*                               D i s p l a y                                */
/******************************************************************************/

void XrdSecEntity::Display(XrdSysError &mDest)
{
   class AttrCB : public XrdSecEntityAttrCB
        {public:
         XrdSecEntityAttrCB::Action Attr(const char *key, const char *val)
                           {mDest.Say(Tid, "Attr  ",key," = '", val, "'");
                            return XrdSecEntityAttrCB::Next;
                           }
         AttrCB(XrdSysError &erp, const char *tid) : mDest(erp), Tid(tid) {}
        ~AttrCB() {}

         XrdSysError &mDest;
         const char  *Tid;
        } displayAttr(mDest, tident);

   char theprot[XrdSecPROTOIDSIZE+1];

// Avoid vulgarities of old gcc compilers that didn't implemented full C++11
//
   typedef long long          int LLint;
   typedef long long unsigned int ULint;

// Make sure the protocol is poperly set
//
   memcpy(theprot, prot, XrdSecPROTOIDSIZE);
   theprot[XrdSecPROTOIDSIZE] = 0;

// Display this object
//
   mDest.Say(tident, " Protocol '", theprot, "'");
   mDest.Say(tident, " Name '", (name ? name : ""), "'");
   mDest.Say(tident, " Host '", (host ? host : ""), "'");
   mDest.Say(tident, " Vorg '", (vorg ? vorg : ""), "'");
   mDest.Say(tident, " Role '", (role ? role : ""), "'");
   mDest.Say(tident, " Grps '", (grps ? grps : ""), "'");
   mDest.Say(tident, " Caps '", (caps ? caps : ""), "'");
   mDest.Say(tident, " Pidn '", (pident ? pident : ""), "'");

   mDest.Say(tident, " Crlen ", std::to_string((LLint)credslen).c_str());
   mDest.Say(tident, " ueid  ", std::to_string((ULint)ueid).c_str());
   mDest.Say(tident, " uid   ", std::to_string((ULint)uid).c_str());
   mDest.Say(tident, " gid   ", std::to_string((ULint)gid).c_str());

// Display it's attributes, if any
//
   eaAPI->List(displayAttr);
}
  
/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
void XrdSecEntity::Init(const char *spV)
{
   memset( prot, 0, sizeof(prot) );
   memset( prox, 0, sizeof(prox) );
   if (spV) strncpy(prot, spV, sizeof(prot)-1);

   name = 0;
   host = 0;
   vorg = 0;
   role = 0;
   grps = 0;
   caps = 0;
   endorsements = 0;
   moninfo = 0;
   creds = 0;
   credslen = 0;
   ueid  = 0;
   addrInfo = 0;
   tident = 0;
   pident = 0;
   sessvar = 0;
   uid     = 0;
   gid     = 0;
   memset(future, 0, sizeof(future));
}
  
/******************************************************************************/
/*                                 R e s e t                                  */
/******************************************************************************/

void XrdSecEntity::Reset(const char *spV)
{
   Init(spV);
   eaAPI->entXtra->Reset();
}
