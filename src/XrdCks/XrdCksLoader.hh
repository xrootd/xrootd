#ifndef __XRDCKSLOADER_HH__
#define __XRDCKSLOADER_HH__
/******************************************************************************/
/*                                                                            */
/*                       X r d C k s L o a d e r . h h                        */
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

class  XrdCksCalc;
class  XrdSysPlugin;
struct XrdVersionInfo;

/*! This class defines the checksum loader interface. It is intended to be used
    by xrootd clients to obtain an instance of a checksum calculation object.
    This object may be builtin or may come from a shared library.
*/
  
class XrdCksLoader
{
public:

//------------------------------------------------------------------------------
//! Get a new XrdCksCalc object that can calculate the checksum corresponding to
//! the specified name. The object can be used to compute checksums on the fly.
//! The object's Recycle() method must be used to delete it. The adler32, crc32,
//! and md5 checksums are natively supported. Up to five more checksum
//! algorithms can be loaded from shared libraries.
//!
//! @param  csNme    The name of the checksum algorithm (e.g. md5).
//! @param  csParms  Any parameters that might be needed by the checksum
//!                  algorithm should it be loaded from a shared library.
//! @param  eBuff    Optional pointer to a buffer to receive the reason for a
//!                  load failure as a null terminated string.
//! @param  eBlen    The length of the buffer.
//! @param  orig     Returns the original object not a new instance of it.
//!                  This is usually used by CksManager during an autoload.
//!
//! @return Success: A pointer to a new checksum calculation object.
//!         Failure: Zero if the corresponding checksum object could not be
//!                  loaded. If eBuff was supplied, it holds the reason.
//------------------------------------------------------------------------------

XrdCksCalc *Load(const char *csName,  const char *csParms=0,
                       char *eBuff=0, int   eBlen=0, bool orig=false);

//------------------------------------------------------------------------------
//! Constructor
//!
//! @param  vInfo    Is the reference to the version information corresponding
//!                  to the xrootd version you compiled with. You define this
//!                  information using the XrdVERSIONINFODEF macro defined in
//!                  XrdVersion.hh. You must supply your version information
//!                  and it must be compatible with the loader and any shared
//!                  libraries that it might load on your behalf.
//!
//! @param  libPath  The path where dynamic checksum calculators are to be
//!                  found and dynamically loaded, if need be. If libPath is
//!                  nil then the default loader search order is used.
//!                  The name of the shared library must follow the naming
//!                  convention "libXrdCksCalc<csName>.so" where <csName> is the
//!                  checksum name. So, an sha256 checksum would try to load
//!                  libXrdCksCalcsha256.so shared library.
//------------------------------------------------------------------------------

           XrdCksLoader(XrdVersionInfo &vInfo, const char *libPath=0);

//------------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------------

          ~XrdCksLoader();

private:

struct csInfo
      {char         *Name;
       XrdCksCalc   *Obj;
       XrdSysPlugin *Plugin;
                     csInfo() : Name(0), Obj(0), Plugin(0) {}
                    ~csInfo() {}
      };

csInfo *Find(const char *Name);

char            *verMsg;     // This member must be the 1st member
XrdVersionInfo  *urVersion;  // This member must be the 2nd member
char            *ldPath;
static const int csMax = 8;
       csInfo    csTab[csMax];
       int       csLast;
};
#endif
