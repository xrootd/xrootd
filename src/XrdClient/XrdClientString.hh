//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientString                                                      // 
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
//                                                                      //
// A string class based on low level functions                          //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRD_CSTRING_H
#define XRD_CSTRING_H

#include <string.h>
#include <stdlib.h>

#define STR_NPOS -1

class XrdClientString {
 private:
   char *data;
   int size;
   int capacity;

   int BufRealloc(int newsize);

   inline void Init() {
      data = 0;
      size = 0;
      capacity = 0;
   }

 public:

   char *c_str() {
      return data;
   }

   XrdClientString() {
      Init();
   }
   XrdClientString(char *str) {
      Init();
      Assign(str);
   }
   XrdClientString(XrdClientString &str) {
      Init();
      Assign(str);
   }

   ~XrdClientString();

   int Assign(char *str);
   int Assign(XrdClientString &str) {
      return Assign(str.c_str());
   }

   int Add(char *str);
   int Add(XrdClientString &str) {
      return Add(str.c_str());
   }

   char At(int pos) {
      if ( (pos >= 0) && (pos < size) )
	 return data[pos];
      else
	 abort();
   }

   int Find(char *str, int start=0);
   int Find(XrdClientString &str, int start=0) {
      return Find(data, start);
   }

   bool BeginsWith(char *str) {
      return ( Find(str) == 0 );
   }
   bool BeginsWith(XrdClientString &str) {
      return ( Find(str.c_str()) == 0 );
   }

   void DeleteFromStart(int howmany);

   void DeleteFromEnd(int howmany);

   // Returns a substring left inclusive -> right exclusive
   XrdClientString &Substr(int start=0, int end=STR_NPOS);

   XrdClientString& operator=(const char *str) {
      Assign((char *)str);
      return *this;
   }
   XrdClientString& operator=(XrdClientString &str) {
      Assign(str);
      return *this;
   }
   XrdClientString& operator+=(const char *str) {
      Add((char *)str);
      return *this;
   }
   XrdClientString& operator+=(XrdClientString &str) {
      Add(str);
      return *this;
   }

   bool operator==(XrdClientString &str) {
      return !strcmp(data, str.c_str());
   }
   bool operator==(const char *str) {
      return !strcmp(data, (char *)str);
   }

   char operator[](int pos) {
      return At(pos);
   }

};


#endif
