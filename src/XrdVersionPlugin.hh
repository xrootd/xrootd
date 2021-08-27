#ifndef __XRDVERSIONPLUGIN_HH__
#define __XRDVERSIONPLUGIN_HH__
/******************************************************************************/
/*                                                                            */
/*                   X r d V e r s i o n P l u g i n . h h                    */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
  
/* The following section defines the versioning rules for plugins. The rules are
   applied by 'XrdSysPlugin.cc'. The rules defined by the XrdVERSIONPLUGIN_RULE
   macro (see below) are used to initialize a data the following data structure.
*/
   struct XrdVersionPlugin
         {const char *pName;    //!< -> plugin object creator function name
          char        vPfxLen;  //!< Generic rule prefix length
          char        vSfxLen;  //!< Generic rule suffix length for preceding
          int         vProcess; //!< version: <0 skip, =0 optional, >0 required
          short       vMajLow;  //!< Lowest compatible major version number
          short       vMinLow;  //!< Lowest compatible minor (< 0 don't check).
         };

/* The rules are defined here because they apply to every class that uses a
   plugin.  This file *must* be updated whenever a plugin interface materially
   changes; including any material changes (layout or size) to any classes
   passed as arguments to the plugin.
*/

// Macros used to define version checking rule values (see explanation below).
//
#define XrdVERSIONPLUGIN_DoNotChk -1
#define XrdVERSIONPLUGIN_Optional  0
#define XrdVERSIONPLUGIN_Required  1

#define XrdVERSIONPLUGIN_Rule(procMode, majorVer, minorVer, piSymbol)\
           {#piSymbol, 0, 0, XrdVERSIONPLUGIN_##procMode, majorVer, minorVer},

/* Each rule must be defined by the XrdVERSIONPLUGIN_Rule macro which takes four
   arguments, as follows:

   procMode: Version procsessing mode:
             DoNotChk -> Skip version check as it's already been done by a
                         previous getPlugin() call for a library symbol.
             Optional -> Version check is optional, do it if version information
                         present but warn if it is missing.
             Required -> Version check required; plugin must define a version
                         number and issue error message if it is missing.

   majorVer: The required major version number. It is checked as follows:
             <   0: major version numbers must be identical.
             >=  0: is the lowest valid major version number allowed.

   minorVer: The required minor version number, It is check as follows:
             <   0: Do not check the minor version number, it's immaterial.
             >=  0: the lowest valid minor version for the major number allowed.

   piSymbol: The plugin's object creator's unquoted function name. When this
             symbol is looked-up, the defined version rule is applied.

   Note: a plugin may not have a major.minor version number greater than the
         program's major.minor version number unless either one is unreleased.
         Unreleased versions can use any version. However, a message is issued.
*/
#define XrdVERSIONPLUGINRULES \
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, SecEntityPin                  )\
        XrdVERSIONPLUGIN_Rule(Required,  4,  8, TcpMonPin                     )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdAccAuthorizeObject         )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdAccAuthorizeObjAdd         )\
        XrdVERSIONPLUGIN_Rule(Optional,  5,  0, XrdBwmPolicyObject            )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdCksAdd2                    )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdCksCalcInit                )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdCksInit                    )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdCmsGetClient               )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdCmsgetVnId                 )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdCmsPerfMonitor             )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdCryptosslFactoryObject     )\
        XrdVERSIONPLUGIN_Rule(Optional,  5,  0, XrdPfcGetDecision       )\
        XrdVERSIONPLUGIN_Rule(DoNotChk,  5,  0, XrdgetProtocol                )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdgetProtocolPort            )\
        XrdVERSIONPLUGIN_Rule(Required,  4,  0, XrdHttpGetSecXtractor         )\
        XrdVERSIONPLUGIN_Rule(Required,  4,  8, XrdHttpGetExtHandler          )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdSysLogPInit                )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdOfsAddPrepare              )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdOfsFSctl                   )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdOfsgetPrepare              )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdOssGetStorageSystem        )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdOssAddStorageSystem2       )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdOssGetStorageSystem2       )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdOssStatInfoInit            )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdOssStatInfoInit2           )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdOucGetCache                )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdOucGetCache2               )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdOucCacheCMInit             )\
        XrdVERSIONPLUGIN_Rule(Optional,  5,  0, XrdOucgetName2Name            )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdSecGetProtocol             )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdSecgetService              )\
        XrdVERSIONPLUGIN_Rule(Optional,  5,  0, XrdSecgsiAuthzFun             )\
        XrdVERSIONPLUGIN_Rule(DoNotChk,  5,  0, XrdSecgsiAuthzInit            )\
        XrdVERSIONPLUGIN_Rule(DoNotChk,  5,  0, XrdSecgsiAuthzKey             )\
        XrdVERSIONPLUGIN_Rule(Optional,  5,  0, XrdSecgsiGMAPFun              )\
        XrdVERSIONPLUGIN_Rule(Optional,  5,  0, XrdSecgsiVOMSFun              )\
        XrdVERSIONPLUGIN_Rule(DoNotChk,  5,  0, XrdSecgsiVOMSInit             )\
        XrdVERSIONPLUGIN_Rule(DoNotChk,  5,  0, XrdSecProtocolgsiInit         )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdSecProtocolgsiObject       )\
        XrdVERSIONPLUGIN_Rule(DoNotChk,  5,  0, XrdSecProtocolkrb5Init        )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdSecProtocolkrb5Object      )\
        XrdVERSIONPLUGIN_Rule(DoNotChk,  5,  0, XrdSecProtocolpwdInit         )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdSecProtocolpwdObject       )\
        XrdVERSIONPLUGIN_Rule(DoNotChk,  5,  0, XrdSecProtocolsssInit         )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdSecProtocolsssObject       )\
        XrdVERSIONPLUGIN_Rule(DoNotChk,  5,  0, XrdSecProtocolunixInit        )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdSecProtocolunixObject      )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdSfsGetFileSystem           )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdSfsGetFileSystem2          )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdSysAddXAttrObject          )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdSysGetXAttrObject          )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdClGetMonitor               )\
        XrdVERSIONPLUGIN_Rule(Required,  5,  0, XrdClGetPlugIn                )\
                             { 0, 0, 0,  0,  0, 0}

#define XrdVERSIONPLUGIN_Maxim(procMode, majorVer, minorVer, piPfx, piSfx)\
           {#piPfx #piSfx, static_cast<char>(strlen(#piPfx)),\
                           static_cast<char>(strlen(#piSfx)),\
            XrdVERSIONPLUGIN_##procMode, majorVer, minorVer},

/* Each generic rule must be defined by the XrdVERSIONPLUGIN_Maxim macro which
   takes five arguments. The first three are exactly the same as defined for
   XrdVERSIONPLUGIN_Rule. The last two define a pefix/suffix match for the
   symbol being looked up, as follows:

   piPfx:    The leading  characters of the plugin's object creator's unquoted
             function name. When this symbol is looked-up, the defined version
             rule is applied if the suffix, if any, also matches.

   piSfx:    The trailing characters of the plugin's object creator's unquoted
             function name. When this symbol is looked-up, the defined version
             rule is applied if the prefix, if any, also matches.

   Note: An attempt is made to match the symbol using specific rules defined
         by XRDVERSIONPLUGIN_Rule before using any generic rules. If a match
         is found the same processing as for specific rules is applied.
*/
#define XrdVERSIONPLUGINMAXIMS\
        XrdVERSIONPLUGIN_Maxim(DoNotChk,  4,  0, XrdSecProtocol, Init         )\
        XrdVERSIONPLUGIN_Maxim(Required,  4,  0, XrdSecProtocol, Object       )\
        XrdVERSIONPLUGIN_Maxim(Optional,  4,  0, XrdCrypto,      FactoryObject)\
                             { 0, 0, 0,  0,  0, 0}

/* The following defines the list of plugins that are included in the base
   code and are to be strictly name versioned upon loading (i.e. fallback
   to an unversioned name is not allowed). This is enforced by XrdOucVerName.
*/
#define XrdVERSIONPLUGINSTRICT       \
        {"libXrdAccSciTokens.so",    \
         "libXrdBlacklistDecision.so", \
         "libXrdBwm.so",             \
         "libXrdCksCalczcrc32.so",   \
         "libXrdClProxyPlugin.so",   \
         "libXrdCmsRedirectLocal.so", \
         "libXrdCryptossl.so",       \
         "libXrdHttp.so",            \
         "libXrdHttpTPC.so",         \
         "libXrdMacaroons.so",       \
         "libXrdN2No2p.so",          \
         "libXrdOssSIgpfsT.so",      \
         "libXrdPfc.so",             \
         "libXrdPss.so",             \
         "libXrdSec.so",             \
         "libXrdSecgsi.so",          \
         "libXrdSecgsiAUTHZVO.so",   \
         "libXrdSecgsiGMAPDN.so",    \
         "libXrdSecgsiVOMS.so",      \
         "libXrdSeckrb5.so",         \
         "libXrdSecProt.so",         \
         "libXrdSecpwd.so",          \
         "libXrdSecsss.so",          \
         "libXrdSecunix.so",         \
         "libXrdSsi.so",             \
         "libXrdSsiLog.so",          \
         "libXrdThrottle.so",        \
         "libXrdVoms.so",            \
         "libXrdXrootd.so",          \
         0}

// The XrdVersionMapD2P maps a directive to the associated plugin creator.
// When two or more such creators exist, the newest one should be used.
//
   struct XrdVersionMapD2P
         {const char *dName;    //!< -> plugin directive name
          const char *pName;    //!< -> plugin object creator function name
         };

#define XrdVERSIONPLUGIN_Mapd(drctv, piSymbol)\
           {#drctv, #piSymbol},

#define XrdVERSIONPLUGINMAPD2P\
        XrdVERSIONPLUGIN_Mapd(ofs.authlib,      XrdAccAuthorizeObject         )\
        XrdVERSIONPLUGIN_Mapd(bwm.policy,       XrdBwmPolicyObject            )\
        XrdVERSIONPLUGIN_Mapd(ofs.ckslib,       XrdCksInit                    )\
        XrdVERSIONPLUGIN_Mapd(ofs.cmslib,       XrdCmsGetClient               )\
        XrdVERSIONPLUGIN_Mapd(cms.vnid,         XrdCmsgetVnId                 )\
        XrdVERSIONPLUGIN_Mapd(cms.perf,         XrdCmsPerfMonitor             )\
        XrdVERSIONPLUGIN_Mapd(pfc.decisionlib,  XrdPfcGetDecision       )\
        XrdVERSIONPLUGIN_Mapd(xrd.protocol,     XrdgetProtocol                )\
        XrdVERSIONPLUGIN_Mapd(http.secxtractor, XrdHttpGetSecXtractor         )\
        XrdVERSIONPLUGIN_Mapd(http.exthandler,  XrdHttpGetExtHandler          )\
        XrdVERSIONPLUGIN_Mapd(@logging,         XrdSysLogPInit                )\
        XrdVERSIONPLUGIN_Mapd(ofs.ctllib,       XrdOfsFSctl                   )\
        XrdVERSIONPLUGIN_Mapd(ofs.preplib,      XrdOfsgetPrepare              )\
        XrdVERSIONPLUGIN_Mapd(ofs.osslib,       XrdOssGetStorageSystem2       )\
        XrdVERSIONPLUGIN_Mapd(oss.statlib,      XrdOssStatInfoInit2           )\
        XrdVERSIONPLUGIN_Mapd(pss.cachelib,     XrdOucGetCache2               )\
        XrdVERSIONPLUGIN_Mapd(pss.ccmlib,       XrdOucCacheCMInit             )\
        XrdVERSIONPLUGIN_Mapd(oss.namelib,      XrdOucgetName2Name            )\
        XrdVERSIONPLUGIN_Mapd(sec.protocol,     XrdSecGetProtocol             )\
        XrdVERSIONPLUGIN_Mapd(xrootd.seclib,    XrdSecgetService              )\
        XrdVERSIONPLUGIN_Mapd(gsi-authzfun,     XrdSecgsiAuthzFun             )\
        XrdVERSIONPLUGIN_Mapd(gsi-gmapfun,      XrdSecgsiGMAPFun              )\
        XrdVERSIONPLUGIN_Mapd(gsi-vomsfun,      XrdSecgsiVOMSFun              )\
        XrdVERSIONPLUGIN_Mapd(sec.protocol-gsi, XrdSecProtocolgsiObject       )\
        XrdVERSIONPLUGIN_Mapd(sec.protocol-krb5,XrdSecProtocolkrb5Object      )\
        XrdVERSIONPLUGIN_Mapd(sec.protocol-pwd, XrdSecProtocolpwdObject       )\
        XrdVERSIONPLUGIN_Mapd(sec.protocol-sss, XrdSecProtocolsssObject       )\
        XrdVERSIONPLUGIN_Mapd(sec.protocol-unix,XrdSecProtocolunixObject      )\
        XrdVERSIONPLUGIN_Mapd(xrootd.fslib,     XrdSfsGetFileSystem2          )\
        XrdVERSIONPLUGIN_Mapd(ofs.xattrlib,     XrdSysGetXAttrObject          )\
        XrdVERSIONPLUGIN_Mapd(xrdcl.monitor,    XrdClGetMonitor               )\
        XrdVERSIONPLUGIN_Mapd(xrdcl.plugin,     XrdClGetPlugIn                )\
                             { 0, 0}
#endif
