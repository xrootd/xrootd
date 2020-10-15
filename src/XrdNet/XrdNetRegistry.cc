/******************************************************************************/
/*                                                                            */
/*                     X r d N e t R e g i s t r y . c c                      */
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

#include <string>
#include <vector>

#include "XrdNet/XrdNetAddr.hh"
#include "XrdNet/XrdNetRegistry.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/

namespace
{
class regEntry
{
public:

static regEntry         *first;
regEntry                *next;
regEntry                *parent;
std::string              hName;
std::vector<std::string> hVec;
uint8_t                  refs;
bool                     rotate;

static void Add(regEntry *rP) {rP->next = first; first = rP;}

static
regEntry   *Find(const char *hName)
                {regEntry *rP = first;
                 while(rP && (hName != rP->hName)) rP = rP->next;
                 if (rP && rP->parent) return rP->parent;
                 return rP;
                }

       void Hold(bool want)
                {if (want) myLock.ReadLock();
                    else   myLock.UnLock();
                }

       void Update(const char **hlist, int hlnum, bool rot)
                  {myLock.WriteLock();
                   hVec.assign(hlist, hlist+hlnum);
                   rotate = rot;
                   myLock.UnLock();
                  }

            regEntry(const char *hname, regEntry *pP)
                    : next(0), parent(pP), hName(hname), refs(0),
                      rotate(false) {}

            regEntry(const char *hname, const char *hlist[], int hlnum, bool rot)
                    : next(0), parent(0), hName(hname), refs(0), rotate(rot)
                    {hVec.assign(hlist, hlist+hlnum);}

           ~regEntry() {}

private:

XrdSysRWLock myLock;
};

regEntry *regEntry::first = 0;

}
  
/******************************************************************************/
/*                  L o c a l   S t a t i c   O b j e c t s                   */
/******************************************************************************/
  
namespace
{
XrdSysMutex regMutex;
}

/******************************************************************************/
/*                              G e t A d d r s                               */
/******************************************************************************/
  
const char *XrdNetRegistry::GetAddrs(const std::string       &hSpec,
                                     std::vector<XrdNetAddr> &aVec, int *ordn,
                                     XrdNetUtils::AddrOpts    opts, int pNum)
{
   regEntry *reP;
   unsigned int refs;

// Find the entry
//
   XrdSysMutexHelper mHelp(regMutex);
   if (!(reP = regEntry::Find(hSpec.c_str())))
      {aVec.clear();
       return "psuedo host not registered";
      }

// Hold this entry as we don't want to hold the global lock doing DNS lookups.
//
   if (reP->rotate) refs = reP->refs++;
      else refs = 0;
   reP->Hold(true);
   mHelp.UnLock();

// Resolve the the host specification (at least one must be resolvable)
//
   XrdNetUtils::GetAddrs(reP->hVec, aVec, ordn, opts, refs, true);

// Drop the hold on the entry and return result
//
   reP->Hold(false);
   if (aVec.size() == 0) return "registry entry unresolvable";
   return 0;
}
  
/******************************************************************************/
/*                              R e g i s t e r                               */
/******************************************************************************/

bool XrdNetRegistry::Register(const char *hName,
                              const char *hList[], int hLNum,
                              std::string *eText, bool rotate)
{
   regEntry *reP;

// Make sure we have valid parameters
//
   if (!hName || *hName != pfx || !hList || hLNum <= 0)
      {if (eText) *eText = "invalid calling arguments";
       return false;
      }

// Run through the list resolving all of the addresses. When registering, all
// of them must be resolvable. When running at least one must be resolvable.
//
   for (int i = 0; i < hLNum; i++) if (!Resolve(hList[i], eText)) return false;

// Do replacement or addition
//
   regMutex.Lock();
   if ((reP = regEntry::Find(hName))) reP->Update(hList, hLNum, rotate);
      else regEntry::Add(new regEntry(hName, hList, hLNum, rotate));
   regMutex.UnLock();

// All done
//
   return true;
}

/******************************************************************************/

bool XrdNetRegistry::Register(const char *hName, const char *hList,
                              std::string *eText, bool rotate)
{
   char *comma, *hosts = strdup(hList);
   std::vector<const char*> hVec;

// Make sure we have valid parameters
//
   if (!hName || *hName != pfx || !hList)
      {if (eText) *eText = "invalid calling arguments";
       return 0;
      }

// Check for alias creation
//
   if (*hList == pfx) return SetAlias(hName, hList, eText);

// Construct a vector of contacts
//
   hVec.reserve(16);
   hVec.push_back(hosts);
   comma = hosts;
   while((comma = index(comma, ',')))
        {*comma++ = 0;
         hVec.push_back(comma);
        }

// Verify that each element has a colon in it
//
   for (int i = 0; i < (int)hVec.size(); i++)
       {if (!index(hVec[i], ':'))
           {if (eText)
               {*eText = "port missing for '"; 
                *eText += hVec[i]; *eText += "'";
               }
            free(hosts);
            return false;
           }
       }

// Register this contact
//
   bool aOK = Register(hName, hVec.data(), (int)hVec.size(), eText, rotate);

// Cleanup and return result
//
   free(hosts);
   return aOK;
}

/******************************************************************************/
/* Private:                      R e s o l v e                                */
/******************************************************************************/
  
bool XrdNetRegistry::Resolve(const char *hSpec, std::string *eText)
{
   XrdNetAddr netAddr;
   const char *emsg;

// Validate the specification.
//
   emsg = netAddr.Set(hSpec);

// Check for errors
//
   if (emsg && strncmp(emsg, "Dynamic ", 8))
      {if (eText)
          {*eText  = "unable to resolve '"; *eText += hSpec;
           *eText += "'; "; *eText += emsg;
          }
       return false;
      }


// All done
//
   return true;
}

/******************************************************************************/
/* Private:                     S e t A l i a s                               */
/******************************************************************************/

bool XrdNetRegistry::SetAlias(const char *hAlias, const char *hName,
                              std::string *eText)
{
   regEntry    *reP;
   const char  *eWhy = 0;

// Verify that the source does not exist and the target does
//
   regMutex.Lock();
   if (regEntry::Find(hAlias)) eWhy = "source already exists";
      else if (!(reP = regEntry::Find(hName))) eWhy = "target does not exist";
   if (eWhy)
      {regMutex.UnLock();
       if (eText)
          {*eText = "alias "; *eText += hAlias; *eText += " not created; ";
           *eText += eWhy;
          }
       return false;
      }

// Add the alias
//
   regEntry::Add(new regEntry(hAlias,reP));
   regMutex.UnLock();
   return true;
}
