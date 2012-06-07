#ifndef __XRDOUCN2NLOADER_HH__
#define __XRDOUCN2NLOADER_HH__
/******************************************************************************/
/*                                                                            */
/*                    X r d O u c N 2 N L o a d e r . h h                     */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
#include "XrdOuc/XrdOucName2Name.hh"

struct XrdVersionInfo;

class XrdOucN2NLoader
{
public:

XrdOucName2Name *Load(const char *libName, XrdVersionInfo &urVer);

                 XrdOucN2NLoader(XrdOucgetName2NameArgs)
                                : eRoute(eDest), cFN(confg),
                                  libParms((parms ? parms : "")),
                                  lclRoot(lroot), rmtRoot(rroot) {}
                ~XrdOucN2NLoader() {}

private:

XrdSysError *eRoute;
const char  *cFN;
const char  *libParms;
const char  *lclRoot;
const char  *rmtRoot;
};
#endif
