#ifndef __SEC_PMANAGER_HH__
#define __SEC_PMANAGER_HH__
/******************************************************************************/
/*                                                                            */
/*                     X r d S e c P M a n a g e r . h h                      */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//       $Id$
  
#include "XrdOuc/XrdOucPthread.hh"

class XrdOucErrInfo;
class XrdSecProtList;
class XrdSecProtocol;

typedef unsigned long XrdSecPMask_t;

class XrdSecPManager
{
public:

XrdSecProtocol *Find(const         char    *pid,    // In
                     const         char    *parg=0);// In

XrdSecProtocol *Find(const         char  *pid,      // In
                                   char **parg,     // Out
                     XrdSecPMask_t       *pnum=0);  // Out

XrdSecProtocol *Get (      char    *sect);  // Out

XrdSecProtocol *Load(XrdOucErrInfo *eMsg,   // In
                     const char    *pid,    // In
                     const char    *parg,   // In
                     const char    *spath,  // In
                     const char     pmode); // In 'c' | 's'

void            setDebug(int dbg) {DebugON = dbg;}

                XrdSecPManager(int dbg=0)
                   {First = Last = 0; DebugON = dbg; protnum = 1;}
               ~XrdSecPManager() {}

private:

XrdSecProtocol    *Add(XrdOucErrInfo *erp,const char *pid,XrdSecProtocol *prot);
XrdSecPMask_t      protnum;
XrdOucMutex        myMutex;
XrdSecProtList    *First;
XrdSecProtList    *Last;
int                DebugON;
};
#endif
