//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientReadAhead                                                   //
//                                                                      //
// Author: Fabrizio Furano (CERN IT-DM, 2009)                           //
//                                                                      //
// Classes to implement a selectable read ahead decision maker          //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRD_CLI_READAHEAD
#define XRD_CLI_READAHEAD



class XrdClientReadAheadMgr {

protected:
   long RASize;
   
public:
   enum XrdClient_RAStrategy {
      RAStr_none,
      RAStr_pureseq,
      RAStr_SlidingAvg
   };
      
   static XrdClientReadAheadMgr *CreateReadAheadMgr(XrdClient_RAStrategy strategy);
   

   XrdClientReadAheadMgr() { RASize = 0; };
   ~XrdClientReadAheadMgr() {};

   virtual int GetReadAheadHint(long long offset, long len, long long &raoffset, long &ralen) = 0;
   virtual int Reset() = 0;
   virtual void SetRASize(long bytes) { RASize = bytes; };
   
   static bool TrimReadRequest(long long &offs, long &len, long rasize);
};








#endif
