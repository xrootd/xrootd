//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdCpWorkLst                                                         //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
//                                                                      //
// A class implementing a list of cps to do for XrdCp                   //
//                                                                      //
//////////////////////////////////////////////////////////////////////////


#include "XrdCpWorkLst.hh"
#include <sys/stat.h>
#include <unistd.h>


using namespace std;


XrdCpWorkLst::XrdCpWorkLst() {
   fWorkList.clear();
   xrda_src = 0;
   xrda_dst = 0;
}

XrdCpWorkLst::~XrdCpWorkLst() {
   fWorkList.clear();
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
	       fWorkList.push_back(url);
         
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
	    fWorkList.push_back(url);
	 else
	    return errno;
      }

      BuildWorkList_loc(d, url);

      closedir(d);

   }

   fWorkIt = fWorkList.begin();
   return 0;
}

// Sets the destination of the file copy
// i.e. decides if it's a directory or file name
int XrdCpWorkLst::SetDest(const char *url) {
   long id, size, flags, modtime;

   if (strstr(url, "root://") == url) {
      // It's an xrd url

      xrda_dst = new XrdClientAdmin(url);
      if (xrda_dst->Connect()) {
	 XrdClientUrlInfo u(url);

	 // We must see if it's a dir
	 fDestIsDir = FALSE;
	 if ( xrda_dst->Stat((char *)u.File.c_str(), id, size, flags, modtime) ) {

	    if (flags & kXR_isDir)
	       fDestIsDir = TRUE;
      
	    fDest = url;
	 }

      }

      delete xrda_dst;
      xrda_dst = 0;
   }
   else {
      // It's a local file or path

      // We must see if it's a dir
      DIR *d = opendir(url);

      fDestIsDir = TRUE;
      if (!d) {
	 if (errno == ENOTDIR)
	    fDestIsDir = FALSE;
	 else
	    return errno;
      }
      fDest = url;
      closedir(d);

   }

   fWorkIt = fWorkList.begin();
   return 0;
}

// Actually builds the worklist expanding the source of the files
int XrdCpWorkLst::BuildWorkList_xrd(string url) {
   vecString entries;
   vecString::iterator it;
   long id, size, flags, modtime;
   string fullpath;
   XrdClientUrlInfo u(url);

   // Invoke the DirList cmd to get the content of the dir
   if (!xrda_src->DirList(u.File.c_str(), entries)) return -1;

   // Cycle on the content and spot all the files
   for (it = entries.begin(); it != entries.end(); it++) {
      fullpath = url + "/" + *it;
      XrdClientUrlInfo u(fullpath);

      // We must see if it's a dir
      // If a dir is found, do it recursively
      if ( xrda_dst->Stat((char *)u.File.c_str(), id, size, flags, modtime) &&
	   (flags & kXR_isDir) ) {

	 BuildWorkList_xrd(fullpath);
      }
      else
	 fWorkList.push_back(*it);
      

   }

   return 0;
}


int XrdCpWorkLst::BuildWorkList_loc(DIR *dir, string path) {
   struct dirent *ent;
   string fullpath;

   // Here we already have an usable dir handle
   // Cycle on the content and spot all the files
   while ( (ent = readdir(dir)) ) {
      struct stat ftype;

      // Assemble full path name.
      fullpath = path + "/" + ent->d_name;

      // Get info for the entry
      if ( lstat(fullpath.c_str(), &ftype) < 0 )
	 continue;
      
      // If it's a dir, then proceed recursively
      if (S_ISDIR (ftype.st_mode)) {
	 DIR *d = opendir(fullpath.c_str());

	 BuildWorkList_loc(d, fullpath);

	 closedir(d);
      }
      else
	 // If it's a file, then add it to the worklist
	 if (S_ISREG(ftype.st_mode))
	    fWorkList.push_back(fullpath);

   }

   return 0;

}


// Get the next cp job to do
bool XrdCpWorkLst::GetCpJob(string &src, string &dest) {

   if (fWorkIt == fWorkList.end()) return FALSE;

   src = *fWorkIt;
   dest = fDest;

   if (fDestIsDir) {

      // If the dest is a directory name, we must concatenate
      // the actual filename, i.e. the token in src following the last /
      unsigned int slpos = src.rfind("/", src.size());

      if (slpos != string::npos) 
	 dest += "/" + src.substr(slpos+1);
	 
   }

   fWorkIt++;

   return TRUE;
}
