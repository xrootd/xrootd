//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdCpWorkLst                                                         //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
//                                                                      //
// A class implementing a list of cp to do for XrdCp                    //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include <sys/types.h>
#include <dirent.h>
#include "XrdClientAdmin.hh"





class XrdCpWorkLst {

   vecString fWorkList;
   vecString::iterator fWorkIt;

   XrdClientAdmin *xrda_src, *xrda_dst;

   string fSrc, fDest;
   bool fDestIsDir;

 public:
   
   XrdCpWorkLst();
   ~XrdCpWorkLst();

   // Sets the source path for the file copy
   int SetSrc(const char *url);

   // Sets the destination of the file copy
   int SetDest(const char *url);

   inline void GetDest(string &dest, bool& isdir) {
      dest = fDest;
      isdir = fDestIsDir;
   }

   // Actually builds the worklist
   int BuildWorkList_xrd(string url);
   int BuildWorkList_loc(DIR *dir, string pat);

   bool GetCpJob(string &src, string &dest);
   
};
