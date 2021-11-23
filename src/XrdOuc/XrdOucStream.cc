/******************************************************************************/
/*                                                                            */
/*                       X r d O u c S t r e a m . c c                        */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Deprtment of Energy               */
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

#include <cctype>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#ifndef WIN32
#include <poll.h>
#include <unistd.h>
#include <strings.h>
#if !defined(__linux__) && !defined(__CYGWIN__) && !defined(__GNU__)
#include <sys/conf.h>
#endif
#include <sys/stat.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#else // WIN32
#include "XrdSys/XrdWin32.hh"
#include <process.h>
#endif // WIN32

#include <set>
#include <string>

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucNSWalk.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                         l o c a l   d e f i n e s                          */
/******************************************************************************/
  
#define MaxARGC 64
#define XrdOucStream_EOM  0x01
#define XrdOucStream_BUSY 0x02
#define XrdOucStream_ELIF 0x80

#define XrdOucStream_CADD 0x010000
#define XrdOucStream_CONT 0xff0000
#define XrdOucStream_CMAX 0x0f0000

#define Erq(p, a, b) Err(p, a, b, (char *)0)
#define Err(p, a, b, c) (ecode=(Eroute ? Eroute->Emsg(#p, a, b, c) : a), -1)
#define Erp(p, a, b, c)  ecode=(Eroute ? Eroute->Emsg(#p, a, b, c) : a)

// The following is used by child processes prior to exec() to avoid deadlocks
//
#define Erx(p, a, b) cerr <<#p <<": " <<XrdSysE2T(a) <<' ' <<b <<endl;

/******************************************************************************/
/*              S t a t i c   M e m b e r s   &   O b j e c t s               */
/******************************************************************************/
  
// The following mutex is used to allow only one fork at a time so that
// we do not leak file descriptors. It is a short-lived lock.
//
namespace {XrdSysMutex forkMutex;}

XrdOucString *XrdOucStream::theCFG = 0;

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/

struct StreamInfo
      {char *myHost;
       char *myName;
       char *myExec;

       std::set<std::string> *fcList;
       std::set<std::string>::iterator itFC;

       StreamInfo() : myHost(0), myName(0), myExec(0),
                      fcList(0) {}
      ~StreamInfo() {if (fcList) delete fcList;}
      };

namespace
{
class contHandler
{
public:

char        *path;
XrdOucTList *tlP;

void  Add(const char *sfx) {tlP = new XrdOucTList(sfx,(int)strlen(sfx),tlP);}

      contHandler() : path(0), tlP(0) {}
     ~contHandler() {XrdOucTList *tlN;
                     while(tlP) {tlN = tlP; tlP = tlP->next; delete tlN;}
                     if (path) free(path);
                    }
};
}
  
/******************************************************************************/
/*                       L o c a l   F u n c t i o n s                        */
/******************************************************************************/

namespace
{
bool KeepFile(const char *fname, XrdOucTList *tlP)
{
   struct sfxList {const char *txt; int len;};
   static sfxList sfx[] = {{".cfsaved",    8},
                           {".rpmsave",    8},
                           {".rpmnew",     7},
                           {".dpkg-old",   9},
                           {".dpkg-dist", 10},
                           {"~",           1}
                          };
   static int sfxLNum = sizeof(sfx)/sizeof(struct sfxList);
   int n;

// We don't keep file that start with a dot
//
   if (*fname == '.') return false;
   n = strlen(fname);

// Process white list first, otherwise use the black list
//
   if (tlP)
      {while(tlP)
            {if (tlP->ival[0] < n && !strcmp(tlP->text, fname+n-tlP->ival[0]))
                return true;
             tlP = tlP->next;
            }
       return false;
      }

// Check all other suffixes we wish to avoid
//
   for (int i = 0; i < sfxLNum; i++)
       {if (sfx[i].len < n && !strcmp(sfx[i].txt, fname+n-sfx[i].len))
           return false;
       }

// This file can be kept
//
   return true;
}
}

/******************************************************************************/
/*               o o u c _ S t r e a m   C o n s t r u c t o r                */
/******************************************************************************/
  
XrdOucStream::XrdOucStream(XrdSysError *erobj, const char *ifname,
                           XrdOucEnv   *anEnv, const char *Pfx)
{
 char *cp;


     if (ifname)
        {myInst = strdup(ifname);
         myInfo = new StreamInfo;
         if (!(cp = index(myInst, ' '))) {cp = myInst; myInfo->myExec = 0;}
            else {*cp = '\0'; cp++;
                  myInfo->myExec = (*myInst ? myInst : 0);
                 }
         if ( (myInfo->myHost = index(cp, '@')))
            {*(myInfo->myHost) = '\0';
             myInfo->myHost++;
             myInfo->myName = (*cp ? cp : 0);
            } else {myInfo->myHost = cp; myInfo->myName = 0;}
        } else {myInst = 0; myInfo = 0;}
     myRsv1 = myRsv2 = 0;

     FD     = -1;
     FE     = -1;
     bsize  = 0;
     buff   = 0;
     bnext  = 0;
     bleft  = 0;
     recp   = 0;
     token  = 0;
     flags  = 0;
     child  = 0;
     ecode  = 0;
     notabs = 0;
     xcont  = 1;
     xline  = 0;
     Eroute = erobj;
     myEnv  = anEnv;
     sawif  = 0;
     skpel  = 0;
     if (myEnv && Eroute)
        {llBuff = (char *)malloc(llBsz);
         llBcur = llBuff; llBok = 0; llBleft = llBsz; *llBuff = '\0';
         Verbose= 1;
        } else {
         Verbose= 0;
         llBuff = 0;
         llBcur = 0;
         llBleft= 0;
         llBok  = 0;
        }
     varVal = (myEnv ? new char[maxVLen+1] : 0);
     llPrefix = Pfx;
}

/******************************************************************************/
/*                                A t t a c h                                 */
/******************************************************************************/

int XrdOucStream::AttachIO(int infd, int outfd, int bsz)
{
    if (Attach(infd, bsz)) return -1;
    FE = outfd;
    return 0;
}
  
int XrdOucStream::Attach(int FileDescriptor, int bsz) 
{

    // Close the current stream. Close will handle unopened streams.
    //
    StreamInfo *saveInfo = myInfo; myInfo = 0;
    Close();
    myInfo = saveInfo;

    // Allocate a new buffer for this stream
    //
    if (!bsz) buff = 0;
       else if (!(buff = (char *)malloc(bsz+1)))
               return Erq(Attach, errno, "allocate stream buffer");

    // Initialize the stream
    //
    FD= FE = FileDescriptor;
    bnext  = buff;
    bsize  = bsz+1;
    bleft  = 0;
    recp   = 0;
    token  = 0;
    flags  = 0;
    ecode  = 0;
    xcont  = 1;
    xline  = 0;
    sawif  = 0;
    skpel  = 0;
    if (llBuff) 
       {llBcur = llBuff; *llBuff = '\0'; llBleft = llBsz; llBok = 0;}
    return  0;
}
  
/******************************************************************************/
/*                               C a p t u r e                                */
/******************************************************************************/

void XrdOucStream::Capture(const char **cVec, bool linefeed)
{
// Make sure we can handle this
//
   if (theCFG && cVec && cVec[0])
      {if (linefeed) theCFG->append("\n# ");
          else theCFG->append("# ");
       int i = 0;
       while(cVec[i]) theCFG->append(cVec[i++]);
       theCFG->append('\n');
      }
}

/******************************************************************************/

XrdOucString *XrdOucStream::Capture(XrdOucString *newCFG)
{
   XrdOucString *oldCFG = theCFG;
   theCFG = newCFG;
   return oldCFG;
}

/******************************************************************************/

XrdOucString *XrdOucStream::Capture()
{
   return theCFG;
}

/******************************************************************************/
/*                                 C l o s e                                  */
/******************************************************************************/

void XrdOucStream::Close(int hold)
{

    // Wait for any associated process on this stream
    //
    if (!hold && child) Drain();
       else child = 0;

    // Close the associated file descriptor if it was open
    //
    if (FD >= 0)             close(FD);
    if (FE >= 0 && FE != FD) close(FE);

    // Release the buffer if it was allocated.
    //
    if (buff) free(buff);

    // Clear all data values by attaching a dummy FD
    //
    FD = FE = -1;
    buff = 0;

    // Check if we should echo the last line
    //
    if (llBuff)
       {if (Verbose && *llBuff && llBok > 1)
           {if (Eroute) Eroute->Say(llPrefix, llBuff);
            if (theCFG) add2CFG(llBuff);
           }
        llBok = 0;
       }

    // Delete any info object we have allocated
    //
    if (myInfo)
       {delete myInfo;
        myInfo = 0;
       }
}

/******************************************************************************/
/*                                 D r a i n                                  */
/******************************************************************************/
  
int XrdOucStream::Drain() 
{
    int Status = 0;

    // Drain any outstanding processes (i.e., kill the process group)
    //
#ifndef WIN32
    int retc;
    if (child) {kill(-child, 9);
                do {retc = waitpid(child, &Status, 0);}
                    while(retc > 0 || (retc == -1 && errno == EINTR));
                child = 0;
               }
#else
    if (child) {
       TerminateProcess((HANDLE)child, 0);
       child = 0;
    }
#endif
    return Status;
}
  
/******************************************************************************/
/*                                  E c h o                                   */
/******************************************************************************/

void XrdOucStream::Echo()
{
   if (llBok > 1 && Verbose && llBuff)
      {if (Eroute) Eroute->Say(llPrefix,llBuff);
       if (theCFG) add2CFG(llBuff);
      }
   llBok = 0;
}

/******************************************************************************/
/*                              E c h o O n l y                               */
/******************************************************************************/

void XrdOucStream::Echo(bool capture)
{
   if (llBok && Verbose && llBuff)
      {if (Eroute) Eroute->Say(llPrefix,llBuff);
       if (capture && theCFG) add2CFG(llBuff);
      }
   llBok = 0;
}
  
/******************************************************************************/
/*                               E   x   e   c                                */
/******************************************************************************/
  
int XrdOucStream::Exec(const char *theCmd, int inrd, int efd)
{
    int j;
    char *cmd, *origcmd, *parm[MaxARGC];

    // Allocate a buffer for the command as we will be modifying it
    //
    origcmd = cmd = (char *)malloc(strlen(theCmd)+1);
    strcpy(cmd, theCmd);
  
    // Construct the argv array based on passed command line.
    //
    for (j = 0; j < MaxARGC-1 && *cmd; j++)
        {while(*cmd == ' ') cmd++;
         if (!(*cmd)) break;
         parm[j] = cmd;
         while(*cmd && *cmd != ' ') cmd++;
         if (*cmd) {*cmd = '\0'; cmd++;}
        }
    parm[j] = (char *)0;

    // Continue with normal processing
    //
    j = Exec(parm, inrd, efd);
    free(origcmd);
    return j;
}

int XrdOucStream::Exec(char **parm, int inrd, int efd)
{
    int fildes[2], Child_in = -1, Child_out = -1, Child_log = -1;

    // Create a pipe. Minimize file descriptor leaks.
    //
    if (inrd >= 0)
       {if (pipe(fildes))
           return Err(Exec, errno, "create input pipe for", parm[0]);
           else {
                 fcntl(fildes[0], F_SETFD, FD_CLOEXEC);
                 Attach(fildes[0]); Child_out = fildes[1];
                }

        if (inrd)
           {if (pipe(fildes))
               return Err(Exec, errno, "create output pipe for", parm[0]);
               else {
                     fcntl(fildes[1], F_SETFD, FD_CLOEXEC);
                     FE = fildes[1]; Child_in  = fildes[0];
                    }
           }
       } else {Child_out = FD; Child_in = FE;}

    // Handle the standard error file descriptor
    //
    if (!efd) Child_log = (Eroute ? dup(Eroute->logger()->originalFD()) : -1);
       else if (efd  >  0) Child_log = efd;
       else if (efd == -2){Child_log = Child_out; Child_out = -1;}
       else if (efd == -3) Child_log = Child_out;

    // Fork a process first so we can pick up the next request. We also
    // set the process group in case the child hasn't been able to do so.
    // Make sure only one fork occurs at any one time (we are the only one).
    //
    forkMutex.Lock();
    if ((child = fork()))
       {if (child < 0)
           {close(Child_in); close(Child_out); forkMutex.UnLock();
            return Err(Exec, errno, "fork request process for", parm[0]);
           }
                  close(Child_out);
        if (inrd) close(Child_in );
        if (!efd && Child_log >= 0) close(Child_log);
        forkMutex.UnLock();
        setpgid(child, child);
        return 0;
       }

    /*****************************************************************/
    /*                  C h i l d   P r o c e s s                    */
    /*****************************************************************/

    // Redirect standard in if so requested
    //
    if (Child_in >= 0)
       {if (inrd)
           {if (dup2(Child_in, STDIN_FILENO) < 0)
               {Erx(Exec, errno, "setting up standard in for " <<parm[0]);
                _exit(255);
               } else if (Child_in != Child_out) close(Child_in);
           }
       }

    // Reassign the stream to be standard out to capture all of the output.
    //
    if (Child_out >= 0)
       {if (dup2(Child_out, STDOUT_FILENO) < 0)
           {Erx(Exec, errno, "setting up standard out for " <<parm[0]);
            _exit(255);
           } else if (Child_out != Child_log) close(Child_out);
       }

    // Redirect stderr of the stream if we can to avoid keeping the logfile open
    //
    if (Child_log >= 0)
       {if (dup2(Child_log, STDERR_FILENO) < 0)
           {Erx(Exec, errno, "set up standard err for " <<parm[0]);
            _exit(255);
           } else close(Child_log);
       }

    // Check if we need to set any envornment variables
    //
    if (myEnv)
       {char **envP;
        int i = 0;
        if ((envP = (char **)myEnv->GetPtr("XrdEnvars**")))
           while(envP[i]) {putenv(envP[i]); i++;}
       }

    // Set our process group (the parent should have done this by now) then
    // invoke the command never to return
    //
    setpgid(0,0);
    execv(parm[0], parm);
    Erx(Exec, errno, "executing " <<parm[0]);
    _exit(255);
}

/******************************************************************************/
/*                               G e t L i n e                                */
/******************************************************************************/
  
char *XrdOucStream::GetLine()
{
   int bcnt, retc;
   char *bp;

// Check if end of message has been reached.
//
   if (flags & XrdOucStream_EOM) return (char *)NULL;

// Find the next record in the buffer
//
   if (bleft > 0)
      {recp = bnext; bcnt = bleft;
       for (bp = bnext; bcnt--; bp++)
           if (!*bp || *bp == '\n')
               {if (!*bp) flags |= XrdOucStream_EOM;
                *bp = '\0';
                bnext = ++bp;
                bleft = bcnt;
                token = recp;
                return recp;
               }
               else if (notabs && *bp == '\t') *bp = ' ';
  
   // There is no next record, so move up data in the buffer.
   //
      strncpy(buff, bnext, bleft);
      bnext = buff + bleft;
      }
      else bnext = buff;

// Prepare to read in more data.
//
    bcnt = bsize - (bnext - buff) -1;
    bp = bnext;

// Read up to the maximum number of bytes. Stop reading should we see a
// new-line character or a null byte -- the end of a record.
//
   recp  = token = buff; // This will always be true at this point
   while(bcnt)
        {do { retc = read(FD, (void *)bp, (size_t)bcnt); }
            while (retc < 0 && errno == EINTR);

         if (retc < 0) {Erp(GetLine,errno,"read request",0); return (char *)0;}
         if (!retc)
            {*bp = '\0';
             flags |= XrdOucStream_EOM;
             bnext = ++bp;
             bleft = 0;
             return buff;
            }

         bcnt -= retc;
         while(retc--)
             if (!*bp || *bp == '\n')
                {if (!*bp) flags |= XrdOucStream_EOM;
                    else *bp = '\0';
                 bnext = ++bp;
                 bleft = retc;
                 return buff;
                } else {
                 if (notabs && *bp == '\t') *bp = ' ';
                 bp++;
                }
         }

// All done, force an end of record.
//
   Erp(GetLine, EMSGSIZE, "read full message", 0);
   buff[bsize-1] = '\0';
   return buff;
}

/******************************************************************************/
/*                              G e t T o k e n                               */
/******************************************************************************/
  
char *XrdOucStream::GetToken(int lowcase) {
     char *tpoint;

     // Verify that we have a token to return;
     //
     if (!token) return (char *)NULL;

     // Skip to the first non-blank character.
     //
     while (*token && *token == ' ') token ++;
     if (!*token) {token = 0; return 0;}
     tpoint = token;

     // Find the end of the token.
     //
     if (lowcase) while (*token && *token != ' ')
                        {*token = (char)tolower((int)*token); token++;}
        else      while (*token && *token != ' ') {token++;}
     if (*token) {*token = '\0'; token++;}

     // All done here.
     //
     return tpoint;
}

char *XrdOucStream::GetToken(char **rest, int lowcase)
{
     char *tpoint;

     // Get the next token
     //
     if (!(tpoint = GetToken(lowcase))) return tpoint;

     // Skip to the first non-blank character.
     //
     while (*token && *token == ' ') token ++;
     if (rest) *rest = token;


     // All done.
     //
     return tpoint;
}

/******************************************************************************/
/*                          G e t F i r s t W o r d                           */
/******************************************************************************/

char *XrdOucStream::GetFirstWord(int lowcase)
{
      // If in the middle of a line, flush to the end of the line. Suppress
      // variable substitution when doing this to avoid errors.
      //
      if (xline)
         {XrdOucEnv *oldEnv = SetEnv(0);
          while(GetWord(lowcase)) {}
          SetEnv(oldEnv);
         }
      return GetWord(lowcase);
}

/******************************************************************************/
/*                        G e t M y F i r s t W o r d                         */
/******************************************************************************/
  
char *XrdOucStream::GetMyFirstWord(int lowcase)
{
   char *var;
   int   skip2fi = 0;

   Echo();

   if (!myInst)
      {if (!myEnv) return add2llB(GetFirstWord(lowcase), 1);
          else {while((var = GetFirstWord(lowcase)) && !isSet(var)) {}
                return add2llB(var, 1);
               }
      }

   do {if (!(var = GetFirstWord(lowcase)))
          {if (sawif && !ecode)
              {ecode = EINVAL;
               if (Eroute) Eroute->Emsg("Stream", "Missing 'fi' for last 'if'.");
              }
           return add2llB(var, 1);
          }

        add2llB(var, 1);

        if (!strcmp("continue", var))
           {if (!docont()) return 0;
            continue;
           }

        if (       !strcmp("if",   var)) var = doif();
        if (var && !strcmp("else", var)) var = doelse();
        if (var && !strcmp("fi",   var))
           {if (sawif) sawif = skpel = skip2fi = 0;
               else {if (Eroute)
                        Eroute->Emsg("Stream", "No preceding 'if' for 'fi'.");
                     ecode = EINVAL;
                    }
            continue;
           }
        if (var && (!myEnv || !isSet(var))) return add2llB(var, 1);
       } while (1);

   return 0;
}

/******************************************************************************/
/*                               G e t W o r d                                */
/******************************************************************************/
  
char *XrdOucStream::GetWord(int lowcase)
{
     char *wp, *ep;

     // A call means the first token was acceptable and we continuing to
     // parse, hence the line is echoable.
     //
     if (llBok == 1) llBok = 2;

     // If we have a token, return it
     //
     xline = 1;
     while((wp = GetToken(lowcase)))
          {if (!myEnv) return add2llB(wp);
           if ((wp = vSubs(wp)) && *wp) return add2llB(wp);
          }

     // If no continuation allowed, return a null (but only once)
     //
     if (!xcont) {xcont = 1; xline = 0; return (char *)0;}

     // Find the next non-blank non-comment line
     //
do  {while(GetLine())
        {// Get the first token (none if it is a blank line)
         //
         if (!(wp = GetToken(lowcase))) continue;

         // If token starts with a pound sign, skip the line
         //
         if (*wp == '#') continue;

         // Process continuations (last non-blank character is a back-slash)
         //
         ep = bnext-2;
         while (ep >= buff && *ep == ' ') ep--;
         if (ep < buff) continue;
         if (*ep == '\\') {xcont = 1; *ep = '\0';}
            else xcont = 0;
         return add2llB((myEnv ? vSubs(wp) : wp));
         }

     if (myInfo && myInfo->fcList)
        {if (myInfo->itFC == myInfo->fcList->end())
            {bleft = 0;
             flags |= XrdOucStream_EOM;
             break;
            }
         const char *path = (*(myInfo->itFC)).c_str();
         myInfo->itFC++;
         if (!docontF(path)) break;
         bleft = 0;
         flags &= ~XrdOucStream_EOM;
        } else break;
    } while(true);

      xline = 0;
      return (char *)0;
}

/******************************************************************************/
/*                               G e t R e s t                                */
/******************************************************************************/
  
int XrdOucStream::GetRest(char *theBuff, int Blen, int lowcase)
{
   char *tp, *myBuff = theBuff;
   int tlen;

// Get remaining tokens
//
   theBuff[0] = '\0';
   while ((tp = GetWord(lowcase)))
         {tlen = strlen(tp);
          if (tlen+1 >= Blen) return 0;
          if (myBuff != theBuff) {*myBuff++ = ' '; Blen--;}
          strcpy(myBuff, tp);
          Blen -= tlen; myBuff += tlen;
         }

// All done
//
   add2llB(0);
   return 1;
}

/******************************************************************************/
/*                              R e t T o k e n                               */
/******************************************************************************/
  
void XrdOucStream::RetToken()
{
     // Check if we can back up
     //
     if (!token || token == recp) return;

     // Find the null byte for the token and remove it, if possible
     //
     while(*token && token != recp) token--;
     if (token != recp) 
        {if (token+1 != bnext) *token = ' ';
         token--;
         while(*token && *token != ' ' && token != recp) token--;
         if (token != recp) token++;
        }

     // If saving line, we must do the same for the saved line
     //
     if (llBuff)
         while(llBcur != llBuff && *llBcur != ' ') {llBcur--; llBleft++;}
}

/******************************************************************************/
/*                                   P u t                                    */
/******************************************************************************/

int XrdOucStream::Put(const char *data, const int dlen) {
    int dcnt = dlen, retc;

    if (flags & XrdOucStream_BUSY) {ecode = ETXTBSY; return -1;}

    while(dcnt)
         {do { retc = write(FE, (const void *)data, (size_t)dlen);}
              while (retc < 0 && errno == EINTR);
          if (retc >= 0) dcnt -= retc;
             else {flags |= XrdOucStream_BUSY;
                   Erp(Put, errno, "write to stream", 0);
                   flags &= ~XrdOucStream_BUSY;
                   return -1;
                  }
         }
    return 0;
}

int XrdOucStream::Put(const char *datavec[], const int dlenvec[]) {
    int i, retc, dlen;
    const char *data;

    if (flags & XrdOucStream_BUSY) {ecode = ETXTBSY; return -1;}

    for (i = 0; datavec[i]; i++)
        {data = datavec[i]; dlen = dlenvec[i];
         while(dlen)
              {do { retc = write(FE, (const void *)data, (size_t)dlen);}
                   while (retc < 0 && errno == EINTR);
               if (retc >= 0) {data += retc; dlen -= retc;}
                  else {flags |= XrdOucStream_BUSY;
                        Erp(Put, errno, "write to stream",0);
                        flags &= ~XrdOucStream_BUSY;
                        return -1;
                       }
              }
        }
    return 0;
}
 
/******************************************************************************/
/*                               P u t L i n e                                */
/******************************************************************************/
  
int XrdOucStream::PutLine(const char *data, int dlen)
{
   static const int plSize = 2048;

// Allocate a buffer if one is not allocated
//
   if (!buff)
      {if (!(buff = (char *)malloc(plSize)))
          return Erq(Attach, errno, "allocate stream buffer");
       bsize = plSize;
      }

// Adjust dlen
//
   if (dlen <= 0) dlen = strlen(data);
   if (dlen >= bsize) dlen = bsize-1;

// Simply insert the line into the buffer, truncating if need be
//
   bnext = recp = token = buff; // This will always be true at this point
   if (dlen <= 0)
      {*buff = '\0';
       flags |= XrdOucStream_EOM;
       bleft = 0;
      } else {
       strncpy(buff, data, dlen);
       *(buff+dlen) = 0;
       bleft = dlen+1;
      }
// All done
//
   return 0;
}

/******************************************************************************/
/*                             W a i t 4 D a t a                              */
/******************************************************************************/

int XrdOucStream::Wait4Data(int msMax)
{
   struct pollfd polltab = {FD, POLLIN|POLLRDNORM, 0};
   int retc;

// Wait until we can actually read something
//
   do {retc = poll(&polltab, 1, msMax);} while(retc < 0 && errno == EINTR);
   if (retc != 1) return (retc ? errno : -1);

// Return correct value
//
   return (polltab.revents & (POLLIN|POLLRDNORM) ? 0 : EIO);
}
  
/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                               a d d 2 C F G                                */
/******************************************************************************/

void XrdOucStream::add2CFG(const char *data, bool isCMT)
{
   if (isCMT) theCFG->append("# ");
   theCFG->append(data);
   theCFG->append('\n');
}
  
/******************************************************************************/
/*                               a d d 2 l l B                                */
/******************************************************************************/

char *XrdOucStream::add2llB(char *tok, int reset)
{
   int tlen;

// Return if not saving data
//
   if (!llBuff) return tok;

// Check if we should flush the previous line
//
   if (reset)
      {llBok  = 1;
       llBcur = llBuff;
       llBleft= llBsz;
      *llBuff = '\0';
      } else if (!llBok) return tok;
                else {llBok = 2;
                      if (llBleft >= 2)
                         {*llBcur++ = ' '; *llBcur = '\0'; llBleft--;}
                     }

// Add in the new token
//
   if (tok)
      {tlen = strlen(tok);
       if (tlen < llBleft)
          {strcpy(llBcur, tok); llBcur += tlen; llBleft -= tlen;}
      }
   return tok;
}
  
/******************************************************************************/
/*                                  E c h o                                   */
/******************************************************************************/
  
bool XrdOucStream::Echo(int ec, const char *t1, const char *t2, const char *t3)
{
   if (Eroute)
      {if (t1) Eroute->Emsg("Stream", t1, t2, t3);
       if (llBok > 1 && Verbose && llBuff) Eroute->Say(llPrefix,llBuff);
      }
   ecode = ec;
   llBok = 0;
   return false;
}

/******************************************************************************/
/*                                d o c o n t                                 */
/******************************************************************************/
  
bool XrdOucStream::docont()
{
   char *theWord;

// A continue is not valid within the scope of an if
//
   if (sawif) return Echo(EINVAL, "'continue' invalid within 'if-fi'.");

// Get the path (keep case), if none then ignore this continue
//
   theWord = GetWord();
   if (!theWord)
      {Echo();
       return true;
      }

// Prepare to handle the directive
//
   contHandler cH;
   cH.path = strdup(theWord);

// Grab additioal tokens which may be suffixes
//
   theWord = GetWord();
   while(theWord && *theWord == '*')
        {if (!*(theWord+1)) return Echo(EINVAL, "suffix missing after '*'.");
         cH.Add(theWord+1);
         theWord = GetWord();
        }

// If we have a token, it better be an if
//
   if (theWord && strcmp(theWord, "if"))
      return Echo(EINVAL, "expecting 'if' but found", theWord);

// Process the 'if'
//
   if (theWord && !XrdOucUtils::doIf(Eroute, *this, "continue directive",
                                myInfo->myHost,myInfo->myName,myInfo->myExec))
      return true;
   Echo();
// if (Eroute) Eroute->Say(llPrefix, "continue ", path, " if true");
// if (Eroute) Eroute->Say(llPrefix, "continue ", bnext);
   return docont(cH.path, cH.tlP);
}
  
/******************************************************************************/

bool XrdOucStream::docont(const char *path, XrdOucTList *tlP)
{
   struct stat Stat;
   bool noentok;

// A continue directive in the context of a continuation is illegal
//
   if ((myInfo && myInfo->fcList) || (flags & XrdOucStream_CONT) != 0)
      return Echo(EINVAL, "'continue' in a continuation is not allowed.");

// Check if this file must exist (we also take care of empty paths)
//
   if ((noentok = (*path == '?')))
      {path++;
       if (!(*path)) return true;
      }

// Check if this is a file or directory
//
   if (stat(path, &Stat))
      {if (errno == ENOENT && noentok) return true;
       if (Eroute)
          {Eroute->Emsg("Stream", errno, "open", path);
           ecode = ECANCELED;
          } else ecode = errno;
       return false;
      }

// For directory continuation, there is much more to do (this can only happen
// once). Note that we used to allow a limited number of chained file
// continuations. No more, but we are still setup to easily do so.
//
   if ((Stat.st_mode & S_IFMT) == S_IFDIR)
      {if (!docontD(path, tlP)) return false;
       path = (*(myInfo->itFC)).c_str();
       myInfo->itFC++;
      } else flags |= XrdOucStream_CADD;

//     if ((flags & XrdOucStream_CONT) > XrdOucStream_CMAX)
//        {if (Eroute)
//            {Eroute->Emsg("Stream", EMLINK, "continue to", path);
//             ecode = ECANCELED;
//            } else ecode = EMLINK;
//         return false;
//        }
//    }

// Continue with the next file
//
   return docontF(path, noentok);
}

/******************************************************************************/
/*                               d o c o n t D                                */
/******************************************************************************/
  
bool XrdOucStream::docontD(const char *path, XrdOucTList *tlP)
{
   static const mode_t isXeq = S_IXUSR | S_IXGRP | S_IXOTH;
   XrdOucNSWalk nsWalk(Eroute, path, 0, XrdOucNSWalk::retFile);
   int rc;

// Get all of the file entries in this directory
//
   XrdOucNSWalk::NSEnt *nsX, *nsP = nsWalk.Index(rc);
   if (rc)
      {if (Eroute) Eroute->Emsg("Stream", rc, "index config files in", path);
       ecode = ECANCELED;
       return false;
      }

// Keep only files of interest
//
   myInfo->fcList = new std::set<std::string>;
   while((nsX = nsP))
        {nsP = nsP->Next;
         if ((nsX->Stat.st_mode & isXeq) == 0 && KeepFile(nsX->File, tlP))
            myInfo->fcList->insert(std::string(nsX->Path));
         delete nsX;
        }

// Check if we have anything in the map
//
   if (myInfo->fcList->size() == 0)
      {delete myInfo->fcList;
       myInfo->fcList = 0;
       return false;
      }

// All done
//
   myInfo->itFC = myInfo->fcList->begin();
   return true;
}
  
/******************************************************************************/
/*                                 c o n t F                                  */
/******************************************************************************/

bool XrdOucStream::docontF(const char *path, bool noentok)
{
   int cFD;

// Open the file and handle any errors
//
   if ((cFD = XrdSysFD_Open(path, O_RDONLY)) < 0)
      {if (errno == ENOENT && noentok) return true;
       if (Eroute)
          {Eroute->Emsg("Stream", errno, "open", path);
           ecode = ECANCELED;
          } else ecode = errno;
       return false;
      }

// Continue to the next file
//
   if (XrdSysFD_Dup2(cFD, FD) < 0)
      {if (Eroute)
          {Eroute->Emsg("Stream", ecode, "switch to", path);
           close(cFD);
           ecode = ECANCELED;
          } else ecode = errno;
       return false;
      }

// Indicate we are switching to anther file
//
   if (Eroute) Eroute->Say("Config continuing with file ", path, " ...");
   bleft = 0;
   close(cFD);
   return true;
}
  
/******************************************************************************/
/*                                d o e l s e                                 */
/******************************************************************************/

char *XrdOucStream::doelse()
{
   char *var;

// An else must be preceeded by an if and not by a naked else
//
   if (!sawif || sawif == 2)
      {if (Eroute) Eroute->Emsg("Stream", "No preceding 'if' for 'else'.");
       ecode = EINVAL;
       return 0;
      }

// If skipping all else caluses, skip all lines until we reach a fi
//
   if (skpel)
      {while((var = GetFirstWord()))
            {if (!strcmp("fi", var)) return var;}
       if (Eroute) Eroute->Emsg("Stream", "Missing 'fi' for last 'if'.");
       ecode = EINVAL;
       return 0;
      }

// Elses are still possible then process one of them
//
   do {if (!(var = GetWord())) // A naked else will always succeed
          {sawif = 2;
           return 0;
          }
       if (strcmp("if", var))  // An else may only be followed by an if
          {Eroute->Emsg("Stream","'else",var,"' is invalid.");
           ecode = EINVAL;
           return 0;
          }
       sawif = 0;
       flags |=  XrdOucStream_ELIF;
       var = doif();
       flags &= ~XrdOucStream_ELIF;
      } while(var && !strcmp("else", var));
   return var;
}
  
/******************************************************************************/
/*                                  d o i f                                   */
/******************************************************************************/

/* Function: doif

   Purpose:  To parse the directive: if [<hlist>] [exec <pgm>] [named <nlist>]
                                     fi

            <hlist> Apply subsequent directives until the 'fi' if this host
                    is one of the hosts in the blank separated list. Each
                    host name may have a single asterisk somewhere in the
                    name to indicate where arbitrry characters lie.

            <pgm>   Apply subsequent directives if this program is named <pgm>.

            <nlist> Apply subsequent directives if this  host instance name
                    is in the list of blank separated names.

   Notes: 1) At least one of hlist, pgm, or nlist must be specified.
          2) The combination of hlist, pgm, nlist must all be true.

   Output: 0 upon success or !0 upon failure.
*/

char *XrdOucStream::doif()
{
    char *var, ifLine[512];
    int rc;

// Check if the previous if was properly closed
//
   if (sawif)
      {if (Eroute) Eroute->Emsg("Stream", "Missing 'fi' for last 'if'.");
       ecode = EINVAL;
      }

// Save the line for context message should we get an error
//
   snprintf(ifLine, sizeof(ifLine), "%s", token);

// Check if we should continue
//
   sawif = 1; skpel = 0;
   if ((rc = XrdOucUtils::doIf(Eroute,*this,"if directive",
                               myInfo->myHost,myInfo->myName,myInfo->myExec)))
      {if (rc >= 0) skpel = 1;
          else {ecode = EINVAL;
                if(Eroute) Eroute->Say(llPrefix,
                                      (flags & XrdOucStream_ELIF ? "else " : 0),
                                       "if ", ifLine);
               }
       return 0;
      }

// Skip all lines until we reach a fi or else
//
   while((var = GetFirstWord()))
        {if (!strcmp("fi",   var)) return var;
         if (!strcmp("else", var)) return var;
        }

// Make sure we have a fi
//
   if (!var) 
      {if (Eroute) Eroute->Emsg("Stream", "Missing 'fi' for last 'if'.");
       ecode = EINVAL;
      }
   return 0;
}

/******************************************************************************/
/*                                 i s S e t                                  */
/******************************************************************************/
  
int XrdOucStream::isSet(char *var)
{
   static const char *Mtxt1[2] = {"setenv", "set"};
   static const char *Mtxt2[2] = {"Setenv variable", "Set variable"};
   static const char *Mtxt3[2] = {"Variable", "Environmental variable"};
   char *tp, *vn, *vp, *pv, Vname[64], ec, Nil = 0;
   int sawEQ, Set = 1;

// Process set var = value | set -v | setenv = value
//
   if (!strcmp("setenv", var)) Set = 0;
      else if (strcmp("set", var)) return 0;

// Now get the operand
//
   if (!(tp = GetToken()))
      return xMsg("Missing variable name after '",Mtxt1[Set],"'.");

// Option flags only apply to set not setenv
//
   if (Set)
  {if (!strcmp(tp, "-q")) {if (llBuff) {free(llBuff); llBuff = 0;}; return 1;}
   if (!strcmp(tp, "-v") || !strcmp(tp, "-V"))
      {if (Eroute)
          {if (!llBuff) llBuff = (char *)malloc(llBsz);
           llBcur = llBuff; llBok = 0; llBleft = llBsz; *llBuff = '\0';
           Verbose = (strcmp(tp, "-V") ? 1 : 2);
          }
       return 1;
      }
  }

// Next may be var= | var | var=val
//
   if ((vp = index(tp, '='))) {sawEQ = 1; *vp = '\0'; vp++;}
      else sawEQ = 0;
   if (strlcpy(Vname, tp, sizeof(Vname)) >= sizeof(Vname))
      return xMsg(Mtxt2[Set],tp,"is too long.");
   if (!Set && !strncmp("XRD", Vname, 3))
      return xMsg("Setenv variable",tp,"may not start with 'XRD'.");

// Verify that variable is only an alphanum
//
   tp = Vname;
   while (*tp && (*tp == '_' || isalnum(*tp))) tp++;
   if (*tp) return xMsg(Mtxt2[Set], Vname, "is non-alphanumeric");

// Now look for the value
//
   if (sawEQ) tp = vp;
      else if (!(tp = GetToken()) || *tp != '=')
              return xMsg("Missing '=' after", Mtxt1[Set], Vname);
              else tp++;
   if (!*tp && !(tp = GetToken())) tp = (char *)"";

// The value may be '$var', in which case we need to get it out of the env if
// this is a set or from our environment if this is a setenv
//
   if (*tp != '$') vp = tp;
      else {pv = tp+1;
                 if (*pv == '(') ec = ')';
            else if (*pv == '{') ec = '}';
            else if (*pv == '[') ec = ']';
            else                 ec = 0;
            if (!ec) vn = tp+1;
               else {while(*pv && *pv != ec) pv++;
                     if (*pv) *pv = '\0';
                        else   ec = 0;
                     vn = tp+2;
                    }
            if (!*vn) {*pv = ec; return xMsg("Variable", tp, "is malformed.");}
            if (!(vp = (Set ? getenv(vn) : myEnv->Get(vn))))
               {if (ec != ']')
                   {xMsg(Mtxt3[Set],vn,"is undefined."); *pv = ec; return 1;}
                vp = &Nil;
               }
            *pv = ec;
           }

// Make sure the value is not too long
//
   if ((int)strlen(vp) > maxVLen)
      return xMsg(Mtxt3[Set], Vname, "value is too long.");

// Set the value
//
   if (Verbose == 2 && Eroute)
      if (!(pv = (Set ? myEnv->Get(Vname) : getenv(Vname))) || strcmp(vp, pv))
         {char vbuff[1024];
          strcpy(vbuff, Mtxt1[Set]); strcat(vbuff, " "); strcat(vbuff, Vname);
          Eroute->Say(vbuff, " = ", vp);
         }
   if (Set) myEnv->Put(Vname, vp);
      else if (!(pv = getenv(Vname)) || strcmp(vp,pv))
              XrdOucEnv::Export(Vname, vp);
   return 1;
}

/******************************************************************************/
/*                                 v S u b s                                  */
/******************************************************************************/
  
char *XrdOucStream::vSubs(char *Var)
{
   char *vp, *sp, *dp, *vnp, ec, bkp, valbuff[maxVLen], Nil = 0;
   int n;

// Check for substitution
//
   if (!Var) return Var;
   sp = Var; dp = valbuff; n = maxVLen-1; *varVal = '\0';

   while(*sp && n > 0)
        {if (*sp == '\\') {*dp++ = *(sp+1); sp +=2; n--; continue;}
         if (*sp != '$'
         || (!isalnum(*(sp+1)) && !index("({[", *(sp+1))))
                {*dp++ = *sp++;         n--; continue;}
         sp++; vnp = sp;
              if (*sp == '(') ec = ')';
         else if (*sp == '{') ec = '}';
         else if (*sp == '[') ec = ']';
         else                 ec = 0;
         if (ec) {sp++; vnp++;}
         while(isalnum(*sp)) sp++;
         if (ec && *sp != ec)
            {xMsg("Variable", vnp-2, "is malformed."); return varVal;}
         bkp = *sp; *sp = '\0';
         if (!(vp = myEnv->Get(vnp)))
            {if (ec != ']') xMsg("Variable", vnp, "is undefined.");
             vp = &Nil;
            }
         while(n && *vp) {*dp++ = *vp++; n--;}
         if (*vp) break;
         if (ec) sp++;
            else *sp = bkp;
        }

   if (*sp) xMsg("Substituted text too long using", Var);
      else {*dp = '\0'; strcpy(varVal, valbuff);}
   return varVal;
}

/******************************************************************************/
/*                                  x M s g                                   */
/******************************************************************************/

int XrdOucStream::xMsg(const char *txt1, const char *txt2, const char *txt3)
{
    if (Eroute) Eroute->Emsg("Stream", txt1, txt2, txt3);
    ecode = EINVAL;
    return 1;
}
