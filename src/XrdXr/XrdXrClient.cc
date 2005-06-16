/*****************************************************************************/
/*                                                                           */
/*                         X r d X r C l i e n t . c c                       */
/*                                                                           */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University */
/*      All Rights Reserved.  See XrdInfo.cc for complete License Terms      */
/*            Produced by Heinz Stockinger for Stanford University           */
/*****************************************************************************/

//         $Id$

const char *XrdXrClientCVSID = "$Id$";

/*****************************************************************************/
/*                             i n c l u d e s                               */
/*****************************************************************************/


#include <iostream.h>         
#include <arpa/inet.h>        // for htonl, htons, ntohl, ntohs
#include <unistd.h>           // for getpid
#include <stdlib.h>           // for free
#include <strings.h>          // for strdup

#include "XrdXr/XrdXrClient.hh"
#include "XrdXrootd/XrdXrootdProtocol.hh"
#include "XrdOuc/XrdOucPlatform.hh"

XrdOucLogger *XrdXrClient::logger = 0;


/*****************************************************************************/
/*                          c o n s t r u c t o r                            */
/*****************************************************************************/

/**
 * Constructor
 * 
 * Input:  hostname  - Hostname of the xrootd to connect
 *         port      - port number of the remote xrootd
 *         logger    - logger object
 */
XrdXrClient::XrdXrClient(const char* hostname, int port, XrdOucLogger *log) 
{
  // Assign the values needed for the connection to the remote xrootd
  //
  hostname_ = strdup(hostname);
  hostOrig_ = 0;
  port_ = portOrig_ = port;
  XrdXrClient::logger = log;

  fileInfo.open = false;

  maxRetry = maxWait = 10;
  maxWaitTime = 3;
 } // Constructor  

/*****************************************************************************/
/*                               l o g i n                                   */
/*****************************************************************************/

/**
 * Login to a remote xrootd server.
 *
 * Input:   username  - Unix id (username) to login at the server
 *                      The username can have a maximum of 8 characters.
 *          role      - role can have one of following two values:
 *                            kXR_useruser  = 0
 *			      kXR_useradmin = 1
 *          tlen      - length of the token supplied in the next parameter
 *          token     - Char token supplied by a previous redirection response
 *  
 * Output:  return 0 if OK, or an error number otherwise.
 */
int XrdXrClient::login(kXR_char      *username,
		       kXR_char       role[1],
		       kXR_int32      tlen,
		       kXR_char      *token)
{
  int rc;
 
  mutex.Lock();
  worker = new XrdXrClientWorker(hostname_,
				 port_,
				 logger);
  
  rc = worker->login(username, role, tlen, token);

  // If the login was correct, store the login information in the fileInfo 
  //
  if (!rc) {
    fileInfo.username = username;
    fileInfo.role[0] = role[0];
    fileInfo.tlen = tlen;
    fileInfo.token = token;
  }
  mutex.UnLock();
  return rc;

} // login


/*****************************************************************************/
/*                               l o g o u t                                 */
/*****************************************************************************/

/**
 * Log out from the remote xrootd server and close a potential open file.
 *
 * Output:  return 0 if OK, or an error number otherwise.
 */
int XrdXrClient::logout()
{
  int         rc     = 0;
  const char* epname = "logout";

  mutex.Lock();

  // Before we logout, check if there is a worker object and an open file that
  // we need to close first
  //
  if (worker) {

    if (fileInfo.open == true) {
      rc = worker->close();
    }

    // If all previous checks are ok, delete the worker object
    //
    if (rc == 0) {
      delete worker;
      TRACE(Logout, "logout ok.");
    }
    else {
      XrEroute.Emsg(epname, "Logout failed.");
      mutex.UnLock(); return -ECANCELED;
    }
  }
  mutex.UnLock();

  // If no worker object existed, just return 0 since no physical logout is 
  // required
  //
  return 0;

} // logout


/*****************************************************************************/
/*                               a u t h                                     */
/*****************************************************************************/

/**
 * Authenticate a client's username to a server
 *
 * Input:   credtype  - This can be a supported creditial type like
 *                         krb4 ... Kerberos 4
 *                         krb5 ... Kerberos 5
 *          cred      - User supplied credential
 * Output:  return 0 if OK, or an error number otherwise.
 */
int XrdXrClient::auth(kXR_char        credtype[4],
                      kXR_char       *cred)
{
  return worker->auth(credtype, cred);
} // auth

/*****************************************************************************/
/*                               o p e n                                     */
/*****************************************************************************/

/**
 * Open a remote file.
 * A possible redirection to another host is handled, too.
 *
 * Input:  path  - filepath of the remote file to be opened
 *         oflag - open flags such as: 0_RDONLY, 0_WRONLY, O_RDWR
 *                 Indicates if the file is opened for read, write or both.
 *         mode  - mode in which file will be opened when newly created
 *                 This corresponds to the file access permissions set at
 *                 file creation time.   
 *
 * Output: return 0 upon success; -errno otherwise.
 */
int XrdXrClient::open(kXR_char    *path,
		      kXR_unt16    oflag,
		      kXR_unt16    mode)
{
  int            status;
  int            rc;
  int            wait     = 0;          // wait count
  int            redirect = 0;          // redirect count
  int            cwTime;                // Wait time for client
  static const char  *epname = "open";

  // We try to open a remote file. Since we might get redirected to another
  // host, we handle that here. In addition, the server might not have all
  // information to satisfy the open request and thus sends a "wait". We handle
  // that, too. Both, the number of redirections and the waits are limited.
  //
  while((wait < maxWait) && (redirect < maxRetry)) {

    status = worker->open(path, oflag, mode);

    switch (status) {
    case -kXR_redirect:
      if ((rc = reconnect(epname))) { return rc;  }
      else                          { redirect++; break; }

    case -kXR_wait:
      if ((cwTime = handleWait(worker->getWaitTime(), epname)) == 0) 
         {wait++; break;}
      else {mutex.UnLock(); return cwTime;};

    case kXR_ok:
      // Fill in the fileInfo structure with the correct values
      //
      fileInfo.open = true;
      fileInfo.path = (kXR_char*) strdup((const char *)path);
      fileInfo.oflag = oflag;
      fileInfo.mode = mode;
      return 0;
      
    default:
      return mapError(static_cast<int>(status));
    }
  } // while

// If we exited the loop then either we exceeded the wait time or we
// exceeded the redirection count
//
   if (wait >= maxWait) return maxWaitTime;
   return -EMLINK;
} // open

/*****************************************************************************/
/*                               r e a d                                     */
/*****************************************************************************/

/**
 * Read 'blen' bytes from the associated file, placing in 'buff'
 * the data and returning the actual number of bytes read.
 * A possible redirection to another host is handled, too.
 *
 * Input:    buff      - Address of the buffer in which to place the data.
 *           offset    - The absolute 64-bit byte offset at which to read.
 *           blen      - The size of the buffer. This is the maximum number
 *                       of bytes that will be read.
 *
 * Output:  Returns the number of bytes read upon success and -errno upon 
 *          failure.
 */
ssize_t XrdXrClient::read(void       *buffer, 
			  kXR_int64   offset, 
			  kXR_int32   blen)
{
  ssize_t        rlen,received = 0;
  int                 rc;
  int                 wait     = 0;      // wait count
  int                 redirect = 0;      // redirect count
  static const char  *epname = "read";

  // Receive the current buffer and check how much still needs to be requested
  // from a new host in case we get redirected. The "new" read then starts 
  // from the offset relative to what was received before the redirection.
  //
  while(blen > 0 && (wait < maxWait) && (redirect < maxRetry)) {

    rlen = worker->read((char*) buffer+received, offset, blen);
   
    if (rlen > 0) {received += rlen; offset += rlen; blen -= rlen;}
       else switch (rlen) {
    case -kXR_redirect:
      if ((rc = reconnect(epname))) { return rc; }
      else {redirect++;

            // Open the file again at the new host
            //
            rc = open(fileInfo.path, fileInfo.oflag, fileInfo.mode);
            if (rc) {
                     XrEroute.Emsg(epname, "Reopen for redirection failed.");
                     return rc;
                    }
            break;
           }

    case -kXR_wait:
      if (handleWait(worker->getWaitTime(), epname) == 0) {wait++; break;} 
      else { return -EBUSY; }

    case 0:  return -ETIMEDOUT;

    default: return  mapError(static_cast<int>(rlen));
    } // switch
  } // while

  return received;
} // read

/*****************************************************************************/
/*                               s t a t                                     */
/*****************************************************************************/

/**
 *  Get file status.
 * 
 *  Input:  buffer    - buffer to hold the stat structure
 *          path      - Path of the file to get status: optional. By default,
 *                      the file currently open will be used
 *
  * Output:  return 0 upon success; -errno otherwise.
 */
int XrdXrClient::stat(struct stat *buffer,
		      kXR_char    *path)
{
  int            status;
  int            rc;
  int            wait     = 0;          // wait count
  int            redirect = 0;          // redirect count
  static const char  *epname = "stat";

  while((wait < maxWait) && (redirect < maxRetry)) {

    status = worker->stat(buffer, path);

    switch (status) {
    case -kXR_redirect:
      if ((rc = reconnect(epname))) { return rc;   } 
      else                          { redirect++; break; }

    case -kXR_wait:
      if (handleWait(worker->getWaitTime(), epname) == 0) {wait++; break;} 
      else { return -EBUSY; }

    case kXR_ok:
      return 0;
      
    default:
      return mapError(status);
    }
  } // while

  return 0;
} // stat

/*****************************************************************************/
/*                               c l o s e                                   */
/*****************************************************************************/

/**
 * Close a remote file
 *
 * Output: return 0 upon success; -errno otherwise.
 */
int XrdXrClient::close() 
{
  mutex.Lock();

  // Free the file path in case the file was opened and we stored the path
  //
  if (fileInfo.open == true) {
    free (fileInfo.path);
    fileInfo.open = false;
  }

  mutex.UnLock();
  return mapError(worker->close());
} // close


/*****************************************************************************/
/*                          d e s t r u c t o r                              */
/*****************************************************************************/

XrdXrClient::~XrdXrClient() 
{
  mutex.Lock();

  // delete all pointers
  //
  delete worker;
  free(hostname_);
  if (hostOrig_) free(hostOrig_);
  
  if (fileInfo.open == true) {
    free (fileInfo.path);
  }
  mutex.UnLock();
} // destructor

/*****************************************************************************/
/*                       P r i v a t e   m e t h o d s                       */
/*****************************************************************************/


/*****************************************************************************/
/*                             s e t H o s t                                 */
/*****************************************************************************/

/**
 * Set the current host to a new one.
 */
void XrdXrClient::setHost(char *host) 
{ 

  mutex.Lock();

  // If the current host is a the first redirector, we remember it's name
  // 
  if (hostOrig_ == NULL && (worker->getHostType() == kXR_LBalServer)) {
    hostOrig_ = strdup(hostname_);
  }

  // Set a new name to be used as the current host
  //
  free(hostname_);
  hostname_ = strdup(host);
  mutex.UnLock();
} // setHost


/*****************************************************************************/
/*                             r e c o n n e c t                             */
/*****************************************************************************/

/**
 *  Logout from the current host and login to the new one: redirection.
 */
int XrdXrClient::reconnect(const char        *epname)
{
  int rc;

  // Assign new hostname and port that we get from the ErrInfo object
  //
  setHost(worker->getRedirectHost()); 
  setPort(worker->getRedirectPort());
  
  TRACE(All, "Redirect " << epname << " to: " << hostname_ << ":" << port_);
  
  // Logout from the current server and login to the new one indicated
  // by the redirection response
  //
  if (logout() == 0) {
    rc = login(fileInfo.username, fileInfo.role);
    if (rc != 0) {
      // Login failed ... error is handled in the login request
      //
      setHost((char*) "NULL");     // unset the hostname
      setPort(0);                  // unset the port name
      XrEroute.Emsg(epname, "Redirection failed.");
      return rc; 
    } else {
      return 0;
    }
  } else { 
    setHost((char*) "NULL");       // unset the hostname
    setPort(0);                    // unset the port name
    XrEroute.Emsg(epname, "Redirection failed.");
    return -ECANCELED;
  }
} // reconnect


/*****************************************************************************/
/*                           h a n d l e W a i t                             */
/*****************************************************************************/

/**
 * Wait for a certain amount of time. If the wait time is greater than the 
 * maximum wait the client can accept, we just return the error code and do
 * not wait.
 *
 * Input:   waitTime - time to wait as requested by the server
 *          epname   - name of the caller method
 *
 * Output:  0 on success; waittime if wait time is longer than the max wait time
 */
int XrdXrClient::handleWait(int waitTime, 
			    const char *epname)
{
  if (waitTime > maxWait) {
    TRACE(All, "Need to wait " << waitTime << 
          " seconds - longer than max. wait time.");
    return waitTime;
  } else {
    TRACE(All, "Waiting for " << waitTime << " seconds.");
    sleep(waitTime); 
  }
  return 0;
} // waitTime

/******************************************************************************/
/*                              m a p E r r o r                               */
/******************************************************************************/
  
int XrdXrClient::mapError(int rc)
{
    if (rc < 0) rc = -rc;
    switch(rc)
       {case 0:                  return  0;
        case kXR_ArgInvalid:     return -EINVAL;
        case kXR_ArgMissing:     return -EINVAL;
        case kXR_ArgTooLong:     return -ENAMETOOLONG;
        case kXR_FileLocked:     return -EDEADLOCK;
        case kXR_FileNotOpen:    return -EBADF;
        case kXR_FSError:        return -EIO;
        case kXR_InvalidRequest: return -ESPIPE;
        case kXR_IOError:        return -EIO;
        case kXR_NoMemory:       return -ENOMEM;
        case kXR_NoSpace:        return -ENOSPC;
        case kXR_NotAuthorized:  return -EACCES;
        case kXR_NotFound:       return -ENOENT;
        case kXR_ServerError:    return -ECANCELED;
        case kXR_Unsupported:    return -ENOTSUP;
        case kXR_noserver:       return -ENETUNREACH;
        case kXR_NotFile:        return -ENOTBLK;
        case kXR_isDirectory:    return -EISDIR;
        default:                 return -ENOMSG;
       }
}
