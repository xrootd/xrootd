/******************************************************************************/
/*                                                                            */
/*                         X r d O u c P r o g . c c                          */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC02-76-SFO0515 with the Deprtment of Energy             */
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
#ifndef WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#else
#include <sys/types.h>
#include "XrdSys/XrdWin32.hh"
#endif

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucProg.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdOucProg::~XrdOucProg()
{
   if (ArgBuff) free(ArgBuff);
   if (myStream) delete myStream;
}

/******************************************************************************/
/*                                  F e e d                                   */
/******************************************************************************/

int XrdOucProg::Feed(const char *data[], const int dlen[])
{
   static XrdSysMutex feedMutex;
   XrdSysMutexHelper  feedHelper;
   int rc;

// Make sure we have a stream
//
   if (!myStream) return EPIPE;
   feedHelper.Lock(&feedMutex);

// Check if this command is still running
//
   if (!myStream->isAlive() && !Restart())
      {if (eDest) eDest->Emsg("Prog" "Unable to restart", Arg[0]);
        return EPIPE;
      }

// Send the line to the program
//
   if (!myStream->Put((const char **)data, (const int *)dlen)) return 0;
   if (eDest) 
      eDest->Emsg("Prog", myStream->LastError(), "feed", Arg[0]);
   if ((rc = Restart()))
      {if (eDest) eDest->Emsg("Prog", rc, "restart", Arg[0]);
       return EPIPE;
      }
   if (!myStream->Put((const char **)data, (const int *)dlen)) return 0;
   if (eDest) 
      eDest->Emsg("Prog", myStream->LastError(), "refeed", Arg[0]);
   return EPIPE;
}
  
/******************************************************************************/
/*                                   R u n                                    */
/******************************************************************************/

#define runWithVec(strmP, theRC)\
                  const char *argV[4]; int argC = 0;\
                  if (arg1) argV[argC++] = arg1;\
                  if (arg2) argV[argC++] = arg2;\
                  if (arg3) argV[argC++] = arg3;\
                  if (arg4) argV[argC++] = arg4;\
                  theRC = Run(strmP, argV, argC)
  
int XrdOucProg::Run(XrdOucStream *Sp, const char *argV[], int argC,
                                      const char *envV[])
{
   const int maxArgs = sizeof(Arg)/sizeof(Arg[0])+4;
   char *myArgs[maxArgs+1];
   int rc, j = numArgs;

// If we have no program, return an error
//
   if (!ArgBuff) 
      {if (eDest) eDest->Emsg("Run", "No program specified");
       return -ENOEXEC;
      }

// Copy the arglist to our private area
//
   memcpy((void *)myArgs, (const void *)Arg, lenArgs);

// Append additional arguments as needed
//
   for (int i = 0; i < argC && j < maxArgs; i++)
       if (argV[i]) myArgs[j++] = (char *)argV[i];

// Make sure we don't have too many
//
   if (j >= maxArgs) 
      {if (eDest) eDest->Emsg("Run", E2BIG, "execute", Arg[0]);
       return -E2BIG;
      }
   myArgs[j] = (char *)0;

// If this is a local process then just execute it inline on this thread
//
   if (myProc) return (*myProc)(Sp, myArgs, j);

// Execute the command, possibly setting an environment.
//
   if (envV)
      {XrdOucEnv progEnv, *oldEnv = Sp->SetEnv(&progEnv);
       progEnv.PutPtr("XrdEnvars**", (void *)envV);
       rc = Sp->Exec(myArgs, 1, theEFD);
       Sp->SetEnv(oldEnv);
      } else rc = Sp->Exec(myArgs, 1, theEFD);

// Diagnose any errors
//
   if (rc)
      {rc = Sp->LastError();
       if (eDest) eDest->Emsg("Run", rc, "execute", Arg[0]);
       return -rc;
      }

// All done, caller will drain output
//
   return 0;
}

/******************************************************************************/

int XrdOucProg::Run(XrdOucStream *Sp, const char *arg1, const char *arg2,
                                      const char *arg3, const char *arg4)
{
   int rc;

// Execute the command
//
   runWithVec(Sp, rc);
   return rc;
}

/******************************************************************************/

int XrdOucProg::Run(const char *arg1, const char *arg2,
                    const char *arg3, const char *arg4)
{
   XrdOucStream cmd;
   char *lp;
   int rc;

// Execute the command
//
   runWithVec(&cmd, rc);
   if (rc) return rc;

// Drain all output
//
   while((lp = cmd.GetLine()))
        if (eDest && *lp) eDest->Emsg("Run", lp);

// All done
//
   return RunDone(cmd);
}

/******************************************************************************/

int XrdOucProg::Run(char *outBuff, int outBsz,
                    const char *arg1, const char *arg2,
                    const char *arg3, const char *arg4)
{
   XrdOucStream cmd;
   char *lp, *tp;
   int n, rc;

// Execute the command
//
   runWithVec(&cmd, rc);
   if (rc) return rc;

// Drain the first line to the output buffer
//
   if (outBuff && outBsz > 0)
      {if ((lp = cmd.GetLine()))
          {while (*lp && *lp == ' ') lp++;
           if ((n = strlen(lp)))
              {tp = lp+n-1;
               while(*tp-- == ' ') n--;
               if (n >= outBsz) n = outBsz-1;
               strncpy(outBuff, lp, n); outBuff += n;
              }
          }
       *outBuff = 0;
      }

// Drain remaining output
//
   while((lp = cmd.GetLine())) {}

// All done
//
   return RunDone(cmd);
}

/******************************************************************************/
/*                               R u n D o n e                                */
/******************************************************************************/

int XrdOucProg::RunDone(XrdOucStream &cmd)
{
   int rc;

// If this is an inline program then just return 0. There is no external process
// and the return code was returned at the time the inline process was run.
//
   if (myProc) return 0;

// Drain the command
//
   rc = cmd.Drain();

// Determine ending status
//
   if (WIFSIGNALED(rc))
      {if (eDest)
          {char buff[16];
           sprintf(buff, "%d", WTERMSIG(rc));
           eDest->Emsg("Run", Arg[0], "killed by signal", buff);
          }
       return -EPIPE;
      }
   if (WIFEXITED(rc))
      {rc = WEXITSTATUS(rc);
       if (rc && eDest) 
          {char buff[16];
           sprintf(buff, "%d", rc);
           eDest->Emsg("Run", Arg[0], "ended with status", buff);
          }
       return -rc;
      }
   return 0; // We'll assume all went well here
}

/******************************************************************************/
/*                                 S e t u p                                  */
/******************************************************************************/
  
int XrdOucProg::Setup(const char *prog, XrdSysError *errP,
                      int (*Proc)(XrdOucStream *, char **, int))
{
   const int maxArgs = sizeof(Arg)/sizeof(Arg[0]);
   char *pp;
   int j;

// Prepare to handle the program
//
   if (ArgBuff) free(ArgBuff);
   pp = ArgBuff = strdup(prog);
   if (!errP) errP = eDest;
  
// Construct the argv array based on passed command line.
//
for (j = 0; j < maxArgs-1 && *pp; j++)
    {while(*pp == ' ') pp++;
     if (!(*pp)) break;
     Arg[j] = pp;
     while(*pp && *pp != ' ') pp++;
     if (*pp) {*pp = '\0'; pp++;}
    }

// Make sure we did not overflow the buffer
//
   if (j == maxArgs-1 && *pp) 
      {if (errP) errP->Emsg("Run", E2BIG, "set up", Arg[0]);
       free(ArgBuff); ArgBuff = 0;
       return -E2BIG;
      }
   Arg[j] = (char *)0;
   numArgs= j;
   lenArgs = sizeof(Arg[0]) * numArgs;

// If this is a local process then just record its address
//
   if ((myProc = Proc)) return 0;

// Make sure the program is really executable
//
   if (access(Arg[0], X_OK))
      {int rc = errno;
       if (errP) errP->Emsg("Run", rc, "set up", Arg[0]);
       free(ArgBuff); ArgBuff = 0;
       return rc;
      }
   return 0;
}
 
/******************************************************************************/
/*                                 S t a r t                                  */
/******************************************************************************/
  
int XrdOucProg::Start()
{

// Create a stream for this command (it is an eror if we are already started)
//
   if (myStream) return EBUSY;
   if (!(myStream = new XrdOucStream(eDest))) return ENOMEM;

// Execute the command and let it linger
//
   theEFD = 0;
   return Run(myStream);
}
 
/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                               R e s t a r t                                */
/******************************************************************************/
  
int XrdOucProg::Restart()
{
   myStream->Close();
   return Run(myStream);
}
