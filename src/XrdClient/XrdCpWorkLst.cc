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
int XrdCpWorkLst::SetSrc(XrdClient **srccli, XrdClientString url,
	      XrdClientString urlopaquedata, bool do_recurse) {

   XrdClientString fullurl(url);

   if (urlopaquedata.GetSize())
      fullurl = url + "?" + urlopaquedata;

   fSrcIsDir = FALSE;

   if (fullurl.Find("root://") == 0) {
      // It's an xrd url

      fSrc = url;

      // We must see if it's a dir
      if (!*srccli)
	 (*srccli) = new XrdClient(fullurl.c_str());

      if ((*srccli)->Open(0, kXR_async) &&
          ((*srccli)->LastServerResp()->status == kXR_ok)) {
	 // If the file has been succesfully opened, we use this url
	 fWorkList.Push_back(fSrc);
      }
      else 
	 if ( do_recurse && 
	      ((*srccli)->LastServerError()->errnum == kXR_isDirectory) ){

	    // So, it's a dir for sure
	    // Let's process it recursively

	    fSrcIsDir = TRUE;

	    xrda_src = new XrdClientAdmin(fullurl.c_str());

	    if (xrda_src->Connect()) {

	       BuildWorkList_xrd(fSrc, urlopaquedata);
	    }

	    delete xrda_src;
	    xrda_src = 0;    

	 }
	 else {
	    // It was not opened, nor it was a dir.
	    return 1;
	    //fWorkList.Push_back(fSrc);
	 }

 


   }
   else {
      // It's a local file or path
      fSrc = url;
      fSrcIsDir = FALSE;

      // We must see if it's a dir
      DIR *d = opendir(url.c_str());

      if (!d) {
	 if (errno == ENOTDIR)
	    fWorkList.Push_back(fSrc);
	 else
	    return errno;
      }

      if (d) {
	 fSrcIsDir = TRUE;
	 BuildWorkList_loc(d, url);

	 closedir(d);
      }

   }

   fWorkIt = 0;
   return 0;
}

// Sets the destination of the file copy
// i.e. decides if it's a directory or file name
// It will delete and set to 0 xrddest if it's not a file
int XrdCpWorkLst::SetDest(XrdClient **xrddest, const char *url,
	       const char *urlopaquedata,
	       kXR_unt16 xrdopenflags) {
   int retval = 0;

   // Special case: if url terminates with "/" then it's a dir
   if (url[strlen(url)-1] == '/') {
      fDest = url;
      fDestIsDir = TRUE;
      return 0;
   }

   if (strstr(url, "root://") == url) {
      // It's an xrd url

      fDest = url;

      if (fSrcIsDir) {
	 fDestIsDir = TRUE;
	 return 0;
      }
      else {

	 // The source is a single file
	 fDestIsDir = FALSE;
	 XrdClientString fullurl(url);

	 if (urlopaquedata) {
	    fullurl += "?";
	    fullurl += urlopaquedata;
	 }

	 // let's see if url can be opened as a file (file-to-file copy)
	 *xrddest = new XrdClient(fullurl.c_str());

	 if ( (*xrddest)->Open(kXR_ur | kXR_uw | kXR_gw | kXR_gr | kXR_or,
			       xrdopenflags) &&
	      ((*xrddest)->LastServerResp()->status == kXR_ok) ) {

	    return 0;

	    //XrdClientUrlInfo u(url);

// 	    // If a file open succeeded, then it's a file good for writing to!
// 	    fDestIsDir = FALSE;

// 	    // In any case we might have been assigned a destination data server
// 	    // Better to take this into account instead of the former one
// 	    if ((*xrddest)->GetCurrentUrl().IsValid()) {
// 	       XrdClientUrlInfo uu;
// 	       uu = (*xrddest)->GetCurrentUrl();
// 	       u.Host = uu.Host;
// 	       u.Port = uu.Port;
// 	       fDest = u.GetUrl();
// 	    }	 

	 } else {

	    // The file open was not successful. Let's see why.

	    if ((*xrddest)->LastServerError()->errnum == kXR_isDirectory) {

	       // It may be only a dir
	       fDestIsDir = TRUE;
	       
	       // Anyway, it's ok
	       retval = 0;
	    }
	    else
	       retval = 1;
	    
	    // If the file has not been opened for writing,
	    // there is no need to keep this instance alive.
	    delete *xrddest;
	    *xrddest = 0;
	    
	    return retval;
	 }

      }

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
int XrdCpWorkLst::BuildWorkList_xrd(XrdClientString url, XrdClientString opaquedata) {
   vecString entries;
   int it;
   long id, flags, modtime;
   long long size;
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

	 BuildWorkList_xrd(fullpath, opaquedata);
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

