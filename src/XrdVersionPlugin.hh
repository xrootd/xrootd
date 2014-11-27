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
          char        vSfxLen;  //!< Generic rule suffix length for preceeding
          int         vProcess; //!< version: <0 skip, =0 optional, >0 required
          short       vMajLow;  //!< Lowest compatible major version number
          short       vMinLow;  //!< Lowest compatible minor (>99 don't check).
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
             <   0: minor version numbers must be identical.
             >=  0: the lowest valid minor version for the major number allowed.
             >  99: Do not check the minor version number, it's immaterial.

   piSymbol: The plugin's object creator's unquoted function name. When this
             symbol is looked-up, the defined version rule is applied.

   Note: a plugin may not have a major.minor version number greater than the
         program's major.minor version number unless either one is unreleased.
         Unreleased versions can use any version. However, a message is issued.
*/
#define XrdVERSIONPLUGINRULES \
        XrdVERSIONPLUGIN_Rule(Required,  4,  0, XrdAccAuthorizeObject         )\
        XrdVERSIONPLUGIN_Rule(Optional,  4,  0, XrdBwmPolicyObject            )\
        XrdVERSIONPLUGIN_Rule(Required,  4,  0, XrdCksCalcInit                )\
        XrdVERSIONPLUGIN_Rule(Required,  4,  0, XrdCksInit                    )\
        XrdVERSIONPLUGIN_Rule(Required,  4,  0, XrdCmsGetClient               )\
        XrdVERSIONPLUGIN_Rule(Required,  4,  0, XrdCryptosslFactoryObject     )\
        XrdVERSIONPLUGIN_Rule(Optional,  4,  0, XrdFileCacheGetDecision       )\
        XrdVERSIONPLUGIN_Rule(DoNotChk,  4,  0, XrdgetProtocol                )\
        XrdVERSIONPLUGIN_Rule(Required,  4,  0, XrdgetProtocolPort            )\
        XrdVERSIONPLUGIN_Rule(Optional,  4,  0, XrdHttpGetSecXtractor         )\
        XrdVERSIONPLUGIN_Rule(Required,  4,  0, XrdOssGetStorageSystem        )\
        XrdVERSIONPLUGIN_Rule(Required,  4,  0, XrdOssStatInfoInit            )\
        XrdVERSIONPLUGIN_Rule(Required,  4,  0, XrdOucGetCache                )\
        XrdVERSIONPLUGIN_Rule(Optional,  4,  0, XrdOucgetName2Name            )\
        XrdVERSIONPLUGIN_Rule(Required,  4,  0, XrdSecGetProtocol             )\
        XrdVERSIONPLUGIN_Rule(Required,  4,  0, XrdSecgetService              )\
        XrdVERSIONPLUGIN_Rule(Optional,  4,  0, XrdSecgsiAuthzFun             )\
        XrdVERSIONPLUGIN_Rule(DoNotChk,  4,  0, XrdSecgsiAuthzInit            )\
        XrdVERSIONPLUGIN_Rule(DoNotChk,  4,  0, XrdSecgsiAuthzKey             )\
        XrdVERSIONPLUGIN_Rule(Optional,  4,  0, XrdSecgsiGMAPFun              )\
        XrdVERSIONPLUGIN_Rule(Optional,  4,  0, XrdSecgsiVOMSFun              )\
        XrdVERSIONPLUGIN_Rule(DoNotChk,  4,  0, XrdSecgsiVOMSInit             )\
        XrdVERSIONPLUGIN_Rule(DoNotChk,  4,  0, XrdSecProtocolgsiInit         )\
        XrdVERSIONPLUGIN_Rule(Required,  4,  0, XrdSecProtocolgsiObject       )\
        XrdVERSIONPLUGIN_Rule(DoNotChk,  4,  0, XrdSecProtocolkrb5Init        )\
        XrdVERSIONPLUGIN_Rule(Required,  4,  0, XrdSecProtocolkrb5Object      )\
        XrdVERSIONPLUGIN_Rule(DoNotChk,  4,  0, XrdSecProtocolpwdInit         )\
        XrdVERSIONPLUGIN_Rule(Required,  4,  0, XrdSecProtocolpwdObject       )\
        XrdVERSIONPLUGIN_Rule(DoNotChk,  4,  0, XrdSecProtocolsssInit         )\
        XrdVERSIONPLUGIN_Rule(Required,  4,  0, XrdSecProtocolsssObject       )\
        XrdVERSIONPLUGIN_Rule(DoNotChk,  4,  0, XrdSecProtocolunixInit        )\
        XrdVERSIONPLUGIN_Rule(Required,  4,  0, XrdSecProtocolunixObject      )\
        XrdVERSIONPLUGIN_Rule(Required,  4,  0, XrdSfsGetFileSystem           )\
        XrdVERSIONPLUGIN_Rule(Required,  4,  0, XrdSfsGetFileSystem2          )\
        XrdVERSIONPLUGIN_Rule(Required,  4,  0, XrdSysGetXAttrObject          )\
        XrdVERSIONPLUGIN_Rule(Required,  4,  0, XrdClGetMonitor               )\
        XrdVERSIONPLUGIN_Rule(Required,  4,  0, XrdClGetPlugIn                )\
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
        {"libXrdBwm.so",             \
         "libXrdCksCalczcrc32.so",   \
         "libXrdCryptossl.so",       \
         "libXrdFileCache.so",       \
         "libXrdHttp.so",            \
         "libXrdOssSIgpfsT.so",      \
         "libXrdPss.so",             \
         "libXrdSec.so",             \
         "libXrdSecgsi.so",          \
         "libXrdSecgsiAUTHZVO.so",   \
         "libXrdSecgsiGMAPDLAP.so",  \
         "libXrdSecgsiGMAPLDAP.so",  \
         "libXrdSeckrb5.so",         \
         "libXrdSecpwd.so",          \
         "libXrdSecsss.so",          \
         "libXrdSecunix.so",         \
         "libXrdXrootd.so",          \
         0}
#endif
