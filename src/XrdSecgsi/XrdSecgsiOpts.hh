#ifndef __XRD_GSIOPTS_H__
#define __XRD_GSIOPTS_H__
/******************************************************************************/
/*                                                                            */
/*                      X r d S e c g s i O p t s . h h                       */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

namespace
{
/******************************************************************************/
/*                   D e f i n e s   a n d   S t r u c t s                    */
/******************************************************************************/
  
#define WARN(x) std::cerr <<"Secgsi warning: " << x <<'\n' <<std::flush

#define OTINIT(a,b,x) {a, b, sizeof(x)/sizeof(x [0]), x}

#define LIB_XRDVOMS "libXrdVoms.so"

struct OptsMap
      {const char *optKey;
       int         optVal;
      };

struct OptsTab
      {const char     *opName;
       int             opDflt;
       int             numMap;
       struct OptsMap *mapOpts;
      };

/******************************************************************************/
/*                            - a u t h z c a l l                             */
/******************************************************************************/

static const int azAlways = 1;
static const int azNoVoms = 0;
  
OptsMap azCallKV[] = {{"always", 1},
                      {"novoms", 0}
                     };

OptsTab azCallOpts = OTINIT("-authzcall",1,azCallKV);
  
/******************************************************************************/
/*                             - a u t h z p x y                              */
/******************************************************************************/
  
static const int azFull     = 0;
static const int azLast     = 1;
static const int azCred     = 1;
static const int azEndo     = 2;

OptsMap azPxyKV[] = {{"creds=fullchain", azCred+(10*azFull)},
                     {"creds=lastcert",  azCred+(10*azLast)},
                     {"endor=fullchain", azEndo+(10*azFull)},
                     {"endor=lastcert",  azEndo+(10*azLast)}
                     };

OptsTab azPxyOpts = OTINIT("-authz",0,azPxyKV);
  
/******************************************************************************/
/*                                   - c a                                    */
/******************************************************************************/
  
static const int caNoVerify = 0;
static const int caVerifyss = 1;
static const int caVerify   = 2;

OptsMap caVerKV[] = {{"noverify", caNoVerify},
                     {"verifyss", caVerifyss},
                     {"verify",   caVerify}
                     };

OptsTab caVerOpts = OTINIT("-ca",caVerifyss,caVerKV);
  
/******************************************************************************/
/*                                  - c r l                                   */
/******************************************************************************/
  
static const int crlIgnore    =  0;
static const int crlTry       =  1;
static const int crlUse       =  2;
static const int crlRequire   =  3;
static const int crlUpdate    = 10;
static const int crlNoUpdt    =  0;

OptsMap crl1KV[] = {{"ignore",       crlIgnore},
                    {"try",          crlTry},
                    {"use",          crlUse},
                    {"use,updt",     crlUse+crlUpdate},
                    {"require",      crlRequire},
                    {"require,updt", crlRequire+crlUpdate}
                   };

OptsTab crlOpts = OTINIT("-crl",crlTry,crl1KV);
  
/******************************************************************************/
/*                               - d l g p x y                                */
/******************************************************************************/
  
static const int dlgIgnore  = 0;
static const int dlgReqSign = 1;
static const int dlgSendpxy = 2; // Only client can set this!

OptsMap sDlgKV[] = {{"ignore",  dlgIgnore},
                    {"request", dlgReqSign}
                   };

OptsTab sDlgOpts = OTINIT("-dlgpxy",dlgIgnore,sDlgKV);
  
/******************************************************************************/
/*                              - g m a p o p t                               */
/******************************************************************************/
  
static const int gmoNoMap     =  0;
static const int gmoTryMap    =  1;
static const int gmoUseMap    =  2;
static const int gmoEntDN     = 10;
static const int gmoEntDNHash =  0;

OptsMap gmoKV[] = {{"nomap",        gmoNoMap},
                   {"nomap,usedn",  gmoNoMap+gmoEntDN},
                   {"trymap",       gmoTryMap},
                   {"trymap,usedn", gmoTryMap+gmoEntDN},
                   {"usemap",       gmoUseMap}
                  };

OptsTab gmoOpts = OTINIT("-gmopts",gmoTryMap,gmoKV);

/******************************************************************************/
/*                             - t r u s t d n s                              */
/******************************************************************************/
  
OptsMap tdnsKV[] = {{"false", 0},
                    {"true",  1}
                   };

OptsTab tdnsOpts = OTINIT("-trustdns",0,tdnsKV);
  
/******************************************************************************/
/*                               - v o m s a t                                */
/******************************************************************************/
  
static const int vatIgnore  = 0;
static const int vatExtract = 1;
static const int vatRequire = 2;

OptsMap vomsatKV[] = {{"ignore",  vatIgnore},
                      {"extract", vatExtract},
                      {"require", vatRequire}
                     };

OptsTab vomsatOpts = OTINIT("-vomsat",vatIgnore,vomsatKV);

/******************************************************************************/
/*                            g e t O p t N a m e                             */
/******************************************************************************/
  
const char *getOptName(OptsTab &oTab, int opval)
{
   for (int i = 0; i < oTab.numMap; i++)
       if (opval == oTab.mapOpts[i].optVal) return oTab.mapOpts[i].optKey;
   return "nothing";
}

/******************************************************************************/
/*                             g e t O p t V a l                              */
/******************************************************************************/
  
int getOptVal(OptsTab &oTab, const char *oVal)
{
   if (isdigit(*oVal))
      {int n = atoi(oVal);
       for (int i = 0; i < oTab.numMap; i++)
           if (n == oTab.mapOpts[i].optVal) return n;
      } else {
       for (int i = 0; i < oTab.numMap; i++)
           if (!strcmp(oVal, oTab.mapOpts[i].optKey))
              return oTab.mapOpts[i].optVal;
      }

   if (oTab.opDflt >= 0)
      {WARN("invalid " <<oTab.opName <<" argument '" <<oVal <<
            "'; using '" <<getOptName(oTab, oTab.opDflt) <<"' instead!");
      }
   return oTab.opDflt;
}

/******************************************************************************/
/*
int getOptVal(OptsTab &oTab1, OptsTab &oTab2, char *oVal)
{
// Check if this is a two-factor option
//
   char *comma = index(oVal, ',');
   if (comma) *comma = 0;

// Handle the first part
//
   int flag = getOptVal(oTab1, oVal);

// Get the second part
//
   if (comma)
      {flag += getOptVal(oTab2, comma+1);
       *comma = ',';
      }
   return flag;
}
*/
}
#endif
