/******************************************************************************/
/*                                                                            */
/*                        X r d O l b A d m i n . c c                         */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//         $Id$

const char *XrdOlbAdminCVSID = "$Id$";

#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "XrdOlb/XrdOlbAdmin.hh"
#include "XrdOlb/XrdOlbConfig.hh"
#include "XrdOlb/XrdOlbManager.hh"
#include "XrdOlb/XrdOlbPrepare.hh"
#include "XrdOlb/XrdOlbTrace.hh"
#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucPlatform.hh"
#include "XrdOuc/XrdOucSocket.hh"
 
/******************************************************************************/
/*                     G l o b a l s   &   S t a t i c s                      */
/******************************************************************************/

extern int              XrdOlbSTDERR;

extern XrdOlbConfig     XrdOlbConfig;

extern XrdOlbPrepare    XrdOlbPrepQ;

extern XrdOucTrace      XrdOlbTrace;

extern XrdOlbManager    XrdOlbSM;

extern XrdOucError      XrdOlbSay;

       XrdOucMutex      XrdOlbAdmin::myMutex;
       XrdOucSemaphore *XrdOlbAdmin::SyncUp = 0;
       int              XrdOlbAdmin::nSync  = 0;
       int              XrdOlbAdmin::POnline= 0;
  

/******************************************************************************/
/*            E x t e r n a l   T h r e a d   I n t e r f a c e s             */
/******************************************************************************/
  
extern "C"
{
void *XrdOlbLoginAdmin(void *carg)
      {XrdOlbAdmin *Admin = new XrdOlbAdmin();
       Admin->Login(*(int *)carg);
       delete Admin;
       return (void *)0;
      }
}
 
/******************************************************************************/
/*                                 L o g i n                                  */
/******************************************************************************/
  
void XrdOlbAdmin::Login(int socknum)
{
   const char *epname = "Admin_Login";
   char *request, *tp;

// Attach the socket FD to a stream
//
   Stream.Attach(socknum);

// The first request better be "login"
//
   if ((request = Stream.GetLine()))
      {DEBUG("Initial admin request: '" <<request <<"'");
       if (!(tp = Stream.GetToken()) || strcmp("login", tp) || !do_Login())
          {XrdOlbSay.Emsg(epname, "Invalid admin login sequence");
           return;
          }
       } else {XrdOlbSay.Emsg(epname, "No admin login specified");
               return;
              }

// Document the login
//
   XrdOlbSay.Emsg(epname, (const char *)Stype, Sname, (char *)"logged in");

// Start receiving requests on this stream
//
   while((request = Stream.GetLine()))
        {DEBUG("received admin request: '" <<request <<"'");
         if ((tp = Stream.GetToken()))
            {     if (!strcmp("resume",   tp)) do_Resume();
             else if (!strcmp("rmdid",    tp)) do_RmDid();
             else if (!strcmp("suspend",  tp)) do_Suspend();
             else XrdOlbSay.Emsg(epname, "invalid admin request,", tp);
            }
        }

// The socket disconnected
//
   XrdOlbSay.Emsg("Login",(const char *)Stype, Sname, (char *)"logged out");

// If this is a primary, we must suspend but do not record this event!
//
   if (Primary) 
      {myMutex.Lock();
       XrdOlbSM.Suspend();
       POnline = 0;
       myMutex.UnLock();
      }
   return;
}

/******************************************************************************/
/*                                 N o t e s                                  */
/******************************************************************************/
  
void *XrdOlbAdmin::Notes(XrdOucSocket *AnoteSock)
{
   const char *epname = "Notes";
   char *request, *tp;
   int rc;

// Bind the udp socket to a stream
//
   Stream.Attach(AnoteSock->Detach());
   Sname = strdup("anon");

// Accept notifications in an endless loop
//
   do {while((request = Stream.GetLine()))
            {DEBUG("received notification: '" <<request <<"'");
             if ((tp = Stream.GetToken()))
                {     if (!strcmp("gone",    tp)) do_RmDid(1);
                 else if (!strcmp("have",    tp)) do_RmDud(1);
                 else if (!strcmp("nostage", tp)) do_NoStage();
                 else if (!strcmp("stage",   tp)) do_Stage();
                 else XrdOlbSay.Emsg(epname, "invalid notification,", tp);
                }
            }
       if ((rc = Stream.LastError())) break;
       rc = Stream.Detach(); Stream.Attach(rc);
      } while(1);

// We should never get here
//
   XrdOlbSay.Emsg(epname, rc, "accept notification");
   return (void *)0;
}

/******************************************************************************/
/*                                 S t a r t                                  */
/******************************************************************************/
  
void *XrdOlbAdmin::Start(XrdOucSocket *AdminSock)
{
   const char *epname = "Start";
   int InSock;
   pthread_t tid;

// If we are in independent mode then let the caller continue
//
   if (!XrdOlbConfig.doWait) {SyncUp->Post(); nSync = 1;}
      else XrdOlbSay.Emsg(epname, "Waiting for primary server to login.");

// Accept connections in an endless loop
//
   while(1) if ((InSock = AdminSock->Accept()) >= 0)
               {if (XrdOucThread_Run(&tid,XrdOlbLoginAdmin,(void *)&InSock))
                   {XrdOlbSay.Emsg(epname, errno, "start admin");
                    close(InSock);
                   }
               } else XrdOlbSay.Emsg(epname, errno, "accept connection");
   return (void *)0;
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                              d o _ L o g i n                               */
/******************************************************************************/
  
int XrdOlbAdmin::do_Login()
{
   char *tp, Ltype = '1';
   int Port = 0;

// Process: login {p | s | u} <name> [port <port>]
//
   if (!(tp = Stream.GetToken()))
      {XrdOlbSay.Emsg("do_Login", "login type not specified");
       return 0;
      }

   if (*(tp+1) == '\0')
      switch (*tp)
             {case 'p': Stype = (char *)"Primary server"; break;
              case 's': Stype = (char *)"Server";         break;
              case 'u': Stype = (char *)"Admin";          break;
              default:  Ltype = 0;                        break;
             }

   if (!Ltype)
      {XrdOlbSay.Emsg("do_Login", "Invalid login type,", tp);
       return 0;
      } else Ltype = *tp;

   if (!(tp = Stream.GetToken()))
      {XrdOlbSay.Emsg("do_Login", "login name not specified");
       return 0;
      } else Sname = strdup(tp);

// Get any additional options
//
   while((tp = Stream.GetToken()))
        {     if (!strcmp(tp, "port"))
                 {if (!(tp = Stream.GetToken()))
                     {XrdOlbSay.Emsg("do_Login", "login port not specified");
                      return 0;
                     }
                  if (XrdOuca2x::a2i(XrdOlbSay,"login port",tp,&Port,0))
                     return 0;
                 }
         else    {XrdOlbSay.Emsg("do_Login", "invalid login option -", tp);
                  return 0;
                 }
        }

// If this is not a primary, we are done. Otherwise there is much more
//
   if (Ltype != 'p') return 1;

// Discard login if this is a duplicate primary server
//
   myMutex.Lock();
   if (POnline && Ltype == 'p')
      {myMutex.UnLock();
       XrdOlbSay.Emsg("do_Login", "Primary server already logged in; login of", 
                                   tp, (char *)"rejected.");
       return 0;
      }

// Indicate we have a primary
//
   Primary = 1;
   POnline = 1;
   if (XrdOlbConfig.doWait) XrdOlbSM.setPort(Port);

// Check if this is the first primary login and resume if we must
//
   if (!nSync) 
      {SyncUp->Post();
       nSync = 1;
       myMutex.UnLock();
       return 1;
      }
   XrdOlbSM.Resume();
   myMutex.UnLock();

   return 1;
}
 
/******************************************************************************/
/*                            d o _ N o S t a g e                             */
/******************************************************************************/
  
void XrdOlbAdmin::do_NoStage()
{
   XrdOlbSay.Emsg("do_NoStage", "nostage requested by", Stype, Sname);
   XrdOlbSM.Stage(0);
   close(open(XrdOlbConfig.NoStageFile, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR));
}
 
/******************************************************************************/
/*                             d o _ R e s u m e                              */
/******************************************************************************/
  
void XrdOlbAdmin::do_Resume()
{
   XrdOlbSay.Emsg("do_Resume", "resume requested by", Stype, Sname);
   unlink(XrdOlbConfig.SuspendFile);
   XrdOlbSM.Resume();
}
 
/******************************************************************************/
/*                              d o _ R m D i d                               */
/******************************************************************************/
  
void XrdOlbAdmin::do_RmDid(int dotrim)
{
   const char *epname = "do_RmDid";
   const char *cmd = "gone ";
   const int   cmdl= strlen(cmd);
   char *tp;

   if (!(tp = Stream.GetToken()))
      {XrdOlbSay.Emsg(epname,"removed path not specified by",Stype,Sname);
       return;
      }

   if (dotrim && XrdOlbConfig.LocalRLen && !(tp = TrimPath(tp)))
      {XrdOlbSay.Emsg(epname, "removed path is null as specified by",
                                  Stype,Sname);
       return;
      }

   XrdOlbPrepQ.Gone(tp);

   DEBUG("Sending managers " <<cmd <<tp);
   XrdOlbSM.Inform(cmd, cmdl, tp, 0);
}
 
/******************************************************************************/
/*                              d o _ R m D u d                               */
/******************************************************************************/
  
void XrdOlbAdmin::do_RmDud(int dotrim)
{
   const char *epname = "do_RmDud";
   const char *cmd = "have ? ";
   const int   cmdl= strlen(cmd);
   char *tp;

   if (!(tp = Stream.GetToken()))
      {XrdOlbSay.Emsg(epname,"removed path not specified by",Stype,Sname);
       return;
      }

   if (dotrim && XrdOlbConfig.LocalRLen && !(tp = TrimPath(tp)))
      {XrdOlbSay.Emsg(epname, "removed path is null as specified by",
                                  Stype,Sname);
       return;
      }

   DEBUG("Sending managers " <<cmd <<tp);
   XrdOlbSM.Inform(cmd, cmdl, tp, 0);
}
 
/******************************************************************************/
/*                              d o _ S t a g e                               */
/******************************************************************************/
  
void XrdOlbAdmin::do_Stage()
{
   XrdOlbSay.Emsg("do_Stage", "stage requested by", Stype, Sname);
   XrdOlbSM.Stage(1);
   unlink(XrdOlbConfig.NoStageFile);
}
  
/******************************************************************************/
/*                            d o _ S u s p e n d                             */
/******************************************************************************/
  
void XrdOlbAdmin::do_Suspend()
{
   XrdOlbSay.Emsg("do_Suspend", "suspend requested by", Stype, Sname);
   XrdOlbSM.Suspend();
   close(open(XrdOlbConfig.SuspendFile, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR));
}
 
/******************************************************************************/
/*                              T r i m P a t h                               */
/******************************************************************************/
  
char *XrdOlbAdmin::TrimPath(char *path)
{
  if (strncmp(path, XrdOlbConfig.LocalRoot,XrdOlbConfig.LocalRLen)) return path;
  if (path[XrdOlbConfig.LocalRLen] != '/')
      return (path[XrdOlbConfig.LocalRLen] ? path : (char *)0);
  return path+XrdOlbConfig.LocalRLen;
}
