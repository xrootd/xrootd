//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientSock                                                     //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// Client Socket with timeout features using XrdNet                     //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

const char *XrdClientSockCVSID = "$Id$";

#include <memory>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include "XrdClient/XrdClientSock.hh"
#include "XrdOuc/XrdOucLogger.hh"
#include "XrdNet/XrdNetSocket.hh"
#include "XrdClient/XrdClientDebug.hh"
#include "XrdClient/XrdClientEnv.hh"

#include <sys/poll.h>

//_____________________________________________________________________________
XrdClientSock::XrdClientSock(XrdClientUrlInfo Host, int windowsize)
{
   // Constructor

   fHost.TcpHost = Host;
   fHost.TcpWindowSize = windowsize;
   fConnected = FALSE;
   fSocket = -1;
}

//_____________________________________________________________________________
XrdClientSock::~XrdClientSock()
{
   // Destructor
   Disconnect();
}

//_____________________________________________________________________________
void XrdClientSock::Disconnect()
{
   // Close the connection

   if (fConnected && fSocket >= 0) {
      ::close(fSocket);
      fConnected = FALSE;
      fSocket = -1;
   }
}

//_____________________________________________________________________________
int XrdClientSock::RecvRaw(void* buffer, int length)
{
   // Read bytes following carefully the timeout rules
   struct pollfd fds_r;
   time_t starttime;
   int bytesread = 0;
   int pollRet;

   // We cycle reading data.
   // An exit occurs if:
   // We have all the data we are waiting for
   // Or a timeout occurs
   // The connection is closed by the other peer

   // Init of the pollfd struct
   if (fSocket < 0) {
       Error("XrdClientSock::RecvRaw", "socket fd is " << fSocket);
       return TXSOCK_ERR;
   }

   fds_r.fd     = fSocket;
//   fds_r.events = POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL;
   fds_r.events = POLLIN;

   starttime = time(0);

   while (bytesread < length) {

      // We cycle on the poll, ignoring the possible interruptions
      // We are waiting for something to come from the socket
      do { 

         // If too much time has elapsed, then we return an error
         if ((time(0) - starttime) > EnvGetLong(NAME_REQUESTTIMEOUT)) {

            if (!EnvGetLong(NAME_GOASYNC) ||
                (DebugLevel() >= XrdClientDebug::kDUMPDEBUG)) //gEnv
               Info(XrdClientDebug::kNODEBUG,
		    "ClientSock::RecvRaw",
		    "Request timed out "<< EnvGetLong(NAME_REQUESTTIMEOUT) << //gEnv
		    "seconds reading " << length << " bytes" <<
		    " from server " << fHost.TcpHost.Host <<
		    ":" << fHost.TcpHost.Port);

	    return TXSOCK_ERR_TIMEOUT;
         }

         // Wait for some event from the socket
	 pollRet = poll(&fds_r,
			1,
			1000 // 1 second as a step
			);

	 if ((pollRet < 0) && (errno != EINTR)) return TXSOCK_ERR;

      } while (pollRet <= 0);

      // If we are here, pollRet is > 0 why?
      //  Because the timeout and the poll error are handled inside the previous loop

      if (fSocket < 0) {
         Error("XrdClientSock::RecvRaw", "since we entered RecvRaw, socket "
	       "file descriptor has changed to " << fSocket);
         return TXSOCK_ERR;
      }

      // First of all, we check if there is something to read
      if (fds_r.revents & (POLLIN | POLLPRI)) {
	 int n = read(fSocket, static_cast<char *>(buffer) + bytesread,
	 	      length - bytesread);

	 // If we read nothing, the connection has been closed by the other side
	 if (n <= 0) {
	    Error("XrdClientSock::RecvRaw", "Error reading from socket: " <<
	       ::strerror(errno));
	    return TXSOCK_ERR;
	 }

	 bytesread += n;
      }

      // Then we check if poll reports a complaint from the socket like disconnections
      if (fds_r.revents & (POLLERR | POLLHUP | POLLNVAL)) {
	 
	 Error( "ClientSock::RecvRaw",
		"Disconnection detected reading " << length <<
                " bytes from socket " << fds_r.fd <<
                " (server[" << fHost.TcpHost.Host <<
                ":" << fHost.TcpHost.Port <<
		"]). Revents=" << fds_r.revents );
	 return TXSOCK_ERR;
      }

   } // while

   // Return number of bytes received
   return bytesread;
}

//_____________________________________________________________________________
int XrdClientSock::SendRaw(const void* buffer, int length)
{
   // Write bytes following carefully the timeout rules
   // (writes will not hang)
   struct pollfd fds_w;
   time_t starttime;
   int byteswritten = 0;
   int pollRet;

   // Init of the pollfd structs. If fSocket is not valid... we can do this anyway
   fds_w.fd     = fSocket;
   fds_w.events = POLLOUT | POLLERR | POLLHUP | POLLNVAL;

   // We cycle until we write all we have to write
   // Or until a timeout occurs

   starttime = time(0);

   while (byteswritten < length) {

      do {
         // If too much time has elapsed, then we return an error
         if ( (time(0) - starttime) > EnvGetLong(NAME_REQUESTTIMEOUT) ) { //gEnv
	    Error( "ClientSock::SendRaw",
		   "Request timed out "<< EnvGetLong(NAME_REQUESTTIMEOUT) << //gEnv
		   "seconds writing " << length << " bytes" <<
		   " to server " << fHost.TcpHost.Host <<
		   ":" << fHost.TcpHost.Port);

	    return TXSOCK_ERR_TIMEOUT;
         }

	 // Wait for some event from the socket
	 pollRet = poll(&fds_w,
			1,
			1000 // 1 second as a step
			);

	 if ((pollRet < 0) && (errno != EINTR)) return TXSOCK_ERR;

      } while (pollRet <= 0);

      // If we are here, pollRet is > 0 why?
      //  Because the timeout and the poll error are handled inside the previous loop

      // First of all, we check if we are allowed to write
      if (fds_w.revents & POLLOUT) {
	 int n = write(fSocket, static_cast<const char *>(buffer) + byteswritten,
		       length - byteswritten);

	 // If we wrote nothing, the connection has been closed by the other
	 if (n <= 0) {
	    Error("ClientSock::SendRaw", "Error writing to a socket: " <<
	    	::strerror(errno));
	    return (TXSOCK_ERR);
	 }

	 byteswritten += n;
      }

      // Then we check if poll reports a complaint from the socket like disconnections
      if (fds_w.revents & (POLLERR | POLLHUP | POLLNVAL)) {

	 Error( "ClientSock::SendRaw",
		"Disconnection detected writing " << length <<
                " bytes to socket " << fds_w.fd <<
                " (server[" << fHost.TcpHost.Host <<
                ":" << fHost.TcpHost.Port <<
		"]). Revents=" << fds_w.revents );
	 
	 return TXSOCK_ERR;
      }

   } // while

   // Return number of bytes sent
   return byteswritten;
}

//_____________________________________________________________________________
void XrdClientSock::TryConnect(bool isUnix)
{
   // Already connected - we are done.
   //
   if (fConnected) {
   	assert(fSocket >= 0);
	return;
   }

   std::auto_ptr<XrdNetSocket> s(new XrdNetSocket());

   // Log the attempt
   //
   if (!isUnix) {
      Info(XrdClientDebug::kHIDEBUG, "ClientSock::TryConnect",
           "Trying to connect to" <<
           fHost.TcpHost.Host << "(" << fHost.TcpHost.HostAddr << "):" <<
           fHost.TcpHost.Port << " Timeout=" << EnvGetLong(NAME_CONNECTTIMEOUT));

      // Connect to a remote host
      //
      fSocket = s->Open(fHost.TcpHost.HostAddr.c_str(),
                        fHost.TcpHost.Port, EnvGetLong(NAME_CONNECTTIMEOUT));
   } else {
      Info(XrdClientDebug::kHIDEBUG, "ClientSock::TryConnect",
           "Trying to UNIX connect to" << fHost.TcpHost.File <<
           "; timeout=" << EnvGetLong(NAME_CONNECTTIMEOUT));

      // Connect to a remote host
      //
      fSocket = s->Open(fHost.TcpHost.File.c_str(), -1, EnvGetLong(NAME_CONNECTTIMEOUT));
   }

   // Check if we really got a connection and the remote host is available
   //
   if (fSocket < 0)  {
      if (isUnix) {
         Info(XrdClientDebug::kHIDEBUG, "ClientSock::TryConnect", "Connection to" <<
           fHost.TcpHost.File << " failed. (" << fSocket << ")");
      } else {
         Info(XrdClientDebug::kHIDEBUG, "ClientSock::TryConnect", "Connection to" <<
           fHost.TcpHost.Host << ":" << fHost.TcpHost.Port << " failed. (" << fSocket << ")");
      }
   } else {
      fConnected = TRUE;
      int detachedFD = s->Detach();
      if (fSocket != detachedFD) {
         Error("ClientSock::TryConnect",
               "Socket detach returned " << detachedFD << " but expected " << fSocket);
      }
   }
}
