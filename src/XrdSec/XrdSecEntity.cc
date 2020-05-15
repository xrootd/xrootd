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

#include <iostream>
#include <map>
#include <string.h>
#include <vector>

#include "XrdSec/XrdSecAttr.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
class XrdSecEntityXtra
{
public:

XrdSysMutex xMutex;

std::vector<XrdSecAttr *> attrVec;

std::map<std::string, std::string> attrMap;

     XrdSecEntityXtra() {}
    ~XrdSecEntityXtra() {}
};

/******************************************************************************/
/*                                   A d d                                    */
/******************************************************************************/
  
bool XrdSecEntity::Add(XrdSecAttr &attr)
{
   XrdSysMutexHelper mHelp(entXtra->xMutex);
   std::vector<XrdSecAttr*>::iterator it;

// Check if this attribute already exists
//
   for (it = entXtra->attrVec.begin(); it != entXtra->attrVec.end(); it++)
       if ((*it)->Signature == attr.Signature) return false;

// Add the attribute object to our list of objects
//
   entXtra->attrVec.push_back(&attr);
   return true;
}

/******************************************************************************/
  
bool XrdSecEntity::Add(const std::string &key,
                       const std::string &val, bool replace)
{
   XrdSysMutexHelper mHelp(entXtra->xMutex);
   std::map<std::string, std::string>::iterator it;
   bool found = false;

// Check if this attribute already exists
//
   it = entXtra->attrMap.find(key);
   if (it != entXtra->attrMap.end())
      {if (!replace) return false;
       found = true;
      }

// Add or replace the value
//
   if (found) it->second = val;
      else entXtra->attrMap.insert(std::make_pair(key, val));
   return true;
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

// Avoid the vulgarities of old gcc compilers thatidn't implemented full C++11
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
   mDest.Say(tident, " Pidn '", (pident ? pident : ""), "'");

   mDest.Say(tident, " Crlen ", std::to_string((LLint)credslen).c_str());
   mDest.Say(tident, " ueid  ", std::to_string((ULint)ueid).c_str());
   mDest.Say(tident, " uid   ", std::to_string((ULint)uid).c_str());
   mDest.Say(tident, " gid   ", std::to_string((ULint)gid).c_str());

// Display it's attributes, if any
//
   List(displayAttr);
}
  
/******************************************************************************/
/*                                   G e t                                    */
/******************************************************************************/

XrdSecAttr *XrdSecEntity::Get(const void *sigkey) const
{
   XrdSysMutexHelper mHelp(entXtra->xMutex);
   std::vector<XrdSecAttr*>::iterator it;

// Return pointer to the attribute if it exists
//
   for (it = entXtra->attrVec.begin(); it != entXtra->attrVec.end(); it++)
       if ((*it)->Signature == sigkey) return *it;

// Attribute not found
//
   return (XrdSecAttr *)0;
}

/******************************************************************************/

bool XrdSecEntity::Get(const std::string &key, std::string &val) const
{
   XrdSysMutexHelper mHelp(entXtra->xMutex);
   std::map<std::string, std::string>::iterator it;

// Return pointer to the attribute if it exists
//
   it = entXtra->attrMap.find(key);
   if (it != entXtra->attrMap.end())
      {val = it->second;
       return true;
      }

// The key does not exists
//
   return false;
}

/******************************************************************************/
/*                                  K e y s                                   */
/******************************************************************************/

std::vector<std::string> XrdSecEntity::Keys() const
{
   XrdSysMutexHelper mHelp(entXtra->xMutex);
   std::map<std::string, std::string>::iterator itM;
   std::vector<std::string> keyVec;

   for (itM  = entXtra->attrMap.begin();
        itM != entXtra->attrMap.end(); itM++) keyVec.push_back(itM->first);

   return keyVec;
}
  
/******************************************************************************/
/*                                  L i s t                                   */
/******************************************************************************/

void XrdSecEntity::List(XrdSecEntityAttrCB &attrCB) const
{
   XrdSysMutexHelper mHelp(entXtra->xMutex);
   std::map<std::string, std::string>::iterator itM;
   std::vector<const char *> attrDel;
   std::vector<const char *>::iterator itV;
   XrdSecEntityAttrCB::Action rc = XrdSecEntityAttrCB::Action::Stop;

   for (itM  = entXtra->attrMap.begin();
        itM != entXtra->attrMap.end(); itM++)
       {rc = attrCB.Attr(itM->first.c_str(), itM->second.c_str());
        if (rc == XrdSecEntityAttrCB::Stop) break;
           else if (rc == XrdSecEntityAttrCB::Delete)
                   attrDel.push_back(itM->first.c_str());
       }

   if (rc != XrdSecEntityAttrCB::Stop) attrCB.Attr(0, 0);

   for (itV  = attrDel.begin(); itV != attrDel.end(); itV++)
       entXtra->attrMap.erase(std::string(*itV));
}
  
/******************************************************************************/
/*                                 R e s e t                                  */
/******************************************************************************/
  
void XrdSecEntity::Reset(bool isnew, const char *spV)
{
   memset( prot, 0, sizeof(prot) );
   if (spV) strncpy(prot, spV, sizeof(prot)-1);

   name = 0;
   host = 0;
   vorg = 0;
   role = 0;
   grps = 0;
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

   if (isnew) entXtra = new XrdSecEntityXtra;
      else ResetXtra();
}

/******************************************************************************/
/*                             R e s e t X t r a                              */
/******************************************************************************/
  
void XrdSecEntity::ResetXtra(bool dodel)
{
   XrdSysMutexHelper mHelp(entXtra->xMutex);

// Cleanup the key-value map
//
   entXtra->attrMap.clear();

// Run through attribute objects, deleting each one
//
   std::vector<XrdSecAttr*>::iterator it;
   for (it = entXtra->attrVec.begin(); it != entXtra->attrVec.end(); it++)
       {(*it)->Delete();}

// Now clear the whole vector
//
   entXtra->attrVec.clear();

// Delete the extension if so wanted
//
   if (dodel)
   {
     mHelp.UnLock(); // we have to unlock the mutex bofere it's destroyed
     delete entXtra; entXtra = 0;
   }
}
