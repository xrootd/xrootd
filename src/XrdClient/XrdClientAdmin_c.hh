/********************************************************************************/
/*                     X T N e t A d m i n _ c i n t f . h h                    */
/*                                    2004                                      */
/*     Produced by Alvise Dorigo & Fabrizio Furano for INFN padova              */
/*                 A C wrapper for XTNetAdmin functionalities                   */
/********************************************************************************/
//
//   $Id$
//
// Author: Alvise Dorigo, Fabrizio Furano


#ifdef SWIG
%module XrdClientAdmin

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

%{
#include "XrdClientAdmin_c.hh"
   %}

#endif

extern "C" {
   // Some prototypes to wrap ctor and dtor
   // In this version we support only one instance to be handled
   // by this wrapper. Supporting more than one instance should be no
   // problem.
   bool XrdCA_Initialize(const char *url);
   bool XrdCA_Terminate();

   // The other functions, slightly modified from the originals

   bool XrdCA_SysStatX(const char *paths_list, unsigned char *binInfo, int numPath);

   char *XrdCA_ExistFiles(const char *filepaths);
   char *XrdCA_ExistDirs(const char *filepaths);
   char *XrdCA_IsFileOnline(const char *filepaths);

   bool XrdCA_Mv(const char *fileDest, const char *fileSrc);
   bool XrdCA_Mkdir(const char *dir, int user, int group, int other);
   bool XrdCA_Chmod(const char *file, int user, int group, int other);
   bool XrdCA_Rm(const char *file);
   bool XrdCA_Rmdir(const char *path);
   bool XrdCA_Prepare(const char *filepaths, unsigned char opts, unsigned char prty);
   char *XrdCA_DirList(const char *dir);
   char *XrdCA_GetChecksum(const char *path);
}
