#ifndef __PROTOCOLSRVR_H__
#define __PROTOCOLSRVR_H__
/******************************************************************************/
/*                                                                            */
/*                 X r d S e c P r o t o c o l s r v r . h h                  */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
  
class XrdSecProtocolsrvr : public XrdSecProtocol
{
public:

// > 0 -> parms  present (more authentication needed)
// = 0 -> client present (authentication suceeded)
// < 0 -> einfo  present (error has occured)
//
int                     Authenticate  (XrdSecCredentials  *cred,    // In
                                       XrdSecParameters  **parms,   // Out
                                       XrdSecClientName   &client,  // Out
                                       XrdOucErrInfo      *einfo=0);// Out

XrdSecCredentials      *getCredentials(XrdSecParameters   *parm=0,
                                       XrdOucErrInfo      *einfo=0)
                        {return (XrdSecCredentials *)0;}

const char             *getParms(int &size, const char *hname=0);

int                     authImplied() {return implauth;}

int                     Configure(const char *cfn);

                        XrdSecProtocolsrvr(XrdOucLogger *lp);
                       ~XrdSecProtocolsrvr() {}      // Server is never deleted

private:

XrdOucError     eDest;
XrdOucTrace    *SecTrace;
XrdSecPManager  PManager;
XrdSecProtBind *bpFirst;
XrdSecProtBind *bpLast;
char           *STBuff;
int             STBlen;
int             Enforce;
int             implauth;

int             add2token(XrdOucError &erp, char *, char **, int &, unsigned long &);
int             ConfigFile(const char *cfn);
int             ConfigXeq(char *var, XrdOucStream &Config, XrdOucError &Eroute);
int             ProtBind_Complete(XrdOucError &Eroute);
int             xpamode(XrdOucStream &Config, XrdOucError &Eroute);
int             xpbind(XrdOucStream &Config, XrdOucError &Eroute);
int             xprot(XrdOucStream &Config, XrdOucError &Eroute);
int             xtrace(XrdOucStream &Config, XrdOucError &Eroute);

};
  
/******************************************************************************/
/*                    X r d S e c S e r v e r O b j e c t                     */
/******************************************************************************/

extern "C"
{
extern XrdSecProtocol *XrdSecProtocolsrvrObject(XrdOucLogger *lp, 
                                                const char *cfn);
}
#endif
