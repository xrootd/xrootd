#ifndef __XRDCNS_LogServer__
#define __XRDCNS_LogServer__
/******************************************************************************/
/*                                                                            */
/*                    X r d C n s L o g S e r v e r . h h                     */
/*                                                                            */
/* (c) 2009 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
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

#include <sys/param.h>

class XrdOucTList;
class XrdCnsLogClient;
class XrdCnsLogFile;
  
class XrdCnsLogServer
{
public:

int  Init(XrdOucTList *rList);

void Run();

     XrdCnsLogServer();
    ~XrdCnsLogServer() {}


private:
void Massage(XrdCnsLogRec *lrP);

XrdCnsLogClient *Client;
XrdCnsLogFile   *logFile;

char             logDir[MAXPATHLEN+1];
char            *logFN;
};
#endif
