/********************************************************************************/
/*                     X T N e t A d m i n _ c i n t f . c c                    */
/*                                    2004                                      */
/*     Produced by Alvise Dorigo & Fabrizio Furano for INFN padova              */
/*                 A C wrapper for XTNetAdmin functionalities                   */
/********************************************************************************/
//
//   $Id$
//
// Author: Alvise Dorigo, Fabrizio Furano

#include "XrdClient/XrdClientAdmin.hh"
#include "XrdClient/XrdClientDebug.hh"
#include "XrdClient/XrdClientString.hh"
#include "XrdClient/XrdClientVector.hh"

#include <rpc/types.h>
#include <stdlib.h>

// We need a reasonable buffer to hold strings to be passed to/from Perl in some cases
char *sharedbuf;

void SharedBufRealloc(long size) {
   sharedbuf = (char *)realloc(sharedbuf, size);
   memset(sharedbuf, 0, size);
}
void SharedBufFree() {
   if (sharedbuf) free(sharedbuf);
   sharedbuf = 0;
}


// Useful to otkenize an input char * into a vector of strings
vecString *Tokenize(const char *str, char sep) {
   XrdClientString s(str);

   int it = 0;
   int itTokenEnd = 0;

   vecString *res = new vecString;

   XrdClientString sl;

   while( (res) && (it < s.GetSize()) )
      {
	 //Eat separators
	 while((it < s.GetSize()) && (s[it] == sep))
	    it++;

	 //Find next token
	 itTokenEnd = it;
	 if (itTokenEnd < s.GetSize())
	    do {
	       itTokenEnd++;
	    } while((itTokenEnd < s.GetSize()) && (s[itTokenEnd] != sep));

	 //Append token to result
	 if(it < itTokenEnd) {
	    sl = s.Substr(it, itTokenEnd);
	    res->Push_back(sl);
	 }

	 it = itTokenEnd;
      }

   return res;
}


void BuildBoolAnswer(vecBool &vb) {
   SharedBufRealloc(vb.GetSize());

   for (int i = 0; i < vb.GetSize(); i++) {
      sharedbuf[i] = '0';
      if (vb[i]) sharedbuf[i] = '1';
   }
   sharedbuf[vb.GetSize()] = '\0';

}



// In this version we support only one instance to be handled
// by this wrapper
XrdClientAdmin *adminst = NULL;

extern "C" {

   bool XrdInitialize(const char *url, int debuglvl) {
      DebugSetLevel(debuglvl);

      if (!adminst)
	 adminst = new XrdClientAdmin(url);
      
      adminst->Connect();
      
      sharedbuf = 0;
      return (adminst != NULL);
   }
   
   bool XrdTerminate() {
      delete adminst;
      adminst = NULL;

      SharedBufFree();

      return TRUE;
   }

   // The other functions, slightly modified from the originals
   //  in order to deal more easily with the perl syntax.
   // Hey these are wrappers!

   bool XrdSysStatX(const char *paths_list, unsigned char *binInfo, int numPath) {
      if (!adminst) return (adminst);
  
      return(adminst->SysStatX(paths_list, binInfo, numPath));
  
   }


   char *XrdExistFiles(const char *filepaths) {
      if (!adminst) return NULL;
      bool res = FALSE;
      vecBool vb;
  
      vecString *vs = Tokenize(filepaths, '\n');

      if (res = adminst->ExistFiles(*vs, vb)) {
	 BuildBoolAnswer(vb);
      }
    
      delete vs;
      return(sharedbuf);

   }

   char *XrdExistDirs(const char *filepaths) {
      if (!adminst) return NULL;
      bool res = FALSE;
      vecBool vb;

      vecString *vs = Tokenize(filepaths, '\n');

      if (res = adminst->ExistDirs(*vs, vb)) {
	 BuildBoolAnswer(vb);
      }
    
      delete vs;
      return(sharedbuf);
 
   }

   char *XrdIsFileOnline(const char *filepaths) {
      if (!adminst) return NULL;
      bool res = FALSE;
      vecBool vb;

      vecString *vs = Tokenize(filepaths, '\n');

      if (res = adminst->IsFileOnline(*vs, vb)) {
	 BuildBoolAnswer(vb);
      }
    
      delete vs;
      return(sharedbuf);

   }




   bool XrdMv(const char *fileDest, const char *fileSrc) {
      if (!adminst) return adminst;

      return(adminst->Mv(fileDest, fileSrc));
   }


   bool XrdMkdir(const char *dir, int user, int group, int other) {
      if (!adminst) return adminst;

      return(adminst->Mkdir(dir, user, group, other));
   }


   bool XrdChmod(const char *file, int user, int group, int other) {
      if (!adminst) return adminst;

      return(adminst->Chmod(file, user, group, other));
   }


   bool XrdRm(const char *file) {
      if (!adminst) return adminst;

      return(adminst->Rm(file));
   }


   bool XrdRmdir(const char *path) {
      if (!adminst) return adminst;

      return(adminst->Rmdir(path));
   }


   bool XrdPrepare(const char *filepaths, unsigned char opts, unsigned char prty) {
      if (!adminst) return adminst;

      bool res;

      vecString *vs = Tokenize(filepaths, '\n');

      res = adminst->Prepare(*vs, opts, prty);

      delete vs;

      return(res);
   }

   char *XrdDirList(const char *dir) {
      vecString entries;
      XrdClientString lst;

      if (!adminst) return 0;

      if (!adminst->DirList(dir, entries)) return 0;

      joinStrings(lst, entries);

      SharedBufRealloc(lst.GetSize()+1);
      strcpy(sharedbuf, lst.c_str());

      return sharedbuf;
   }


   char *XrdGetChecksum(const char *path) {
      if (!adminst) return 0;

      char *chksum = 0;

      // chksum now is a memory block allocated by the client itself
      // containing the 0-term response data
      if ( adminst->GetChecksum((kXR_char *)path, (kXR_char **)&chksum) ) {

	 // The data has to be copied to the sharedbuf
	 // to deal with perl parameter passing
	 long sz = strlen(chksum) + 1;

	 SharedBufRealloc(sz);
	 strncpy(sharedbuf, chksum, sz);

         free(chksum);

	 return sharedbuf;
      }
      else return 0;

   }


} // extern c

