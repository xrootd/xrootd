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

#include "XrdClient/XrdClientString.hh"


XrdClientString::~XrdClientString() {
   if (data) free(data);
}

int XrdClientString::BufRealloc(int newsize) {
   int sz, blks;
   void *newdata;

   if (newsize < 0) return 1;

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



int XrdClientString::Add(char const *str) {
   int sz = strlen(str);

   if ( !BufRealloc(sz+size) ) {
      if (size)
	 strcat(data, str);
      else
	 strcpy(data, str);

      size += sz;
      return 0;
   }
   return 1;
}


int XrdClientString::Find(char *str, int start) {
   char *p = strstr(data+start, str);

   if (p) return (p-data);
   else return STR_NPOS;
}

int XrdClientString::RFind(char *str, int start) {
   if (start == STR_NPOS) start = size-1;

   for (int i = start; i >= 0; i--) {
      int p = Find(str, i);
      
      if (p != STR_NPOS) return p;
   }
   
   return STR_NPOS;

}


void XrdClientString::EraseFromStart(int howmany) {
   size -= howmany;
   memmove(data, data+howmany, size+1);
      
   BufRealloc(size+1);
}

void XrdClientString::EraseFromEnd(int howmany) {
   size -= howmany;
   BufRealloc(size+1);

   data[size] = '\0';
}

// Returns a substring left inclusive -> right exclusive
XrdClientString XrdClientString::Substr(int start, int end) {
   if ( (end == STR_NPOS) || (end > size) )  end = size;

   if ( (start == STR_NPOS) || (start > size) )  start = size;

   char *buf = (char *)malloc(end-start+1);
   strncpy(buf, data+start, end-start);
   buf[end-start] = '\0';
      
   XrdClientString s(buf);
      
   free(buf);
      
   return(s);
}


// Operator << is useful to print a string into a stream
ostream &operator<< (ostream &os, const XrdClientString &obj) {
   os << ((XrdClientString)obj).c_str();
   return os;
}

XrdClientString const operator+(const char *str1, const XrdClientString str2) {
   XrdClientString res((char *)str1);

   res.Add((XrdClientString &)str2);
   return res;
}
