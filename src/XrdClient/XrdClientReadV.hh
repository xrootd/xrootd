//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientReadV                                                       //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2006)                          //
//                                                                      //
// Helper functions for the vectored read functionality                 //
//                                                                      //
//////////////////////////////////////////////////////////////////////////



#ifndef XRD_CLIENT_READV
#define XRD_CLIENT_READV

class XrdClientConn;
#include "XProtocol/XPtypes.hh"

class XrdClientReadV {
public:

    // Builds a request and sends it to the server
    // If destbuf == 0 the request is sent asynchronously
    static kXR_int64 ReqReadV(XrdClientConn *xrdc, char *handle, char *destbuf,
			      kXR_int64 *offsets, int *lens, int nbuf, kXR_int64 maxoffs);

    // Picks a readv response and puts the individual chunks into the dest buffer
    static kXR_int32 UnpackReadVResp(char *destbuf, char *respdata, kXR_int32 respdatalen,
			       kXR_int64 *offsets, int *lens, int nbuf);

    // Picks a readv response and puts the individual chunks into the cache
    static kXR_int32 SubmitToCacheReadVResp(XrdClientConn *xrdc, char *respdata,
				      kXR_int32 respdatalen);




};





#endif
