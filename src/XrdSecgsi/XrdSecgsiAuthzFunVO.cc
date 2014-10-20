/******************************************************************************/
/*                                                                            */
/*                X r d S e c g s i A u t h z F u n V O . c c                 */
/*                                                                            */
/* (c) 2011 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
  
/* Trivial AuthzKey/Func(), propogates the VO as a unix user and/or group.

1. To be used with gsi parametrized like:

   sec.protocol /opt/xrootd/lib/x86_64_linux_26 gsi \
       -certdir:/etc/grid-security/certificates \
       -cert:/etc/grid-security/xrd/xrdcert.pem \
       -key:/etc/grid-security/xrd/xrdkey.pem -crl:3 \
       -authzfun:libXrdAuthzVO.so -authzfunparms:<parms> \
       -gmapopt:10 -gmapto:0

2. The optional authzfunparms is formatted as a CGI string with one or more
   of the following key-value pairs:

   debug=1
   valido=<vlist>
   vo2grp=<gspec>
   vo2usr=<uspec>

   Where: debug   - turns debugging on.
          vlist   - specifies a comma-separated list of vo names that are
                    acceptable. If not specified, all vo's are accepted.
                    Otherwise, failure is returned if the the vo is not in
                    the list of vo's.
          gspec   - specifies how the vo is to be inserted into a group name.
                    Specify a printf-like format string with a single %s. This
                    is where the vo name is inserted. So, "%s" simply makes the
                    group name the vo name.
          uspec   - specifies how the vo is to be inserted into a user name.
                    The same rules apply as for gspec. If uspec is not specified
                    then the name comes from distinguished name in the
                    certificate (i.e. text after '/CN=') with spaces turned 
                    into underscores and the vo is not used. If uspec is
                    specified as a single asterisk (*) then the name field is
                    not touched and is as set by the gsi module.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "XrdVersion.hh"

#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucLock.hh"

/******************************************************************************/
/*                   V e r s i o n   I n f o r m a t i o n                    */
/******************************************************************************/
  
XrdVERSIONINFO(XrdSecgsiAuthzFun,secgsiauthz);

XrdVERSIONINFO(XrdSecgsiAuthzKey,secgsiauthz);

XrdVERSIONINFO(XrdSecgsiAuthzInit,secgsiauthz);

/******************************************************************************/
/*                    E x t e r n a l   F u n c t i o n s                     */
/******************************************************************************/
  
// The following functions are called by the authz plug-in driver.
//
extern "C"
{
  int XrdSecgsiAuthzInit(const char *cfg);
  int XrdSecgsiAuthzFun(XrdSecEntity &entity);
  int XrdSecgsiAuthzKey(XrdSecEntity &entity, char **key);
}

/******************************************************************************/
/*                      G l o b a l   V a r i a b l e s                       */
/******************************************************************************/
  
namespace
{
  const  int   g_certificate_format = 1;
  const  int   g_maxvolen           = 255;
  static char *g_valido             = 0;
  static char *g_vo2grp             = 0;
  static char *g_vo2usr             = 0;
  static int   g_debug              = 0;
  static int   g_cn2usr             = 1;
}

/******************************************************************************/
/*                         L o c a l   D e f i n e s                          */
/******************************************************************************/
  
#undef  PRINT
#define PRINT(y) if (g_debug) {std::cerr << y << "\n";}
#undef  PROUT
#define PROUT(_x_) \
        std::cerr <<inf_pfx <<"entity." #_x_ "='" <<(entity._x_ ? entity._x_ : "") <<"'.\n"

/******************************************************************************/
/*                     X r d S e c g s i A u t h z F u n                      */
/******************************************************************************/

/* Replace the group list and/or user name with the VO name if the VO is ours.

   Return <0 on fatal failure
          >0 error (this will still log the guy in, it seems)
           0 success, local username in entity.name
*/

int XrdSecgsiAuthzFun(XrdSecEntity &entity)
{
   static const char* inf_pfx = "INFO in AuthzFun: ";
   static XrdSysMutex Mutex;
   const char *vtxt = "", *etxt = 0;
   char vbuff[(g_maxvolen+1)*2];
   int i, n;

// We must have a vo, it must be shorter than 255 bytes, and it must be in our
// vo list of we have one
//
        if (!entity.vorg) etxt = "missing";
   else if ((n = strlen(entity.vorg)) > g_maxvolen) etxt = "too long";
   else if (g_valido)
           {*vbuff = ',';
            strcpy(vbuff+1, entity.vorg);
            if (!strstr(g_valido, vbuff))
               {vtxt = entity.vorg; etxt = " not allowed";}
           }

// Check if we passed the tests
//
   if (etxt)
      {std::cerr <<"AuthzVO: Invalid cert; vo " <<vtxt <<etxt <<std::endl;
       return -1;
      }

// Format group name if so wanted
//
   if (g_vo2grp)
      {snprintf(vbuff, sizeof(vbuff), g_vo2grp, entity.vorg);
       if (entity.grps) free(entity.grps);
       entity.grps = strdup(vbuff);
      }

// Format user  name if so wanted
//
   if (g_vo2usr)
      {snprintf(vbuff, sizeof(vbuff), g_vo2usr, entity.vorg);
       if (entity.name) free(entity.name);
       entity.name = strdup(vbuff);
      } else if (g_cn2usr && entity.name && (vtxt=strstr(entity.name,"/CN=")))
                {char *cP = vbuff;
                 if ((n = strlen(vtxt+4)) > g_maxvolen) n = g_maxvolen;
                 strncpy(vbuff, vtxt+4, n); vbuff[n] = 0;
                 while(*cP) {if (*cP == ' ') *cP = '_'; cP++;}
                 for (i = n-1; i >= 0; i--) {if (*cP == '_') *cP = 0;}
                 if (*vbuff)
                    {if (entity.name) free(entity.name);
                     entity.name = strdup(vbuff);
                    }
                }

// If debugging then print information. However, get a global mutex to keep
// from inter-leaving these lines with other threads, as much as possible.
//
   if (g_debug)
      {XrdOucLock lock(&Mutex);
       PROUT(name); PROUT(host); PROUT(grps); PROUT(vorg); PROUT(role);
      }

// All done
//
   return 0;
}

/******************************************************************************/
/*                     X r d S e c g s i A u t h z K e y                      */
/******************************************************************************/

int XrdSecgsiAuthzKey(XrdSecEntity &entity, char **key)
{
   // Return key by which entity.creds will be hashed.
   // For now return entity.creds itself.
   // The plan is to use DN + VO endorsements in the future.

   static const char* err_pfx = "ERR  in AuthzKey: ";
   static const char* inf_pfx = "INFO in AuthzKey: ";

   // Must have got something
   if (!key) {
      PRINT(err_pfx << "'key' is not defined!");
      return -1;
   }

   PRINT(inf_pfx << "Returning creds of len " << entity.credslen << " as key.");

   // Set the key
   *key = new char[entity.credslen + 1];
   strcpy(*key, entity.creds);

   return entity.credslen;
}

/******************************************************************************/
/*                    X r d S e c g s i A u t h z I n i t                     */
/******************************************************************************/

int XrdSecgsiAuthzInit(const char *cfg)
{
   // Return:
   //   -1 on falure
   //    0 to get credentials in raw form
   //    1 to get credentials in PEM base64 encoded form

   static const char* inf_pfx = "INFO in AuthzInit: ";
   XrdOucEnv *envP;
   char cfgbuff[2048], *sP;
   int i;

// The configuration string may mistakingly include other parms following
// the auzparms. So, trim the string.
//
   if (cfg)
      {i = strlen(cfg);
       if (1 >= (int)sizeof(cfgbuff)) i = sizeof(cfgbuff)-1;
       strncpy(cfgbuff, cfg, i);
       cfgbuff[i] = 0;
       if ((sP = index(cfgbuff, ' '))) *sP = 0;
      }
   if (!cfg || !(*cfg)) return g_certificate_format;

// Parse the config line (it's in cgi format)
//
   envP = new XrdOucEnv(cfgbuff);

// Set debug value
//
   if ((sP = envP->Get("debug")) && *sP == '1') g_debug = 1;

// Get the mapping strings
//
   if ((g_vo2grp = envP->Get("vo2grp"))) g_vo2grp = strdup(g_vo2grp);
   if ((g_vo2usr = envP->Get("vo2usr")))
      {g_cn2usr = 0;
       g_vo2usr = (!strcmp(g_vo2usr, "*") ? 0 :     strdup(g_vo2usr));
      }

// Now process the valid vo's
//
   if ((sP = envP->Get("valido")))
      {i = strlen(sP);
       g_valido = (char *)malloc(i+2);
       *g_valido = ',';
       strcpy(g_valido+1, sP);
      }

// All done with environment
//
   delete envP;

// All done.
//
   PRINT(inf_pfx <<"cfg='"<< (cfg ? cfg : "null") << "'.");
   return g_certificate_format;
}
