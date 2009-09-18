/////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientReadAhead                                                   //
//                                                                      //
// Author: Fabrizio Furano (CERN IT-DM, 2009)                           //
//                                                                      //
// Classes to implement a selectable read ahead decision maker          //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "XrdClientReadAhead.hh"
#include "XrdClientConst.hh"




#include <iostream>


bool XrdClientReadAheadMgr::TrimReadRequest(long long &offs, long &len, long rasize) {

    long long newoffs;
    long newlen, blksz;

    blksz = 128*1024;
    long long lastbyte;
    newoffs = offs;
    lastbyte = offs+len+blksz-1;
    lastbyte = (lastbyte / blksz) * blksz - 1;
    
    newlen = lastbyte-newoffs+1;

    //std::cerr << "Trim: " << offs << "," << len << " --> " << newoffs << "," << newlen << std::endl;
    offs = newoffs;
    len = newlen;
    return true;

}




// -----------------------------------------------------------------------






// A basic implementation. Purely sequential read ahead
class XrdClientReadAhead_pureseq : public XrdClientReadAheadMgr {

protected:
   long long RALast;

public:

   XrdClientReadAhead_pureseq() {
      RALast = 0;
   }

   virtual int GetReadAheadHint(long long offset, long len, long long &raoffset, long &ralen);

   virtual int Reset() {
      RALast = 0;
      return 0;
   }

};





int XrdClientReadAhead_pureseq::GetReadAheadHint(long long offset, long len, long long &raoffset, long &ralen) {

   // We read ahead only if (offs+len) lies in an interval of RALast not bigger than the readahead size
   if ( (RALast - (offset+len) < RASize) &&
        (RALast - (offset+len) > -RASize) &&
        (RASize > 0) ) {
      
      // This is a HIT case. Async readahead will try to put some data
      // in advance into the cache. The higher the araoffset will be,
      // the best chances we have not to cause overhead
      raoffset = xrdmax(RALast, offset + len);
      ralen = xrdmin(RASize,
                     offset + len + RASize -
                     xrdmax(offset + len, RALast));
      
      if (ralen > 0) {
         TrimReadRequest(raoffset, ralen, RASize);
         RALast = raoffset + ralen;
         return 0;
      }
   }
   
   return 1;
   
};


XrdClientReadAheadMgr *XrdClientReadAheadMgr::CreateReadAheadMgr(XrdClient_RAStrategy strategy) {
   XrdClientReadAheadMgr *ramgr = 0;

   switch (strategy) {

   case RAStr_none:
      break;

   case RAStr_pureseq: {
         ramgr = new XrdClientReadAhead_pureseq();
         break;
      }
   case RAStr_SlidingAvg: {
         //ramgr = new XrdClientReadAhead_slidingavg();
         break;
      }
     
   }

   return ramgr;
}
