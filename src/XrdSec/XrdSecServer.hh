#ifndef __XRDSECSERVER_H__
#define __XRDSECSERVER_H__
/******************************************************************************/
/*                                                                            */
/*                       X r d S e c S e r v e r . h h                        */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//       $Id$

#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucLogger.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdSec/XrdSecPManager.hh"

class XrdSecProtBind;
class XrdOucTrace;
  
class XrdSecServer : XrdSecService
{
public:

const char             *getParms(int &size, const char *hname=0);

// = 0 -> No protocol can be returned (einfo has the reason)
// ! 0 -> Address of protocol object is bing returned.
//
XrdSecProtocol         *getProtocol(const char              *host,    // In
                                    const struct sockaddr   &hadr,    // In
                                    const XrdSecCredentials *cred,    // In
                                    XrdOucErrInfo           *einfo=0);// Out

int                     Configure(const char *cfn);

                        XrdSecServer(XrdOucLogger *lp);
                       ~XrdSecServer() {}      // Server is never deleted

private:

XrdOucError     eDest;
XrdOucTrace    *SecTrace;
XrdSecPManager  PManager;
XrdSecProtBind *bpFirst;
XrdSecProtBind *bpLast;
XrdSecProtBind *bpDefault;
char           *SToken;
char           *STBuff;
int             STBlen;
int             Enforce;
int             implauth;

int             add2token(XrdOucError &erp,char *,char **,int &,XrdSecPMask_t &);
int             ConfigFile(const char *cfn);
int             ConfigXeq(char *var, XrdOucStream &Config, XrdOucError &Eroute);
int             ProtBind_Complete(XrdOucError &Eroute);
int             xpbind(XrdOucStream &Config, XrdOucError &Eroute);
int             xpparm(XrdOucStream &Config, XrdOucError &Eroute);
int             xprot(XrdOucStream &Config, XrdOucError &Eroute);
int             xtrace(XrdOucStream &Config, XrdOucError &Eroute);
};
#endif
