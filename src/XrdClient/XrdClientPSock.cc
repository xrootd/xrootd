//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientPSock                                                       //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2006)                          //
//                                                                      //
// Client Socket with parallel streams and timeout features using XrdNet//
//                                                                      //
//////////////////////////////////////////////////////////////////////////

const char *XrdClientPSockCVSID = "$Id$";

#include <memory>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include "XrdClient/XrdClientPSock.hh"
#include "XrdOuc/XrdOucLogger.hh"
#include "XrdNet/XrdNetSocket.hh"
#include "XrdClient/XrdClientDebug.hh"
#include "XrdClient/XrdClientEnv.hh"

#ifndef WIN32
#include <unistd.h>
#include <sys/poll.h>
#else
#include "XrdSys/XrdWin32.hh"
#endif

//_____________________________________________________________________________
XrdClientPSock::XrdClientPSock(XrdClientUrlInfo Host, int windowsize):
    XrdClientSock(Host, windowsize) {

    fReinit_fd = true;
}

//_____________________________________________________________________________
XrdClientPSock::~XrdClientPSock()
{
   // Destructor
   Disconnect();
}

//_____________________________________________________________________________
int CloseSockFunc(int K, int V, void *arg) {
    ::close(V);
    
    // And also we delete this item by returning < 0
    return -1;
}
//_____________________________________________________________________________
void XrdClientPSock::Disconnect()
{
   // Close the connection

    if (fConnected) {

	// Make the SocketPool invoke the closing of all sockets
	fSocketPool.Apply( CloseSockFunc, 0 );

	//fSocketPool.Purge();

	fConnected = FALSE;
   }
}

//_____________________________________________________________________________
int FdSetSockFunc(int sockid, int sockdescr, void *arg) {
    fd_set *fds = (fd_set *)arg;
    
    if ( (sockdescr >= 0) )
	FD_SET(sockdescr, fds);

    // And we continue
    return 0;
}
//_____________________________________________________________________________
int XrdClientPSock::RecvRaw(void* buffer, int length, int substreamid,
			   int *usedsubstreamid)
{
   // Read bytes following carefully the timeout rules
   time_t starttime;
   int bytesread = 0;
   int selRet;
   fd_set rset;

   // We cycle reading data.
   // An exit occurs if:
   // We have all the data we are waiting for
   // Or a timeout occurs
   // The connection is closed by the other peer

   if (!fConnected) {
       Error("XrdClientPSock::RecvRaw", "Not connected.");
       return TXSOCK_ERR;
   }
   if (GetMainSock() < 0) {
       Error("XrdClientPSock::RecvRaw", "cannot find main socket.");
       return TXSOCK_ERR;
   }


   fReinit_fd = true;
   starttime = time(0);

   // Interrupt may be set by external calls via, e.g., Ctrl-C handlers
   fInterrupt = FALSE;
   while (bytesread < length) {




      // We cycle on the select, ignoring the possible interruptions
      // We are waiting for something to come from the socket(s)
      do { 

	  if (fReinit_fd) {
	      // Init of the fd_set thing
	      FD_ZERO(&rset);

	      // We are interested only in reading and errors

	      if (substreamid == -1) {
		  // We are interested in any sock
		  fSocketPool.Apply( FdSetSockFunc, (void *)&rset );
	      } else {
		  int sock = GetSock(substreamid);
		  FD_SET(sock, &rset);
	      }

	      fReinit_fd = false;
	  }

         // If too much time has elapsed, then we return an error
         if ((time(0) - starttime) > EnvGetLong(NAME_REQUESTTIMEOUT)) {

	    return TXSOCK_ERR_TIMEOUT;
         }

	 struct timeval tv = { 0, 100000 }; // .1 second as timeout step

	 // Wait for some events from the socket pool
	 selRet = select(FD_SETSIZE, &rset, NULL, NULL, &tv);

	 if ((selRet < 0) && (errno != EINTR)) {
	     Error("XrdClientSock::RecvRaw", "Error selecting from socket: " <<
		   ::strerror(errno));
	     return TXSOCK_ERR;
	 }

      } while (selRet <= 0 && !fInterrupt);

      // If we are here, selRet is > 0 why?
      //  Because the timeout and the select error are handled inside the previous loop
      // But we could have been requested to interrupt

      if (GetMainSock() < 0) {
         Error("XrdClientPSock::RecvRaw", "since we entered RecvRaw, the main socket "
	       "file descriptor has been removed.");
         return TXSOCK_ERR;
      }

      // If we have been interrupt, reset the interrupt and exit
      if (fInterrupt) {
         fInterrupt = FALSE;
         Error("XrdClientPSock::RecvRaw", "got interrupt");
         return TXSOCK_ERR_INTERRUPT;
      }

      // First of all, we check if there is something to read from any sock.
      // the first we find is ok for now
      for (int ii = 0; ii < FD_SETSIZE; ii++) {

	  if (FD_ISSET(ii, &rset)) {
	      int n = 0;
	      
	      n = ::recv(ii, static_cast<char *>(buffer) + bytesread,
			     length - bytesread, 0);

	      // If we read nothing, the connection has been closed by the other side
	      if (n <= 0) {
		  Error("XrdClientSock::RecvRaw", "Error reading from socket: " <<
			::strerror(errno));
		  return TXSOCK_ERR;
	      }

	      bytesread += n;
	      
	      // If we need to loop more than once to get the whole amount
	      // of requested bytes, then we have to select only on this fd which
	      // started providing a chunk of data
	      FD_ZERO(&rset);
	      FD_SET(ii, &rset);
	      if (usedsubstreamid) *usedsubstreamid = GetSockId(ii);

	      // We got some data, hence we stop scanning the fd list,
	      // but we remain stuck to the socket which started providing data
	      break;
	  }
      }

   } // while

   // Return number of bytes received
   // And also usedparsockid has been initialized with the sockid we got something from
   return bytesread;
}

int XrdClientPSock::SendRaw(const void* buffer, int length, int substreamid) {

    int sfd = GetSock(substreamid);
    return XrdClientSock::SendRaw(buffer, length, sfd);

}

//_____________________________________________________________________________
void XrdClientPSock::TryConnect(bool isUnix) {
    // Already connected - we are done.
    //
    if (fConnected) {
   	assert(GetMainSock() >= 0);
	return;
    }

    int s = TryConnect_low(isUnix);

    if (s >= 0) {
	int z = 0;
	fSocketPool.Rep(0, s);
	fSocketIdPool.Rep(s, z);
    }

}

int XrdClientPSock::TryConnectParallelSock() {

    int s = TryConnect_low();

    if (s >= 0) {
	int tmp = XRDCLI_PSOCKTEMP;
	fSocketPool.Rep(XRDCLI_PSOCKTEMP, s);
	fSocketIdPool.Rep(s, tmp);
    }

    return s;
}

int XrdClientPSock::EstablishParallelSock(int sockid) {

    int s = GetSock(XRDCLI_PSOCKTEMP);

    if (s >= 0) {
	fSocketPool.Del(XRDCLI_PSOCKTEMP);
	fSocketIdPool.Del(s);

	fSocketPool.Rep(sockid, s);
	fSocketIdPool.Rep(s, sockid);

	Info(XrdClientDebug::kUSERDEBUG,
	     "XrdClientSock::EstablishParallelSock", "Sockid " << sockid << " established.");

	return 0;
    }

    return -1;
}
