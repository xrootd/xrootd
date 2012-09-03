/******************************************************************************/
/*                                                                            */
/*                   X r d C l i e n t A d m i n _ c . h h                    */
/*                                                                            */
/* 2004 Produced by Alvise Dorigo & Fabrizio Furano for INFN padova           */
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

/********************************************************************************/
/*                 A C wrapper for XTNetAdmin functionalities                   */
/********************************************************************************/

#ifdef SWIG
%module XrdClientAdmin
%include typemaps.i                       // Load the typemaps librayr

 // This tells SWIG to treat an char * argument with name res as
 // an output value.  

%typemap(argout) char *OUTPUT {
   $result = sv_newmortal();
   sv_setnv($result, arg2);
   argvi++;                     /* Increment return count -- important! */
}

// We don't care what the input value is. Ignore, but set to a temporary variable

%typemap(in,numinputs=0) char *OUTPUT(char junk) {
   $1 = &junk;
}

%apply char *OUTPUT { char *ans };

// For the stat function to return an array containing the
// various fields of the answer
%apply long *OUTPUT {long *id};   // Make "result" an output parameter
%apply long long *OUTPUT {long long *size};   // Make "result" an output parameter
%apply long *OUTPUT {long *flags};   // Make "result" an output parameter
%apply long *OUTPUT {long *modtime};   // Make "result" an output parameter

%{
#include "XrdClient/XrdClientAdmin_c.hh"
   %}

#endif

extern "C" {
   // Some prototypes to wrap ctor and dtor
   // In this version we support only one instance to be handled
   // by this wrapper. Supporting more than one instance should be no
   // problem.
   bool XrdInitialize(const char *url, const char *EnvValues);
   bool XrdTerminate();

   // The other functions, slightly modified from the originals
   char *XrdSysStatX(const char *paths_list);

   char *XrdExistFiles(const char *filepaths);
   char *XrdExistDirs(const char *filepaths);
   char *XrdIsFileOnline(const char *filepaths);

   bool XrdMv(const char *fileSrc, const char *fileDest);
   bool XrdMkdir(const char *dir, int user, int group, int other);
   bool XrdChmod(const char *file, int user, int group, int other);
   bool XrdRm(const char *file);
   bool XrdRmdir(const char *path);
   bool XrdPrepare(const char *filepaths, unsigned char opts, unsigned char prty);
   char *XrdDirList(const char *dir);
   char *XrdGetChecksum(const char *path);
   char *XrdGetCurrentHost();

   bool XrdStat(const char *fname, long *id, long long *size, long *flags, long *modtime);
}
