/******************************************************************************/
/*                                                                            */
/*                     X r d X r o o t d A d m i n . c c                      */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//         $Id$

const char *XrdXrootdAdminCVSID = "$Id$";

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "Xrd/XrdLink.hh"
#include "XrdNet/XrdNetSocket.hh"
#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucPlatform.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include "XrdXrootd/XrdXrootdAdmin.hh"
#include "XrdXrootd/XrdXrootdTrace.hh"
 
/******************************************************************************/
/*                     G l o b a l s   &   S t a t i c s                      */
/******************************************************************************/

extern XrdOucTrace     *XrdXrootdTrace;

       XrdOucError     *XrdXrootdAdmin::eDest;
  
/******************************************************************************/
/*            E x t e r n a l   T h r e a d   I n t e r f a c e s             */
/******************************************************************************/
  
void *XrdXrootdInitAdmin(void *carg)
      {XrdXrootdAdmin Admin;
       return Admin.Start((XrdNetSocket *)carg);
      }

void *XrdXrootdLoginAdmin(void *carg)
      {XrdXrootdAdmin *Admin = new XrdXrootdAdmin();
       Admin->Login(*(int *)carg);
       delete Admin;
       return (void *)0;
      }
 
/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
int XrdXrootdAdmin::Init(XrdOucError *erp, XrdNetSocket *asock)
{
   const char *epname = "Init";
   pthread_t tid;

   eDest = erp;
   if (XrdOucThread::Run(&tid, XrdXrootdInitAdmin, (void *)asock,
                         0, "Admin traffic"))
      {eDest->Emsg(epname, errno, "start admin");
       return 0;
      }
   return 1;
}

/******************************************************************************/
/*                                 L o g i n                                  */
/******************************************************************************/
  
void XrdXrootdAdmin::Login(int socknum)
{
   const char *epname = "Admin";
   char *request, *tp;
   int rc;

// Attach the socket FD to a stream
//
   Stream.SetEroute(eDest);
   Stream.AttachIO(socknum, socknum);

// The first request better be "login"
//
   if ((request = Stream.GetLine()))
      {if (!(tp = Stream.GetToken()) || strcmp("login", tp) || !do_Login())
          {eDest->Emsg(epname, "Invalid admin login sequence");
           return;
          }
       } else {eDest->Emsg(epname, "No admin login specified");
               return;
              }

// Document the login and go process the stream
//
   eDest->Emsg(epname, "Admin", TraceID, "logged in");
   Xeq();
}

/******************************************************************************/
/*                                 S t a r t                                  */
/******************************************************************************/
  
void *XrdXrootdAdmin::Start(XrdNetSocket *AdminSock)
{
   const char *epname = "Start";
   int InSock;
   pthread_t tid;

// Accept connections in an endless loop
//
   while(1) if ((InSock = AdminSock->Accept()) >= 0)
               {if (XrdOucThread::Run(&tid,XrdXrootdLoginAdmin,(void *)&InSock))
                   {eDest->Emsg(epname, errno, "start admin");
                    close(InSock);
                   }
               } else eDest->Emsg(epname, errno, "accept connection");
   return (void *)0;
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                              d o _ A b o r t                               */
/******************************************************************************/

int XrdXrootdAdmin::do_Abort()
{
   char *msg;
   int   mlen, rc;

// Handle: abort <target> [msg]
//
   if ((rc = getTarget("abort", &msg))) return rc;

// Get optional message
//
   msg = getMsg(msg, mlen);

// Send off the unsolicited response
//
   if (msg) return sendResp("abort", kXR_asyncab, msg, mlen);
            return sendResp("abort", kXR_asyncab);
}
 
/******************************************************************************/
/*                               d o _ C o n t                                */
/******************************************************************************/

int XrdXrootdAdmin::do_Cont()
{
   int rc;

// Handle: cont <target>
//
   if ((rc = getTarget("cont"))) return rc;

// Send off the unsolicited response
//
   return sendResp("cont", kXR_asyncgo);
}
  
/******************************************************************************/
/*                               d o _ D i s c                                */
/******************************************************************************/

int XrdXrootdAdmin::do_Disc()
{
   kXR_int32 msg[2];
   char *tp;
   int rc;

// Handle: disc <target> <wsec> <msec>
//
   if ((rc = getTarget("abort"))) return rc;

// Make sure times are specified
//
   if (!(tp = Stream.GetToken()) || !(msg[0] = strtol(tp, 0, 10)))
      return sendErr(8, "disc", " reconnect interval missing or invalid.");
   if (!(tp = Stream.GetToken()) || !(msg[1] = strtol(tp, 0, 10)))
      return sendErr(8, "disc", "reconnect timeout missing or invalid.");

// Send off the unsolicited response
//
   msg[0] = htonl(msg[0]); msg[1] = htonl(msg[1]);
   return sendResp("disc", kXR_asyncdi, (const char *)msg, sizeof(msg));
}
  
/******************************************************************************/
/*                              d o _ L o g i n                               */
/******************************************************************************/
  
int XrdXrootdAdmin::do_Login()
{
   char *tp;

// Process: login <name>
//
   if (!(tp = Stream.GetToken()))
      {eDest->Emsg("do_Login", "login name not specified");
       return 0;
      } else strlcpy(TraceID, tp, sizeof(TraceID));

 // All done
//
   return 1;
}
 
/******************************************************************************/
/*                                d o _ L s c                                 */
/******************************************************************************/

int XrdXrootdAdmin::do_Lsc()
{
   const char *fmt1 = "<resp id=\"%s\"><rc>0</rc><conn>";
   static int fmt1len = strlen(fmt1);
   const char *fmt2 = "</conn></resp>\n";
   static int fmt2len = strlen(fmt2);
   char buff[1024];
   const char *mdat[3] = {buff, " ", 0};
         int   mlen[3] = {0,      1, 0};
   int i, rc, curr = -1;

// Handle: list <target>
//
   if ((rc = getTarget("list"))) return rc;

// Send teh header of teh response
//
   i = sprintf(buff, fmt1, reqID);
   if (Stream.Put(buff, i)) return -1;

// Return back matching client list
//
   while((mlen[0] = XrdLink::getName(curr, buff, sizeof(buff), &Target)))
        if (Stream.Put(mdat, mlen)) return -1;
   return Stream.Put(fmt2, fmt2len);
}

/******************************************************************************/
/*                                d o _ M s g                                 */
/******************************************************************************/
  
int XrdXrootdAdmin::do_Msg()
{
   char *msg;
   int rc, mlen;

// Handle: msg <target> [msg]
//
   if ((rc = getTarget("msg", &msg))) return rc;

// Get optional message
//
   msg = getMsg(msg, mlen);

// Send off the unsolicited response
//
   if (msg) return sendResp("msg", kXR_asyncms, msg, mlen);
            return sendResp("msg", kXR_asyncms);
}
 
/******************************************************************************/
/*                              d o _ P a u s e                               */
/******************************************************************************/

int XrdXrootdAdmin::do_Pause()
{
   kXR_int32 msg;
   char *tp;
   int rc;

// Handle: pause <target> <wsec>
//
   if ((rc = getTarget("pause"))) return rc;

// Make sure time is specified
//
   if (!(tp = Stream.GetToken()) || !(msg = strtol(tp, 0, 10)))
      return sendErr(8, "pause", "time missing or invalid.");

// Send off the unsolicited response
//
   msg = htonl(msg);
   return sendResp("disc", kXR_asyncwt, (const char *)&msg, sizeof(msg));
}

/******************************************************************************/
/*                                d o _ R e d                                 */
/******************************************************************************/
  
int XrdXrootdAdmin::do_Red()
{
   struct msg {kXR_int32 port; char buff[8192];} myMsg;
   int rc, hlen, tlen, bsz;
   char *tp, *pn, *qq;

// Handle: redirect <target> <host>:<port>[?token]
//
   if ((rc = getTarget("redirect", 0))) return rc;

// Get the redirect target
//
   if (!(tp = Stream.GetToken()) || *tp == ':')
      return sendErr(8, "redirect", "destination host not specified.");

// Get the port number
//
   if (!(pn = index(tp, ':')) || !(myMsg.port = strtol(pn+1, &qq, 10)))
      return sendErr(8, "redirect", "port missing or invalid.");
   myMsg.port = htonl(myMsg.port);

// Copy out host
//
   *pn = '\0';
   if ((hlen = strlcpy(myMsg.buff,tp,sizeof(myMsg.buff))) >= sizeof(myMsg.buff))
      return sendErr(8, "redirect", "destination host too long.");

// Copy out the token
//
   if (qq && *qq == '?')
      {bsz = sizeof(myMsg.buff) - hlen;
       if ((tlen = strlcpy(myMsg.buff+hlen,qq,bsz)) >= bsz)
          return sendErr(8, "redirect", "token too long.");
      } else tlen = 0;

// Send off the unsolicited response
//
   return sendResp("redirect", kXR_asyncrd, (const char *)&myMsg, hlen+tlen+4);
}

/******************************************************************************/
/*                                g e t M s g                                 */
/******************************************************************************/
  
char *XrdXrootdAdmin::getMsg(char *msg, int &mlen)
{
   if (msg) while(*msg == ' ') msg++;
   if (msg && *msg)  mlen = strlen(msg)+1;
      else {msg = 0; mlen = 0;}
   return  msg;
}

/******************************************************************************/
/*                              g e t r e q I D                               */
/******************************************************************************/
  
int XrdXrootdAdmin::getreqID()
{
   char *tp;

   if (!(tp = Stream.GetToken()))
      {reqID[0] = '?'; reqID[1] = '\0';
       return sendErr(4, "request", "id not specified.");
      }

   if (strlen(tp) >= sizeof(reqID))
      {reqID[0] = '?'; reqID[1] = '\0';
       return sendErr(4, "request", "id too long.");
      }

   strcpy(reqID, tp);
   return 0;
}

/******************************************************************************/
/*                             g e t T a r g e t                              */
/******************************************************************************/
  
int XrdXrootdAdmin::getTarget(const char *act, char **rest)
{
   char *tp;

// Get the target
//
   if (!(tp = Stream.GetToken(rest)))
      return sendErr(8, act, "target not specified.");
   Target.Set(tp);
   return 0;
}
 
/******************************************************************************/
/*                               s e n d E r r                                */
/******************************************************************************/
  
int XrdXrootdAdmin::sendErr(int rc, const char *act, const char *msg)
{
   const char *fmt = "<resp id=\"%s\"><rc>%d</rc><msg>%s %s</msg></resp>\n";
   char buff[1024];
   int blen;

   blen = snprintf(buff, sizeof(buff)-1, fmt, reqID, rc, act, msg);
   buff[sizeof(buff)-1] = '\0';

   return Stream.Put(buff, blen);
}
 
/******************************************************************************/
/*                                s e n d O K                                 */
/******************************************************************************/
  
int XrdXrootdAdmin::sendOK(int sent)
{
   const char *fmt = "<resp id=\"%s\"><rc>0</rc><sent>%d</sent></resp>\n";
   char buff[1024];
   int blen;

   blen = snprintf(buff, sizeof(buff)-1, fmt, reqID, sent);
   buff[sizeof(buff)-1] = '\0';

   return Stream.Put(buff, blen);
}
 
/******************************************************************************/
/*                              s e n d R e s p                               */
/******************************************************************************/
  
int XrdXrootdAdmin::sendResp(const char *act, XActionCode anum)
{
   XrdLink *lp;
   const kXR_int32 net4 = htonl(4);
   int numsent = 0, curr = -1;

// Complete the response header
//
   usResp.act = htonl(anum);
   usResp.len = net4;

// Send off the messages
//
   while((lp = XrdLink::Find(curr, &Target)))
        {TRACE(RSP, "sending " <<lp->ID <<' ' <<act);
         if (lp->Send((const char *)&usResp, sizeof(usResp))>0) numsent++;
        }

// Now send the response to the admin guy
//
   return sendOK(numsent);
}

/******************************************************************************/
  
int XrdXrootdAdmin::sendResp(const char *act, XActionCode anum,
                             const char *msg, int msgl)
{
   struct iovec iov[2];
   XrdLink *lp;
   int numsent = 0, curr = -1, bytes = sizeof(usResp)+msgl;

// Complete the response header
//
   usResp.act = htonl(anum);
   usResp.len = htonl(msgl+4);

// Construct message vector
//
   iov[0].iov_base = (caddr_t)&usResp;
   iov[0].iov_len  = sizeof(usResp);
   iov[1].iov_base = (caddr_t)msg;
   iov[1].iov_len  = msgl;

// Send off the messages
//
   while((lp = XrdLink::Find(curr, &Target)))
        {TRACE(RSP, "sending " <<lp->ID <<' ' <<act <<' ' <<msg);
         if (lp->Send(iov, 2, bytes)>0) numsent++;
        }

// Now send the response to the admin guy
//
   return sendOK(numsent);
}
 
/******************************************************************************/
/*                                   X e q                                    */
/******************************************************************************/
  
void XrdXrootdAdmin::Xeq()
{
   const char *epname = "Xeq";
   int rc;
   char *request, *tp;

// Start receiving requests on this stream
// Format: <msgid> <cmd> <args>
//
   rc = 0;
   while((request = Stream.GetLine()) && !rc)
        {TRACE(DEBUG, "received admin request: '" <<request <<"'");
         if ((rc = getreqID())) continue;
         if ((tp = Stream.GetToken()))
            {     if (!strcmp("abort",    tp)) rc = do_Abort();
             else if (!strcmp("cont",     tp)) rc = do_Cont();
             else if (!strcmp("disc",     tp)) rc = do_Disc();
             else if (!strcmp("lsc",      tp)) rc = do_Lsc();
             else if (!strcmp("msg",      tp)) rc = do_Msg();
             else if (!strcmp("pause",    tp)) rc = do_Pause();
             else if (!strcmp("redirect", tp)) rc = do_Red();
             else {eDest->Emsg(epname, "invalid admin request,", tp);
                   rc = sendErr(4, tp, "is an invalid request.");
                  }
            }
        }

// The socket disconnected
//
   eDest->Emsg("Admin", "Admin", TraceID, "logged out");
   return;
}
