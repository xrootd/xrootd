#ifndef _XRDOLB_TRACE_H
#define _XRDOLB_TRACE_H
/******************************************************************************/
/*                                                                            */
/*                        X r d O l b T r a c e . h h                         */
/*                                                                            */
/* (C) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC03-76-SFO0515 with the Deprtment of Energy             */
/******************************************************************************/

//         $Id$

#include "XrdOuc/XrdOucTrace.hh"

#ifndef NODEBUG

#include <iostream.h>
#include "XrdOuc/XrdOucTrace.hh"

#define TRACE_ALL   0x0000
#define TRACE_Debug 0x0001

#define DEBUG(y) if (XrdOlbTrace.What) \
                    {XrdOlbTrace.Beg(0,epname); cerr <<y; XrdOlbTrace.End();}

#else

#define DEBUG(x, y)

#endif
#endif
