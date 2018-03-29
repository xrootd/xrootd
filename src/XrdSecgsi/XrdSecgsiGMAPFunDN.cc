/******************************************************************************/
/*                                                                            */
/*             X r d S e c g s i G M A P F u n D N . c c                      */
/*                                                                            */
/* (c) 2011, G. Ganis / CERN                                                  */
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
/*                                                                            */
/******************************************************************************/

/* ************************************************************************** */
/*                                                                            */
/* GMAP function implementation extracting info from the DN                   */
/*                                                                            */
/* ************************************************************************** */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "XrdVersion.hh"

#include "XrdOuc/XrdOucHash.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"

static XrdSysError   dnDest(0, "gmapdn_");
static XrdSysLogger  dnLogger;
static XrdOucTrace  *dnTrace = 0;

#define TRACE_Authen   0x0002
#define EPNAME(x)    static const char *epname = x;
#define PRINT(y)    {if (dnTrace) {dnTrace->Beg(epname); cerr <<y; dnTrace->End();}}
#define DEBUG(y)   if (dnTrace && (dnTrace->What & TRACE_Authen)) PRINT(y)


/******************************************************************************/
/*                   V e r s i o n   I n f o r m a t i o n                    */
/******************************************************************************/
  
XrdVERSIONINFO(XrdSecgsiGMAPFun,secgsigmap);

/******************************************************************************/
/*                     G l o b a l s   &   S t a t i c s                      */
/******************************************************************************/

enum XrdSecgsi_Match {kFull     = 0,
                      kBegins   = 1,
                      kEnds     = 2,
                      kContains = 4
                     };

class XrdSecgsiMapEntry_t
{
public:
   XrdSecgsiMapEntry_t(const char *v, const char *u, int t) : val(v), user(u), type(t) { }

   XrdOucString  val;
   XrdOucString  user;
   int           type;
};

static XrdOucHash<XrdSecgsiMapEntry_t> gMappings;

/******************************************************************************/
/*                   F u n c t i o n s   &   M e t h o d s                    */
/******************************************************************************/
  
//__________________________________________________________________________
static int FindMatchingCondition(const char *, XrdSecgsiMapEntry_t *mc, void *xmp)
{
   // Print content of entry 'ui' and go to next

   XrdSecgsiMapEntry_t *mpe = (XrdSecgsiMapEntry_t *)xmp;

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


int XrdSecgsiGMAPInit(const char *cfg);

//
// Main function
//
extern "C"
{
char *XrdSecgsiGMAPFun(const char *dn, int now)
{
   // Implementation of XrdSecgsiGMAPFun extracting the information from the 
   // distinguished name 'dn'
   EPNAME("GMAPFunDN");

   // Init the relevant fields (only once)
   if (now <= 0) {
      if (XrdSecgsiGMAPInit(dn) != 0)
         return (char *)-1;
      return (char *)0;
   }

   // Output
   char *name = 0;

   XrdSecgsiMapEntry_t *mc = 0;
   // Try the full match first
   if ((mc = gMappings.Find(dn))) {
      // Get the associated user
      name = new char[mc->val.length() + 1];
      strcpy(name, mc->val.c_str());
   } else {
      // Else scan the available mappings
      mc = new XrdSecgsiMapEntry_t(dn, "", kFull);
      gMappings.Apply(FindMatchingCondition, (void *)mc);
      if (mc->user.length() > 0) {
         name = new char[mc->user.length() + 1];
         strcpy(name, mc->user.c_str());
      }
   }
   if (name) {
      DEBUG("mapping DN '"<<dn<<"' to '"<<name<<"'");
   } else {
      DEBUG("no valid match found for DN '"<<dn<<"'");
   }
  
   // Done
   return name;
}}

//
// Init the relevant parameters from a dedicated config file
//
int XrdSecgsiGMAPInit(const char *parms)
{
   // Initialize the relevant parameters
   //      parms = "[cfg]|[d|dbg|debug]"
   // The config file 'cfg' can also be defined by XRDGSIGMAPDNCF.
   // The flag 'd|dbg|debug' enables some verbosity.
   // Return 0 on success, -1 otherwise
   EPNAME("GMAPInitDN");

   bool debug = 0;
   XrdOucString pps(parms), p, cfg;
   int from = 0;
   while ((from = pps.tokenize(p, from, '|')) != -1) {
      if (p.length() > 0) {
         if (p == "d" || p == "dbg" || p == "debug") {
            debug = 1;
         } else {
            cfg = p;
         }
      }
   }
   // Initiate error logging and tracing
   dnDest.logger(&dnLogger);
   dnTrace = new XrdOucTrace(&dnDest);
   if (debug) dnTrace->What |= TRACE_Authen;

   if (cfg.length() <= 0) cfg = getenv("XRDGSIGMAPDNCF");
   if (cfg.length() <= 0) {
      PRINT("ERROR: undefined config file path");
      return -1;
   }

   FILE *fcf = fopen(cfg.c_str(), "r");
   if (fcf) {
      char l[4096], val[4096], usr[256];
      while (fgets(l, sizeof(l), fcf)) {
         int len = strlen(l);
         if (len < 2) continue;
         if (l[0] == '#') continue;
         if (l[len-1] == '\n') l[len-1] = '\0';
         if (sscanf(l, "%4096s %256s", val, usr) >= 2) {
            XrdOucString stype = "matching";
            char *p = &val[0];
            int type = kFull;
            if (val[0] == '^') {
               // Starts-with
               type = kBegins;
               p = &val[1];
               stype = "beginning with";
            } else {
               int vlen = strlen(val);
               if (val[vlen-1] == '$') {
                  // Ends-with
                  type = kEnds;
                  val[vlen-1] = '\0';
                  stype = "ending with";
               } else if (val[vlen-1] == '+') {
                  // Contains
                  type = kContains;
                  val[vlen-1] = '\0';
                  stype = "containing";
               }
            }
            // Register
            gMappings.Add(p, new XrdSecgsiMapEntry_t(p, usr, type));
            //
            DEBUG("mapping DNs "<<stype<<" '"<<p<<"' to '"<<usr<<"'");
         }
      }
      fclose(fcf);
   } else {
      PRINT("ERROR: config file '"<<cfg<<"' could not be open (errno: "<<errno<<")");
      return -1;
   }
   // Done
   return 0;
}
