/******************************************************************************/
/*                                                                            */
/*                      X r d O d c M a n a g e r . c c                       */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//          $Id$

const char *XrdOdcManagerCVSID = "$Id$";

#include "XrdOdc/XrdOdcManager.hh"
#include "XrdOdc/XrdOdcMsg.hh"
#include "XrdOdc/XrdOdcTrace.hh"

#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucLink.hh"
#include "XrdOuc/XrdOucNetwork.hh"
#include "XrdOuc/XrdOucPthread.hh"
 
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
extern XrdOucTrace OdcTrace;

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOdcManager::XrdOdcManager(XrdOucError *erp, char *host, int port, int cw)
{

// Set error object
//
   eDest   = erp;
   Host    = strdup(host);
   Port    = port;
   Link    = 0;
   Active  = 0;
   mytid   = 0;
   Network = new XrdOucNetwork(eDest, 0, &OdcTrace, TRACE_Debug);

// Compute dally value
//
   dally = cw / 2 - 1;
   if (dally < 3) dally = 3;
      else if (dally > 10) dally = 10;
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdOdcManager::~XrdOdcManager()
{
  if (Network) delete Network;
  if (Link)    Link->Recycle();
  if (Host)    free(Host);
  if (mytid)   XrdOucThread_Kill(mytid);
}
  
/******************************************************************************/
/*                                  S e n d                                   */
/******************************************************************************/
  
int XrdOdcManager::Send(char *msg, int mlen)
{
   int allok = 0;

// Determine message length
//
   if (!mlen) mlen = strlen(msg);

// Send the request
//
   if (Active)
      {myData.Lock();
       if (Link)
          if (!(allok = (Link && Link->Send(msg, mlen, 0) == 0)))
             {Active = 0;
              Link->Close();
             }
       myData.UnLock();
      }

// All done
//
   return allok;
}
  
int XrdOdcManager::Send(const struct iovec *iov, int iovcnt)
{
   int allok = 0;

// Send the request
//
   if (Active)
      {myData.Lock();
       if (Link)
          if (!(allok = (Link && Link->Send(iov, iovcnt, 0) == 0)))
             {Active = 0;
              Link->Close();
             }
       myData.UnLock();
      }

// All done
//
   return allok;
}

/******************************************************************************/
/*                                 S t a r t                                  */
/******************************************************************************/
  
void *XrdOdcManager::Start()
{
   char *msg;
   int   msgid, retc;

// First step is to connect to the manager
//
   do {Hookup();

       // Now simply start receiving messages on the stream
       //
       while((msg = Receive(msgid))) XrdOdcMsg::Reply(msgid, msg);

       // Tear down the connection
       //
       myData.Lock();
       Active = 0;
       if (Link)
          {retc = Link->LastError();
           Link->Recycle(); 
           Link = 0;
          } else retc = 0;
       myData.UnLock();

       // Indicate the problem
       //
       if (retc) eDest->Emsg("Manager", retc, "receive msg from", Host);
          else   eDest->Emsg("Manager", "Disconnected from", Host);
       Sleep(dally);
      } while(1);

// We should never get here
//
   return (void *)0;
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                                H o o k u p                                 */
/******************************************************************************/
  
void XrdOdcManager::Hookup()
{
   XrdOucLink *lp;
   int tries = 12, opts = OUC_NODELAY;

// Keep trying to connect to the manager
//
   do {while(!(lp = Network->Connect(Host, Port, opts)))
            {Sleep(dally);
             if (tries--) opts = OUC_NODELAY | OUC_NOEMSG;
                else     {opts = OUC_NODELAY; tries = 12;}
            }
       if (lp->Send((char *)"login director\n") == 0) break;
       lp->Recycle();
      } while(1);

// All went well, finally
//
   myData.Lock();
   Link   = lp;
   Active = 1;
   myData.UnLock();

// Tell the world
//
   eDest->Emsg("Manager", "Connected to", Host);
}

/******************************************************************************/
/*                                 S l e e p                                  */
/******************************************************************************/
  
void XrdOdcManager::Sleep(int slpsec)
{
   int retc;
   struct timespec lftp, rqtp = {slpsec, 0};

   while ((retc = nanosleep(&rqtp, &lftp)) < 0 && errno == EINTR)
         {rqtp.tv_sec  = lftp.tv_sec;
          rqtp.tv_nsec = lftp.tv_nsec;
         }

   if (retc < 0) eDest->Emsg("Manager", errno, "sleep");
}

/******************************************************************************/
/*                               R e c e i v e                                */
/******************************************************************************/
  
char *XrdOdcManager::Receive(int &msgid)
{
   EPNAME("Receive")
   char *lp, *tp, *rest;
   if ((lp=Link->GetLine()) && *lp)
      {DEBUG("Server: Received from " <<Link->Name() <<": " <<lp);
       if ((tp=Link->GetToken(&rest)))
          {errno = 0;
           msgid  = (int)strtol(tp, (char **)NULL, 10);
           if (!errno) return rest;
           eDest->Emsg("Manager", "Invalid msgid from", Host);
          }
      }
   return 0;
}
