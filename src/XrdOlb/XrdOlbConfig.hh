#ifndef _OLB_CONFIG_H_
#define _OLB_CONFIG_H_
/******************************************************************************/
/*                                                                            */
/*                       X r d O l d C o n f i g . h h                        */
/*                                                                            */
/* (C) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC03-76-SFO0515 with the Deprtment of Energy             */
/******************************************************************************/

//         $Id$

#include "XrdOlb/XrdOlbPList.hh"
#include "XrdOlb/XrdOlbTypes.hh"
#include "XrdOuc/XrdOucTList.hh"
  
class XrdOlbMeter;
class XrdOucError;
class XrdOucProg;
class XrdOucSecurity;
class XrdOucSocket;
class XrdOucStream;

class XrdOlbConfig
{
public:

int   Configure(int argc, char **argv);
int   ConfigXeq(char *var, XrdOucStream &Config, XrdOucError *eDest);
int   GenLocalPath(const char *oldp, char *newp);
int   GenRemotePath(const char *oldp, char *newp);
int   GenMsgID(char *oldmid, char *buff, int blen);
int   Master() {return isMaster;}
int   Slave()  {return isSlave;}

int         LUPDelay;     // Maximum delay at look-up
int         SUPCount;     // Minimum server count
int         SUPDelay;     // Maximum delay at start-up
int         MaxLoad;      // Maximum load
int         MaxDelay;     // Maximum load delay
int         MsgTTL;       // Maximum msg lifetime
int         RefReset;     // Min seconds between ref count reset
int         AskPerf;      // Seconds between perf queries
int         AskPing;      // Number of ping requests per AskPerf window
int         LogPerf;      // AskPerf intervals before logging perf

int         PortTCP;      // TCP Port to listen on
int         PortUDPm;     // UCP Port to listen on (master)
int         PortUDPs;     // UCP Port to listen on (slave )

int         P_cpu;        // % CPU Capacity in load factor
int         P_fuzz;       // %     Capacity to fuzz when comparing
int         P_io;         // % I/O Capacity in load factor
int         P_load;       // % MSC Capacity in load factor
int         P_mem;        // % MEM Capacity in load factor
int         P_pag;        // % PAG Capacity in load factor

int         DiskLinger;   // Master Only
int         DiskMin;      // Minimum KB needed of space
int         DiskAdj;      // KB to deduct from selected space
int         DiskWT;       // Seconds to defer client while waiting for space
int         DiskAsk;      // Seconds between disk inquiries
int         DiskSS;       // This is a staging server

int         sched_RR;     // 1 -> Simply do round robbin scheduling
int         doWait;       // 1 -> Wait for a data end-point

char        *LocalRoot;   // Slave Only
int          LocalRLen;
char        *RemotRoot;   // Slave Only
int          RemotRLen;
char        *MsgGID;
int          MsgGIDL;
char        *myName;
XrdOucTList *myMasters;

XrdOucProg  *ProgCH;      // Slave only chmod
XrdOucProg  *ProgMD;      // Slave only mkdir
XrdOucProg  *ProgMV;      // Slave only mv
XrdOucProg  *ProgRD;      // Slave only rmdir
XrdOucProg  *ProgRM;      // Slave only rm

XrdOlbPList_Anchor PathList;
XrdOlbMeter       *Meter;
XrdOucSocket      *AdminSock;
XrdOucSocket      *AnoteSock;

      XrdOlbConfig() {ConfigDefaults();}
     ~XrdOlbConfig();

private:

XrdOucSocket *ASocket(char *path, const char *fn, mode_t mode, int isudp=0);
int  concat_fn(const char *prefix, const int   pfxlen,
               const char *path,         char *buffer);
void ConfigDefaults(void);
int  ConfigProc(void);
int  isExec(XrdOucError *eDest, const char *ptype, char *prog);
int  PidFile(void);
int  setupMaster(void);
int  setupSlave(void);
void Usage(int rc);
int  xapath(XrdOucError *edest, XrdOucStream &Config);
int  xallow(XrdOucError *edest, XrdOucStream &Config);
int  xcache(XrdOucError *edest, XrdOucStream &Config);
int  Fsysadd(XrdOucError *edest, int chk, char *fn);
int  xdelay(XrdOucError *edest, XrdOucStream &Config);
int  xfsxq(XrdOucError *edest, XrdOucStream &Config);
int  xfxhld(XrdOucError *edest, XrdOucStream &Config);
int  xlclrt(XrdOucError *edest, XrdOucStream &Config);
int  xpath(XrdOucError *edest, XrdOucStream &Config);
int  xperf(XrdOucError *edest, XrdOucStream &Config);
int  xpidf(XrdOucError *edest, XrdOucStream &Config);
int  xping(XrdOucError *edest, XrdOucStream &Config);
int  xport(XrdOucError *edest, XrdOucStream &Config);
int  xprep(XrdOucError *edest, XrdOucStream &Config);
int  xrmtrt(XrdOucError *edest, XrdOucStream &Config);
int  xsched(XrdOucError *edest, XrdOucStream &Config);
int  xspace(XrdOucError *edest, XrdOucStream &Config);
int  xsubs(XrdOucError *edest, XrdOucStream &Config);
int  xthreads(XrdOucError *edest, XrdOucStream &Config);
int  xtrace(XrdOucError *edest, XrdOucStream &Config);

XrdOucSecurity   *Police;
XrdOucTList      *monPath;
char             *AdminPath;
int               AdminMode;
char             *pidPath;
char             *ConfigFN;
int               isMaster;
int               isSlave;
char             *perfpgm;
int               perfint;
int               cachelife;
int               pendplife;
};
#endif
