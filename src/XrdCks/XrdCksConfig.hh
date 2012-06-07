#ifndef __XRDCKSCONFIG_HH__
#define __XRDCKSCONFIG_HH__
/******************************************************************************/
/*                                                                            */
/*                       X r d C k s C o n f i g . h h                        */
/*                                                                            */
/* (c) 2011 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

#include "XrdOuc/XrdOucTList.hh"

class XrdCks;
class XrdOucStream;
class XrdSysError;

struct XrdVersionInfo;
  
class XrdCksConfig
{
public:

XrdCks *Configure(const char *dfltCalc=0, int rdsz=0);

int     Manager() {return CksLib != 0;}

int     Manager(const char *Path, const char *Parms);

int     ParseLib(XrdOucStream &Config);

        XrdCksConfig(const char *cFN, XrdSysError *Eroute, int &aOK,
                     XrdVersionInfo *vInfo);
       ~XrdCksConfig() {XrdOucTList *tP;
                        if (CksLib)  free(CksLib);
                        if (CksParm) free(CksParm);
                        while((tP = CksList)) {CksList = tP->next; delete tP;}
                       }

private:
XrdCks      *getCks(int rdsz);

XrdSysError    *eDest;
const char     *cfgFN;
char           *CksLib;
char           *CksParm;
XrdOucTList    *CksList;
XrdOucTList    *CksLast;
XrdVersionInfo *myVersion;
};
#endif
