//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClient                                                            //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// A UNIX reference client for xrootd.                                  //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRD_CLIENT_H
#define XRD_CLIENT_H


//////////////////////////////////////////////////////////////////////////
//                                                                      //
//                                                                      //
// Some features:                                                       //
//  - Automatic server kind recognition (xrootd load balancer, xrootd   //
//    data server, old rootd)                                           //
//  - Fault tolerance for read/write operations (read/write timeouts    //
//    and retry)                                                        //
//  - Internal connection timeout (tunable indipendently from the OS    //
//    one)                                                              //
//  - Full implementation of the xrootd protocol                        //
//  - handling of redirections from server                              //
//  - Connection multiplexing                                           //
//  - Asynchronous operation mode                                       //
//  - High performance read caching with read-ahead                     //
//  - Thread safe                                                       //
//  - Tunable log verbosity level (0 = nothing, 3 = dump read/write     //
//    buffers too!)                                                     //
//  - Many parameters configurable. But the default are OK for nearly   //
//    all the situations.                                               //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "XrdClientAbs.hh"
#include <string>
#include "XrdClientConn.hh"



struct XrdClientOpenInfo {
   bool      opened;
   kXR_int16 mode;
   kXR_int16 options;
};

struct XrdClientStatInfo {
   int stated;
   long long size;
   long id;
   long flags;
   long modtime;
};

class XrdClient : public XrdClientAbs {

private:

   XrdClientConn*               fConnModule;

   char                   fHandle[4];          // The file handle returned by the server,
                                               // to use for successive requests

   struct XrdClientOpenInfo     fOpenPars;   // Just a container for the last parameters
                                       // passed to a Open method

   bool                   fOpenWithRefresh;
   int                    fReadAheadSize;

   struct XrdClientStatInfo     fStatInfo;

   bool                   fUseCache;

   XrdClientUrlInfo             fInitialUrl;

   bool         TryOpen(kXR_int16 mode, kXR_int16 options);
   bool         LowOpen(const char *file, kXR_int16 mode, kXR_int16 options,
			char *additionalquery = 0);

public:

   XrdClient(const char *url);
   virtual ~XrdClient();
  
   bool         OpenFileWhenRedirected(char *newfhandle, bool &wasopen);
   bool         ProcessUnsolicitedMsg(XrdClientUnsolMsgSender *sender,
                                        XrdClientMessage *unsolmsg);

   bool         Close();

   bool         Sync();

   bool         Copy(const char *localpath);

   inline bool  IsOpen() { return fOpenPars.opened; }

   bool         Open(kXR_int16 mode, kXR_int16 options);

   int         Read(const void *buf, long long offset, int len);

   bool         Stat(struct XrdClientStatInfo *stinfo);

   bool         Write(const void *buf, long long offset, int len);

   int          LastErr(const char *err = 0);
};

#endif
