#ifndef _OOUC_TRACE_H
#define _OOUC_TRACE_H
/******************************************************************************/
/*                                                                            */
/*                          o o u c _ T r a c e . h                           */
/*                                                                            */
/* (C) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*      All Rights Reserved. See XrdVersion.cc for complete License Terms     */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC03-76-SFO0515 with the Deprtment of Energy             */
/******************************************************************************/

//         $Id$

#include <iostream.h>

#include "XrdOuc/XrdOucError.hh"

class XrdOucTrace
{
public:

inline  void        Beg(const char *tid=0, const char *usr=0, const char *sid=0)
                       {eDest->TBeg(usr, tid, sid);}

inline  void        End() {eDest->TEnd();}

inline  int         Tracing(int mask) {return mask & What;}

        int         What;

                    XrdOucTrace(XrdOucError *erp) {eDest = erp; What = 0;}
                   ~XrdOucTrace() {}

static char *bin2hex(char *data, int dlen, char *buff=0);

private:
        XrdOucError *eDest;
};
#endif
