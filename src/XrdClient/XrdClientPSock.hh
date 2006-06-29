//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientPSock                                                       //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2006)                          //
//                                                                      //
// Client Socket with multiple streams and timeout features             //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//           $Id$

#ifndef XRC_PSOCK_H
#define XRC_PSOCK_H

#include <XrdClient/XrdClientSock.hh>
#include <XrdOuc/XrdOucRash.hh>

#define XRDCLI_PSOCKTEMP -2

class XrdClientPSock: public XrdClientSock {

friend class XrdClientPhyConnection;

private:

    // To translate from socket id to socket descriptor
    XrdOucRash<int, int> fSocketPool;

    int GetSock(int id) {
	int *fd = fSocketPool.Find(id);
	if (fd) return *fd;
	else return -1;
    }
    int GetMainSock() {
	return GetSock(0);
    }

    // To translate from socket descriptor to socket id
    XrdOucRash<int, int> fSocketIdPool;


    int GetSockId(int sock) {
	int *id = fSocketIdPool.Find(sock);
	if (id) return *id;
	else return -1;
    }

protected:

    virtual int    SaveSocket() {
	// this overwrites the main stream
	int *fd = fSocketPool.Find(0);

	fSocketIdPool.Del(*fd);
	fSocketPool.Del(0);

	fConnected = 0;
	fInterrupt = 0;

	if (fd) return *fd;
	else return 0;
    }

public:
   XrdClientPSock(XrdClientUrlInfo host, int windowsize = 0);
   virtual ~XrdClientPSock();

    // Gets length bytes from the parsockid socket
    // If parsockid = -1 then
    //  gets length bytes from any par socket, and returns the usedsubstreamid
    //   where it got the bytes from
    virtual int    RecvRaw(void* buffer, int length, int substreamid = -1,
			   int *usedsubstreamid = 0);


    // Send the buffer to the specified substream
    // if substreamid <= 0 then use the main socket
    virtual int    SendRaw(const void* buffer, int length, int substreamid = 0);

    virtual void   TryConnect(bool isUnix = 0);

    virtual int TryConnectParallelSock();

    virtual int EstablishParallelSock(int sockid);

   virtual void   Disconnect();

   bool   IsConnected() {return fConnected;}
};

#endif
