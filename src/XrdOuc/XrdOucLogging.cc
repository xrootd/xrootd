/******************************************************************************/
/*                                                                            */
/*                      X r d O u c L o g g i n g . c c                       */
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

#include <cerrno>
#include <cstdio>
#include <cstring>

#include "XrdVersion.hh"

#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucLogging.hh"
#include "XrdOuc/XrdOucPinLoader.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucUtils.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysLogging.hh"
#include "XrdSys/XrdSysLogPI.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                     T h r e a d   I n t e r f a c e s                      */
/******************************************************************************/

namespace
{
int   cseLvl = 0;
int   stdErr = 0;

struct dbgHdr
      {char theDate[6]; // yymmdd
       char sep1;       // single space
       char theHH[2];   // hh
       char colon1;     // :
       char theMM[2];   // mm
       char colon2;     // :
       char theSS[2];   // SS
      };

bool  BadHdr(dbgHdr *dLine)
      {if (dLine->sep1 != ' ' || dLine->colon1 != ':' || dLine->colon2 != ':')
          return true;

       if (dLine->theHH[0] < '0' || dLine->theHH[0] > '2'
       ||  dLine->theHH[1] < '0' || dLine->theHH[0] > '3'
       ||  dLine->theMM[0] < '0' || dLine->theMM[0] > '5'
       ||  dLine->theMM[1] < '0' || dLine->theMM[1] > '9'
       ||  dLine->theSS[0] < '0' || dLine->theSS[0] > '5'
       ||  dLine->theSS[1] < '0' || dLine->theSS[1] > '9') return true;

       for (int i = 0; i < 6; i++)
           if (dLine->theDate[i] < '0' || dLine->theDate[i] > '9') return true;

       return false;
      }

  
void *LoggingStdErr(void *carg)
      {XrdOucStream seStream;
       struct timeval seTime = {0,0};
       struct iovec   ioV;

       seStream.Attach(stdErr, 4096);
       do {if ((ioV.iov_base = seStream.GetLine()))
              {ioV.iov_len = strlen((const char *)ioV.iov_base);
               if (cseLvl == 1)
                  {if (ioV.iov_len < (int)sizeof(dbgHdr)
                   || BadHdr((dbgHdr *)ioV.iov_base)) continue;
                  }
               XrdSysLogging::Forward(seTime, 0, &ioV, 1);
              }
          } while(true);
       return 0;
      }
}

/******************************************************************************/
/*                             c o n f i g L o g                              */
/******************************************************************************/
  
bool XrdOucLogging:: configLog(XrdSysError &eDest,
                               XrdOucLogging::configLogInfo &logInfo)
{
   struct tmpstr {char *arg; char *arg2, *arg3;
                        tmpstr(const char *str) : arg(strdup(str)),
                                                  arg2(0), arg3(0) {}
                       ~tmpstr() {if (arg)  free(arg);
                                  if (arg2) free(arg2);
                                  if (arg3) free(arg3);
                                 }
                 };

   static XrdVERSIONINFODEF(myVersion, XrdLogConfig, XrdVNUMBER, XrdVERSION);
   XrdSysLogging::Parms logParms;
   char *logPI = 0, *logFN = 0;
   int argc;

// Check for stderr output
//
   if (!strcmp(logInfo.logArg, "-")) return true;
   tmpstr  opt(logInfo.logArg);

// Check if this specified a plugin
//
   if (*opt.arg == '@')
      {char *parms = index(opt.arg, ',');
       logPI = opt.arg+1;
       if (!(*logPI))
          {eDest.Emsg("Config", "Log plugin library not specified.");
           return false;
          }
       if (parms)
          {char *eol, *pval;
           int rc;
           opt.arg3 = strdup(parms); *parms = 0; parms = opt.arg3;
           if ((pval = varVal(",bsz=", parms, eol, ',')))
              {long long bsz;
               rc = XrdOuca2x::a2sz(eDest,"-l bsz",pval,&bsz,0,1048576);
               if (eol) *eol = ',';
               if (rc < 0) return false;
               if (bsz && bsz < 8192) bsz = 8192;
               logParms.bufsz = static_cast<int>(bsz);
              }
           if ((pval = varVal(",cse=", parms, eol, ',')))
              {rc = XrdOuca2x::a2i(eDest,"-l cse",pval,&cseLvl,0,2);
               if (eol) *eol = ',';
               if (rc < 0) return false;
              }
           logFN = varVal(",logfn=", parms, eol, ',');
          }
       } else logFN = opt.arg;

// Handle any logfile name
//
   if (logFN)
      {     if (*logFN == '=')
               {if (*(logFN+1) == '\0')
                   {eDest.Emsg("Config", "Logfile name not specified.");
                    return false;
                   }
                logParms.logfn = ++logFN;
               }
       else if (strcmp(logFN, "-"))
               {if (!(logFN = XrdOucUtils::subLogfn(eDest,logInfo.iName,strdup(logFN))))
                   return false;
                logParms.logfn = opt.arg2 = logFN;
               }
       else logParms.logfn = logFN;
      }

// Handle plugin, if any
//
   if (logPI)
      {XrdSysLogPInit_t logPInit;
       XrdOucPinLoader lpiLib(&eDest, &myVersion, "logging", logPI);
       char **lpiArgs = configLPIArgs(logInfo.xrdEnv, argc);
       if (!(logPInit = (XrdSysLogPInit_t)lpiLib.Resolve("XrdSysLogPInit")))
          {eDest.Emsg("Config","Unable to find logging plugin object in",logPI);
           lpiLib.Unload();
           return false;
          }
       if (!(logParms.logpi = (*logPInit)(logInfo.cfgFn, lpiArgs, argc)))
          {eDest.Emsg("Config", "Logging plugin initialization failed.");
           lpiLib.Unload();
           return false;
          }
      }

// Now complete logging configuration
//
   logParms.keepV = logInfo.keepV;
   logParms.hiRes = logInfo.hiRes;
   if (!XrdSysLogging::Configure(*(eDest.logger()), logParms))
      {eDest.Emsg("Config", "Log configuration failed.");
       return false;
      }

// Export the directory where the log file exists. We can modify the logfn
// as it should have been copied. Note logFN is the same as logParms.logfn.
//

   if (logFN && (logPI = rindex(logFN,'/'))) *(logPI+1) = '\0';
      else logParms.logfn = "./";
   XrdOucEnv::Export("XRDLOGDIR", logParms.logfn);

// If there is a plugin but alternate output has not been specified, then we
// must capture stderr output and feed it to the logging plugin.
//
   if (logPI && !logFN && cseLvl)
      {pthread_t tid;
       int pipeFD[2], dupStdErr = XrdSysFD_Dup(STDERR_FILENO);
       if (dupStdErr < 0 || XrdSysFD_Pipe(pipeFD) < 0
       ||  XrdSysFD_Dup2(pipeFD[1], STDERR_FILENO) < 0)
          {eDest.Emsg("Config",errno, "creating a pipe to capture stderr.");
           close(dupStdErr);
           return false;
          }
       close(pipeFD[1]);
       if (XrdSysThread::Run(&tid,LoggingStdErr,(void *)0,0,"stderr router"))
          {XrdSysFD_Dup2(dupStdErr, STDERR_FILENO);
           eDest.Emsg("Config", errno, "start stderr router");
           close(pipeFD[0]); close(dupStdErr); return false;
          }
       stdErr = pipeFD[0];
       close(dupStdErr);
      }


// All done
//
   return true;
}

/******************************************************************************/
/* Private:                c o n f i g L P I A r g s                          */
/******************************************************************************/
  
char **XrdOucLogging::configLPIArgs(XrdOucEnv *envP, int &argc)
{
   static char theLPI[] = {'l', 'o', 'g', 0};
   static char *dfltArgv[] = {0, 0};
   char       **lpiArgv = 0;

// Find our arguments, if any
//
   if (envP && (lpiArgv  = (char **)envP->GetPtr("xrdlog.argv**")))
      argc = envP->GetInt("xrdlog.argc");

// Verify that we have some and substitute if not
//
   if (!lpiArgv || argc < 1)
      {if (!envP || !(dfltArgv[0] = (char *)envP->GetPtr("argv[0]")))
          dfltArgv[0] = theLPI;
       lpiArgv = dfltArgv;
       argc = 1;
      }

// Return the argv pointer
//
   return lpiArgv;
}

/******************************************************************************/
/* Private:                       v a r V a l                                 */
/******************************************************************************/

char *XrdOucLogging::varVal(const char *var, char *line, char *&eol, char delim)
{
// Find variable in the line
//
   char *result = strstr(line, var);
   if (!result) return 0;

// Push up to the value and find the end
//
   result += strlen(var);
   if (!delim) eol = 0;
      else if ((eol = index(result, delim))) *eol = 0;

// All done
//
   return result;
}
