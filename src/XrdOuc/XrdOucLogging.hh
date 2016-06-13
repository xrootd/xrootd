#ifndef __XRDOUCLOGGING_HH__
#define __XRDOUCLOGGING_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d O u c L o g g i n g . h h                       */
/*                                                                            */
/* (c) 2016 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
  
class XrdSysError;
class XrdOucEnv;

class XrdOucLogging
{
public:

       struct configLogInfo
             {const char     *logArg;
              XrdOucEnv      *xrdEnv;
              const char     *iName;
              const char     *cfgFn;
              int             keepV;
              bool            hiRes;
              configLogInfo() : logArg(0), xrdEnv(0), iName(0), cfgFn(0),
                                keepV(1),  hiRes(false) {}
             };

static bool  configLog(XrdSysError &eDest, configLogInfo &logInfo);

       XrdOucLogging() {}
      ~XrdOucLogging() {}

private:
static char **configLPIArgs(XrdOucEnv *envP, int &argc);
static char  *varVal(const char *var, char *line, char *&eol, char delim);
};
#endif
