#ifndef __SEC_INTERFACE_H__
#define __SEC_INTERFACE_H__
/******************************************************************************/
/*                                                                            */
/*                    X r d S e c I n t e r f a c e . h h                     */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//       $Id$ 

#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>

/******************************************************************************/
/*  X r d S e c C r e d e n t i a l s   &   X r d S e c P a r a m e t e r s   */
/******************************************************************************/
  
// The following structure is used to pass security information back and forth
//
struct XrdSecBuffer
{
       int   size;
       char *buffer;

       XrdSecBuffer(char *bp=0, int sz=0) {buffer = membuf = bp; size=sz;}
      ~XrdSecBuffer() {if (membuf) free(membuf);}
private:
        char *membuf;
};

// When the buffer is used for credentials, the start of the buffer always
// holds the credential protocol name (e.g., krb4) as a string. The client
// will get credentials and the size will be filled out so that the contents
// of buffer can be easily transmitted to the server.
//
typedef XrdSecBuffer XrdSecCredentials;

// When the buffer is used for parameters, the contents must be interpreted
// in the context that it is used. For instance, the server will send the
// security configuration parameters on the initial login. The format differs
// from. say, the x.500 continuation paremeters that would be sent during
// PKI authentication via an "authmore" return status.
//
typedef XrdSecBuffer XrdSecParameters;
  
/******************************************************************************/
/*                      X r d S e c C l i e n t N a m e                       */
/******************************************************************************/

// This structure is returned during server authentication. The server may
// also return XrdSecParms indicating that additional authentication handshakes
// are need (see XrdProtocolsrvr for more information).
//
#define XrdSecPROTOIDSIZE 8

struct XrdSecClientName
{
       char   prot[XrdSecPROTOIDSIZE];  // Protocol used
       char   name[256];                // Maximum length name we will support!
       char   host[MAXHOSTNAMELEN];
       struct sockaddr hostaddr;
       char  *tident;                   // Trace identifier

       XrdSecClientName() {prot[0] = name[0] = host[0] = '\0'; tident=0;}
      ~XrdSecClientName() {}
};
  
/******************************************************************************/
/*                        X r d S e c P r o t o c o l                         */
/******************************************************************************/

// The XrdSecProtocol is used to generate client-side authentication credentials
// and to authenticate those credentials on the server. When a server indicates 
// that authentication is needed (i.e., it returns security parameters), the 
// client must call XrdSecCreateContext() to get an appropriate XrdSecProtocol
// (i.e., one specific to the authentication protocol that needs to be used). 
// Then the client can use the first form getCredentials() to generate the 
// appropriate identification information. On subsequent calls in response to
// "authmore" the client must use the second form, providing the additional
// parameters the the server sends. The server uses Authenticate() to verify
// the credentials. When XrdOucErrInfo is null (as it will usually be), error
// messages are routed to standard error. See XrdSecCreateContext().
//
class XrdOucErrInfo;

class XrdSecProtocol
{
public:

// > 0 -> parms  present (more authentication needed)
// = 0 -> client present (authentication suceeded)
// < 0 -> einfo  present (error has occured)
//
virtual int                Authenticate  (XrdSecCredentials  *cred,
                                          XrdSecParameters  **parms,
                                          XrdSecClientName   &client,
                                          XrdOucErrInfo      *einfo=0)=0;

virtual XrdSecCredentials *getCredentials(XrdSecParameters   *parm=0,
                                          XrdOucErrInfo      *einfo=0)=0;

virtual const char        *getParms(int &psize, const char *host=0)=0;

              XrdSecProtocol() {}
virtual      ~XrdSecProtocol() {}
};
 
/******************************************************************************/
/*                     X r d S e c G e t P r o t o c o l                      */
/******************************************************************************/

// The following external routine creates a secuyrity context and returns an
// XrdSecProtocol object to be used for authentication purposes. The caller
// provides the IP address of the remote connection along with the parameters
// provided by the server. A null return means an error occured. The message
// is sent to standard error unless and XrdOucErrInfo class is provided to
// capture the message. There should be one context per physical TCP/IP
// connection. When the connection is closed, XrdSecDelProtocol() should be
// called.
//
extern XrdSecProtocol *XrdSecGetProtocol(const struct sockaddr  &netaddr,
                                         const XrdSecParameters &parms,
                                               XrdOucErrInfo    *einfo=0);
 
/******************************************************************************/
/*                     X r d S e c D e l P r o t o c o l                      */
/******************************************************************************/
  
// When the client is through with a security context, it must call
// XrdSecDelProtocol() to free up the XrdSecProtocol object.
//
extern void XrdSecDelProtocol(XrdSecProtocol *secobj);
#endif
