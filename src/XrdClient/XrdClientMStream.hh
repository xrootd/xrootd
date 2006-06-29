//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientMStream                                                     //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2006)                          //
//                                                                      //
// Helper code for XrdClient to handle multistream behavior             //
//                                                                      //
//////////////////////////////////////////////////////////////////////////


#ifndef XRD_CLI_MSTREAM
#define XRD_CLI_MSTREAM

#include "XrdClient/XrdClientConn.hh"

class XrdClientMStream {





public:

    // Establish all the parallel streams, stop
    // adding streams at the first creation refusal/failure
    static int EstablishParallelStreams(XrdClientConn *cliconn);

    // Add a parallel stream to the pool used by the given client inst
    static int AddParallelStream(XrdClientConn *cliconn);

    // Remove a parallel stream to the pool used by the given client inst
    static int RemoveParallelStream(XrdClientConn *cliconn, int substream = -1);

    // Binds the pending temporary parallel stream to the current session
    // Returns into newid the substreamid assigned by the server
    static bool BindPendingStream(XrdClientConn *cliconn, int substreamid, int &newid);

};




#endif
