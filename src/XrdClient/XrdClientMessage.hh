//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdMessage                                                           // 
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// A message coming from a physical connection. I.e. a server response  //
//  or some kind of error                                               //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRC_MESSAGE_H
#define XRC_MESSAGE_H

#include "XrdXProtocol.hh"
#include "XrdClientSock.hh"

class XrdPhyConnection;

class XrdMessage {

private:
   bool           fAllocated;
   void           *fData;
   bool           fMarshalled;
   short          fStatusCode;

   short       CharStreamid2Int(kXR_char *charstreamid);
   void        Int2CharStreamid(kXR_char *charstreamid, short intstreamid);

public:

   enum EXrdMSCStatus {             // Some status codes useful
      kXrdMSC_ok       = 0,
      kXrdMSC_readerr  = 1,
      kXrdMSC_writeerr = 2,
      kXrdMSC_timeout  = 3
   };
   ServerResponseHeader fHdr;

   XrdMessage(ServerResponseHeader header);
   XrdMessage();

   ~XrdMessage();

   bool               CreateData();
   inline int         DataLen() { return fHdr.dlen; }
   void              *DonateData();
   inline void       *GetData() {return fData;}
   inline int         GetStatusCode() { return fStatusCode;}
   inline int         HeaderStatus() { return fHdr.status; }
   inline short       HeaderSID() { return CharStreamid2Int(fHdr.streamid); }
   bool               IsAttn() { return (fHdr.status == kXR_attn); }
   inline bool        IsError() { return (fStatusCode != kXrdMSC_ok); };
   inline bool        IsMarshalled() { return fMarshalled; }
   void               Marshall();
   inline bool        MatchStreamid(short sid) { return (HeaderSID() == sid);}
   int                ReadRaw(XrdPhyConnection *phy);
   inline void        SetHeaderStatus(kXR_int16 sts) { fHdr.status = sts; }
   inline void        SetMarshalled(bool m) { fMarshalled = m; }
   inline void        SetStatusCode(kXR_int16 status) { fStatusCode = status; }
   void               Unmarshall();

};

#endif
