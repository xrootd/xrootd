#ifndef __XRDSYSPLUGIN__
#define __XRDSYSPLUGIN__
/******************************************************************************/
/*                                                                            */
/*                       X r d S y s P l u g i n . h h                        */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
  
#include <string.h>

struct XrdVersionInfo;

class XrdSysError;

//------------------------------------------------------------------------------
//! Handy class to load run-time plugins and optionally check if the version
//! is compatible with the caller's version number. Version numbers are defined
//! as "aaa.bb.cc" where aaa is a decimnal major version, bb is a decimal minor
//! version, and cc is the decimal patch version number. Only the major and,
//! optionally, minor version numbers are checked. The checking rules are
//! defined in XrdVersion.hh and are rather liberal in nature. In order to
//! check versions, the plugin versioning rule must be defined in XrdVersion.hh
//! and constructor #2 or #3 must be used. The symbolic name of the plugin's
//! version information is the plugin symbol being looked up appended with
//! "Version" and must be defined as an XrdVersionInfo structure.
//------------------------------------------------------------------------------

class XrdSysPlugin
{
public:

//------------------------------------------------------------------------------
//! Get the address of a plugin from a shared library, opening the plug-in
//! shared library if not already open. Symbols in the library are local.
//!
//! @param  pname    the plug-in extern "C" symbolic name
//! @param  optional when  0 then issue error message when symbol isn't found.
//!                  Otherwise, the mising symbol is treated as an error.
//!
//! @return Success: the address of the symbol in the shared library/executable.
//!                  The address becomes invalid when this object is deleted
//!                  unless Persist() is called prior to deletion.
//!         Failure: Null
//------------------------------------------------------------------------------

void *getPlugin(const char *pname, int optional=0);

//------------------------------------------------------------------------------
//! Get the address of a plugin from a shared library, opening the plug-in
//! shared library if not already open and optionally make the symbols global.
//!
//! @param  pname    the plug-in extern "C" symbolic name
//! @param  optional when  0 then issue error message when symbol isn't found.
//!                  Otherwise, the mising symbol is treated as an error. When
//!                  optional is greater than 1, the load message is suppressed.
//! @param  global   when !0 then the symbols defined in the plug-in shared
//!                  library are made available for symbol resolution of
//!                  subsequently loaded libraries.
//! @return Success: the address of the symbol in the shared library/executable.
//!                  The address becomes invalid when this object is deleted
//!                  unless Persist() is called prior to deletion.
//!         Failure: Null
//------------------------------------------------------------------------------

void *getPlugin(const char *pname, int optional, bool global);

//------------------------------------------------------------------------------
//! Make library persistent even when the plugin object is deleted. Note that
//! if getPlugin() is called afterwards, the library will be re-opened!
//!
//! @return pointer to the opened shared library.
//------------------------------------------------------------------------------

void *Persist() {void *lHan = libHandle; libHandle = 0; return lHan;}

//------------------------------------------------------------------------------
//! Preload a shared library. This method is meant for those threading models
//! that require libraries to be opened in the main thread (e.g. MacOS). This
//! method is meant to be called before therads start and is not thread-safe.
//!
//! @param  path     -> to the library path, typically this should just be the
//!                     library filename so that LD_LIBRARY_PATH is used to
//!                     discover the directory path. This allows getPlugin()
//!                     to properly match preloaded libraries.
//! @param  ebuff    -> buffer where eror message is to be placed. The mesage
//!                     will always end with a null byte. If no error buffer
//!                     is supplied, any error messages are discarded.
//! @param  eblen    -> length of the supplied buffer, eBuff.
//!
//! @return True      The library was preloaded.
//!         False     The library could not be preloaded, ebuff, if supplied,
//!                   contains the error message text.
//------------------------------------------------------------------------------

static
bool  Preload(const char *path,  char *ebuff=0, int eblen=0);

//------------------------------------------------------------------------------
//! Compare two versions for compatability, optionally printing a warning.
//!
//! @param  vInf1 -> Version information for source.
//! @param  vInf2 -> Version information for target.
//! @param  noMsg -> If true, no error messages are written to stderr.
//!
//! @return True if versions are compatible (i.e. major and minor versions are
//!         identical as required for locally linked code); false otherwise.
//------------------------------------------------------------------------------

static
bool  VerCmp(XrdVersionInfo &vInf1, XrdVersionInfo &vInf2, bool noMsg=false);

//------------------------------------------------------------------------------
//! Constructor #1 (version number checking is not to be performed)
//!
//! @param  erp      -> error message object to display error messages.
//! @param  path     -> path to the shared library containing a plug-in. If NULL
//!                     the the executable image is searched for the plug-in.
//!                     Storage must persist while this object is alive.
//------------------------------------------------------------------------------

      XrdSysPlugin(XrdSysError *erp, const char *path)
                  : eDest(erp), libName(0), libPath(path ? strdup(path) : 0),
                    libHandle(0), myInfo(0), eBuff(0), eBLen(0), msgCnt(-1) {}

//------------------------------------------------------------------------------
//! Constructor #2 (version number checking may be performed)
//!
//! @param  erp      -> error message object to display error messages.
//! @param  path     -> path to the shared library containing a plug-in. If NULL
//!                     the the executable image is searched for the plug-in.
//!                     Storage must persist while this object is alive.
//! @param  lname    -> logical name of the plugin library (e.g. osslib) to be
//!                     used in any error messages.
//!                     Storage must persist while this object is alive.
//! @param  vinf     -> permanent version information of the plug-in loader.
//!                     If zero, then no version checking is performed.
//! @param  msgNum   -> Number of times getPlugin() is to produce a version
//!                     message for a loaded plugin. The default is always.
//------------------------------------------------------------------------------

      XrdSysPlugin(XrdSysError *erp, const char *path, const char *lname,
                   XrdVersionInfo *vinf=0, int msgNum=-1)
                  : eDest(erp), libName(lname),
                    libPath(path ? strdup(path) : 0), libHandle(0),
                    myInfo(vinf), eBuff(0), eBLen(0), msgCnt(msgNum) {}

//------------------------------------------------------------------------------
//! Constructor #3 (version number checking may be performed and any error
//!                 is returned in a supplied buffer)
//!
//! @param  ebuff    -> buffer where eror message is to be placed. The mesage
//!                     will always end with a null byte.
//! @param  eblen    -> length of the supplied buffer, eBuff.
//! @param  path     -> path to the shared library containing a plug-in. If NULL
//!                     the the executable image is searched for the plug-in.
//!                     Storage must persist while this object is alive.
//! @param  lname    -> logical name of the plugin library (e.g. osslib) to be
//!                     used in any error messages.
//!                     Storage must persist while this object is alive.
//! @param  vinf     -> permanent version information of the plug-in loader.
//!                     If Zero, then no version checking is performed.
//! @param  msgNum   -> Number of times getPlugin() is to produce a version
//!                     message for a loaded plugin. The default is always.
//------------------------------------------------------------------------------

      XrdSysPlugin(char *ebuff, int eblen, const char *path, const char *lname,
                   XrdVersionInfo *vinf=0, int msgNum=-1)
                  : eDest(0), libName(lname),
                    libPath(path ? strdup(path) : 0), libHandle(0),
                    myInfo(vinf), eBuff(ebuff), eBLen(eblen), msgCnt(msgNum) {}

//------------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------------

     ~XrdSysPlugin();

private:
enum            cvResult {cvBad = 0, cvNone, cvMissing, cvClean, cvDirty};

cvResult        badVersion(XrdVersionInfo &urInfo,char mmv,int majv,int minv);
cvResult        chkVersion(XrdVersionInfo &urInfo, const char *pname, void *lh);
static int      DLflags();
static void    *Find(const char *libname);
void            Inform(const char *txt1,   const char *txt2=0, const char *txt3=0,
                       const char *txt4=0, const char *txt5=0, int noHush=0);
cvResult        libMsg(const char *txt1, const char *txt2, const char *mSym=0);
const char     *msgSuffix(const char *Word, char *buff, int bsz);

XrdSysError    *eDest;
const char     *libName;
char           *libPath;
void           *libHandle;
XrdVersionInfo *myInfo;
char           *eBuff;
int             eBLen;
int             msgCnt;

struct PLlist {PLlist  *next;
               char    *libPath;
               void    *libHandle;
              };

static PLlist *plList;
};
#endif
