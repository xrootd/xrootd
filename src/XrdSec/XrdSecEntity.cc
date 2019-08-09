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

#include <map>
#include <string.h>
#include <vector>

#include "XrdSec/XrdSecAttr.hh"
#include "XrdSec/XrdSecEntity.hh"

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
class XrdSecEntityAttrs
{
public:

std::vector<XrdSecAttr *> attrVec;

std::map<std::string, std::string> attrMap;

     XrdSecEntityAttrs() {}
    ~XrdSecEntityAttrs() {}
};

class XrdSecEntityXtend
{
public:

     XrdSecEntityXtend() {}
    ~XrdSecEntityXtend() {}
};

/******************************************************************************/
/*                                   A d d                                    */
/******************************************************************************/
  
bool XrdSecEntity::Add(XrdSecAttr &attr)
{
   XrdSysMutexHelper mHelp(eMutex);

// Check if this attribute already exists
//
   if (!entAttrs) entAttrs = new XrdSecEntityAttrs;
      else {std::vector<XrdSecAttr*>::iterator it;
            for (it  = entAttrs->attrVec.begin();
                 it != entAttrs->attrVec.end(); it++)
                {if ((*it)->Signature == attr.Signature) return false;}
           }

// Add the attribute object to our list of objects
//
   entAttrs->attrVec.push_back(&attr);
   return true;
}

/******************************************************************************/
  
bool XrdSecEntity::Add(const std::string &key,
                       const std::string &val, bool replace)
{
   XrdSysMutexHelper mHelp(eMutex);
   std::map<std::string, std::string>::iterator it;
   bool found = false;

// Check if this attribute already exists
//
   if (!entAttrs) entAttrs = new XrdSecEntityAttrs;
      else {it = entAttrs->attrMap.find(key);
            if (it != entAttrs->attrMap.end())
               {if (!replace) return false;
                found = true;
           }   }

// Add or replace the value
//
   if (found) it->second = val;
      else entAttrs->attrMap.insert(std::make_pair(key, val));
   return true;
}

/******************************************************************************/
/*                                   G e t                                    */
/******************************************************************************/

XrdSecAttr *XrdSecEntity::Get(const void *sigkey)
{
   XrdSysMutexHelper mHelp(eMutex);

// Return pointer to the attribute if it exists
//
   if (entAttrs)
      {std::vector<XrdSecAttr*>::iterator it;
       for (it  = entAttrs->attrVec.begin();
            it != entAttrs->attrVec.end(); it++)
           {if ((*it)->Signature == sigkey) return *it;}
      }

// Attribute not found
//
   return (XrdSecAttr *)0;
}

/******************************************************************************/

bool XrdSecEntity::Get(const std::string &key, std::string &val)
{
   XrdSysMutexHelper mHelp(eMutex);

// Return pointer to the attribute if it exists
//
   if (entAttrs)
      {std::map<std::string, std::string>::iterator it;
       it = entAttrs->attrMap.find(key);
       if (it != entAttrs->attrMap.end())
          {val = it->second;
           return true;
          }
      }

// The key does not exists
//
   return false;
}

/******************************************************************************/
/*                                 R e s e t                                  */
/******************************************************************************/
  
void XrdSecEntity::Reset(bool isnew,  const char *spV, const char *dpV)
{
   memset( pros, 0, sizeof(pros) );
   if (dpV) strncpy(pros, dpV, sizeof(prot)-1);
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
   rsvd = 0;
   addrInfo = 0;
   tident = 0;
   sessvar = 0;
   uid     = 0;
   gid     = 0;
   memset(future, 0, sizeof(future));

   if (isnew)
      {entAttrs = 0;
       entXtend = 0;
      } else {
       if (entAttrs) ResetAttrs();
       entXtend = 0; // No extension for now.
      }
}

/******************************************************************************/
/*                            R e s e t A t t r s                             */
/******************************************************************************/
  
void XrdSecEntity::ResetAttrs(bool dodel)
{
   XrdSysMutexHelper mHelp(eMutex);

// If we have no attributes we are done
//
   if (!entAttrs) return;

// Cleanup the key-value map
//
   entAttrs->attrMap.clear();

// Run through attribute objects, deleting each one
//
   std::vector<XrdSecAttr*>::iterator it;
   for (it = entAttrs->attrVec.begin(); it != entAttrs->attrVec.end(); it++)
       {(*it)->Delete();}

// Now clear the whole vector
//
   entAttrs->attrVec.clear();

// Delete the extension if so wanted
//
   if (dodel) {delete entAttrs; entAttrs = 0;}
}
