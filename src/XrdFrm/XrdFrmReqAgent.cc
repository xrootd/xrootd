/******************************************************************************/
/*                                                                            */
/*                     X r d F r m R e q A g e n t . c c                      */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//          $Id$

const char *XrdFrmReqAgentCVSID = "$Id$";

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "XrdFrm/XrdFrmConfig.hh"
#include "XrdFrm/XrdFrmReqAgent.hh"
#include "XrdFrm/XrdFrmReqBoss.hh"
#include "XrdFrm/XrdFrmReqFile.hh"
#include "XrdFrm/XrdFrmRequest.hh"
#include "XrdFrm/XrdFrmTrace.hh"
#include "XrdNet/XrdNetMsg.hh"
#include "XrdNet/XrdNetOpts.hh"
#include "XrdNet/XrdNetSocket.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPlatform.hh"

using namespace XrdFrm;

/******************************************************************************/
/* Private:                          A d d                                    */
/******************************************************************************/
  
void XrdFrmReqAgent::Add(XrdOucStream  &Request, char *Tok,
                         XrdFrmReqBoss &Server)
{
   XrdFrmRequest myReq;
   const char *Miss = 0;
   char *tp, *op, PingMsg[3];

// Handle: op[<traceid>] <requestid> <npath> <prty> <mode> <path> [. . .]
//
// op: + | & | ^ | < | = | >
//
   memset(&myReq, 0, sizeof(myReq));
   myReq.OPc[0] = *Tok; myReq.OPc[1] = ' ';
   PingMsg[1]   = *Tok;
   if (*Tok == '=' || *Tok == '^') myReq.Options |= XrdFrmRequest::Purge;
   Tok++;

   if (*Tok) strlcpy(myReq.User, Tok, sizeof(myReq.User));
      else   strlcpy(myReq.User, Config.myProg, sizeof(myReq.User));

   if (!(tp = Request.GetToken())) Miss = "request id";
      else strlcpy(myReq.ID, tp, sizeof(myReq.ID));

   if (!Miss)
      {if (!(tp = Request.GetToken())) Miss = "notify path";
          else strlcpy(myReq.Notify, tp, sizeof(myReq.Notify));
      }

   if (!Miss)
      {if (!(tp = Request.GetToken())) Miss = "priority";
          else {myReq.Prty = atoi(tp);
                if (myReq.Prty < 0) myReq.Prty = 0;
                   else if (myReq.Prty > XrdFrmRequest::maxPrty)
                            myReq.Prty = XrdFrmRequest::maxPrty;
               }
      }

   if (!Miss)
      {if (!(tp = Request.GetToken())) Miss = "mode";
          else {if (index(tp,'w')) myReq.Options |= XrdFrmRequest::makeRW;
                if (*myReq.Notify != '-')
                   {if (index(tp,'s') ||  index(tp,'n'))
                       myReq.Options |= XrdFrmRequest::msgSucc;
                    if (index(tp,'f') || !index(tp,'q'))
                       myReq.Options |= XrdFrmRequest::msgFail;
                   }
               }
      }

   if (!Miss && !(tp = Request.GetToken())) Miss = "path";

// Check for any errors
//
   if (Miss) {Say.Emsg("Agent_Add", Miss, "missing in '+' request.");
              return;
             }

// Add all paths in the request
//
   do {strlcpy(myReq.LFN, tp, sizeof(myReq.LFN));
       if ((op = index(tp, '?'))) myReq.Opaque = op-tp;
          else myReq.Opaque = 0;
       myReq.LFO = 0;
       if (myReq.LFN[0] != '/' && !(myReq.LFO = chkURL(myReq.LFN)))
          Say.Emsg("Agent_Add", "Invalid url -", myReq.LFN);
          else Server.Add(myReq);
       if ((tp = Request.GetToken())) memset(myReq.LFN, 0, sizeof(myReq.LFN));
      } while(tp);

// Wake up the server
//
   PingMsg[0] = '!'; PingMsg[2] = 0;
   Ping(PingMsg);
}

/******************************************************************************/
/* Private:                         B o s s                                   */
/******************************************************************************/

XrdFrmReqBoss *XrdFrmReqAgent::Boss(char bType)
{

// Return the boss corresponding to the type
//
   switch(bType)
         {case 0  : return &PreStage;
          case '+': return &PreStage;
          case '^':
          case '&': return &Migrated;
          case '<': return &GetFiler;
          case '=':
          case '>': return &PutFiler;
          default:  break;
         }
   return 0;
}
  
/******************************************************************************/
/* Private:                       c h k U R L                                 */
/******************************************************************************/
  
int XrdFrmReqAgent::chkURL(const char *Url)
{
   const char *Elem;

// Verify that this is a valid url and return offset to the lfn
//
   if (!(Elem = index(Url, ':'))) return 0;
   if (Elem[1] != '/' || Elem[2] != '/') return 0;
   if (!(Elem = index(Elem+3, '/')) || Elem[1] != '/') return 0;
   Elem++;

// At this point ignore all leading slashes but one
//
   while(Elem[1] == '/') Elem++;
   return Elem - Url;
}

/******************************************************************************/
/* Private:                          D e l                                    */
/******************************************************************************/
  
void XrdFrmReqAgent::Del(XrdOucStream  &Request, char *Tok,
                         XrdFrmReqBoss &Server)
{
   XrdFrmRequest myReq;

// If the requestid is adjacent to the operation, use it o/w get it
//
   if (!(*Tok) && (!(Tok = Request.GetToken()) || !(*Tok)))
      {Say.Emsg("Del", "request id missing in cancel request.");
       return;
      }

// Copy the request ID into the request and remove it from peer server
//
   memset(&myReq, 0, sizeof(myReq));
   strlcpy(myReq.ID, Tok, sizeof(myReq.ID));
   Server.Del(myReq);
}

/******************************************************************************/
/* Private:                         L i s t                                   */
/******************************************************************************/
  
void XrdFrmReqAgent::List(XrdOucStream &Request, char *Tok)
{
   static struct ITypes {const char *IName; XrdFrmReqFile::Item ICode;}
                 ITList[] = {{"lfn",    XrdFrmReqFile::getLFN},
                             {"lfncgi", XrdFrmReqFile::getLFNCGI},
                             {"mode",   XrdFrmReqFile::getMODE},
                             {"obj",    XrdFrmReqFile::getOBJ},
                             {"objcgi", XrdFrmReqFile::getOBJCGI},
                             {"op",     XrdFrmReqFile::getOP},
                             {"prty",   XrdFrmReqFile::getPRTY},
                             {"qwt",    XrdFrmReqFile::getQWT},
                             {"rid",    XrdFrmReqFile::getRID},
                             {"tod",    XrdFrmReqFile::getTOD},
                             {"note",   XrdFrmReqFile::getNOTE},
                             {"tid",    XrdFrmReqFile::getUSER}};
   static const int ITNum = sizeof(ITList)/sizeof(struct ITypes);

   XrdFrmReqFile::Item Items[ITNum];
   XrdFrmReqBoss *bossP;
   int n = 0, i;
   char *tp;

   while((tp = Request.GetToken()) && n <= ITNum)
        {for (i = 0; i < ITNum; i++)
             if (!strcmp(tp, ITList[i].IName))
                {Items[n++] = ITList[i].ICode; break;}
        }

// List entries queued for specific servers
//
   if (!(*Tok)) {PreStage.List(Items, n); GetFiler.List(Items, n);}
      else do {if ((bossP = Boss(*Tok))) bossP->List(Items, n);
              } while(*(++Tok));
   cout <<endl;
}

/******************************************************************************/
/* Private:                         P i n g                                   */
/******************************************************************************/

void XrdFrmReqAgent::Ping(char *Tok)
{
   static XrdNetMsg udpMsg(&Say, Config.c2sFN);
   static int udpOK = 0;
   struct stat buf;
   int n;

// Setup the message and send it
//
   n = strlen(Tok);
   Tok[n] = '\n';
   if (udpOK || !stat(Config.c2sFN, &buf)) {udpMsg.Send(Tok, n+1); udpOK = 1;}
   Tok[n] = '\0';
}

/******************************************************************************/
/* Public:                          P o n g                                   */
/******************************************************************************/
  
void *XrdFrmReqAgentUDP(void *parg)
{
    XrdFrmReqAgent::Pong();
    return (void *)0;
}
  
void XrdFrmReqAgent::Pong()
{
   EPNAME("Pong");
   static int udpFD = -1;
   XrdOucStream Request(&Say);
   XrdFrmReqBoss *bossP;
   char *tp;

// Get a UDP socket for the server if we haven't already done so and start
// a thread to re-enter this code and wait for messages from an agent.
//
   if (udpFD < 0)
      {XrdNetSocket *udpSock;
       pthread_t tid;
       int retc;
       if ((udpSock = XrdNetSocket::Create(&Say, Config.AdminPath,
                   "xfrd.udp", Config.AdminMode, XRDNET_UDPSOCKET)))
          {udpFD = udpSock->Detach(); delete udpSock;
           if ((retc = XrdSysThread::Run(&tid, XrdFrmReqAgentUDP, (void *)0,
                                         XRDSYSTHREAD_BIND, "Agent")))
              Say.Emsg("main", retc, "create udp agent listner");
          }
       return;
      }

// Hookup to the udp socket as a stream
//
   Request.Attach(udpFD, 64*1024);

// Now simply get requests (see XrdFrmReqAgent for details). Here we screen
// out ping and list requests.
//
   while((tp = Request.GetLine()))
        {DEBUG(": '" <<tp <<"'");
         switch(*tp)
               {case '?': break;
                case '!': if ((tp = Request.GetToken()))
                             while(*tp++)
                                  {if ((bossP = Boss(*tp))) bossP->Wakeup(1);}
                          break;
                default:  Process(Request);
               }
        }

// We should never get here (but....)
//
   Say.Emsg("Server", "Lost udp connection!");
}

/******************************************************************************/
/* Public:                       P r o c e s s                                */
/******************************************************************************/
  
void XrdFrmReqAgent::Process(XrdOucStream &Request)
{
   char *tp;

// Each frm request comes in as:
//
// Copy in:    <[<traceid>] <reqid> <npath> <prty> <mode> <path> [. . .]
// Copy purge: =[<traceid>] <reqid> <npath> <prty> <mode> <path> [. . .]
// Copy out:   >[<traceid>] <reqid> <npath> <prty> <mode> <path> [. . .]
// Migrate:    &[<traceid>] <reqid> <npath> <prty> <mode> <path> [. . .]
// Migr+Purge: ^[<traceid>] <reqid> <npath> <prty> <mode> <path> [. . .]
// Stage:      +[<traceid>] <reqid> <npath> <prty> <mode> <path> [. . .]
// Cancel in:  - <requestid>
// Cancel out: ~ <requestid>
// List:       ?[<][+][&][>]
// Wakeup:     ![<][+][&][>]
//
   if ((tp = Request.GetToken()))
      switch(*tp)
            {case '+':  Add(Request, tp,   PreStage); break;
             case '<':  Add(Request, tp,   GetFiler); break;
             case '=':
             case '>':  Add(Request, tp,   PutFiler); break;
             case '&':
             case '^':  Add(Request, tp,   Migrated); break;
             case '-':  Del(Request, tp+1, PreStage);
                        Del(Request, tp+1, GetFiler);
                        break;
             case '~':  Del(Request, tp+1, Migrated);
                        Del(Request, tp+1, PutFiler);
                        break;
             case '?': List(Request, tp+1);           break;
             case '!': Ping(tp);                      break;
             default: Say.Emsg("Agent", "Invalid request, '", tp, "'.");
            }
}

/******************************************************************************/
/* Public:                         S t a r t                                  */
/******************************************************************************/
  
int XrdFrmReqAgent::Start()
{
   EPNAME("Agent");
   XrdOucStream Request;
   char *tp;

// Attach stdin to the Request stream
//
   Request.Attach(STDIN_FILENO, 8*1024);

// Process all input
//
   while((tp = Request.GetLine()))
        {DEBUG ("Request: '" <<tp <<"'");
         Process(Request);
        }

// If we exit then we lost the connection
//
   Say.Emsg("Agent", "Exiting; lost request connection!");
   return 8;
}
