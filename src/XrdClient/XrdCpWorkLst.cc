//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdCpWorkLst                                                         //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
//                                                                      //
// A class implementing a list of cps to do for XrdCp                   //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//   $Id$

#include "XrdClient/XrdCpWorkLst.hh"
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>


using namespace std;


XrdCpWorkLst::XrdCpWorkLst() {
   fWorkList.Clear();
   xrda_src = 0;
   xrda_dst = 0;
}

XrdCpWorkLst::~XrdCpWorkLst() {
   fWorkList.Clear();
}

// Sets the source path for the file copy
// i.e. expand the given url to the list of the files it involves
int XrdCpWorkLst::SetSrc(const char *url) {
   long id, size, flags, modtime;

   if (strstr(url, "root://") == url) {
      // It's an xrd url

      xrda_src = new XrdClientAdmin(url);
      if (xrda_src->Connect()) {
	 XrdClientUrlInfo u(url);

	 fDestIsDir = FALSE;

	 // We must see if it's a dir
	 if ( xrda_src->Stat((char *)u.File.c_str(), id, size, flags, modtime) ) {
	    fSrc = url;

	    if (flags & kXR_isDir)
	       BuildWorkList_xrd(url);
	    else
	       fWorkList.Push_back(fSrc);
         
	 }
	 else {
	    XrdClientString u(url);
	    fWorkList.Push_back(u); 
	 }

      }

      delete xrda_src;
      xrda_src = 0;
   }
   else {
      // It's a local file or path
      fSrc = url;

      // We must see if it's a dir
      DIR *d = opendir(url);

      if (!d) {
	 if (errno == ENOTDIR)
	    fWorkList.Push_back(fSrc);
	 else
	    return errno;
      }

      if (d) {
	 BuildWorkList_loc(d, url);

	 closedir(d);
      }

   }

   fWorkIt = 0;
   return 0;
}

// Sets the destination of the file copy
// i.e. decides if it's a directory or file name
int XrdCpWorkLst::SetDest(const char *url) {
   long id, size, flags, modtime;

   // Special case: if url terminates with "/" then it's a dir
   if (url[strlen(url)-1] == '/') {
      fDest = url;
      fDestIsDir = TRUE;
      return 0;
   }

   if (strstr(url, "root://") == url) {
      // It's an xrd url
      fDest = url;

      xrda_dst = new XrdClientAdmin(url);
      if (xrda_dst->Connect()) {
	 XrdClientUrlInfo u(url);

	 // We must see if it's a dir
	 fDestIsDir = FALSE;
	 if ( xrda_dst->Stat((char *)u.File.c_str(), id, size, flags, modtime) ) {

	    if (flags & kXR_isDir)
	       fDestIsDir = TRUE;
      
	 }

	 // In any case we might have been assigned a destination data server
	 // Better to take this into account instead of the former one
	 if (xrda_dst->GetCurrentUrl().IsValid()) {
	    XrdClientUrlInfo uu;
	    uu = xrda_dst->GetCurrentUrl();
	    u.Host = uu.Host;
	    u.Port = uu.Port;
	    fDest = u.GetUrl();
	 }	 

      }

      delete xrda_dst;
      xrda_dst = 0;
   }
   else {
      // It's a local file or path

      if (strcmp(url, "-")) {
	 // We must see if it's a dir
	 DIR *d = opendir(url);

	 fDestIsDir = TRUE;
	 if (!d) {
	    if (errno == ENOTDIR)
	       fDestIsDir = FALSE;
	    else
	       if (errno == ENOENT)
		  fDestIsDir = FALSE;
	       else
		  return errno;
	 }
	 fDest = url;
	 if (d) closedir(d);

      }
      else {
	 // dest is stdout
	 fDest = url;
	 fDestIsDir = FALSE;
      }

   }

   fWorkIt = 0;
   return 0;
}

// Actually builds the worklist expanding the source of the files
int XrdCpWorkLst::BuildWorkList_xrd(XrdClientString url) {
   vecString entries;
   int it;
   long id, size, flags, modtime;
   XrdClientString fullpath;
   XrdClientUrlInfo u(url);

   // Invoke the DirList cmd to get the content of the dir
   if (!xrda_src->DirList(u.File.c_str(), entries)) return -1;

   // Cycle on the content and spot all the files
   for (it = 0; it < entries.GetSize(); it++) {
      fullpath = url + "/" + entries[it];
      XrdClientUrlInfo u(fullpath);

      // We must see if it's a dir
      // If a dir is found, do it recursively
      if ( xrda_src->Stat((char *)u.File.c_str(), id, size, flags, modtime) &&
	   (flags & kXR_isDir) ) {

	 BuildWorkList_xrd(fullpath);
      }
      else
	 fWorkList.Push_back(fullpath);
      

   }

   return 0;
}


int XrdCpWorkLst::BuildWorkList_loc(DIR *dir, XrdClientString path) {
   struct dirent *ent;
   XrdClientString fullpath;

   // Here we already have an usable dir handle
   // Cycle on the content and spot all the files
   while ( (ent = readdir(dir)) ) {
      struct stat ftype;

      if ( !strcmp(ent->d_name, ".") ||
	   !strcmp(ent->d_name, "..") )
	 continue;

      // Assemble full path name.
      fullpath = path + "/" + ent->d_name;

      // Get info for the entry
      if ( lstat(fullpath.c_str(), &ftype) < 0 )
	 continue;
      
      // If it's a dir, then proceed recursively
      if (S_ISDIR (ftype.st_mode)) {
	 DIR *d = opendir(fullpath.c_str());

	 if (d) {
	    BuildWorkList_loc(d, fullpath);

	    closedir(d);
	 }
      }
      else
	 // If it's a file, then add it to the worklist
	 if (S_ISREG(ftype.st_mode))
	    fWorkList.Push_back(fullpath);

   }

   return 0;

}


// Get the next cp job to do
bool XrdCpWorkLst::GetCpJob(XrdClientString &src, XrdClientString &dest) {

   if (fWorkIt >= fWorkList.GetSize()) return FALSE;

   src = fWorkList[fWorkIt];
   dest = fDest;

   if (fDestIsDir) {

      // If the dest is a directory name, we must concatenate
      // the actual filename, i.e. the token in src following the last /
      int slpos = src.RFind((char *)"/");

      if (slpos != STR_NPOS) 
	 dest += src.Substr(slpos);
	 
   }

   fWorkIt++;

   return TRUE;
}

