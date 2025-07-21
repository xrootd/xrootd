/******************************************************************************/
/*                                                                            */
/*                         X r d M o n i t o r . c c                          */
/*                                                                            */
/* (c) 2024 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <string.h>

#include "Xrd/XrdMonitor.hh"
#include "Xrd/XrdMonRoll.hh"
#include "XrdSys/XrdSysError.hh"

/******************************************************************************/
/*                         G l o b a l O b j e c t s                          */
/******************************************************************************/

namespace XrdGlobal
{
extern XrdSysError       Log;
}
using namespace XrdGlobal;
  
/******************************************************************************/
/* Private:                      F i n d S e t                                */
/******************************************************************************/
  
XrdMonitor::RegInfo* XrdMonitor::FindSet(const char* setName, int sType)
{
   for (auto it = regVec.begin(); it != regVec.end(); it++)
       if ((*it)->setType & sType && !std::strcmp((*it)->setName, setName))
          return *it;
   return 0;
}

/******************************************************************************/
/*                                F o r m a t                                 */
/******************************************************************************/

int XrdMonitor::Format(char* buff, int bsize, int& item, int opts)
{
// Make sure we are in the range of things to format
//
   if (item < 0 || item >= (int)regVec.size()) return 0;

// Skip over types that are not wanted
//
   while((regVec[item]->setType & opts) == 0)
        {item++;
         if (item >= (int)regVec.size()) return 0;
        }

// Format the item
//
   if (opts & F_JSON) bsize = FormJSON(*regVec[item], buff, bsize);
      else            bsize = FormXML (*regVec[item], buff, bsize);
   item++;
   return bsize;
}
  
/******************************************************************************/

int XrdMonitor::Format(char* buff, int bsize, const char* setName, int opts)
{
// Find the set and format it
//
   RegInfo* regInfo = FindSet(setName, opts);
   if (!regInfo) return 0;
   if (opts & F_JSON) bsize = FormJSON(*regInfo, buff, bsize);
      else            bsize = FormXML (*regInfo, buff, bsize);
   return bsize;
}

/******************************************************************************/
/* Private:                     F o r m J S O N                               */
/******************************************************************************/

#define Updt(x) if (x >= bsize) return 0; bsize -= x; buff += x; bLen += x

int  XrdMonitor::FormJSON(XrdMonitor::RegInfo& regInfo, char* buff, int bsize)
{
   int n, bLen = 0;
   unsigned int kVal;

// Format the header
//
   n = snprintf(buff, bsize, "%s", regInfo.Json.hdr);
   Updt(n);

// Format all the variable
//
   for (int i = 0; i < (int)regInfo.Json.key.size(); i++)
       {kVal = *regInfo.keyVal[i];
        n = snprintf(buff, bsize, "%s%u", regInfo.Json.key[i], kVal);
        Updt(n);
       }

// Insert end and return
//
   n = snprintf(buff, bsize, "}");
   Updt(n);
   return bLen;  
}

/******************************************************************************/
/* Private:                      F o r m X M L                                */
/******************************************************************************/

int  XrdMonitor::FormXML(XrdMonitor::RegInfo& regInfo, char* buff, int bsize)
{
   int n, bLen = 0;
   unsigned int kVal;

// Format the header
//
   n = snprintf(buff, bsize, "%s", regInfo.Xml.hdr);
   Updt(n);

// Format all the variables
//
   for (int i = 0; i < (int)regInfo.Xml.keyBeg.size(); i++)
       {kVal = *regInfo.keyVal[i];
        n = snprintf(buff, bsize, "%s%u%s", regInfo.Xml.keyBeg[i], kVal,
                                            regInfo.Xml.keyEnd[i]);
        Updt(n);
       }

// Insert end and return
//
   n = snprintf(buff, bsize, "%s", "</stats>");
   Updt(n);
   return bLen;  
}
  
/******************************************************************************/
/*                              R e g i s t e r                               */
/******************************************************************************/
  
bool XrdMonitor::Register(XrdMonRoll::rollType  setType, const char* setName,
                          XrdMonRoll::setMember setVec[])
{   char buff[512];
    int numE = 0;

// Make sure this map has not been previously defined
//
   if (FindSet(setName,-1)) return false;

// Count the number of elements
//
   for(int i = 0; setVec[i].varName; i++) numE++;
   if (!numE) return true;

// Determine type of set
//
   int sType;
   switch(setType)
         {case XrdMonRoll::AddOn:    sType = isAdon; break;
          case XrdMonRoll::Misc:     sType = isAdon; break; // Deprecated
          case XrdMonRoll::Plugin:   sType = isPlug; break;
          case XrdMonRoll::Protocol: sType = isPlug; break; // Deprecated
          default:                   sType = isPlug; break;
         }

// Allocate a regInfo object
//
   RegInfo* regInfo = new RegInfo(setName, sType);
   regInfo->Json.key.reserve(numE);
   regInfo->Xml.keyBeg.reserve(numE);
   regInfo->Xml.keyEnd.reserve(numE);
   regInfo->keyVal = new RAtomic_uint*[numE]();

// Create the registry
//
   for(int i = 0;i < numE; i++)
      {snprintf(buff, sizeof(buff), ",\"%s\":", setVec[i].varName);
       regInfo->Json.key.push_back(strdup(buff));
       snprintf(buff, sizeof(buff), "<%s>", setVec[i].varName);
       regInfo->Xml.keyBeg.push_back(strdup(buff));
       snprintf(buff, sizeof(buff), "</%s>", setVec[i].varName);
       regInfo->Xml.keyEnd.push_back(strdup(buff));
       regInfo->keyVal[i] = &setVec[i].varValu;
      }
   regInfo->Json.key[0]++;

// Complete the registry
//
   snprintf(buff, sizeof(buff), "\"stats_%s\":{", setName);
   regInfo->Json.hdr = strdup(buff);
   snprintf(buff, sizeof(buff), "<stats id=\"%s\">", setName);
   regInfo->Xml.hdr = strdup(buff);

// Insert registry into the map
//
   regVec.push_back(regInfo);

// Display information
//
   char etxt[256];
   snprintf(etxt, sizeof(etxt), "%s set %s registered with %d variable(s)",
            (setType == XrdMonRoll::Misc ? "plugin" : "protocol"),setName,numE);
   Log.Say("Config monitor: ", etxt);
   return true;
}
