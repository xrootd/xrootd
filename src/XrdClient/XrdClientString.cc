//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientString                                                      // 
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
//                                                                      //
// A string class based on low level functions                          //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//         $Id: XrdClientString.cc,v 1.0 2004/11/10 06:24:32 furano Exp 

#include "XrdClientString.hh"


XrdClientString::~XrdClientString() {
   if (data) free(data);
}

int XrdClientString::BufRealloc(int newsize) {
   int sz, blks;
   void *newdata;

   if (newsize <= 0) return 0;

   blks = (newsize+1) / 256;

   sz = (blks+1) * 256;

   newdata = realloc(data, sz);

   if (sz < capacity)
      data[sz-1] = '\0';

   if (newdata) {
      data = (char *)newdata;
      capacity = sz;
      return 0;
   }

   return 1;
}

int XrdClientString::Assign(char *str) {
   int sz = strlen(str);

   if ( !BufRealloc(sz) ) {
      size = sz;
      strcpy(data, str);
      return 0;
   }
   
   return 1;

}

int XrdClientString::Add(char *str) {
   int sz = strlen(str);

   if ( !BufRealloc(sz+size) ) {
      size += sz;
      strcat(data, str);
      return 0;
   }
   return 1;
}


int XrdClientString::Find(char *str, int start) {
   char *p = strstr(data+start, str);

   if (p) return (p-data);
   else return 0;
}



void XrdClientString::DeleteFromStart(int howmany) {
   size -= howmany;
   memmove(data, data+howmany, size+1);
      
   BufRealloc(size+1);
}

void XrdClientString::DeleteFromEnd(int howmany) {
   size -= howmany;
   BufRealloc(size+1);

   data[size] = '\0';
}

// Returns a substring left inclusive -> right exclusive
XrdClientString &XrdClientString::Substr(int start, int end) {
   if (end == STR_NPOS) end = size;

   char *buf = (char *)malloc(end-start+1);
   strncpy(buf, data+start, end-start);
   buf[end-start] = '\0';
      
   XrdClientString *s = new XrdClientString(buf);
      
   free(buf);
      
   return(*s);
}
