/******************************************************************************/
/*                                                                            */
/*                     X r d O f s C o n f i g P I . c c                      */
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
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <sys/param.h>

#include "XrdVersion.hh"

#include "XrdAcc/XrdAccAuthorize.hh"

#include "XrdCks/XrdCks.hh"
#include "XrdCks/XrdCksConfig.hh"

#include "XrdCms/XrdCmsClient.hh"

#include "XrdOfs/XrdOfs.hh"
#include "XrdOfs/XrdOfsConfigPI.hh"
#include "XrdOfs/XrdOfsFSctl_PI.hh"
#include "XrdOfs/XrdOfsPrepare.hh"

#include "XrdOss/XrdOss.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucPinLoader.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucUtils.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFAttr.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPlugin.hh"

/******************************************************************************/
/*                     G l o b a l s   &   S t a t i c s                      */
/******************************************************************************/
  
namespace
{
const char *drctv[] = {"xattrlib", "authlib", "ckslib", "cmslib",
                         "ctllib", "osslib", "preplib"};

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
  
XrdOfsConfigPI::XrdOfsConfigPI(const char  *cfn,  XrdOucStream     *cfgP,
                               XrdSysError *errP, XrdSfsFileSystem *sfsP,
                               XrdVersionInfo *verP)
                 : autPI(0), cksPI(0), cmsPI(0), ctlPI(0), prpPI(0), ossPI(0),
                   sfsPI(sfsP), urVer(verP),
                   Config(cfgP),  Eroute(errP), CksConfig(0), ConfigFN(cfn),
                   CksAlg(0), CksRdsz(0), ossXAttr(false), ossCksio(0),
                   prpAuth(true), Loaded(false), LoadOK(false), cksLcl(false)
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

// Set Pushable attributes
//
   pushOK[PIX(theAtrLib)] = true;
   pushOK[PIX(theAutLib)] = true;
   pushOK[PIX(theCksLib)] = false;
   pushOK[PIX(theCmsLib)] = false;
   pushOK[PIX(theCtlLib)] = true;
   pushOK[PIX(theOssLib)] = true;
   pushOK[PIX(thePrpLib)] = true;
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
/* Private:                       A d d L i b                                 */
/******************************************************************************/

bool XrdOfsConfigPI::AddLib(XrdOfsConfigPI::TheLib what)
{
   struct xxxLP newALP;
   char *val, *path;
   char  parms[2048];
   int  i, xLib = PIX(what);

// Get the path
//
   if (!(val = Config->GetWord()) || !val[0])
      {Eroute->Emsg("Config", drctv[PIX(what)],"not specified"); return false;}
   path = strdup(val);

// Get the parameters
//
   if (!Config->GetRest(parms, sizeof(parms)))
      {Eroute->Emsg("Config", drctv[xLib], "parameters too long");
       free(path);
       return false;
      }

// Add this library
//
   i = ALP[xLib].size();
   ALP[xLib].push_back(newALP);
   ALP[xLib][i].lib   = path;
   if (*parms) ALP[xLib][i].parms = strdup(parms);
   return true;
}
  
/******************************************************************************/
/* Private:                    A d d L i b A t r                              */
/******************************************************************************/

bool   XrdOfsConfigPI::AddLibAtr(XrdOucEnv *envP, XrdSysXAttr *&atrPI)
{
   const char *epName = "XrdSysAddXAttrObject";
   XrdSysAddXAttrObject_t ep;
   int n = ALP[PIX(theAtrLib)].size();

   for (int i = 0; i < n; i++)
       {const char *path  = ALP[PIX(theAtrLib)][i].lib;
        const char *parms = ALP[PIX(theAtrLib)][i].parms;
        XrdOucPinLoader myLib(Eroute, urVer, "xattrlib", path);
        ep = (XrdSysAddXAttrObject_t)(myLib.Resolve(epName));
        if (!ep) return false;
        atrPI = ep(Eroute, ConfigFN, parms, envP, atrPI);
        if (!atrPI) return false;
       }
   return true;
}

/******************************************************************************/
/* Private:                    A d d L i b A u t                              */
/******************************************************************************/

bool   XrdOfsConfigPI::AddLibAut(XrdOucEnv *envP)
{
   const char *epName = "XrdAccAuthorizeObjAdd";
   int n = ALP[PIX(theAutLib)].size();

   for (int i = 0; i < n; i++)
       {const char *path  = ALP[PIX(theAutLib)][i].lib;
        const char *parms = ALP[PIX(theAutLib)][i].parms;
        XrdAccAuthorizeObjAdd_t addAut;
        XrdOucPinLoader myLib(Eroute, urVer, "authlib", path);
        addAut = (XrdAccAuthorizeObjAdd_t)myLib.Resolve(epName);
        if (!addAut) return false;
        autPI = addAut(Eroute->logger(), ConfigFN, parms, envP, autPI);
        if (!autPI) return false;
       }
   return true;
}
  

/******************************************************************************/
/* Private:                    A d d L i b C t l                              */
/******************************************************************************/

bool   XrdOfsConfigPI::AddLibCtl(XrdOucEnv *envP)
{
   const char *objName = "XrdOfsFSctl";
   XrdOfsFSctl_PI *ctlObj;
   int n = ALP[PIX(theCtlLib)].size();

   for (int i = 0; i < n; i++)
       {const char *path  = ALP[PIX(theCtlLib)][i].lib;
        const char *parms = ALP[PIX(theCtlLib)][i].parms;
        XrdOucPinLoader myLib(Eroute, urVer, "ctllib", path);
        ctlObj = (XrdOfsFSctl_PI *)myLib.Resolve(objName);
        if (!ctlObj) return false;
        ctlObj->eDest = Eroute;
        ctlObj->prvPI = ctlPI;
        ctlPI = ctlObj;
        ctlLP theCTL = {ctlObj, parms};
        ctlVec.push_back(theCTL);
       }
   return true;
}
  
/******************************************************************************/
/* Private:                    A d d L i b O s s                              */
/******************************************************************************/

bool   XrdOfsConfigPI::AddLibOss(XrdOucEnv *envP)
{
   const char *epName = "XrdOssAddStorageSystem2";
   int n = ALP[PIX(theOssLib)].size();

   for (int i = 0; i < n; i++)
       {const char *path  = ALP[PIX(theOssLib)][i].lib;
        const char *parms = ALP[PIX(theOssLib)][i].parms;
        XrdOssAddStorageSystem2_t addOss2;
        XrdOucPinLoader myLib(Eroute, urVer, "osslib", path);

        addOss2 = (XrdOssGetStorageSystem2_t)myLib.Resolve(epName);
        if (!addOss2) return false;
        ossPI = addOss2(ossPI, Eroute->logger(), ConfigFN, parms, envP);
        if (!ossPI) return false;
       }
   return true;
}

/******************************************************************************/
/* Private:                    A d d L i b P r p                              */
/******************************************************************************/

bool   XrdOfsConfigPI::AddLibPrp(XrdOucEnv *envP)
{
   const char *epName = "XrdOfsAddPrepare";
   int n = ALP[PIX(thePrpLib)].size();

   for (int i = 0; i < n; i++)
       {const char *path  = ALP[PIX(thePrpLib)][i].lib;
        const char *parms = ALP[PIX(thePrpLib)][i].parms;
        XrdOfsAddPrepare_t addPrp;
        XrdOucPinLoader myLib(Eroute, urVer, "preplib", path);
        addPrp = (XrdOfsAddPrepare_t)myLib.Resolve(epName);
        if (!addPrp) return false;
        prpPI = addPrp(Eroute, ConfigFN, parms, sfsPI, ossPI, envP, prpPI);
        if (!prpPI) return false;
       }
   return true;
}
  
/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/

bool   XrdOfsConfigPI::Configure(XrdCmsClient *cmscP, XrdOucEnv *envP)
{
   return 0 != cmscP->Configure(ConfigFN, LP[PIX(theCmsLib)].parms, envP);
}
  
/******************************************************************************/
/*                             C o n f i g C t l                              */
/******************************************************************************/

bool   XrdOfsConfigPI::ConfigCtl(XrdCmsClient *cmsP, XrdOucEnv *envP)
{
   struct XrdOfsFSctl_PI::Plugins thePI = {autPI, cmsP, ossPI, sfsPI};

// If there is no fsctl plugin, we are done.
//
   if (!ctlPI) return true;

// Initialize all of the plugin in FIFO order.
//
   if (!ctlPI->Configure(ConfigFN, LP[PIX(theCtlLib)].parms, envP, thePI))
      return false;

   int n = ctlVec.size();

   for (int i = 0; i < n; i++)
       {if (!ctlVec[i].ctlPI->Configure(ConfigFN,ctlVec[i].parms,envP,thePI))
           return false;
       }
   return true;
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
   XrdOucUtils::toLower(CksAlg);
}
  
/******************************************************************************/
/*                               D i s p l a y                                */
/******************************************************************************/

void   XrdOfsConfigPI::Display()
{
   xxxLP *lP;
   char *oP, buff[4096];
   const char *theLib;
   int n, aI = PIX(theAtrLib), oI = PIX(theOssLib);

// Display what we have
//
   for (int i = 0; i < maxXXXLib; i++)
       {oP = LP[i].opts;
             if (i != aI)   lP = &LP[i];
        else if (ossXAttr) {lP = &LP[oI]; oP = 0;}
        else                lP = &LP[i];
        n = ALP[i].size();
        if (n || lP->lib)
           {theLib = (lP->lib ? lP->lib : "default");
            snprintf(buff, sizeof(buff), "ofs.%s %s%s %s", drctv[i],
                     (oP ? oP : ""), theLib,  (lP->parms ? lP->parms : ""));
            Eroute->Say("       ", buff);
            for (int k = 0; k < n; k++)
                {lP = &(ALP[i][k]);
                 snprintf(buff, sizeof(buff), "ofs.%s ++ %s %s", drctv[i],
                         lP->lib, (lP->parms ? lP->parms : ""));
                 Eroute->Say("       ", buff);
                }
           }

       }
}
  
/******************************************************************************/
/*                                  L o a d                                   */
/******************************************************************************/
  
bool XrdOfsConfigPI::Load(int loadLib, XrdOucEnv *envP)
{
   extern XrdSysXAttr *XrdSysXAttrActive;
   extern XrdOss *XrdOssGetSS(XrdSysLogger *, const char *, const char *,
                              const char   *, XrdOucEnv  *, XrdVersionInfo &);
   bool aOK;

// Check if load was already called as we can only try once
//
   if (Loaded) return LoadOK;
   Loaded = true;

// Load the osslib first if so wanted. Note that the default osslib always
// wants osscksio unless it was stealthly overriden.
//
   if (DO_LOAD(theOssLib))
      {const char *ossLib = LP[PIX(theOssLib)].lib;
       if (!ossLib && !ossCksio) ossCksio = 1;
       if (!(ossPI = XrdOssGetSS(Eroute->logger(), ConfigFN, ossLib,
                        LP[PIX(theOssLib)].parms, envP, *urVer))) return false;
       if (ossLib && envP && (ossLib = envP->Get("oss.lib")))
          {free(LP[PIX(theOssLib)].lib);
           LP[PIX(theOssLib)].lib = strdup(ossLib);
          }
       if (!AddLibOss(envP)) return false;
      }

// Now setup the extended attribute plugin if so desired
//
   if (DO_LOAD(theAtrLib))
      {     if (ossXAttr && LP[PIX(theOssLib)].lib)
               aOK = SetupAttr(theOssLib, envP);
       else if             (LP[PIX(theAtrLib)].lib)
               aOK = SetupAttr(theAtrLib, envP);
       else {XrdSysXAttr *theObj = XrdSysXAttrActive;
             if (!AddLibAtr(envP, theObj)) aOK = false;
                else {if (theObj != XrdSysXAttrActive)
                          XrdSysFAttr::SetPlugin(theObj, true);
                      aOK = true;
                     }
           }
       if (!aOK) return false;
      }
   XrdSysFAttr::Xat->SetMsgRoute(Eroute);

// Setup authorization if we need to
//
   if (DO_LOAD(theAutLib) && !SetupAuth(envP)) return false;

// Setup checksumming if we need to
//
   if (DO_LOAD(theCksLib))
      {if (!CksConfig)
          {Eroute->Emsg("Config", "Unable to load checksum manager; "
                                  "incompatible versions.");
           return false;
          }
       cksPI = CksConfig->Configure(CksAlg, CksRdsz,
                                    (ossCksio > 0 ? ossPI : 0), envP);
       if (!cksPI) return false;
      }

// Setup the cms if we need to
//
   if (DO_LOAD(theCmsLib) && !SetupCms()) return false;

// Setup the fsctl plugin if need be
//
   if (DO_LOAD(theCtlLib) && !SetupCtl(envP)) return false;

// Setup the prepare plugin if need be
//
   if (DO_LOAD(thePrpLib) && !SetupPrp(envP)) return false;

// All done
//
   LoadOK = true;
   return true;
}
  
/******************************************************************************/
/*                                   N e w                                    */
/******************************************************************************/
  
XrdOfsConfigPI *XrdOfsConfigPI::New(const char  *cfn,  XrdOucStream   *cfgP,
                                    XrdSysError *errP, XrdVersionInfo *verP,
                                    XrdSfsFileSystem *sfsP)
{
// Handle caller's version if so indicated
//
   if (verP && !XrdSysPlugin::VerCmp(*verP, XrdVERSIONINFOVAR(XrdOfs)))
      return 0;

// Return an actual instance
//
   return new XrdOfsConfigPI(cfn, cfgP, errP, sfsP, verP);
}

/******************************************************************************/
/*                                O s s C k s                                 */
/******************************************************************************/

bool   XrdOfsConfigPI::OssCks() {return ossCksio > 0;}
  
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
                             {int libType;
                              if (CksConfig->ParseLib(*Config, libType))
                                 return false;
                              if (libType) cksLcl = libType == 1;
                              RepLib(theCksLib, CksConfig->ManLib(), nullParms, false);
                              return true;
                             }
                          Eroute->Emsg("Config", "Checksum version error!");
                          return false;
                          break;
          case theCmsLib: break;
                          break;
          case theCtlLib: break;
                          break;
          case theOssLib: return ParseOssLib();
                          break;
          case thePrpLib: return ParsePrpLib();
                          break;
          default:        Eroute->Emsg("Config", "Invalid plugin Parse() call");
                          return false;
                          break;
         }

// Get the path
//
   if (!(val = Config->GetWord()) || !val[0])
      {Eroute->Emsg("Config", drctv[PIX(what)],"not specified"); return false;}

// If this may be a pushable library, then see if the pushable tag is present
//
   if (!strcmp("++", val))
      {if (pushOK[PIX(what)]) return AddLib(what);
       Eroute->Emsg("Config", "'++' option not supported for",
                              drctv[PIX(what)], "directive.");
       return false;
      }

// Set the lib and parameters
//
   return RepLib(what, val);
}

/******************************************************************************/
/* Private:                  P a r s e A t r L i b                            */
/******************************************************************************/
  
/* Function: ParseAtrLib

   Purpose:  To parse the directive: xattrlib {osslib | [++] <path>} [<parms>]

             ++        stack this plugin.
             osslib    The plugin resides in the osslib plugin.
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

// Check for a push wrapper
//
   if (!strcmp("++", val)) return AddLib(theAtrLib);

// Record the path and parms
//
   ossXAttr = !strcmp("osslib", val);
   return RepLib(theAtrLib, (ossXAttr ? 0 : val));
}

/******************************************************************************/
/* Private:                  P a r s e O s s L i b                            */
/******************************************************************************/
  
/* Function: ParseOssLib

   Purpose:  To parse the directive: osslib [++ | <opts>] <path> [<parms>]
             <opts>: [+cksio] [+mmapio] [+xattr] [<opts>]

             +cksio    use the oss plugin for checkum I/O. This is now the
                       default for the native oss plugin unless +mmapio set.
             +mmapio   use memory mapping i/o (previously the default). This
                       is not documented but here just in case.
             +xattr    the library contains the xattr plugin.
             <path>    the path of the oss library to be used.
             <parms>   optional parms to be passed

  Output: true upon success or false upon failure.
*/

bool XrdOfsConfigPI::ParseOssLib()
{
   char *val, oBuff[80];
   int   oI = PIX(theOssLib);

// Check if we are pushing another library here
//
   if ((val = Config->GetWord()) && !strcmp("++",val)) return AddLib(theOssLib);

// Reset to defaults
//
    ossCksio = 0;
    ossXAttr = false;
    if (LP[oI].opts) {free(LP[oI].opts); LP[oI].opts = 0;}
    *oBuff = 0;

// Get the path and parms, and process keywords
//
   while(val)
        {     if (!strcmp("+cksio",  val))
                 {if (!ossCksio) strcat(oBuff, "+cksio  "); ossCksio =  1;}
         else if (!strcmp("+mmapio", val))
                 {if ( ossCksio) strcat(oBuff, "+mmapio "); ossCksio = -1;}
         else if (!strcmp("+xattr",  val))
                 {if (!ossXAttr) strcat(oBuff, "+xattr ");  ossXAttr = true;}
         else break;
         val = Config->GetWord();
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
/* Private:                  P a r s e P r p L i b                            */
/******************************************************************************/
  
/* Function: ParsePrpLib

   Purpose:  To parse the directive: preplib [++ | [<opts>]} <path> [<parms>]
             <opts>: [+noauth]

             ++        Stack this plugin.
             +noauth   do not apply authorization to path list.
             <path>    the path of the prepare library to be used.
             <parms>   optional parms to be passed

  Output: true upon success or false upon failure.
*/

bool XrdOfsConfigPI::ParsePrpLib()
{
   char *val, oBuff[80];
   int   oI = PIX(thePrpLib);

// Check if we are pushing another library here
//
   if ((val = Config->GetWord()) && !strcmp("++",val)) return AddLib(thePrpLib);

// Reset to defaults
//
    prpAuth = true;
    if (LP[oI].opts) {free(LP[oI].opts); LP[oI].opts = 0;}
    *oBuff = 0;

// Get the path and parms, and process keywords
//
   while(val)
        {     if (!strcmp("+noauth",  val))
                 {if (prpAuth) strcat(oBuff, "+noauth "); prpAuth = false;}
         else break;
         val = Config->GetWord();
        }

// Check if we a library path
//
   if (!val || !val[0])
      {Eroute->Emsg("Config", "preplib not specified"); return false;}

// Record the path and parameters
//
   if (*oBuff) LP[oI].opts = strdup(oBuff);
   return RepLib(thePrpLib, val);
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

bool   XrdOfsConfigPI::Plugin(XrdOfsFSctl_PI  *&piP)
{      return (piP = ctlPI) != 0;}

bool   XrdOfsConfigPI::Plugin(XrdOfsPrepare   *&piP)
{      return (piP = prpPI) != 0;}

bool   XrdOfsConfigPI::Plugin(XrdOss          *&piP)
{      return (piP = ossPI) != 0;}

/******************************************************************************/
/*                              P r e p A u t h                               */
/******************************************************************************/

bool   XrdOfsConfigPI::PrepAuth() {return prpAuth;}
  
/******************************************************************************/
/*                                  P u s h                                   */
/******************************************************************************/

bool XrdOfsConfigPI::Push(TheLib what, const char *plugP, const char *parmP)
{
   struct xxxLP newALP;
   int  i, xLib = PIX(what);

// Make sure this library is pushable
//
   if (!pushOK[xLib]) return false;

// Add this library
//
   i = ALP[xLib].size();
   ALP[xLib].push_back(newALP);
   ALP[xLib][i].lib = strdup(plugP);
   if (parmP && *parmP) ALP[xLib][i].parms = strdup(parmP);
   return true;
}
  
/******************************************************************************/
/* Private:                       R e p L i b                                 */
/******************************************************************************/
  
bool XrdOfsConfigPI::RepLib(XrdOfsConfigPI::TheLib what,
                            const char *newLib, const char *newParms, bool parseParms)
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
            if (parseParms && !Config->GetRest(parms, sizeof(parms)))
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

bool XrdOfsConfigPI::SetupAttr(XrdOfsConfigPI::TheLib what, XrdOucEnv *envP)
{
   XrdSysGetXAttrObject_t ep;
   XrdSysXAttr *theObj;
   char *AtrLib   = LP[PIX(what)].lib;
   char *AtrParms = LP[PIX(what)].parms;

// Create a plugin object
//
  {XrdOucPinLoader myLib(Eroute, urVer, "xattrlib", AtrLib);
   ep = (XrdSysGetXAttrObject_t)(myLib.Resolve("XrdSysGetXAttrObject"));
   if (!ep) return false;
   if (strcmp(AtrLib, myLib.Path()))
      {free(AtrLib); AtrLib = LP[PIX(what)].lib = strdup(myLib.Path());}
  }

// Get the Object now
//
   if (!(theObj = ep(Eroute, ConfigFN, AtrParms))) return false;

// Push any additional objects
//
   if (!AddLibAtr(envP, theObj)) return false;

// Tell the interface to use this object instead of the default implementation
//
   XrdSysFAttr::SetPlugin(theObj);
   return true;
}

/******************************************************************************/
/* Private:                    S e t u p A u t h                              */
/******************************************************************************/

bool XrdOfsConfigPI::SetupAuth(XrdOucEnv *envP)
{
   extern XrdAccAuthorize *XrdAccDefaultAuthorizeObject
                          (XrdSysLogger   *lp,    const char   *cfn,
                           const char     *parm,  XrdVersionInfo &vInfo);

   XrdAccAuthorizeObject_t  ep1;
   XrdAccAuthorizeObject2_t ep2;
   char *AuthLib   = LP[PIX(theAutLib)].lib;
   char *AuthParms = LP[PIX(theAutLib)].parms;

// Authorization comes from the library or we use the default
//
   if (!AuthLib)
      {if (!(autPI = XrdAccDefaultAuthorizeObject
                        (Eroute->logger(), ConfigFN, AuthParms, *urVer)))
          return false;
       return AddLibAut(envP);
      }

// Create a plugin object. It will be version 2 or version 1, in that order
//
  {XrdOucPinLoader myLib(Eroute, urVer, "authlib", AuthLib);
   ep2 = (XrdAccAuthorizeObject2_t)(myLib.Resolve("XrdAccAuthorizeObject2"));
   if (!ep2)
      {ep1 = (XrdAccAuthorizeObject_t)(myLib.Resolve("XrdAccAuthorizeObject"));
       if (!ep1) return false;
       if (!(autPI = ep1(Eroute->logger(), ConfigFN, AuthParms))) return false;
      } else {
       if (!(autPI = ep2(Eroute->logger(), ConfigFN, AuthParms, envP)))
          return false;
      }
   if (strcmp(AuthLib, myLib.Path()))
      {free(AuthLib); AuthLib = LP[PIX(theAutLib)].lib = strdup(myLib.Path());}
  }

// Process additional wrapper objects now
//
   return AddLibAut(envP);
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

/******************************************************************************/
/*                              S e t u p C t l                               */
/******************************************************************************/
  
bool XrdOfsConfigPI::SetupCtl(XrdOucEnv *envP)
{
   XrdOfsFSctl_PI *obj = 0;
   char *CtlLib   = LP[PIX(theCtlLib)].lib;

// Load the plugin if we have to
//
   if (LP[PIX(theCtlLib)].lib)
      {XrdOucPinLoader myLib(Eroute, urVer, "ctllib", CtlLib);
       obj = (XrdOfsFSctl_PI *)(myLib.Resolve("XrdOfsFSctl"));
       if (!obj) return false;
       if (strcmp(CtlLib, myLib.Path()))
          {free(CtlLib);
           CtlLib = LP[PIX(theCtlLib)].lib = strdup(myLib.Path());
          }
      } else return true;

// Record the object (it will be fully initialized later_
//
   obj->eDest = Eroute;
   obj->prvPI = 0;
   ctlPI = obj;
   return AddLibCtl(envP);
}

/******************************************************************************/
/*                              S e t u p P r p                               */
/******************************************************************************/
  
bool XrdOfsConfigPI::SetupPrp(XrdOucEnv *envP)
{
   XrdOfsgetPrepare_t ep = 0;
   char *PrpLib   = LP[PIX(thePrpLib)].lib;
   char *PrpParms = LP[PIX(thePrpLib)].parms;

// Load the plugin if we have to
//
   if (LP[PIX(thePrpLib)].lib)
      {XrdOucPinLoader myLib(Eroute, urVer, "preplib", PrpLib);
       ep = (XrdOfsgetPrepare_t)(myLib.Resolve("XrdOfsgetPrepare"));
       if (!ep) return false;
       if (strcmp(PrpLib, myLib.Path()))
          {free(PrpLib);
           PrpLib = LP[PIX(thePrpLib)].lib = strdup(myLib.Path());
          }
      } else return true;

// Get the Object now
//
   if (!(prpPI = ep(Eroute, ConfigFN, PrpParms, sfsPI, ossPI, envP)))
      return false;
   return AddLibPrp(envP);
}
