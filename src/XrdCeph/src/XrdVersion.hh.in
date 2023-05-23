/******************************************************************************/
/*                                                                            */
/*                      X r d V e r s i o n . h h . i n                       */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

// this file is automatically updated by the genversion.sh script
// if you touch anything make sure that it still works

#ifndef __XRD_VERSION_H__
#define __XRD_VERSION_H__

#define XrdVERSION  "unknown"

// Numeric representation of the version tag
// The format for the released code is: xyyzz, where: x is the major version,
// y is the minor version and zz is the bugfix revision number
// For the non-released code the value is 1000000
#define XrdVNUMUNK  1000000
#define XrdVNUMBER  1000000

#if XrdDEBUG
#define XrdVSTRING XrdVERSION "_dbg"
#else
#define XrdVSTRING XrdVERSION
#endif

// The following defines the shared library version number of any plug-in.
// Generally, all plug-ins have a uniform version number releative to a
// specific compilation. This version is appended to the so-name and for
// dylibs becomes part of he actual filename (MacOS format).
//
#ifndef XRDPLUGIN_SOVERSION
#define XRDPLUGIN_SOVERSION "4"
#endif

#define XrdDEFAULTPORT 1094;

// The following macros extract version digits from a numeric version number
#define XrdMajorVNUM(x) x/10000
#define XrdMinorVNUM(x) x/100%100
#define XrdPatchVNUM(x) x%100

// The following structure defines the standard way to record a version. You can
// determine component version numbers within an object file by simply executing
// "strings <objectfile> | grep '@V:'".
//
struct XrdVersionInfo {int vNum; const char vOpt; const char vPfx[3];\
                                 const char vStr[40];};

// Macro to define the suffix to use when generating the extern version symbol.
// This is used by SysPlugin. We cannot use it here as cpp does not expand the
// macro when catenating tokens togther and we want to avoid yet another macro.
//
#define XrdVERSIONINFOSFX "_"

// The following macro defines a local copy of version information. Parameters:
// x  -> The variable name of the version information structure
// y  -> An unquoted 1- to 15-character component name (e.g. cmsd, seckrb5)
// vn -> The integer version number to be used
// vs -> The string  version number to be used
//
#define XrdVERSIONINFODEF(x,y,vn,vs) \
        XrdVersionInfo x = \
        {vn, (sizeof(#y)-1) & 0x0f,{'@','V',':'}, #y " " vs}

// The following macro defines an externally referencable structure that records
// the version used to compile code. It is used by the plugin loader. Parms:
// x -> The variable name of the version information structure
// y -> An unquoted 1- to 15-character component name (e.g. cmsd, seckrb5, etc).
//
#define XrdVERSIONINFO(x,y) \
        extern "C" {XrdVERSIONINFODEF(x##_,y,XrdVNUMBER,XrdVERSION);}

// The following macro is an easy way to declare externally defined version
// information. This macro must be used at file level.
//
#define XrdVERSIONINFOREF(x) extern "C" XrdVersionInfo x##_

// The following macro can be used to reference externally defined version
// information. As the composition of the symbolic name may change you should
// use this macro to refer to the version information declaration.
//
#define XrdVERSIONINFOVAR(x) x##_
#endif
