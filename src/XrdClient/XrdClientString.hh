//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientString                                                      // 
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
//                                                                      //
// A string class based on low level functions                          //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//         $Id: XrdClientString.hh,v 1.0 2004/11/10 06:24:32 furano Exp 

#ifndef XRD_CSTRING_H
#define XRD_CSTRING_H

#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <stdio.h>

using namespace std;

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

   // Returns a c string
   char const *c_str() {
      return data;
   }

   // Various constructors
   XrdClientString() {
      Init();
   }
   XrdClientString(char *str) {
      Init();
      Assign(str);
   }
   XrdClientString(const char *str) {
      Init();
      Assign((char *)str);
   }
   XrdClientString(const XrdClientString &str) {
      Init();
      Assign((XrdClientString &)str);
   }

   ~XrdClientString();

   // Assigns a string to this object
   int Assign(char *str) {
      int sz = strlen(str);
      
      if ( !BufRealloc(sz) ) {
	 size = sz;
	 strcpy(data, str);
	 return 0;
      }
      
      return 1;
      
   }

   int Assign(XrdClientString &str) {
      int sz = strlen((char *)str.c_str());

      if ( !BufRealloc(sz) ) {
	 size = sz;
	 strcpy(data, str.c_str());
	 return 0;
      }
      
      return 1;
      
   }

   // Concatenates a string to this object
   int Add(char const *str);
   int Add(XrdClientString &str) {
      return Add(str.c_str());
   }

   // Bounded array like access
   char &At(int pos) {
      if ( (pos >= 0) && (pos < size) )
	 return data[pos];
      else
	 abort();
   }
   char &operator[] (unsigned int pos) {
      return At(pos);
   }

   // Returns the position of the first occurrence of str
   int Find(char *str, int start=0);
   int Find(XrdClientString &str, int start=0) {
      return Find(data, start);
   }

   int RFind(char *str, int start = STR_NPOS);
   int RFind(XrdClientString &str, int start = STR_NPOS) {
      return RFind((char *)str.c_str(), start);
   }

   bool BeginsWith(char *str) {
      return ( Find(str) == 0 );
   }
   bool BeginsWith(XrdClientString &str) {
      return ( Find((char *)str.c_str()) == 0 );
   }

   bool EndsWith(char *str) {
      int sz = strlen(str);

      return ( !strcmp(data+size-sz, str) );
   }
   bool EndsWith(XrdClientString &str) {
      return ( EndsWith((char *)str.c_str()) );
   }

   // To delete parts of this string
   void EraseFromStart(int howmany);
   void EraseFromEnd(int howmany);
   void EraseToEnd(int firsttoerase) {
      EraseFromEnd(size-firsttoerase);
   }

   int GetSize() {
      return size;
   }

   // Returns a substring left inclusive -> right exclusive
   XrdClientString Substr(int start=0, int end=STR_NPOS);

   // Assignment operator overloading
   XrdClientString operator=(const char *str) {
      Assign((char *)str);
      return *this;
   }
   XrdClientString operator=(XrdClientString str) {
      Assign(str);
      return *this;
   }

   // Concatenation operator overloading
   XrdClientString& operator+=(const char *str) {
      Add((char *)str);
      return *this;
   }
   //   XrdClientString& operator+=(XrdClientString &str) {
   //   Add(str);
   //   return *this;
   //}
   XrdClientString& operator+=(XrdClientString str) {
      Add(str);
      return *this;
   }
   XrdClientString& operator+=(int numb) {
      char buf[20];

      sprintf(buf, "%d", numb);
      Add(buf);
      return *this;
   }

   XrdClientString operator+(const char *str) {
      XrdClientString res((char *)c_str());

      res.Add((char *)str);
      return res;
   }
   XrdClientString operator+(XrdClientString &str) {
      XrdClientString res((char *)c_str());

      res.Add(str);
      return res;

   }

   // Test for equalness
   bool operator==(XrdClientString &str) {
      return !strcmp(data, str.c_str());
   }
   bool operator==(const char *str) {
      return !strcmp(data, (char *)str);
   }
   // Test for diversity
   bool operator!=(XrdClientString &str) {
      return strcmp(data, str.c_str());
   }
   bool operator!=(const char *str) {
      return strcmp(data, (char *)str);
   }


   // Char array-like behavior
   char &operator[] (int pos) {
      return At(pos);
   }

};

// Operator << is useful to print a string into a stream
ostream &operator<< (ostream &os, const XrdClientString &obj);


XrdClientString const operator+(const char *str1, const XrdClientString str2);
#endif
