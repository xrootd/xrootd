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

#include <set>
#include <stack>
#include <stdexcept>
#include <stdio.h>
#include <string.h>

#include "Xrd/XrdMonitor.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysError.hh"

/******************************************************************************/
/*                         G l o b a l O b j e c t s                          */
/******************************************************************************/

namespace XrdGlobal
{
extern XrdSysError Log;
}
using namespace XrdGlobal;
  
/******************************************************************************/
/*                     L o c a l   D e f i n i t i o n s                      */
/******************************************************************************/
/*
using MRIFam = XrdMonRoll::Item::Family;

using MRISch = XrdMonRoll::Item::Schema;

using MRITrt = XrdMonRoll::Item::Trait;
*/
/******************************************************************************/
/*                   X r d M o n i t o r : : R e g I n f o                    */
/******************************************************************************/

XrdMonitor::RegInfo::RegInfo(const char* sName, const char* tName,
                            XrdMonitor::sType sTVal)   
                   : typName(strdup(tName)),
                     setName(strdup(sName)),
                     setType(sTVal)
{   char buff[512];
    snprintf(buff,sizeof(buff),"%s %s.Item[%%d]%%s",tName,sName);
    eTmplt = strdup(buff);
}

XrdMonitor::RegInfo::~RegInfo()
{
   if (typName)  free(typName);
   if (setName)  free(setName);
   if (Json_hdr) free(Json_hdr);
   if (Xml_hdr)  free(Xml_hdr);
   if (eTmplt)   free(eTmplt);
}
  
/******************************************************************************/
/*                            X r d M o n i t o r                             */
/******************************************************************************/
  
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

#define AddC(x) {if (bsize-- < 1) return 0; *buff++ = x; bLen++;}

#define Updt(x) {if (x >= bsize) return 0; bsize -= x; buff += x; bLen += x;}

int  XrdMonitor::FormJSON(XrdMonitor::RegInfo& regInfo, char* buff, int bsize)
{
   char* aStart;
   int n, bLen = 0;

// Format the header
//
   n = snprintf(buff, bsize, "%s", regInfo.Json_hdr);
   Updt(n);

// Prine the first object
//
   aStart = buff;

// Format all the elements as needed
//
   for (int i = 0; i < regInfo.iCount; i++)
       {XrdMonRoll::Item& item = regInfo.iVec[i];

        // First handle non-data/key elements because they have no separator
        //
        if (item.Kind == MRIFam::isSchema)
           {if (item.Plan ==  MRISch::endArray)  {AddC(']'); continue;}
            if (item.Plan ==  MRISch::endObject) {AddC('}'); continue;}
           } else if (item.Kind == MRIFam::isMutex)
                     {if (item.doLK) item.mtxP->Lock();
                         else item.mtxP->UnLock();
                      continue;
                     }

        // Add a comma if we need to
        //
        if (buff != aStart) {AddC(',');}

        // Add the key if it needs to apear
        // 
        if (!item.Array)
           {n = snprintf(buff, bsize, "\"%s\":", item.keyP);
            Updt(n);
           }

        // Insert proper value of schema element
        //
        switch(item.Kind)
              {case MRIFam::isBinary:
                    if (!(n = V2S(regInfo, item, buff, bsize))) return 0;         
                    Updt(n);
                    break;
               case MRIFam::isSchema:
                    if (item.Plan ==  MRISch::begArray)
                       {AddC('['); aStart = buff;}
                       else if (item.Plan ==  MRISch::begObject)
                               {AddC('{'); aStart = buff;}
                    break;
               case MRIFam::isText:
                    if (!(n = V2T(regInfo, item, buff, bsize, true))) return 0;         
                    Updt(n);
                    break;
               default: ValErr(regInfo, item.iNum,
                               "Unknown item family encountered!!!");
                        return 0;
              }
       }

// Insert end and return
//
   AddC('}');
   return bLen;  
}

/******************************************************************************/
/* Private:                      F o r m X M L                                */
/******************************************************************************/

int  XrdMonitor::FormXML(XrdMonitor::RegInfo& regInfo, char* buff, int bsize)
{
   int n, bLen = 0;
//   unsigned int kVal;

// Format the header
//
   n = snprintf(buff, bsize, "%s", regInfo.Xml_hdr);
   Updt(n);

// Format all the variables
//
   for (int i = 0; i < regInfo.iCount; i++)
       {XrdMonRoll::Item& item = regInfo.iVec[i];

        // First handle non-data/key  elements
        //
        if (item.Kind == MRIFam::isSchema)
           {if (item.Plan ==  MRISch::endArray
            ||  item.Plan ==  MRISch::endObject)
               {n = snprintf(buff, bsize, "</%s>", item.keyP);
                Updt(n);
                continue;
               }
           } else if (item.Kind == MRIFam::isMutex)
                     {if (item.doLK) item.mtxP->Lock();
                         else item.mtxP->UnLock();
                      continue;
                     }

        // All XML values have start and end tags. Insert the front tag.
        // 
        n = snprintf(buff, bsize, "<%s>", item.keyP);
        Updt(n);

        // Insert proper value of schema element
        //
        switch(item.Kind)
              {case MRIFam::isBinary:
                    if (!(n = V2S(regInfo, item, buff, bsize))) return 0;         
                    Updt(n);
                    break;
               case MRIFam::isSchema:
                    if (item.Plan == MRISch::begArray
                    ||  item.Plan == MRISch::begObject) continue;
                    break;
               case MRIFam::isText:
                    if (!(n = V2T(regInfo, item, buff, bsize, false))) return 0;         
                    Updt(n);
                    break;
               default: ValErr(regInfo, item.iNum,
                               "Unknown item family encountered!!!");
                        return 0;
              }

        // All XML values have start and end tags. Insert the back tag.
        // 
        n = snprintf(buff, bsize, "</%s>", item.keyP);
        Updt(n);
       }

// Insert end and return
//
   n = snprintf(buff, bsize, "%s", "</stats>");
   Updt(n);
   return bLen;  
}

/******************************************************************************/
/* Private:                      R e g F a i l                                */
/******************************************************************************/
  
bool XrdMonitor::RegFail(const char* TName, const char* SName, const char* why)
{
   char buff[512];

// Format message and spit it out
//
   snprintf(buff, sizeof(buff), "Attempt to register monitor summary for "
            "%s set %s failed; %s", TName, SName, why);
   Log.Say("Config warning:", buff);
   return false;
}
  
/******************************************************************************/
/*                              R e g i s t e r                               */
/******************************************************************************/

bool XrdMonitor::Register(XrdMonRoll::rollType  setType, const char* setName,
                          XrdMonRoll::Item itemVec[], int itemCnt)
{
   const char* tName = "Plugin";
   char buff[512];

// Determine type of set
//
   sType stype;
   switch(setType)
         {case XrdMonRoll::AddOn:
          case XrdMonRoll::Misc:     stype = isAdon;  // Deprecated
                                     tName = "AddOn";
                                     break;
          case XrdMonRoll::Plugin:
          case XrdMonRoll::Protocol: stype = isPlug;  // Deprecated
                                     break;
          default:                   stype = isPlug;
                                     break;
         }

// Reject invalid sets
//
   if (itemCnt < 1 || itemVec == 0)
      return RegFail(tName, setName, "invaled parameters");

// Make sure this map has not been previously defined
//
   if (FindSet(setName,-1))
      return RegFail(tName, setName, "set name already registered");


// Allocate a new registry
//
   RegInfo* regInfo = new RegInfo(setName, tName, stype);
   regInfo->iCount  = itemCnt;
   regInfo->iVec    = itemVec;

// Before registering this specification we need to validate it
//
   if (!Validate(*regInfo))
      {delete regInfo;
       return RegFail(tName, setName, "invalid description");
      }

// Complete the registry
//
   regInfo->eTmplt = strdup(buff);
   snprintf(buff, sizeof(buff), "\"stats_%s\":{", setName);
   regInfo->Json_hdr = strdup(buff);
   snprintf(buff, sizeof(buff), "<stats id=\"%s\">", setName);
   regInfo->Xml_hdr = strdup(buff);

// Add registry to our list of registries
//
   regVec.push_back(regInfo);

// Display information
//
   snprintf(buff, sizeof(buff), "%s set %s registered with %d items(s)",
            tName, setName,itemCnt);
   Log.Say("Config monitor: ", buff);
   return true;
}

/******************************************************************************/
/* Private:                          V 2 S                                    */
/******************************************************************************/

// Warning: This function may only be called for Clan types isAtomic, isFloat,
//          or isDouble.

int XrdMonitor::V2S(XrdMonitor::RegInfo& rI, XrdMonRoll::Item& item,
                    char* buff, int blen)
{
   std::string s;

// Test first for the most common case - isAtomic
//
   if (item.Clan == MRITrt::isAtomic || item.Clan == MRITrt::isBtomic) try
      {if (item.Clan == MRITrt::isAtomic)
          s = std::visit([](const auto arg) -> std::string
                           {auto val = arg->load();
                            return std::to_string(val);
                           }, item.ratV);
          else
          s = std::visit([](const auto arg) -> std::string
                           {auto val = arg->load();
                            return std::to_string(val);
                           }, item.rbtV);

      }  catch (const std::runtime_error& e)
               {char eBuff[512];
                snprintf(eBuff, sizeof(eBuff), rI.eTmplt, int(item.iNum),
                                " atomic number formatting failed;");
                Log.Emsg("XrdMonitor", eBuff, e.what());
                return 0;
               }
      else if (item.Clan == MRITrt::isFloat) s = std::to_string(*item.fltP);
              else s = std::to_string(*item.dblP);

// Copy result to the supplied buffer
//
   return snprintf(buff, blen, "%s", s.c_str());
}

/******************************************************************************/
/* Private:                          V 2 T                                    */
/******************************************************************************/

// This method may only be called for Clan types isChar and isString
//
int XrdMonitor::V2T(XrdMonitor::RegInfo& rI, XrdMonRoll::Item& item,
                    char* buff, int blen, bool isJSON)
{
   const char* text = (item.Clan == MRITrt::isChar ? item.chrP
                                                   : item.strP->c_str());

// Copy result to the supplied buffer
//
   if (isJSON) return snprintf(buff, blen, "\"%s\"", text);
   return snprintf(buff, blen, "%s", text);
}
  
/******************************************************************************/
/* Private:                       V a l E n d                                 */
/******************************************************************************/

void XrdMonitor::ValEnd(bool& isBad, XrdMonitor::RegInfo& rI, const char* aoT, 
                        const char* begKey, XrdMonRoll::Item* endP)
{
// If there is no starting key then the starting item generated an error
// and there is no reason to perform any tests. Otherwise, if there is no
// ending key, then we simply use the starting key. If there is an ending key
// it must match the starting key.
//
   if (begKey)
      {if (!(endP->keyP)) endP->keyP = begKey;
          else {if (strcmp(begKey, endP->keyP))
                   {char etxt[80];
                    snprintf(etxt, sizeof(etxt),
                             "end%s key differs from beg%s key", aoT, aoT);
                    isBad = ValErr(rI, endP->iNum, etxt);
                   }
                }
      }
}
  
/******************************************************************************/
/* Private:                       V a l E r r                                 */
/******************************************************************************/

bool XrdMonitor::ValErr(XrdMonitor::RegInfo& rInfo, int iNum, const char* eTxt)
{
   char eBuff[1024];

// Format the header
//
   snprintf(eBuff, sizeof(eBuff), rInfo.eTmplt, iNum, " incorrect;");

// Send of message
//
   Log.Say("Config warning: MonRoll ", eBuff, eTxt);
   return false;
}
  
/******************************************************************************/
/* Private:                       V a l K e y                                 */
/******************************************************************************/

void XrdMonitor::ValKey(bool& isBad, XrdMonitor::RegInfo& rI, 
                        XrdMonRoll::Item* itemP)
{
// Keys are required in object context and are option in array context.
// When JSON is emitted keys in array context are ignored but used for XML.
// Otherwise, a key must be specified.
//
   if (itemP->Array)  {if (!(itemP->keyP)) itemP->keyP = "item";}
      else {if (!(itemP->keyP))
               isBad = ValErr(rI, itemP->iNum, "key not specified");
           }
}
  
/******************************************************************************/
/* Private:                     V a l i d a t e                               */
/******************************************************************************/

#define VALERR(x) isBad |= ValErr(regInfo, i, x)
  
bool XrdMonitor::Validate(XrdMonitor::RegInfo& regInfo)
{
   std::set<XrdSysMutex*> lokActv;
   std::stack<XrdMonRoll::Item*> synStax;
   bool isBad = false;

// Add a defined element to out stacks so that top() is always defined
//
   XrdMonRoll::Item myItem("", MRISch::unk);
   synStax.push(&myItem);

// Run through the the items making sure they provide a consistent syntax
//
   for (int i = 0; i < regInfo.iCount; i++)
       {XrdMonRoll::Item* itemP = &regInfo.iVec[i];
        itemP->iNum = static_cast<short>(i);
        if (itemP->keyP && *(itemP->keyP) == 0) itemP->keyP = 0;
        itemP->Array = synStax.top()->Plan == MRISch::begArray;
 
        switch(itemP->Kind)
              {case MRIFam::isBinary:
                    ValKey(isBad, regInfo, itemP);
                    break;
               case MRIFam::isMutex:
                    if (itemP->doLK)
                       {if (lokActv.find(itemP->mtxP) == lokActv.end())
                           lokActv.insert(itemP->mtxP);
                           else VALERR("locks a previously locked mutex");
                       } else { 
                        auto it = lokActv.find(itemP->mtxP);
                        if (it != lokActv.end()) lokActv.erase(it);
                           else VALERR("unLocks a mutex not previously locked");
                       }
                     break;
               case MRIFam::isSchema:
                    if (itemP->Plan == MRISch::begArray
                    ||  itemP->Plan == MRISch::begObject)
                       {ValKey(isBad, regInfo, itemP);
                        synStax.push(itemP);
                        break;
                       }
                    {XrdMonRoll::Item* topP = synStax.top();
                     if (itemP->Plan == MRISch::endArray)
                        {if (topP->Plan == MRISch::begArray)
                            {synStax.pop();
                             ValEnd(isBad, regInfo, "Array", topP->keyP, itemP);  
                            } else {
                             VALERR("endArray is not paired with begArray");
                            }
                         break;
                        }
                     if (itemP->Plan == MRISch::endObject)
                        {if (topP->Plan == MRISch::begObject)
                            {synStax.pop();
                             ValEnd(isBad, regInfo, "Object", topP->keyP, itemP);  
                            } else {
                             VALERR("endObject is not paired with begObject");
                            }
                         break;
                        }
                    }
                    VALERR("Invalid schema specification");
                    break;
               case MRIFam::isText:
                    if (itemP->Clan == MRITrt::isChar && !(itemP->chrP))
                       {VALERR("text value pointer is nil");}
                    ValKey(isBad, regInfo, itemP);
                    break;
               default: VALERR("Unknown item family encountered!!!");
                        break;
              }
       }

// Make sure everything has ended
//
   if (!isBad)
      {char eBuff[1024];
       if (synStax.size() > 1)
          {snprintf(eBuff, sizeof(eBuff), regInfo.eTmplt,
                    int(synStax.top()->iNum), " definition not ended");
           Log.Say("Config warning: MonRoll ", eBuff);
          }
       if (!lokActv.empty())
          {snprintf(eBuff, sizeof(eBuff), regInfo.eTmplt, 0,
                    " one or more locks not unlocked!");
           Log.Say("Config warning: MonRoll ", eBuff);
          }
      }

// Return result
//
   if (isBad) return false;
   return true;
}
