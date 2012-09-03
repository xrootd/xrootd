#ifndef __XrdRootdProtocol_H__
#define __XrdRootdProtocol_H__
/******************************************************************************/
/*                                                                            */
/*                   X r d R o o t d P r o t o c o l . h h                    */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/
 
#include "Xrd/XrdProtocol.hh"

/******************************************************************************/
/*                    x r d _ P r o t o c o l _ R o o t d                     */
/******************************************************************************/

class XrdSysError;
class XrdOucTrace;
class XrdLink;
class XrdScheduler;

class XrdRootdProtocol : XrdProtocol
{
public:

       void          DoIt() {}

       XrdProtocol  *Match(XrdLink *lp);

       int           Process(XrdLink *lp) {return -1;}

       void          Recycle(XrdLink *lp, int x, const char *y) {}

       int           Stats(char *buff, int blen, int do_sync);

                     XrdRootdProtocol(XrdProtocol_Config *pi,
                                 const char *pgm, const char **pap);
                    ~XrdRootdProtocol() {} // Never gets destroyed

private:

XrdScheduler      *Scheduler;
const char        *Program;
const char       **ProgArg;
XrdSysError       *eDest;
XrdOucTrace       *XrdTrace;
int                stderrFD;
int                ReadWait;
static int         Count;
static const char *TraceID;
};
#endif
