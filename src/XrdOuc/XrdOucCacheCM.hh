#ifndef __XRDPUCCACHECM_HH__
#define __XRDPUCCACHECM_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d O u c C a c h e C M . h h                       */
/*                                                                            */
/* (c) 2018 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

/* The class defined here is used to implement a cache context management
   plugin. It is loaded as run time can interact with the cache, as needed.
   The pss.ccmlib directive is used to load the plugin. However, it is only
   loaded if a cache has been enabled for use.

   Your plug-in must exist in a shared library and have the following extern C
   function defined:
*/

//------------------------------------------------------------------------------
//! Initialize a cache context management plugin.
//!
//! @param  Cache         Reference to the object that interacts with the cache.
//! @param  Logger     -> The message routing object to be used in conjunction
//!                       with an XrdSysError object for error messages. When
//!                       nil, you should use cerr.
//! @param  Config     -> The name of the config file. When nil there was no
//!                       configuration file.
//! @param  Parms      -> Any parameters specified after the path on the
//!                       pss.ccmlib directive. If there are no parameters, the
//!                       pointer may be zero.
//! @param  envP       -> Environmental information.; nil if none.
//!
//! @return True          Upon success.
//!         False         Upon failure.
//!
//! The function must be declared as an extern "C" function in a loadable
//! shared library as follows:
//------------------------------------------------------------------------------

class XrdOucEnv;
class XrdPosixCache;
class XrdSysLogger;

typedef bool (*XrdOucCacheCMInit_t)(XrdPosixCache &Cache,
                                    XrdSysLogger  *Logger,
                                    const char    *Config,
                                    const char    *Parms,
                                    XrdOucEnv     *envP);
/*!
   extern "C"
   {
   bool XrdOucCacheCMInit(XrdPosixCache &Cache,   // Cache interface
                          XrdSysLogger  *Logger,  // Where messages go
                          const char    *Config,  // Config file used
                          const char    *Parms,   // Optional parm string
                          XrdOucEnv     *envP);   // Environmental information
   }
*/

//------------------------------------------------------------------------------
//! Declare compilation version.
//!
//! Additionally, you *should* declare the xrootd version you used to compile
//! your plug-in. While not currently required, it is highly recommended to
//! avoid execution issues should the class definition change. Declare it as:
//------------------------------------------------------------------------------

/*! #include "XrdVersion.hh"
    XrdVERSIONINFO(XrdOucCacheCMInit,<name>);

    where <name> is a 1- to 15-character unquoted name identifying your plugin.
*/
#endif
