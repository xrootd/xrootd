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
   char *c_str() {
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
   XrdClientString(XrdClientString &str) {
      Init();
      Assign(str);
   }

   ~XrdClientString();

   // Assigns a string to this object
   int Assign(char *str);
   int Assign(XrdClientString &str) {
      return Assign(str.c_str());
   }

   // Concatenates a string to this object
   int Add(char *str);
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

   bool BeginsWith(char *str) {
      return ( Find(str) == 0 );
   }
   bool BeginsWith(XrdClientString &str) {
      return ( Find(str.c_str()) == 0 );
   }

   // To delete parts of this string
   void DeleteFromStart(int howmany);
   void DeleteFromEnd(int howmany);

   // Returns a substring left inclusive -> right exclusive
   XrdClientString &Substr(int start=0, int end=STR_NPOS);

   // Assignment operator overloading
   XrdClientString& operator=(const char *str) {
      Assign((char *)str);
      return *this;
   }
   XrdClientString& operator=(XrdClientString &str) {
      Assign(str);
      return *this;
   }

   // Concatenation operator overloading
   XrdClientString& operator+=(const char *str) {
      Add((char *)str);
      return *this;
   }
   XrdClientString& operator+=(XrdClientString &str) {
      Add(str);
      return *this;
   }

   // Test for equalness
   bool operator==(XrdClientString &str) {
      return !strcmp(data, str.c_str());
   }
   bool operator==(const char *str) {
      return !strcmp(data, (char *)str);
   }

   // Char array-like behavior
   char &operator[] (int pos) {
      return At(pos);
   }

};

// Operator << is useful to print a string into a stream
std::ostream &operator<< (std::ostream &os, XrdClientString &obj) {
   os << obj.c_str();
   return os;
}

#endif
