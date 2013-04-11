/******************************************************************************/
/*                                                                            */
/*                     X r d C p W o r k L s t . c c                          */
/*                                                                            */
/* Author: Fabrizio Furano (INFN Padova, 2004)                                */
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

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// A class implementing a list of cps to do for XrdCp                   //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "XrdClient/XrdClientEnv.hh"
#include "XrdClient/XrdCpWorkLst.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdSys/XrdSysDir.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include <sys/stat.h>
#include <errno.h>
#include <sstream>
#ifndef WIN32
#include <unistd.h>
#else
#include "XrdSys/XrdWin32.hh"
#endif

using namespace std;

// To print out the last server error info
//____________________________________________________________________________
void PrintLastServerError(XrdClient *cli) {
  struct ServerResponseBody_Error *e;

  if ( cli && (e = cli->LastServerError()) )
    cerr << "Last server error " << e->errnum << " ('" << e->errmsg << "')" <<
      endl;

}

const char *ServerError(XrdClient *cli) {
  struct ServerResponseBody_Error *e;

  if ( cli && (e = cli->LastServerError()) ) return e->errmsg;
  return "Copy failed for unknown reasons!";
}

bool PedanticOpen4Write(XrdClient *cli, kXR_unt16 mode, kXR_unt16 options) {
   // To be really pedantic we must disable the parallel open
   bool paropen = !(options & kXR_delete);

   if (!cli) return false;

   if ( !cli->Open(mode, options, paropen) ) {

      if ( (cli->LastServerError()->errnum == kXR_NotFound)
           && (options & kXR_delete) ) {
         // We silently try to remove the dest file, ignoring the result
         XrdClientAdmin adm(cli->GetCurrentUrl().GetUrl().c_str());
         if (adm.Connect()) {
            adm.Rm( cli->GetCurrentUrl().File.c_str() );
         }

         // And then we try again
         if ( !cli->Open(mode, options, paropen) )
            return false;

      }
      else return false;
   }

   return true;
}

//------------------------------------------------------------------------------
// Check if the opaque data provides the file size information and add it
// if needed
//------------------------------------------------------------------------------
XrdOucString AddSizeHint( const char *dst, off_t size )
{
  // to be removed when we have no more <3.0.4 servers
  // needed because of a bug fixed by 787446f38296698d2881ed45d3009336bde0834d
  if( !EnvGetLong( NAME_XRDCP_SIZE_HINT ) )
    return dst;

  XrdOucString dest = dst;
  std::stringstream s;
  if( dest.find( "?oss.asize=" ) == STR_NPOS &&
      dest.find( "&oss.asize=" ) == STR_NPOS )
  {
    s << dst;
    if( dest.find( "?" ) == STR_NPOS )
      s << "?";
    else
      s << "&";
    s << "oss.asize=" << size;
    dest = s.str().c_str();
  }
  return dest;
}


XrdCpWorkLst::XrdCpWorkLst() {
   fWorkList.Clear();
   xrda_src = 0;
   xrda_dst = 0;
   pSourceSize = 0;
   srcPathLen = 0;
}

XrdCpWorkLst::~XrdCpWorkLst() {
   fWorkList.Clear();
}

// Sets the source path for the file copy
// i.e. expand the given url to the list of the files it involves
int XrdCpWorkLst::SetSrc(XrdClient **srccli, XrdOucString url,
	      XrdOucString urlopaquedata, bool do_recurse, int newCP) {

   XrdOucString fullurl(url);

   if (urlopaquedata.length())
      fullurl = url + "?" + urlopaquedata;

   fSrcIsDir = FALSE;

   if (fullurl.beginswith("root://") || fullurl.beginswith("xroot://") ) {
      // It's an xrd url

      fSrc = url;

      // We must see if it's a dir
      if (!*srccli)
	 (*srccli) = new XrdClient(fullurl.c_str());

      //------------------------------------------------------------------------
      // We're opening a remote file as a source, let's push it to the
      // working list and remember the size so that it could be hinted to
      // the destination server later
      //------------------------------------------------------------------------
      if ((*srccli)->Open(0, kXR_async) &&
          ((*srccli)->LastServerResp()->status == kXR_ok)) {
	 // If the file has been succesfully opened, we use this url
	 fWorkList.Push_back(fSrc);
         XrdClientStatInfo stat;
         (*srccli)->Stat(&stat);
         pSourceSize = stat.size;
      }
      else 
	 if ( do_recurse && 
	      ((*srccli)->LastServerError()->errnum == kXR_isDirectory) ){


 	    delete (*srccli);
	    *srccli = 0;

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
  	  if (!newCP) PrintLastServerError(*srccli);
	    return 1;
	    //fWorkList.Push_back(fSrc);
	 }

 


   }
   else {
      // It's a local file or path
      fSrc = url;
      fSrcIsDir = FALSE;

      // We must see if it's a dir
      XrdSysDir d(url.c_str());

      if (!d.isValid()) {
         if (d.lastError() == ENOTDIR)
         {
            fWorkList.Push_back(fSrc);
            struct stat statStruct;
            if( stat( fSrc.c_str(), &statStruct ) )
              return errno;
            pSourceSize = statStruct.st_size;
         }
         else
            return d.lastError();
      } else {
         fSrcIsDir = TRUE;
         srcPathLen = strlen(url.c_str())+1;
         BuildWorkList_loc(&d, url);
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
	       kXR_unt16 xrdopenflags, int newCP) {
   int retval = 0;

   // Special case: if url terminates with "/" then it's a dir
   if (url[strlen(url)-1] == '/') {
      fDest = url;
      fDestIsDir = TRUE;
      return 0;
   }

   if ( (strstr(url, "root://") == url) ||
        (strstr(url, "xroot://") == url) ) {
      // It's an xrd url

      fDest = url;

      if (fSrcIsDir) {
	 fDestIsDir = TRUE;
         // Make sure fDest ends with a '/' to avoid problems in 
         // path formation later on
         if (!fDest.endswith('/'))
            fDest += '/';
	 return 0;
      }
      else {

	 // The source is a single file
	 fDestIsDir = FALSE;
	 XrdOucString fullurl(url);

	 if (urlopaquedata) {
	    if ((*urlopaquedata) != '?') fullurl += "?";
	    fullurl += urlopaquedata;
	 }

         fullurl = AddSizeHint( fullurl.c_str(), pSourceSize );

	 // let's see if url can be opened as a file (file-to-file copy)
	 *xrddest = new XrdClient(fullurl.c_str());

	 if ( PedanticOpen4Write(*xrddest, kXR_ur | kXR_uw | kXR_gw | kXR_gr | kXR_or,
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

               // Make sure fDest ends with a '/' to avoid problems in 
               // path formation later on
               if (!fDest.endswith('/'))
                  fDest += '/';

	       // Anyway, it's ok
	       retval = 0;
	    }
	    else {
	       if (!newCP) PrintLastServerError(*xrddest);
	       retval = 1;
	    }
	    
	    // If the file has not been opened for writing,
	    // there is no need to keep this instance alive.
     if (!newCP) {delete *xrddest; *xrddest = 0;}
	    
	    return retval;
	 }

      }

   }
   else {
      // It's a local file or path
      
      if (strcmp(url, "-")) {

         fDestIsDir = TRUE;
         // We must see if it's a dir
         struct stat st;
         if (lstat(url, &st) == 0) {
            if (!S_ISDIR(st.st_mode))
               fDestIsDir = FALSE;
         } else {
            if (errno == ENOENT)
               fDestIsDir = FALSE;
            else
               return errno;
         }
         fDest = url;
         // Make sure fDest ends with a '/' to avoid problems in 
         // path formation later on
         if (fDestIsDir && !fDest.endswith('/'))
            fDest += '/';
	 return 0;
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
int XrdCpWorkLst::BuildWorkList_xrd(XrdOucString url, XrdOucString opaquedata) {
   vecString entries;
   int it;
   long id, flags, modtime;
   long long size;
   XrdOucString fullpath;
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


int XrdCpWorkLst::BuildWorkList_loc(XrdSysDir *dir, XrdOucString path)
{

   char *ent = 0;
   XrdOucString fullpath;

   // Here we already have an usable dir handle
   // Cycle on the content and spot all the files
   while (dir && (ent = dir->nextEntry())) {

      if (!strcmp(ent, ".") || !strcmp(ent, ".."))
         continue;

      // Assemble full path name.
      fullpath = path + "/" + ent;

      // Get info for the entry
      struct stat ftype;
      if ( lstat(fullpath.c_str(), &ftype) < 0 )
         continue;

      // If it's a dir, then proceed recursively
      if (S_ISDIR(ftype.st_mode)) {
         XrdSysDir d(fullpath.c_str());

         if (d.isValid())
            BuildWorkList_loc(&d, fullpath);
      } else
         // If it's a file, then add it to the worklist
         if (S_ISREG(ftype.st_mode))
            fWorkList.Push_back(fullpath);
   }

   return 0;
}


// Get the next cp job to do
bool XrdCpWorkLst::GetCpJob(XrdOucString &src, XrdOucString &dest) {

   if (fWorkIt >= fWorkList.GetSize()) return FALSE;

   src = fWorkList[fWorkIt];
   dest = fDest;

   if (fDestIsDir) {

      // If the dest is a directory name, we must concatenate
      // the actual filename, i.e. the token in src following the last /
      // when the source is not local recursive. Otherwise, we need to
      // concatenate the relative path after the specified path.
      int slpos = (srcPathLen ? srcPathLen : src.rfind('/'));

      if (slpos != STR_NPOS)
         dest += XrdOucString(src, slpos);
   }

   fWorkIt++;

   return TRUE;
}

