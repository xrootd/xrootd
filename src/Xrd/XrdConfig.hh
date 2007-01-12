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

class XrdNetSecurity;
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

int   ASocket(const char *path, const char *fname, mode_t mode);
int   ConfigProc(void);
int   getUG(char *parm, uid_t &theUid, gid_t &theGid);
int   setFDL();
int   Setup(char *dfltp);
void  UnderCover(void);
void  Usage(int rc);
int   xallow(XrdOucError *edest, XrdOucStream &Config);
int   xapath(XrdOucError *edest, XrdOucStream &Config);
int   xbuf(XrdOucError *edest, XrdOucStream &Config);
int   xnet(XrdOucError *edest, XrdOucStream &Config);
int   xlog(XrdOucError *edest, XrdOucStream &Config);
int   xport(XrdOucError *edest, XrdOucStream &Config);
int   xprot(XrdOucError *edest, XrdOucStream &Config);
int   xsched(XrdOucError *edest, XrdOucStream &Config);
int   xtrace(XrdOucError *edest, XrdOucStream &Config);
int   xtmo(XrdOucError *edest, XrdOucStream &Config);
int   yport(XrdOucError *edest, const char *ptyp, const char *pval);

static const char  *TraceID;

XrdProtocol_Config  ProtInfo;
XrdNetSecurity     *Police;
const char         *myProg;
const char         *myName;
const char         *myDomain;
const char         *myInsName;
char               *myInstance;
char               *AdminPath;
char               *ConfigFN;
char               *PidPath;
XrdConfigProt      *Firstcp;
XrdConfigProt      *Lastcp;
int                 Net_Blen;
int                 Net_Opts;

int                 PortTCP;      // TCP Port to listen on
int                 PortUDP;      // UDP Port to listen on (currently unsupported)
int                 AdminMode;
char                isProxy;
char                setSched;
};
#endif
