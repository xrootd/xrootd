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

#include "XrdClientAdmin.hh"
#include "XrdClientDebug.hh"
#include "XrdClientString.hh"
#include "XrdClientVector.hh"

#include <rpc/types.h>


// We need a reasonable buffer to hold strings to be passed to/from Perl in some cases
char *sharedbuf;

void SharedBufRealloc(long size) {
   sharedbuf = (char *)realloc(sharedbuf, size);
   memset(sharedbuf, 0, size);
}
void SharedBufFree() {
   if (sharedbuf) free(sharedbuf);
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

   bool XrdCA_Initialize(const char *url, int debuglvl) {
      DebugSetLevel(debuglvl);

      if (!adminst)
	 adminst = new XrdClientAdmin(url);
      
      adminst->Connect();
      
      sharedbuf = 0;
      return (adminst != NULL);
   }
   
   bool XrdCA_Terminate() {
      delete adminst;
      adminst = NULL;

      SharedBufFree();

      return TRUE;
   }

   // The other functions, slightly modified from the originals
   //  in order to deal more easily with the perl syntax.
   // Hey these are wrappers!

   bool XrdCA_SysStatX(const char *paths_list, unsigned char *binInfo, int numPath) {
      if (!adminst) return (adminst);
  
      return(adminst->SysStatX(paths_list, binInfo, numPath));
  
   }


   char *XrdCA_ExistFiles(const char *filepaths) {
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

   char *XrdCA_ExistDirs(const char *filepaths) {
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

   char *XrdCA_IsFileOnline(const char *filepaths) {
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




   bool XrdCA_Mv(const char *fileDest, const char *fileSrc) {
      if (!adminst) return adminst;

      return(adminst->Mv(fileDest, fileSrc));
   }


   bool XrdCA_Mkdir(const char *dir, int user, int group, int other) {
      if (!adminst) return adminst;

      return(adminst->Mkdir(dir, user, group, other));
   }


   bool XrdCA_Chmod(const char *file, int user, int group, int other) {
      if (!adminst) return adminst;

      return(adminst->Chmod(file, user, group, other));
   }


   bool XrdCA_Rm(const char *file) {
      if (!adminst) return adminst;

      return(adminst->Rm(file));
   }


   bool XrdCA_Rmdir(const char *path) {
      if (!adminst) return adminst;

      return(adminst->Rmdir(path));
   }


   bool XrdCA_Prepare(const char *filepaths, unsigned char opts, unsigned char prty) {
      if (!adminst) return adminst;

      bool res;

      vecString *vs = Tokenize(filepaths, '\n');

      res = adminst->Prepare(*vs, opts, prty);

      delete vs;

      return(res);
   }

   char *XrdCA_DirList(const char *dir) {
      vecString entries;
      XrdClientString lst;

      if (!adminst) return 0;

      if (!adminst->DirList(dir, entries)) return 0;

      joinStrings(lst, entries);

      SharedBufRealloc(lst.GetSize()+1);
      strcpy(sharedbuf, lst.c_str());

      return sharedbuf;
   }


   char *XrdCA_GetChecksum(const char *path) {
      if (!adminst) return 0;

      memset(sharedbuf, 0, sizeof(sharedbuf));

      adminst->GetChecksum((kXR_char *)path, (kXR_char *)sharedbuf);

      return sharedbuf;
   }

}
