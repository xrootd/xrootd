#ifndef _ODC_CONFIG_H
#define _ODC_CONFIG_H
/******************************************************************************/
/*                                                                            */
/*                       X r d O d c C o n f i g . h h                        */
/*                                                                            */
/* (C) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC03-76-SFO0515 with the Deprtment of Energy             */
/******************************************************************************/

//          $Id$

#include "XrdOdc/XrdOdcConfDefs.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdOuc/XrdOuca2x.hh"
  
class XrdOucError;
class XrdOucStream;

/******************************************************************************/
/*                    X r d O d c C o n f i g   C l a s s                     */
/******************************************************************************/

#define ODC_FAILOVER 'f'
#define ODC_ROUNDROB 'r'
  
class XrdOdcConfig
{
public:

int           Configure(char *cfn, const char *mode);

int           ConWait;      // Seconds to wait for a manager connection
int           RepWait;      // Seconds to wait for manager replies
int           RepWaitMS;    // RepWait*1000 for poll()
int           RepDelay;     // Seconds to delay before retrying manager
int           RepNone;      // Max number of consecutive non-responses
int           msgKeep;      // Max message objects to keep

XrdOdcPselT   pselType;     // How ports are selected
int           portVec[maxPORTS];  // Port numbers to balance (max of 15)
int           pselSkey;     // Shared memory key for data recording
int           pselMint;     // Monitoring interval

char         *OLBPath;      // Path to the local olb for target nodes

XrdOucTList  *ManList;      // List of managers for remote redirection
XrdOucTList  *PanList;      // List of managers for proxy  redirection
unsigned char SMode;        // Manager selection mode
unsigned char SModeP;       // Manager selection mode (proxy)

      XrdOdcConfig(XrdOucError *erp, int port=0)
                  {ConWait = 10; RepWait = 6; RepWaitMS = 3000; RepDelay = 5;
                   ManList = PanList = 0; portVec[0] = 0;
                   SMode = SModeP = ODC_FAILOVER;
                   pselSkey = 1312; pselMint = 60;
                   eDest = erp; lclPort = port;
                   OLBPath = 0; RepNone = 8; msgKeep = 128;
                  }
     ~XrdOdcConfig();

private:
int ConfigProc(char *cfn);
int ConfigXeq(char *var, XrdOucStream &Config);
int xapath(XrdOucError *eDest, XrdOucStream &Config);
int xconw(XrdOucError *eDest, XrdOucStream &Config);
int xmang(XrdOucError *eDest, XrdOucStream &Config);
int xmsgk(XrdOucError *eDest, XrdOucStream &Config);
int xpbal(XrdOucError *eDest, XrdOucStream &Config);
int xpsel(XrdOucError *eDest, XrdOucStream &Config);
int xreqs(XrdOucError *eDest, XrdOucStream &Config);
int xtrac(XrdOucError *eDest, XrdOucStream &Config);

XrdOucError   *eDest;
int            lclPort;
};
#endif
