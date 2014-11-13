/******************************************************************************/
/*                                                                            */
/*                     X r d O f s C o n f i g P I . h h                      */
/*                                                                            */
/* (c) 2014 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/param.h>

#include "XrdVersion.hh"

#include "XrdAcc/XrdAccAuthorize.hh"

#include "XrdCks/XrdCks.hh"
#include "XrdCks/XrdCksConfig.hh"

#include "XrdCms/XrdCmsClient.hh"

#include "XrdOfs/XrdOfs.hh"
#include "XrdOfs/XrdOfsConfigPI.hh"

#include "XrdOss/XrdOss.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucPinLoader.hh"
#include "XrdOuc/XrdOucStream.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFAttr.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPlugin.hh"

/******************************************************************************/
/*                     G l o b a l s   &   S t a t i c s                      */
/******************************************************************************/
  
namespace
{
const char *drctv[] = {"xattrlib", "authlib", "ckslib", "cmslib", "osslib"};

const char *nullParms = 0;
}

XrdVERSIONINFOREF(XrdOfs);

/******************************************************************************/
/*                               D e f i n e s                                */
/******************************************************************************/
  
#define DO_LOAD(x) loadLib & (x & ~libIXMask)

#define PIX(x) x & libIXMask

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOfsConfigPI::XrdOfsConfigPI(const char  *cfn,  XrdOucStream   *cfgP,
                               XrdSysError *errP, XrdVersionInfo *verP)
                 : autPI(0), cksPI(0), cmsPI(0), ossPI(0), urVer(verP),
                   Config(cfgP),  Eroute(errP), CksConfig(0), ConfigFN(cfn),
                   CksAlg(0), CksRdsz(0), ossXAttr(false), ossCksio(false),
                   Loaded(false), LoadOK(false)
{
   int rc;

// Clear the library table
//
   memset(defLib, 0, sizeof(defLib));

// Set correct version
//
   if (!verP) urVer = &XrdVERSIONINFOVAR(XrdOfs);

// Preallocate the checksum configurator
//
   CksConfig = new XrdCksConfig(ConfigFN, Eroute, rc, *urVer);
   if (!rc) {delete CksConfig; CksConfig = 0;}
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdOfsConfigPI::~XrdOfsConfigPI()
{
   if (CksConfig) delete CksConfig;
   if (CksAlg)    free(CksAlg);
}
  
/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/

bool   XrdOfsConfigPI::Configure(XrdCmsClient *cmscP, XrdOucEnv *envP)
{
   return 0 != cmscP->Configure(ConfigFN, LP[PIX(theCmsLib)].parms, envP);
}
  
/******************************************************************************/
/*                               D e f a u l t                                */
/******************************************************************************/

void   XrdOfsConfigPI::Default(XrdOfsConfigPI::TheLib what, const char *lpath,
                                                            const char *lparm)
{
   int n = PIX(what);

   if (n < maxXXXLib && !RepLib(what, lpath, (lparm ? lparm : nullParms)))
      defLib[n] = true;
}
  
/******************************************************************************/
/*                             D e f a u l t C S                              */
/******************************************************************************/

void   XrdOfsConfigPI::DefaultCS(const char *alg)
{
   if (CksAlg) free(CksAlg);
   CksAlg = strdup(alg);
}
  
/******************************************************************************/
/*                               D i s p l a y                                */
/******************************************************************************/

void   XrdOfsConfigPI::Display()
{
   xxxLP *lP;
   char *oP, buff[2046];
   int aI = PIX(theAtrLib), oI = PIX(theOssLib);

// Display what we have
//
   for (int i = 0; i < maxXXXLib; i++)
       {oP = LP[i].opts;
             if (i != aI)   lP = &LP[i];
        else if (ossXAttr) {lP = &LP[oI]; oP = 0;}
        else                lP = &LP[i];
        if (lP->lib)
           {snprintf(buff, sizeof(buff), "ofs.%s %s%s %s", drctv[i],
                     (oP ? oP : ""), lP->lib, (lP->parms ? lP->parms : ""));
            Eroute->Say("       ", buff);
           }
       }
}
  
/******************************************************************************/
/*                                  L o a d                                   */
/******************************************************************************/
  
bool XrdOfsConfigPI::Load(int loadLib, XrdOucEnv *envP)
{
   extern XrdOss *XrdOssGetSS(XrdSysLogger *, const char *, const char *,
                              const char   *, XrdOucEnv  *, XrdVersionInfo &);
   bool aOK;

// Check if load was already called as we can only try once
//
   if (Loaded) return LoadOK;
   Loaded = true;

// Load the osslib first if so wanted
//
   if (DO_LOAD(theOssLib))
      {const char *ossLib = LP[PIX(theOssLib)].lib;
       if (!(ossPI = XrdOssGetSS(Eroute->logger(), ConfigFN, ossLib,
                        LP[PIX(theOssLib)].parms, envP, *urVer))) return false;
       if (ossLib && envP && (ossLib = envP->Get("oss.lib")))
          {free(LP[PIX(theOssLib)].lib);
           LP[PIX(theOssLib)].lib = strdup(ossLib);
          }
      }

// Now setup the extended attribute plugin if so desired
//
   if (DO_LOAD(theAtrLib))
      {     if (ossXAttr && LP[PIX(theOssLib)].lib) aOK = SetupAttr(theOssLib);
       else if             (LP[PIX(theAtrLib)].lib) aOK = SetupAttr(theAtrLib);
       else aOK = true;
       if (!aOK) return false;
      }
   XrdSysFAttr::Xat->SetMsgRoute(Eroute);

// Setup authorization if we need to
//
   if (DO_LOAD(theAutLib) && !SetupAuth()) return false;

// Setup checksumming if we need to
//
   if (DO_LOAD(theCksLib))
      {if (!CksConfig)
          {Eroute->Emsg("Config", "Unable to load checksum manager; "
                                  "incompatible versions.");
           return false;
          }
       cksPI = CksConfig->Configure(CksAlg, CksRdsz, (ossCksio ? ossPI : 0));
       if (!cksPI) return false;
      }

// Setup the cms if we need to
//
   if (DO_LOAD(theCmsLib) && !SetupCms()) return false;

// All done
//
   LoadOK = true;
   return true;
}
  
/******************************************************************************/
/*                                   N e w                                    */
/******************************************************************************/
  
XrdOfsConfigPI *XrdOfsConfigPI::New(const char  *cfn,  XrdOucStream   *cfgP,
                                    XrdSysError *errP, XrdVersionInfo *verP)
{
// Handle caller's version if so indicated
//
   if (verP && !XrdSysPlugin::VerCmp(*verP, XrdVERSIONINFOVAR(XrdOfs)))
      return 0;

// Return an actual instance
//
   return new XrdOfsConfigPI(cfn, cfgP, errP, verP);
}

/******************************************************************************/
/*                                O s s C k s                                 */
/******************************************************************************/

bool   XrdOfsConfigPI::OssCks() {return ossCksio;}
  
/******************************************************************************/
/*                                 P a r s e                                  */
/******************************************************************************/
  
bool XrdOfsConfigPI::Parse(TheLib what)
{
   char *val;

// Fan out based on what was specified
//
   switch(what)
         {case theAtrLib: return ParseAtrLib();
                          break;
          case theAutLib: break;
          case theCksLib: if (CksConfig)
                             {if (CksConfig->ParseLib(*Config)) return false;
                              RepLib(theCksLib, CksConfig->ManLib(), nullParms);
                              return true;
                             }
                          Eroute->Emsg("Config", "Checksum version error!");
                          return false;
                          break;
          case theCmsLib: break;
                          break;
          case theOssLib: return ParseOssLib();
                          break;
          default:        Eroute->Emsg("Config", "Invalid plugin Parse() call");
                          return false;
                          break;
         }

// Get the path
//
   if (!(val = Config->GetWord()) || !val[0])
      {Eroute->Emsg("Config", drctv[PIX(what)],"not specified"); return false;}

// Set the lib and parameterss
//
   return RepLib(what, val);
}

/******************************************************************************/
/* Private:                  P a r s e A t r L i b                            */
/******************************************************************************/
  
/* Function: ParseAtrLib

   Purpose:  To parse the directive: xattrlib {osslib | <path>} [<parms>]

             <path>    the path of the xattr library to be used.
             <parms>   optional parms to be passed

  Output: true upon success or false upon failure.
*/

bool XrdOfsConfigPI::ParseAtrLib()
{
   char *val;

// Get the path and parms
//
   if (!(val = Config->GetWord()) || !val[0])
      {Eroute->Emsg("Config", "xattrlib not specified"); return false;}

// Record the path and parms
//
   ossXAttr = !strcmp("osslib", val);
   return RepLib(theAtrLib, (ossXAttr ? 0 : val));
}

/******************************************************************************/
/* Private:                  P a r s e O s s L i b                            */
/******************************************************************************/
  
/* Function: ParseOssLib

   Purpose:  To parse the directive: osslib [<opts>] <path> [<parms>]
             <opts>: [+cksio] [+xattr] [<opts>]

             +cksio    use the oss plugin for checkum I/O.
             +xattr    the library contains the xattr plugin.
             <path>    the path of the oss library to be used.
             <parms>   optional parms to be passed

  Output: true upon success or false upon failure.
*/

bool XrdOfsConfigPI::ParseOssLib()
{
   char *val, oBuff[80];
   int   oI = PIX(theOssLib);

// Reset to defaults
//
    ossCksio = false;
    if (LP[oI].opts) {free(LP[oI].opts); LP[oI].opts = 0;}
    *oBuff = 0;

// Get the path and parms, and process keywords
//
   while((val = Config->GetWord()))
        {     if (!strcmp("+cksio",  val))
                 {if (!ossCksio) strcat(oBuff, "+cksio "); ossCksio = true;}
         else if (!strcmp("+xattr",  val))
                 {if (!ossXAttr) strcat(oBuff, "+xattr "); ossXAttr = true;}
         else break;
        }

// Check if we an osslib
//
   if (!val || !val[0])
      {Eroute->Emsg("Config", "osslib not specified"); return false;}

// Record the path and parameters
//
   if (*oBuff) LP[oI].opts = strdup(oBuff);
   return RepLib(theOssLib, val);
}

/******************************************************************************/
/*                                P l u g i n                                 */
/******************************************************************************/
  
bool   XrdOfsConfigPI::Plugin(XrdAccAuthorize *&piP)
{      return (piP = autPI) != 0;}

bool   XrdOfsConfigPI::Plugin(XrdCks          *&piP)
{      return (piP = cksPI) != 0;}

bool   XrdOfsConfigPI::Plugin(XrdCmsClient_t   &piP)
{      return (piP = cmsPI) != 0;}

bool   XrdOfsConfigPI::Plugin(XrdOss          *&piP)
{      return (piP = ossPI) != 0;}

/******************************************************************************/
/* Private:                       R e p L i b                                 */
/******************************************************************************/
  
bool XrdOfsConfigPI::RepLib(XrdOfsConfigPI::TheLib what,
                            const char *newLib, const char *newParms)
{
   const char *parmP;
   char  parms[2048];
   int  xLib = PIX(what);

// Replace any existing library specification
//
   if (LP[xLib].lib && newLib)
      {if (!strcmp(LP[xLib].lib, newLib) && defLib[xLib])
          {const char *dfltLib = (newParms ? newLib : LP[xLib].lib);
           Eroute->Say("Config warning: ", "specified ", drctv[xLib],
                       " overrides default ", dfltLib);
          }
       free(LP[xLib].lib);
       defLib[xLib] = false;
      }
    LP[xLib].lib = (newLib ? strdup(newLib) : 0);

// Get any parameters
//
   if (newParms) parmP = (newParms == nullParms ? 0 : newParms);
      else {*parms = 0; parmP = parms;
            if (!Config->GetRest(parms, sizeof(parms)))
               {Eroute->Emsg("Config", drctv[xLib], "parameters too long");
                return false;
               }
           }

// Record the parameters
//
   if (LP[xLib].parms) free(LP[xLib].parms);
   LP[xLib].parms = (*parmP ? strdup(parmP) : 0);
   return true;
}

/******************************************************************************/
/*                            S e t C k s R d S z                             */
/******************************************************************************/

void   XrdOfsConfigPI::SetCksRdSz(int rdsz) {CksRdsz = rdsz;}
  
/******************************************************************************/
/* Private:                    S e t u p A t t r                              */
/******************************************************************************/

bool XrdOfsConfigPI::SetupAttr(XrdOfsConfigPI::TheLib what)
{
   XrdSysXAttr *(*ep)(XrdSysError *, const char *, const char *);
   XrdSysXAttr *theObj;
   char *AtrLib   = LP[PIX(what)].lib;
   char *AtrParms = LP[PIX(what)].parms;

// Create a plugin object
//
  {XrdOucPinLoader myLib(Eroute, urVer, "xattrlib", AtrLib);
   ep = (XrdSysXAttr *(*)(XrdSysError *, const char *, const char *))
                         (myLib.Resolve("XrdSysGetXAttrObject"));
   if (!ep) return false;
   if (strcmp(AtrLib, myLib.Path()))
      {free(AtrLib); AtrLib = LP[PIX(what)].lib = strdup(myLib.Path());}
  }

// Get the Object now
//
   if (!(theObj = ep(Eroute, ConfigFN, AtrParms))) return false;

// Tell the interface to use this object instead of the default implementation
//
   XrdSysFAttr::SetPlugin(theObj);
   return true;
}

/******************************************************************************/
/* Private:                    S e t u p A u t h                              */
/******************************************************************************/

bool XrdOfsConfigPI::SetupAuth()
{
   extern XrdAccAuthorize *XrdAccDefaultAuthorizeObject
                          (XrdSysLogger   *lp,    const char   *cfn,
                           const char     *parm,  XrdVersionInfo &vInfo);

   XrdAccAuthorize *(*ep)(XrdSysLogger *, const char *, const char *);
   char *AuthLib   = LP[PIX(theAutLib)].lib;
   char *AuthParms = LP[PIX(theAutLib)].parms;

// Authorization comes from the library or we use the default
//
   if (!AuthLib) return 0 != (autPI = XrdAccDefaultAuthorizeObject
                             (Eroute->logger(), ConfigFN, AuthParms, *urVer));

// Create a plugin object
//
  {XrdOucPinLoader myLib(Eroute, urVer, "authlib", AuthLib);
   ep = (XrdAccAuthorize *(*)(XrdSysLogger *, const char *, const char *))
                             (myLib.Resolve("XrdAccAuthorizeObject"));
   if (!ep) return false;
   if (strcmp(AuthLib, myLib.Path()))
      {free(AuthLib); AuthLib = LP[PIX(theAutLib)].lib = strdup(myLib.Path());}
  }

// Get the Object now
//
   return 0 != (autPI = ep(Eroute->logger(), ConfigFN, AuthParms));
}

/******************************************************************************/
/* Private:                     S e t u p C m s                               */
/******************************************************************************/
  
bool XrdOfsConfigPI::SetupCms()
{
   char *CmsLib = LP[PIX(theCmsLib)].lib;

// Load the plugin if we have to
//
   if (LP[PIX(theCmsLib)].lib)
      {XrdOucPinLoader myLib(Eroute, urVer, "cmslib", CmsLib);
       cmsPI = (XrdCmsClient *(*)(XrdSysLogger *, int, int, XrdOss *))
                                  (myLib.Resolve("XrdCmsGetClient"));
       if (!cmsPI) return false;
       if (strcmp(CmsLib, myLib.Path()))
          {free(CmsLib);
           CmsLib = LP[PIX(theCmsLib)].lib = strdup(myLib.Path());
          }
      }
   return true;
}
