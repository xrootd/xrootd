/******************************************************************************/
/*                                                                            */
/*                        X r d M p x S t a t s . c c                         */
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

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>

#include "XrdApps/XrdMpxXml.hh"
#include "XrdNet/XrdNetAddr.hh"
#include "XrdNet/XrdNetOpts.hh"
#include "XrdNet/XrdNetSocket.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPthread.hh"
  
/******************************************************************************/
/*                      G l o b a l   V a r i a b l e s                       */
/******************************************************************************/

namespace XrdMpx
{
       XrdSysLogger       Logger;

       XrdSysError        Say(&Logger, "mpxstats");

static const int          addSender = 0x0001;

       int                Opts;
};

using namespace XrdMpx;
  
/******************************************************************************/
/*                             X r d M p x O u t                              */
/******************************************************************************/
  
class XrdMpxOut
{
public:

struct statsBuff
      {statsBuff      *Next;
       XrdNetSockAddr  From;
       int             Dlen;
       char            Data[8190];
       char            Pad[2];
     };

void       Add(statsBuff *sbP);

statsBuff *getBuff();

void      *Run(XrdMpxXml *xP);

           XrdMpxOut() : Ready(0), inQ(0), Free(0) {}
          ~XrdMpxOut() {}

private:

XrdSysMutex     myMutex;
XrdSysSemaphore Ready;

statsBuff      *inQ;
statsBuff      *Free;
};

/******************************************************************************/
/*                        X r d M p x O u t : : A d d                         */
/******************************************************************************/
  
void XrdMpxOut::Add(statsBuff *sbP)
{

// Add this to the queue and signal the processing thread
//
   myMutex.Lock();
   sbP->Next = inQ;
   inQ = sbP;
   Ready.Post();
   myMutex.UnLock();
}

/******************************************************************************/
/*                    X r d M p x O u t : : g e t B u f f                     */
/******************************************************************************/
  
XrdMpxOut::statsBuff *XrdMpxOut::getBuff()
{
   statsBuff *sbP;

// Use an available buffer or allocate one
//
   myMutex.Lock();
   if ((sbP = Free)) Free = sbP->Next;
      else sbP = new statsBuff;
   myMutex.UnLock();
   return sbP;
}

/******************************************************************************/
/*                        X r d M p x O u t : : R u n                         */
/******************************************************************************/
  
void *XrdMpxOut::Run(XrdMpxXml *xP)
{
   XrdNetAddr theAddr;
   const char *Host = 0;
   char *bP, obuff[sizeof(statsBuff)*2];
   statsBuff *sbP;
   int wLen, rc;

// Simply loop formating and outputing the buffers
//
   while(1)
        {Ready.Wait();
         myMutex.Lock();
         if ((sbP = inQ)) inQ = sbP->Next;
         myMutex.UnLock();
         if (!sbP) continue;
         if (xP)
            {if (!(Opts & addSender)) Host = 0;
                else if (theAddr.Set(&(sbP->From.Addr))) Host = 0;
                        else Host = theAddr.Name();
             wLen = xP->Format(Host, sbP->Data, obuff);
             bP = obuff;
            } else {
             bP = sbP->Data;
             *(bP + sbP->Dlen) = '\n';
             wLen = sbP->Dlen+1;
            }

         while(wLen > 0)
              {do {rc = write(STDOUT_FILENO, bP, wLen);}
                  while(rc < 0 && errno == EINTR);
               wLen -= rc; bP += rc;
              }

         myMutex.Lock(); sbP->Next = Free; Free = sbP; myMutex.UnLock();
        }

// Should never get here
//
   return (void *)0;
}

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
namespace XrdMpx
{
XrdMpxOut statsQ;
};

/******************************************************************************/
/*                     T h r e a d   I n t e r f a c e s                      */
/******************************************************************************/
  
void *mainOutput(void *parg)
{
    XrdMpxXml *xP = static_cast<XrdMpxXml *>(parg);
    return statsQ.Run(xP);
}

/******************************************************************************/
/*                                 U s a g e                                  */
/******************************************************************************/
  
void Usage(int rc)
{
   cerr <<"\nUsage: mpxstats [-f {cgi|flat|xml}] -p <port> [-s]" <<endl;
   exit(rc);
}

/******************************************************************************/
/*                                  m a i n                                   */
/******************************************************************************/
  
int main(int argc, char *argv[])
{
   extern char *optarg;
   extern int opterr, optopt;
   sigset_t myset;
   pthread_t tid;
   XrdMpxXml::fmtType fType = XrdMpxXml::fmtXML;
   XrdMpxOut::statsBuff *sbP = 0;
   XrdNetSocket mySocket(&Say);
   XrdMpxXml *xP = 0;
   SOCKLEN_t fromLen;
   int Port = 0, retc, udpFD;
   char buff[64], c;
   bool Debug;

// Process the options
//
   opterr = 0; Debug = false; Opts = 0;
   if (argc > 1 && '-' == *argv[1]) 
      while ((c = getopt(argc,argv,"df:p:s")) && ((unsigned char)c != 0xff))
     { switch(c)
       {
       case 'd': Debug = true;
                 break;
       case 'f':      if (!strcmp(optarg, "cgi" )) fType = XrdMpxXml::fmtCGI;
                 else if (!strcmp(optarg, "flat")) fType = XrdMpxXml::fmtFlat;
                 else if (!strcmp(optarg, "xml" )) fType = XrdMpxXml::fmtXML;
                 else {Say.Emsg(":", "Invalid format - ", optarg); Usage(1);}
                 break;
       case 'h': Usage(0);
                 break;
       case 'p': if (!(Port = atoi(optarg)))
                    {Say.Emsg(":", "Invalid port number - ", optarg); Usage(1);}
                 break;
       case 's': Opts |= addSender;
                 break;
       default:  sprintf(buff,"'%c'", optopt);
                 if (c == ':') Say.Emsg(":", buff, "value not specified.");
                    else Say.Emsg(0, buff, "option is invalid");
                 Usage(1);
                 break;
       }
     }

// Make sure port has been specified
//
   if (!Port) {Say.Emsg(":", "Port has not been specified."); Usage(1);}

// Turn off sigpipe and host a variety of others before we start any threads
//
   signal(SIGPIPE, SIG_IGN);  // Solaris optimization
   sigemptyset(&myset);
   sigaddset(&myset, SIGPIPE);
   sigaddset(&myset, SIGCHLD);
   pthread_sigmask(SIG_BLOCK, &myset, NULL);

// Set the default stack size here
//
   if (sizeof(long) > 4) XrdSysThread::setStackSize((size_t)1048576);
      else               XrdSysThread::setStackSize((size_t)786432);

// Create a UDP socket and bind it to a port
//
   if (mySocket.Open(0, Port, XRDNET_SERVER|XRDNET_UDPSOCKET, 0) < 0)
      {Say.Emsg(":", -mySocket.LastError(), "create udp socket"); exit(4);}
   udpFD = mySocket.Detach();

// Establish format
//
   if (fType != XrdMpxXml::fmtXML) xP = new XrdMpxXml(fType, Debug);

// Now run a thread to output whatever we get
//
   if ((retc = XrdSysThread::Run(&tid, mainOutput, (void *)xP,
                                 XRDSYSTHREAD_BIND, "Output")))
      {Say.Emsg(":", retc, "create output thread"); exit(4);}

// Now simply wait for the messages
//
   fromLen = sizeof(sbP->From);
   while(1)
        {sbP = statsQ.getBuff();
         retc = recvfrom(udpFD, sbP->Data, sizeof(sbP->Data), 0,
                               &sbP->From.Addr, &fromLen);
         if (retc < 0) {Say.Emsg(":", retc, "recv udp message"); exit(8);}
         sbP->Dlen = retc;
         statsQ.Add(sbP);
        }

// Should never get here
//
   return 0;
}
