/******************************************************************************/
/*                                                                            */
/*                         X r d O u c P r o g . c c                          */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC03-76-SFO0515 with the Deprtment of Energy             */
/******************************************************************************/

//          $Id$

const char *XrdOucProgCVSID = "$Id$";

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucProg.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include "XrdOuc/XrdOucStream.hh"

/******************************************************************************/
/*                                   R u n                                    */
/******************************************************************************/
  
int XrdOucProg::Run(XrdOucStream *Sp,char *arg1,char *arg2,char *arg3,char *arg4)
{
   const int maxArgs = sizeof(Arg)/sizeof(Arg[0]);
   char *myArgs[maxArgs];
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
   if (arg1 && j < maxArgs) Arg[j++] = arg1;
   if (arg2 && j < maxArgs) Arg[j++] = arg2;
   if (arg3 && j < maxArgs) Arg[j++] = arg3;
   if (arg4 && j < maxArgs) Arg[j++] = arg4;

// Make sure we don't have too many
//
   if (j >= maxArgs) 
      {if (eDest) eDest->Emsg("Run", E2BIG, (char *)"execute", Arg[0]);
       return -E2BIG;
      }
   Arg[j] = (char *)0;

// Execute the command
//
   if (Sp->Exec(Arg))
      {rc = Sp->LastError();
       if (eDest) eDest->Emsg("Run", rc, (char *)"execute", Arg[0]);
       return -rc;
      }

// All done, caller will drain output
//
   return 0;
}

/******************************************************************************/

int XrdOucProg::Run(char *arg1,char *arg2,char *arg3,char *arg4)
{
   XrdOucStream cmd;
   char *lp;
   int rc;

// Execute the command
//
   if ((rc = Run(&cmd, arg1, arg2, arg3, arg4))) return rc;

// Drain all output
//
   while((lp = cmd.GetLine()))
        if (eDest && *lp) eDest->Emsg("Run", (const char *)lp);

// Drain the command
//
   rc = cmd.Drain();

// Determine ending status
//
   if (WIFSIGNALED(rc))
      {if (eDest)
          {char buff[16];
           sprintf(buff, "%d", WTERMSIG(rc));
           eDest->Emsg("Run",(const char *)Arg[0],(char *)"killed by signal",buff);
          }
       return -EPIPE;
      }
   if (WIFEXITED(rc))
      {rc = WEXITSTATUS(rc);
       if (rc && eDest) 
          {char buff[16];
           sprintf(buff, "%d", rc);
           eDest->Emsg("Run",(const char *)Arg[0],(char *)"ended with status",buff);
          }
       return -rc;
      }
   return 0; // We'll assume all went well here
}

/******************************************************************************/
/*                                 S e t u p                                  */
/******************************************************************************/
  
int  XrdOucProg::Setup(char *prog, XrdOucError *errP)
{
   const int maxArgs = sizeof(Arg)/sizeof(Arg[0]);
   char *pp;
   int j;

// Prepare to handle the program
//
   if (ArgBuff) free(ArgBuff);
   pp = ArgBuff = strdup(prog);
   if (errP) eDest = errP;
  
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
      {if (errP) errP->Emsg("Run", E2BIG, (char *)"set up", Arg[0]);
       free(ArgBuff); ArgBuff = 0;
       return -E2BIG;
      }
   Arg[j] = (char *)0;
   numArgs= j;
   lenArgs = sizeof(Arg[0]) * numArgs;

// Make sure the program is really executable
//
   if (access((const char *)Arg[0], X_OK))
      {int rc = errno;
       if (errP) errP->Emsg("Run", rc, (char *)"set up", Arg[0]);
       free(ArgBuff); ArgBuff = 0;
       return rc;
      }
   return 0;
}
