#ifndef __XrdRootdProtocol_H__
#define __XrdRootdProtocol_H__
/******************************************************************************/
/*                                                                            */
/*                      r o o t d _ P r o t o c o l . h                       */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*      All Rights Reserved. See XrdVersion.cc for complete License Terms     */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//       $Id$
 
#include "Xrd/XrdProtocol.hh"

/******************************************************************************/
/*                    x r d _ P r o t o c o l _ R o o t d                     */
/******************************************************************************/

class XrdOucError;
class XrdOucTrace;
class XrdLink;
class XrdScheduler;

class XrdRootdProtocol : XrdProtocol
{
public:

       void          DoIt() {}

       XrdProtocol  *Match(XrdLink *lp);

       int           Process(XrdLink *lp) {return -1;}

       void          Recycle() {}

       int           Stats(char *buff, int blen);

       void          syncStats() {}

                     XrdRootdProtocol(XrdProtocol_Config *pi,
                                 const char *pgm, const char **pap);
                    ~XrdRootdProtocol() {} // Never gets destroyed

private:

XrdScheduler      *Scheduler;
const char        *Program;
const char       **ProgArg;
XrdOucError       *eDest;
XrdOucTrace       *XrdTrace;
int                stderrFD;
int                ReadWait;
static int         Count;
static const char *TraceID;
};
#endif
