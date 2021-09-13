/******************************************************************************/
/*                                                                            */
/*                   X r d S e c E n t i t y A t t r . c c                    */
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
#include <vector>

#include "XrdSec/XrdSecAttr.hh"
#include "XrdSec/XrdSecEntityXtra.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                                   A d d                                    */
/******************************************************************************/
  
bool XrdSecEntityAttr::Add(XrdSecAttr &attr)
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
  
bool XrdSecEntityAttr::Add(const std::string &key,
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
/*                                   G e t                                    */
/******************************************************************************/

XrdSecAttr *XrdSecEntityAttr::Get(const void *sigkey)
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

bool XrdSecEntityAttr::Get(const std::string &key, std::string &val)
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

std::vector<std::string> XrdSecEntityAttr::Keys()
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

void XrdSecEntityAttr::List(XrdSecEntityAttrCB &attrCB)
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
