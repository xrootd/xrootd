#ifndef __XRDOUCPINLOADER_HH__
#define __XRDOUCPINLOADER_HH__
/******************************************************************************/
/*                                                                            */
/*                    X r d O u c P i n L o a d e r . h h                     */
/*                                                                            */
/* (c) 2014 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

//------------------------------------------------------------------------------
//! This include file define a class that loads a versioned plugin.
//------------------------------------------------------------------------------
  
//------------------------------------------------------------------------------
//! Load a plugin. Internal plugin version checking is performed when the
//! plugin is actually initialized by the caller. The caller is responsible
//! for persisting the image after initializing the plugin.
//!
//! @return !0    Pointer to the successfully loaded.
//! @return =0    The plugin could not be loaded.
//------------------------------------------------------------------------------

class  XrdSysError;
class  XrdSysPlugin;
struct XrdVersionInfo;

class XrdOucPinLoader
{
public:

//------------------------------------------------------------------------------
//! Export the plugin object for manual management.
//!
//! @return !0    Pointer to the plugin object. It is disassociated from this
//!               object and must be manually managed.
//! @return =0    Either no plugin object has been created or it has been
//!               exported.
//------------------------------------------------------------------------------

XrdSysPlugin *Export() {XrdSysPlugin *tmp = piP; piP = 0; return tmp;}

//------------------------------------------------------------------------------
//! Set export range of symbols in the plugin.
//!
//! @param glbl   when true then the symbols defined in the plug-in shared
//!               library are made available for symbol resolution of
//!               subsequently loaded libraries.
//------------------------------------------------------------------------------

void          Global(bool glbl) {global = glbl;}

//------------------------------------------------------------------------------
//! Get the last message placed in the buffer.
//!
//! @return Pointer to the last message. If there is no buffer or no message
//!         exists, a null string is returned.
//------------------------------------------------------------------------------

const char   *LastMsg() {return (errBP ? errBP : "");}

//------------------------------------------------------------------------------
//! Get the actual path that was or will tried for loading.
//!
//! @return       Pointer to the path that was loaded if called after Resolve()
//!               or the path that will be attempted to be loaded. If the
//!               path is invalid, a single question mark is returned.
//------------------------------------------------------------------------------

const char   *Path() {return tryLib;}

//------------------------------------------------------------------------------
//! Resolve a desired symbol from the plugin image.
//!
//! @param  symbl Pointer to the name of the symbol to resolve.
//! @param  mcnt  Maximum number of version messages to be displayed.
//!
//! @return !0    The address of the symbol.
//! @return =0    The symbol could not be resolved.
//------------------------------------------------------------------------------

void         *Resolve(const char *symbl, int mcnt=1);

//------------------------------------------------------------------------------
//! Unload any plugin that may be associated with this object. The plugin image
//! will not be persisted when this object is deleted.
//!
//! @param  dodel When true, the object is deleted (this only works if it is
//!               created via new). Otherwise, plugin is only unloaded.
//------------------------------------------------------------------------------

void          Unload(bool dodel=false);

//------------------------------------------------------------------------------
//! Constructor #1
//!
//! @param errP   Pointer to the message routing object.
//! @param vInfo  Pointer to the version information of the caller. If the
//!               pointer is nil, no version checking occurs.
//! @param drctv  Pointer to the directive that initiated the load. The text is
//!               used in error messages to relate the directive to the error.
//!               E.g. "oofs.osslib" -> "Unable to load ofs.osslib plugin...."
//! @param plib   Pointer to the shared library path that contains the plugin.
//------------------------------------------------------------------------------

              XrdOucPinLoader(XrdSysError    *errP,
                              XrdVersionInfo *vInfo,
                              const char     *drctv,
                              const char     *plib);

//------------------------------------------------------------------------------
//! Constructor #2
//!
//! @param eBuff  Pointer to a buffer to receive messages.
//! @param eBlen  Length of the buffer.
//! @param vInfo  Pointer to the version information of the caller. If the
//!               pointer is nil, no version checking occurs.
//! @param drctv  Pointer to the directive that initiated the load (see above).
//! @param plib   Pointer to the shared library path that contains the plugin.
//------------------------------------------------------------------------------

              XrdOucPinLoader(char           *eBuff,
                              int             eBlen,
                              XrdVersionInfo *vInfo,
                              const char     *drctv,
                              const char     *plib);

//------------------------------------------------------------------------------
//! Constructor #3 (An internal message buffer is allocated. You can get the
//!                 message, if any, using LastMsg())
//!
//! @param vInfo  Pointer to the version information of the caller. If the
//!               pointer is nil, no version checking occurs.
//! @param drctv  Pointer to the directive that initiated the load (see above).
//! @param plib   Pointer to the shared library path that contains the plugin.
//------------------------------------------------------------------------------

              XrdOucPinLoader(XrdVersionInfo *vInfo,
                              const char     *drctv,
                              const char     *plib);

//------------------------------------------------------------------------------
//! Destructor
//!
//! Upon deletion, if the plugin was successfully loaded, it is persisted.
//------------------------------------------------------------------------------

             ~XrdOucPinLoader();

private:
void            Inform(const char *txt1,   const char *txt2=0,
                       const char *txt3=0, const char *txt4=0,
                       const char *txt5=0);
void            Init(const char *drctv, const char *plib);

XrdSysError    *eDest;
XrdSysPlugin   *piP;
XrdVersionInfo *viP;
const char     *dName;
const char     *tryLib;
char           *theLib;
char           *altLib;
char           *errBP;
int             errBL;
bool            global;
bool            frBuff;
};
#endif
