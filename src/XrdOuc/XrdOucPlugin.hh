#ifndef __XRDOUCPLUGIN__
#define __XRDOUCPLUGIN__
/******************************************************************************/
/*                                                                            */
/*                       X r d O u c P l u g i n . h h                        */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//       $Id$

class XrdOucError;

class XrdOucPlugin
{
public:

void *getPlugin(const char *pname, int errok=0);

      XrdOucPlugin(XrdOucError *erp, const char *path)
                  {eDest = erp; libPath = path; libHandle = 0;}
     ~XrdOucPlugin();

private:

XrdOucError *eDest;
const char  *libPath;
void        *libHandle;
};
#endif
