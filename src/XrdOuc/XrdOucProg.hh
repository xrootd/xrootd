#ifndef __OOUC_PROG__
#define __OOUC_PROG__
/******************************************************************************/
/*                                                                            */
/*                         X r d O u c P r o g . h h                          */
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

#include <sys/types.h>

class XrdSysError;
class XrdOucStream;

class XrdOucProg
{
public:

// When creating an Prog object, you may pass an optional error routing object.
// If you do so, error messages and all command output will be writen via the 
// error object. Otherwise, errors will be returned quietly.
//
            XrdOucProg(XrdSysError *errobj=0, int efd=-1)
                      : eDest(errobj), myStream(0), myProc(0), ArgBuff(0),
                        numArgs(0), theEFD(efd) {Arg[0] = 0;}

           ~XrdOucProg();

// Feed() send a data to the program started by Start(). Several variations
// exist to accomodate various needs. Note that should the program not be
// running when Feed() is called, it is restarted.
//
int Feed(const char *data[], const int dlen[]);

int Feed(const char *data, int dlen)
        {const char *myData[2] = {data, 0};
         const int   myDlen[2] = {dlen, 0};
         return Feed(myData, myDlen);
        }

int Feed(const char *data) {return Feed(data, (int)strlen(data));}

// getStream() returns the stream created by Start(). Use the object to get
// lines written by the started program.
//
XrdOucStream *getStream() {return myStream;}

// Run executes the command that was passed via Setup(). You may pass
// up to four additional arguments that will be added to the end of any
// existing arguments. The ending status code of the program is returned.
//
int          Run(XrdOucStream *Sp,  const char *arg1=0, const char *arg2=0,
                                    const char *arg3=0, const char *arg4=0);

int          Run(const char *arg1=0, const char *arg2=0,
                 const char *arg3=0, const char *arg4=0);

int          Run(char *outBuff, int outBsz,
                 const char *arg1=0, const char *arg2=0,
                 const char *arg3=0, const char *arg4=0);

// RunDone should be called to drain the output stream and get the ending
// status of the running process.
//
int          RunDone(XrdOucStream &cmd);

// Start executes the command that was passed via Setup(). The started
// program is expected to linger so that you can send directives to it
// via its standard in. Use Feed() to do this. If the output of the command
// is wanted, use getStream() to get the stream object and use it to read
// lines the program sends to standard out.
//
int          Start(void);

// Setup takes a command string and sets up a parameter list. If a Proc pointer
// is passed, then the command executes via that function. Otherwise, it checks
// that the program (first token) is executable.
// Zero is returned upon success, otherwise a -errno is returned,
//
int          Setup(const char *prog, 
                   XrdSysError *errP=0,
                   int (*Proc)(XrdOucStream *, char **, int)=0
                  );

/******************************************************************************/
  
private:
  int           Restart();
  XrdSysError  *eDest;
  XrdOucStream *myStream;
  int           (*myProc)(XrdOucStream *, char **, int);
  char         *ArgBuff;
  char         *Arg[64];
  int           numArgs;
  int           lenArgs;
  int           theEFD;
};
#endif
