/*****************************************************************************/
/*                                                                           */
/*                         X r d O s s P r o x y . c c                       */
/*                                                                           */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*           Produced by Heinz Stockinger for Stanford University            */
/*****************************************************************************/

//         $Id$

const char *XrdOssProxyCVSID = "$Id$";

/*****************************************************************************/
/*                             i n c l u d e s                               */
/*****************************************************************************/

#include <pwd.h>

#include "XrdOss/XrdOssProxy.hh"
#include "XrdOss/XrdOssApi.hh"

/*****************************************************************************/
/*                 E r r o r   R o u t i n g   O b j e c t                   */
/*****************************************************************************/
  
XrdOucError OssProxyEroute(0, "proxy_");

extern XrdOssSys   XrdOssSS;
extern XrdOucError OfsEroute;
extern XrdOucTrace OssTrace;

/*****************************************************************************/
/*                               O p e n                                     */
/*****************************************************************************/

/**
 * Open the file 'path' in the mode indicated by 'mode'.
 *
 * Input:    path      - The fully qualified name of the file to open.
 *           oflag     - Standard open flags.
 *           mode      - Create mode (i.e., rwx).
 *           env       - Environmental information.
 *
 * Output:   XrdOssOK upon success; -errno otherwise.
 */
int XrdOssProxy::Open(const char *path, 
		      int         oflag, 
		      mode_t      mode, 
		      XrdOucEnv  &Env) 
{

  int           rc;

  client = new XrdXrClient(hostname_, port_, OfsEroute.logger());

  // Switch on debugging if server is started with -d
  //
  if (OssTrace.What > 0) {
    client->setDebug();
  }

  // Set the maximum time to wait. If the server wants the client to wait
  // longer than the time set here, the wait time is passed back to the caller
  //
  client->setMaxWaitTime(XrdOssSS.MaxTwiddle); 
  
  // We first need to login to the remote xrootd server
  //
  kXR_char role[1] = {kXR_useruser};
  if ( (rc = client->login((kXR_char*) getpwuid(getuid())->pw_name, 
			(kXR_char*) role)) ) {
    return rc;
  }
  
  // Once the login is done, we can try to open the remote file
  //
  return client->open((kXR_char*)path, (kXR_unt16) oflag, (kXR_unt16) mode);

} // Open

