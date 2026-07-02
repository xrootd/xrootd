/******************************************************************************/
/*                                                                            */
/*                          X r d S y s E 2 T . c c                           */
/*                                                                            */
/*(c) 2019 by the Board of Trustees of the Leland Stanford, Jr., University   */
/*Produced by Andrew Hanushevsky for Stanford University under contract       */
/*           DE-AC02-76-SFO0515 with the Deprtment of Energy                  */
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
  
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

#include "XrdSys/XrdSysPthread.hh"

#ifdef __GNU__
#define ERRNOBASE 0x40000000
#else
#define ERRNOBASE 0
#endif

namespace
{
static const int errSlots = 144;

// All state used by XrdSysE2T() lives in this struct. It is exposed through a
// function-local static (see errTable() below) so that it is constructed on
// first use. XrdSysE2T() is a low-level helper that may be invoked from the
// static initializers of other translation units (e.g. XrdNetIdentity.cc);
// a function-local static guarantees construction before use regardless of
// cross-TU static initialization order, avoiding the static init order fiasco.
//
struct ErrTable
{
   XrdSysMutex                e2sMutex;
   std::map<int, std::string> e2sMap;
   const char*                Errno2String[errSlots] = {0};
   int                        maxErrno = 0;

   ErrTable();
};

ErrTable::ErrTable()
{
   char *eTxt, eBuff[80];
   int lastGood = 0;

// Premap all known error codes.
//
   for(int i = 1; i < errSlots; i++)
      {eTxt = strerror(ERRNOBASE + i);
       if (eTxt)
          { eTxt = strdup(eTxt);
           *eTxt = tolower(*eTxt);
           Errno2String[i] = eTxt;
           lastGood = i;
          }
      }
   // NOTE: On systems without EAUTH (authentication error; currently all Glibc systems but GNU Hurd),
   // EAUTH is remapped to EBADE ('invalid exchange').  Given there's no current XRootD use of a
   // syscall that can return EBADE, we assume EBADE really means authentication denied.
#if defined(EBADE)
   if (EBADE - ERRNOBASE > 0 && EBADE - ERRNOBASE < errSlots) {
      if (Errno2String[EBADE - ERRNOBASE])
         free((char*)Errno2String[EBADE - ERRNOBASE]);
      Errno2String[EBADE - ERRNOBASE] = "authentication failed - possible invalid exchange";
      if (EBADE - ERRNOBASE > lastGood)
         lastGood = EBADE - ERRNOBASE;
   }
   else {
      e2sMap[EBADE] = "authentication failed - possible invalid exchange";
   }
#endif

   // Supply generic message for missing ones
   //
   for (int i = 1; i < lastGood; i++)
       {if (!Errno2String[i])
           {snprintf(eBuff, sizeof(eBuff), "unknown error %d", ERRNOBASE + i);
            Errno2String[i] = strdup(eBuff);
           }
       }

// Record the highest valid one
//
   Errno2String[0] = "no error";
   maxErrno = lastGood;
}

// Construct-on-first-use accessor for the error table.
//
ErrTable& errTable()
{
   static ErrTable table;
   return table;
}
}

/******************************************************************************/
/*                             X r d S y s E 2 T                              */
/******************************************************************************/
  
const char *XrdSysE2T(int errcode)
{
   ErrTable &T = errTable();
   char eBuff[80];

// Check if we can return this immediately
//
   if (errcode == 0) return T.Errno2String[0];
   if (errcode > ERRNOBASE && errcode <= ERRNOBASE + T.maxErrno)
      return T.Errno2String[errcode - ERRNOBASE];

// If this is a negative value, then return a generic message
//
   if (errcode < 0) return "negative error";

// Our errno registration wasn't sufficient, so check if it's already
// registered and if not, register it.
//
   T.e2sMutex.Lock();
   std::string &eTxt = T.e2sMap[errcode];
   if (!eTxt.size())
      {snprintf(eBuff, sizeof(eBuff), "unknown error %d", errcode);
       eTxt = std::string(eBuff);
       T.e2sMap[errcode] = eTxt;
      }

// Return the result
//
   T.e2sMutex.UnLock();
   return eTxt.c_str();
}
