//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientAdminConn                                                   //
//                                                                      //
// Author: G. Ganis (CERN, 2007)                                        //
//                                                                      //
// High level handler of connections for XrdClientAdmin.                //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//       $Id$

#ifndef XRD_ADMIN_CONN_H
#define XRD_ADMIN_CONN_H


#include "XrdClient/XrdClientConn.hh"

class XrdClientAdminConn : public XrdClientConn {

private:

    bool             fInit;
    bool             fRedirected; // TRUE if has been redirected
    XrdOucHash<int>  fDataConn;  // Data Servers connection IDs

public:
    XrdClientAdminConn() : XrdClientConn(), fInit(0) { }
    virtual ~XrdClientAdminConn() { }

    bool                       GetAccessToSrv();
    XReqErrorType              GoToAnotherServer(XrdClientUrlInfo newdest);
    bool                       SendGenCommand(ClientRequest *req, 
                                              const void *reqMoreData,
                                              void **answMoreDataAllocated,
                                              void *answMoreData, bool HasToAlloc,
                                              char *CmdName, int substreamid = 0);
};

#endif
