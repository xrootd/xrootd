/******************************************************************************/
/*                                                                            */
/*                    X r d O u c G M a p . h h                               */
/*                                                                            */
/* (c) 2006 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

/* ************************************************************************** */
/*                                                                            */
/* Interface to grid map files                                                */
/*                                                                            */
/* This code was initially in XrdSecProtocolgsi. It has been extracted        */
/* to allow usage in other contexts, namely XrdHttp.                          */
/*                                                                            */
/* ************************************************************************** */

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucGMap.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdOuc/XrdOucStream.hh"

enum XrdOucGMap_Match {kFull     = 0,
                       kBegins   = 1,
                       kEnds     = 2,
                       kContains = 4
                      };

#define PRINT(t,n,y)    {if (t) {t->Beg(n); cerr <<y; t->End();}}
#define DEBUG(d,t,n,y)  {if (d && t) {t->Beg(n); cerr <<y; t->End();}}

//__________________________________________________________________________
static int FindMatchingCondition(const char *, XrdSecGMapEntry_t *mc, void *xmp)
{
   // Print content of entry 'ui' and go to next

   XrdSecGMapEntry_t *mpe = (XrdSecGMapEntry_t *)xmp;

   bool match = 0;
   if (mc && mpe) {
      if (mc->type == kContains) {
         if (mpe->val.find(mc->val) != STR_NPOS) match = 1;
      } else if (mc->type == kBegins) {
         if (mpe->val.beginswith(mc->val)) match = 1;
      } else if (mc->type == kEnds) {
         if (mpe->val.endswith(mc->val)) match = 1;
      } else {
         if (mpe->val.matches(mc->val.c_str())) match = 1;
      }
      if (match) mpe->user = mc->user;
   }

   // We stop if matched, otherwise we continue
   return (match) ? 1 : 0;
}

// Getter
//
extern "C"
{
XrdOucGMap *XrdOucgetGMap(XrdOucGMapArgs)
{
   // Create a XrdOucGMap object and return it if valid
   XrdOucGMap *gm = new XrdOucGMap(eDest, mapfn, parms);
   if (gm && gm->isValid()) return gm;
   if (gm) delete gm;
   return (XrdOucGMap *)0;
}}

// Constructor
//
XrdOucGMap::XrdOucGMap(XrdOucGMapArgs)
          : valid(0), mf_mtime(-1), notafter(-1), timeout(600), elogger(eDest), tracer(0), dbg(0)
{
   // Set tracer
   //
   tracer = new XrdOucTrace(eDest);

   // Parse parameters, if any
   //
   XrdOucString pp(parms), p;
   if (pp.length() > 0) {
      int from = 0;
      while ((from = pp.tokenize(p, from, '|')) != -1) {
         // Debug
         if (p == "debug" || p == "dbg") {
            dbg = 1; 
         } else if (p.beginswith("to=")) {
            p.erasefromstart(3);
            if (p.isdigit()) {
               timeout = p.atoi();
            } else {
               PRINT(tracer, "XrdOucGMap", "timeout value badly formatted ("<<p<<"): ignoring");
            }
         }
      }
   }

   // Set notafter is timeout is active
   //
   if (timeout > 0) notafter = time(0) + (time_t) timeout;

   // Set the file name
   //
   mf_name = mapfn;
   if (mf_name.length() <= 0) {
      mf_name = getenv("GRIDMAP");
      if (mf_name.length() <= 0)
         mf_name = "/etc/grid-security/grid-mapfile";
   }
   // Check if it can be read
   //
   if (access(mf_name.c_str(), R_OK) != 0) {
      PRINT(tracer, "XrdOucGMap", "cannot access grid map file '"<< mf_name <<"'in read mode; errno: "
                                  <<errno<<" - aborting");
      return;
   }

   // Load the file
   //
   if (load(mf_name.c_str()) != 0) {
      PRINT(tracer, "XrdOucGMap", "problems loading file "<<mf_name<<" - aborting");
      return;
   }

   // Done
   valid = 1;
}

// Loader
//
int XrdOucGMap::load(const char *mf, bool force)
{

   // We need an exclusive lock here
   xsl.Lock(xs_Exclusive);

   // Check if we need to load
   //
   struct stat st;
   if (stat(mf_name.c_str(), &st) != 0) {
      PRINT(tracer, "XrdOucGMap::load", "cannot access grid map file; errno: "<<errno<<" - aborting");
      // Delete the stored information if the file has been deleted
      if (errno == ENOENT) mappings.Purge();
      xsl.UnLock();
      return -1;
   }
#if defined(__APPLE__)
   if (mf_mtime > 0 && (mf_mtime >= st.st_mtimespec.tv_sec) && !force) {
#else
   if (mf_mtime > 0 && (mf_mtime >= st.st_mtim.tv_sec) && !force) {
#endif
      DEBUG(dbg, tracer, "XrdOucGMap::load", "map information up-to-date: no need to load");
      xsl.UnLock();
      return 0;
   }

   // Delete the stored information
   //
   mappings.Purge();

   // Read the file
   //
   int  fD, rc;
   const char *inst = getenv("XRDINSTANCE") ? getenv("XRDINSTANCE") : "gmap config instance";
   XrdOucEnv myEnv;
   XrdOucStream mapf(elogger, inst, &myEnv, "");
   
   if ( (fD = open(mf_name.c_str(), O_RDONLY, 0)) < 0) {
      PRINT(tracer, "XrdOucGMap::load", "ERROR: map file '"<<mf_name<<"' could not be open (errno: "<<errno<<")");
      xsl.UnLock();
      return -1;
   }
   mapf.Attach(fD);

   // Now start reading records until eof.
   //
   char *var;
   while ((var = mapf.GetLine())) {
      int len = strlen(var);
      if (len < 2) continue;
      if (var[0] == '#') continue;

       // Extract DN
      char *p0 = &var[0];
      char cr = ' ';
      if (p0[0] == '"') {
         p0 = &var[1];
         cr = '"';
      }
      char *p = p0;
      int l0 = 0;
      while (p0[l0] != cr)
         l0++;
      p0 = (p0 + l0 + 1);
      while (*p0 == ' ')
         p0++;
      // Check for special delimiters    
      char stype[20] = {"matching"};
      int type = kFull;
      if (p[0] == '^') {
         // Starts-with
         type = kBegins;
         p++;
         l0--;
         strcpy(stype, "beginning with");
      } else {
         if (p[l0-1] == '$') {
            // Ends-with
            type = kEnds;
            p[--l0] = '\0';
            strcpy(stype, "ending with");
         } else if (p[l0-1] == '+') {
            // Contains
            type = kContains;
            p[--l0] = '\0';
            strcpy(stype, "containing");
         }
      }
      XrdOucString udn(p, l0);

      // Extract username
      XrdOucString usr(p0);

      // Register
      if (usr.length() > 0) {
         mappings.Add(p, new XrdSecGMapEntry_t(udn.c_str(), usr.c_str(), type));
         DEBUG(dbg, tracer, "XrdOucGMap::load", "mapping DN: '"<<udn<<"' to user: '"<< usr <<"' (type:'"<< stype <<"')");
      } else {
         PRINT(tracer, "XrdOucGMap::load", "ERROR: uncomplete line found in file '"<<mf_name
                     <<"': "<<var<<" - skipping");
      }
   }
   // Now check if any errors occured during file i/o
   //
   if ((rc = mapf.LastError()))
      PRINT(tracer, "XrdOucGMap::load", "ERROR: reading file '"<<mf_name<<"': "<<rc);
   mapf.Close();

   // Store the modification time
   //
#if defined(__APPLE__)
   mf_mtime = st.st_mtimespec.tv_sec;
#else
   mf_mtime = st.st_mtim.tv_sec;
#endif

   // Done
   xsl.UnLock();
   return 0;
}

// Mapper
//
int XrdOucGMap::dn2user(const char *dn, char *user, int ulen, time_t now)
{

   // Check if we need to reload the information
   //
   if (notafter > 0) {
      if (now <= 0) now = time(0);
      if (notafter < now) {
         // Reload the file
         if (load(mf_name.c_str()) != 0) {
            PRINT(tracer, "XrdOucGMap::dn2user", "problems loading file "<<mf_name);
            return -1;
         }
         if (timeout > 0) notafter = now + (time_t) timeout;
      }
   }
  
   // A shared lock is enough
   xsl.Lock(xs_Shared);
 
   // Search
   //
   XrdSecGMapEntry_t *mc = 0;
   // Try the full match first
   //
   if ((mc = mappings.Find(dn))) {
      // Save the associated user
      strncpy(user, mc->user.c_str(), ulen);
      user[ulen-1] = 0;
   } else {
      // Else scan the available mappings
      //
      mc = new XrdSecGMapEntry_t(dn, "", kFull);
      mappings.Apply(FindMatchingCondition, (void *)mc);
      if (mc->user.length() > 0) {
         strncpy(user, mc->user.c_str(), ulen);
         user[ulen-1] = 0;
      }
   }
   if (strlen(user)) {
      DEBUG(dbg, tracer, "XrdOucGMap::dn2user", "mapping DN '"<<dn<<"' to '"<<user<<"'");
   } else {
      DEBUG(dbg, tracer, "XrdOucGMap::dn2user", "no valid match found for DN '"<<dn<<"'");
   }
  
   // Done
   xsl.UnLock();
   return 0;

}

