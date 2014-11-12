#ifndef __XRDOFSCONFIGPI_HH__
#define __XRDOFSCONFIGPI_HH__
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

#include "XrdCms/XrdCmsClient.hh"

class  XrdAccAuthorize;
class  XrdCks;
class  XrdCksConfig;
class  XrdOss;
class  XrdOucEnv;
class  XrdOucStream;
class  XrdSysError;
struct XrdVersionInfo;

//-----------------------------------------------------------------------------
//! The XrdOfsConfigPI is a helper class to handle ofs plugins.
//-----------------------------------------------------------------------------

class XrdOfsConfigPI
{
public:

//-----------------------------------------------------------------------------
//! The following pointers are set after a call to Load(). A nil pointer means
//! either the plugin was not requested to be loaded or the load failed.
//-----------------------------------------------------------------------------

XrdAccAuthorize *autPI;    //!< -> Authorization plugin
XrdCks          *cksPI;    //!< -> Checksum manager plugin
XrdCmsClient_t   cmsPI;    //!< -> Cms client object generator plugin
XrdOss          *ossPI;    //!< -> Oss plugin

//-----------------------------------------------------------------------------
//! The following enum is passed either alone or in combination to various
//! methods to indicate what plugin is being referenced.
//-----------------------------------------------------------------------------

enum TheLib {theAtrLib = 0x0100,  //!< Extended attribute plugin
             theAutLib = 0x0201,  //!< Authorization plugin
             theCksLib = 0x0402,  //!< Checksum manager plugin
             theCmsLib = 0x0803,  //!< Cms client plugin
             theOssLib = 0x1004,  //!< Oss plugin
             allXXXLib = 0x1705,  //!< All plugins (Load() only)
             maxXXXLib = 0x0005,  //!< Maximum different plugins
             libIXMask = 0x00ff
            };

//-----------------------------------------------------------------------------
//! Configure the cms client.
//!
//! @param   cmscP   Pointer to the cms client instance.
//! @param   envP    Pointer to the environment normally passed to the cms
//!                  client istance.
//!
//! @return          true upon success and false upon failure.
//-----------------------------------------------------------------------------

bool   Configure(XrdCmsClient *cmscP, XrdOucEnv *envP)
               {return 0 != cmscP->Configure(ConfigFN,
                                   LP[theCmsLib & libIXMask].parms, envP);
               }

//-----------------------------------------------------------------------------
//! Set the default plugin path and parms. This method may be called before or
//! after the configuration file is parsed.
//!
//! @param   what    The enum that specified which plugin is being set.
//! @param   lpath   The plugin library path
//! @param   lparm   The plugin parameters (0 if none)
//-----------------------------------------------------------------------------

void   Default(TheLib what, const char *lpath, const char *lparm=0);

//-----------------------------------------------------------------------------
//! Set the default checksum algorithm. This method must be called before
//! Load() is called.
//!
//! @param   alg     Pointer to the default algorithm name, it is duplicated.
//-----------------------------------------------------------------------------

void   DefaultCS(const char *alg);

//-----------------------------------------------------------------------------
//! Display configuration settings.
//-----------------------------------------------------------------------------

void   Display();

//-----------------------------------------------------------------------------
//! Load required plugins. This is a one time call!
//!
//! @param   what    A "or" combination of TheLib enums specifying which
//!                  plugins need to be loaded.
//! @param   envP    Pointer to the environment normally passed to the default
//!                  oss plugin at load time.
//!
//! @return          true upon success and false upon failure.
//-----------------------------------------------------------------------------

bool   Load(int what, XrdOucEnv *envP=0);

//-----------------------------------------------------------------------------
//! Check if the checksum plugin uses the oss plugin.
//!
//! @return  True if the plugin uses the oss plugin, false otherwise.
//-----------------------------------------------------------------------------

bool   OssCks() {return ossCksio;}

//-----------------------------------------------------------------------------
//! Parse a plugin directive.
//!
//! @param   what    The enum specifying which plugin directive to parse.
//! @return          true upon success and false upon failure.
//-----------------------------------------------------------------------------

bool   Parse(TheLib what);

//-----------------------------------------------------------------------------
//! Set the checksum read size
//!
//! @param   rdsz    The chesum read size buffer.
//-----------------------------------------------------------------------------

void   SetCksRdSz(int rdsz) {CksRdsz = rdsz;}

//-----------------------------------------------------------------------------
//! Constructor
//!
//! @param   cfn     Pointer to the configuration file name.
//! @param   cfgP    Pointer to the stream that reads the configuration file.
//! @param   errP    Pointer to the error message object that routes messages.
//! @param   verP    Pointer to the version information of the object creator.
//!                  If zero, the version information of this object is used.
//!                  Generally, if the creator resides in a different shared
//!                  library, the creator's version should be supplied.
//-----------------------------------------------------------------------------

       XrdOfsConfigPI(const char *cfn, XrdOucStream *cfgP, XrdSysError *errP,
                      XrdVersionInfo *verP=0);

//-----------------------------------------------------------------------------
//! Destructor
//-----------------------------------------------------------------------------

      ~XrdOfsConfigPI();

private:

bool          ParseAtrLib();
bool          ParseOssLib();
bool          RepLib(TheLib what, const char *newLib, const char *newParms=0);
bool          SetupAttr(TheLib what);
bool          SetupAuth();
bool          SetupCms();

XrdVersionInfo *urVer;
XrdOucStream *Config;
XrdSysError  *Eroute;
XrdCksConfig *CksConfig;
const char   *ConfigFN;

struct xxxLP
      {char  *lib;
       char  *parms;
       char  *opts;
              xxxLP() : lib(0), parms(0), opts(0) {}
             ~xxxLP() {if (lib)   free(lib);
                       if (parms) free(parms);
                       if (opts)  free(opts);
                      }
      }       LP[maxXXXLib];
char         *CksAlg;
int           CksRdsz;
bool          defLib[maxXXXLib];
bool          ossXAttr;
bool          ossCksio;
bool          Loaded;
bool          LoadOK;
};
#endif
