//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientVector                                                      // 
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
//                                                                      //
// A simple vector class based on low level functions                   //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//         $Id$


#ifndef XRD_CLIVEC_H
#define XRD_CLIVEC_H

template<class T>
class XrdClientVector {


 private:
   T *data;

   // Remember that the unit is sizeof(T)
   int size;
   int capacity;

   int BufRealloc(int newsize);

   inline void Init() {
      data = 0;
      size = 0;
      capacity = 0;
   }

 public:

   int GetSize() {
      return size;
   }

   XrdClientVector() {
      Init();
   }

   void Push_back(T& item) {
      int sz = strlen(str);
      
      if ( !BufRealloc(size+1) ) {
	 data[size++] = item;
      return 0;
      }
      return 1;
   }

   T &Pop_back() {
      size--;
      return (At[size]);
  
   }

   T &Pop_front() {
      T &res;

      res = At(0);

      size--;
      memmove(data, data+1, size * sizeof(T));
      return (res);
   }


   // Bounded array like access
   T &At(int pos) {
      if ( (pos >= 0) && (pos < size) )
	 return data[pos];
      else
	 abort();
   }
   T &operator[] (int pos) {
      return At(pos);
   }

};

template <class T>
int XrdClientVector<T>::BufRealloc(int newsize) {
   int sz, blks;
   void *newdata;

   if (newsize <= 0) return 0;

   blks = (newsize) / 256 + 1;

   sz = blks * 256;

   newdata = realloc(data, sz*sizeof(T));

   if (newdata) {
      data = (T *)newdata;
      capacity = sz;
      return 0;
   }

   return 1;
}


#endif
