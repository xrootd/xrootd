#ifndef _XRD_CONFIG_H
#define _XRD_CONFIG_H
/******************************************************************************/
/*                                                                            */
/*                          X r d C o n f i g . h h                           */
/*                                                                            */
/* (C) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC03-76-SFO0515 with the Deprtment of Energy             */
/******************************************************************************/

//          $Id$ 

#include "Xrd/XrdProtocol.hh"

class XrdOucSecurity;
class XrdOucStream;
class XrdConfigProt;

class XrdConfig
{
public:

int   Configure(int argc, char **argv);

int   ConfigXeq(char *var, XrdOucStream &Config, XrdOucError *eDest=0);

      XrdConfig();
     ~XrdConfig() {}

private:

int   ASocket(const char *path,const char *dname,const char *fname,mode_t mode);
int   ConfigProc(void);
int   PidFile(char *pname);
int   setFDL();
int   Setup(char *dfltp);
void  Usage(char *mprg, int rc);
int   xallow(XrdOucError *edest, XrdOucStream &Config);
int   xapath(XrdOucError *edest, XrdOucStream &Config);
int   xbuf(XrdOucError *edest, XrdOucStream &Config);
int   xcon(XrdOucError *edest, XrdOucStream &Config);
int   xnet(XrdOucError *edest, XrdOucStream &Config);
int   xpidf(XrdOucError *edest, XrdOucStream &Config);
int   xport(XrdOucError *edest, XrdOucStream &Config);
int   xprot(XrdOucError *edest, XrdOucStream &Config);
char *xprotparms(XrdOucError *eDest, XrdOucStream &Config);
int   xsched(XrdOucError *edest, XrdOucStream &Config);
int   xtrace(XrdOucError *edest, XrdOucStream &Config);
int   xtmo(XrdOucError *edest, XrdOucStream &Config);
int   yport(XrdOucError *edest, const char *ptyp, char *pval);

static const char  *TraceID;

XrdProtocol_Config  ProtInfo;
XrdOucSecurity     *Police;
char               *myName;
char               *myDomain;
char               *AdminPath;
int                 AdminMode;
char               *ConfigFN;
char               *PidPath;
XrdConfigProt      *Firstcp;
XrdConfigProt      *Lastcp;
int                 Net_Blen;
int                 Net_Opts;

int                 PortTCP;      // TCP Port to listen on
int                 PortUDP;      // UDP Port to listen on (currently unsupported)
char                isProxy;
char                setSched;
};
#endif
