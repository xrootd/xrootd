#ifndef __XRDOFSTPCAUTH_HH__
#define __XRDOFSTPCAUTH_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d O f s T P C A u t h . h h                       */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

#include <stdlib.h>
#include <string.h>
  
#include "XrdOfs/XrdOfsTPC.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdOfsTPCAuth : XrdOfsTPC
{
friend class XrdOfsTPCJob;
public:

        int   Add(Facts &Args);

        void  Del();

inline  int   Expired() {return (time(0) >= expT);}

        int   Expired(const char *Dst, int cnt=1);

static  int   Get(Facts &Args, XrdOfsTPCAuth **theTPC);

static  int   RunTTL(int Init);

              XrdOfsTPCAuth(int vTTL) : expT(vTTL+time(0)) {}

             ~XrdOfsTPCAuth() {}

private:

static XrdOfsTPCAuth *Find(Facts &Args);

static XrdSysMutex    authMutex;
static XrdOfsTPCAuth *authQ;
       XrdOfsTPCAuth *Next;
       time_t         expT;
};
#endif
