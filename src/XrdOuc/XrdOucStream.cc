/******************************************************************************/
/*                                                                            */
/*                         o o u c _ S t r e a m . c                          */
/*                                                                            */
/* (C) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*      All Rights Reserved. See XrdVersion.cc for complete License Terms     */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Deprtment of Energy               */
/******************************************************************************/

//        $Id$ 

const char *XrdOucStreamCVSID = "$Id$";

#include "Experiment/Experiment.hh"

#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stropts.h>
#include <stdio.h>
#ifndef __linux__
#include <sys/conf.h>
#endif
#include <sys/stat.h>
#include <sys/stropts.h>
#include <sys/termios.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "XrdOuc/XrdOucStream.hh"

/******************************************************************************/
/*                         l o c a l   d e f i n e s                          */
/******************************************************************************/
  
#define MaxARGC 64
#define XrdOucStream_EOM  0x01
#define XrdOucStream_BUSY 0x02

#define Erq(p, a, b) Err(p, a, b, (char *)0)
#define Err(p, a, b, c) (ecode=(Eroute ? Eroute->Emsg(#p, a, b, c) : a), -1)

#ifdef SUNCC
#define NewProc fork1()
#else
#define NewProc fork()
#endif

/******************************************************************************/
/*               o o u c _ S t r e a m   C o n s t r u c t o r                */
/******************************************************************************/
  
XrdOucStream::XrdOucStream(XrdOucError *erobj)
{
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
    Close();

    // Allocate a new buffer for this stream
    //
    if (!bsz) buff = 0;
       else if (!(buff = (char *)malloc(bsz+1)))
               return Erq(Attach, errno, "allocating stream buffer");

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
    return  0;
}
  
/******************************************************************************/
/*                                 C l o s e                                  */
/******************************************************************************/

void XrdOucStream::Close(int hold)
{
    int retc=0;

    // Wait for any associated process on this stream
    //
    if (!hold) Drain();
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
}

/******************************************************************************/
/*                                 D r a i n                                  */
/******************************************************************************/
  
int XrdOucStream::Drain() 
{
    int Status = 0, retc;

    // Drain any outstanding processes.
    //
    if (child) {kill(child, 9);
                do {retc = waitpid(child, &Status, 0);}
                    while(retc > 0 || (retc == -1 && errno == EINTR));
                child = 0;
               }
    return Status;
}
  
/******************************************************************************/
/*                               E   x   e   c                                */
/******************************************************************************/
  
int XrdOucStream::Exec(char *cmd, int inrd)
{
    int j;
    char *parm[MaxARGC];
  
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
    return Exec(parm, inrd);
}

int XrdOucStream::Exec(char **parm, int inrd)
{
    int retc, fildes[2], Child_in, Child_out;

    // Create a pipe
    //
    if (pipe(fildes))
       return Err(Exec, errno, "creating in pipe for", parm[0]);
       else {Attach(fildes[0]); Child_out = fildes[1];
             fcntl(fildes[0], F_SETFD, FD_CLOEXEC);
            }

    if (inrd)
       if (pipe(fildes))
               return Err(Exec, errno, "creating out pipe for", parm[0]);
               else {FE = fildes[1]; Child_in  = fildes[0];
                     fcntl(fildes[1], F_SETFD, FD_CLOEXEC);
                    }

    // Fork a process first so we can pick up the next request.
    //
    if (child = NewProc)
       {          close(Child_out);
        if (inrd) close(Child_in );
        if (child < 0)
           return Err(Exec, errno, "forking request process for ", parm[0]);
        return 0;
       }

    /*****************************************************************/
    /*                  C h i l d   P r o c e s s                    */
    /*****************************************************************/

    // Redirect standard in if so requested
    //
    if (inrd)
       if (dup2(Child_in, STDIN_FILENO) < 0)
          {Err(Exec, errno, "setting up standard in for", parm[0]);
           exit(255);
          } else close(Child_in);

    // Reassign the stream to be standard out to capture all of the output.
    //
    if (dup2(Child_out, STDOUT_FILENO) < 0)
       {Err(Exec, errno, "setting up standard out for", parm[0]);
        exit(255);
       } else close(Child_out);

    // Invoke the command never to return
    //
    execv(parm[0], parm);
    Err(Exec, errno, "executing", parm[0]);
    exit(255);
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
   ecode = 0;
   recp  = token = buff; // This will always be true at this point
   while(bcnt)
        {do { retc = read(FD, (void *)bp, (size_t)bcnt); }
            while (retc < 0 && errno == EINTR);

         if (retc < 0) {Erq(GetLine, errno, "reading request");
                        return (char *)0;}
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
   Erq(GetLine, EMSGSIZE, "record truncated");
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
     *rest = token;


     // All done.
     //
     return tpoint;
}

/******************************************************************************/
/*                          G e t F i r s t W o r d                           */
/******************************************************************************/

char *XrdOucStream::GetFirstWord(int lowcase)
{
      // If in the middle of a line, flush to the end of the line
      //
      if (xline) while(GetWord(lowcase));
      return GetWord(lowcase);
}

/******************************************************************************/
/*                               G e t W o r d                                */
/******************************************************************************/
  
char *XrdOucStream::GetWord(int lowcase)
{
     char *wp, *ep;

     // If we have a token, return it
     //
     xline = 1;
     if (wp = GetToken(lowcase)) return wp;

     // If no continuation allowed, return a null (but only once)
     //
     if (!xcont) {xcont = 1; xline = 0; return (char *)0;}

     // Find the next non-blank non-comment line
     //
     while(GetLine())
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
         return wp;
         }
      xline = 0;
      return (char *)0;
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
                   Erq(Put, errno, "writing to stream");
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
                        Erq(Put, errno, "writing to stream");
                        flags &= ~XrdOucStream_BUSY;
                        return -1;
                       }
              }
        }
    return 0;
}
