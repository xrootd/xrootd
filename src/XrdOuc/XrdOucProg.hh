#ifndef __OOUC_PROG__
#define __OOUC_PROG__
/******************************************************************************/
/*                                                                            */
/*                         X r d O u c P r o g . h h                          */
/*                                                                            */
/* (C) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*      All Rights Reserved. See XrdVersion.cc for complete License Terms     */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC03-76-SFO0515 with the Deprtment of Energy             */
/******************************************************************************/

//          $Id$

#include <sys/types.h>

class XrdOucError;
class XrdOucStream;

class XrdOucProg
{
public:

// When creating an Prog object, you may pass an optional error routing object.
// If you do so, error messages and all command output will be writen via the 
// error object. Otherwise, errors will be returned quietly.
//
            XrdOucProg(XrdOucError *errobj=0)
                      {eDest = errobj; ArgBuff = Arg[0] = 0; numArgs = 0;}

           ~XrdOucProg() {if (ArgBuff) free(ArgBuff);}

// Run executes the command that was passed via Setup(). You may pass
// up to four additional arguments that will be added to the end of any
// existing arguments. The ending status code of the program is returned.
//
int          Run(XrdOucStream *Sp,  char *arg1=0, char *arg2=0,
                                    char *arg3=0, char *arg4=0);

int          Run(char *arg1=0, char *arg2=0, char *arg3=0, char *arg4=0);

// Setup takes a command string, checks that the program is executable and
// sets up a parameter list structure.
// Zero is returned upon success, otherwise a -errno is returned,
//
int          Setup(char *prog, XrdOucError *errP=0);

/******************************************************************************/
  
private:
  XrdOucError *eDest;
  char        *ArgBuff;
  char        *Arg[64];
  int          numArgs;
  int          lenArgs;
};
#endif
