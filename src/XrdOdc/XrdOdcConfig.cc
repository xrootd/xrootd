/******************************************************************************/
/*                                                                            */
/*                       X r d O d c C o n f i g . c c                        */
/*                                                                            */
/* (C) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC03-76-SFO0515 with the Deprtment of Energy             */
/******************************************************************************/

//          $Id$

const char *XrdOdcConfigCVSID = "$Id$";

#include <unistd.h>
#include <ctype.h>
#include <iostream.h>
#include <strings.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <fcntl.h>

#include "XrdOdc/XrdOdcConfig.hh"
#include "XrdOdc/XrdOdcMsg.hh"
#include "XrdOdc/XrdOdcTrace.hh"
#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucNetwork.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucTList.hh"

/******************************************************************************/
/*                               d e f i n e s                                */
/******************************************************************************/

#define TS_Xeq(x,m)    if (!strcmp(x,var)) return m(eDest, Config);

#define ODC_Prefix    "odc."
#define ODC_PrefLen   sizeof(ODC_Prefix)-1

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdOdcConfig::~XrdOdcConfig()
{
  XrdOucTList *tp, *tpp;

  tpp = ManList;
  while((tp = tpp)) {tpp = tp->next; delete tp;}
  tpp = PanList;
  while((tp = tpp)) {tpp = tp->next; delete tp;}
}

/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
int XrdOdcConfig::Configure(char *cfn, const char *mode)
{
/*
  Function: Establish configuration at start up time.

  Input:    None.

  Output:   0 upon success or !0 otherwise.
*/
   int NoGo = 0;
   char *temp;

// Tell the world we have started
//
   eDest->Emsg("Config", mode, (char *)"redirection initialization started");

// Process the configuration file
//
   if (!(NoGo = ConfigProc(cfn)))
           if (*mode == 'L')
              {if (!lclPort || !portVec[0])
                  {eDest->Emsg("Config","No ports for local balancing specified.");
                   NoGo=1;
                  }
              }
      else if (*mode == 'P')
              {if (!PanList)
                  {eDest->Emsg("Config", "Proxy manager not specified.");
                   NoGo=1;
                  }
              }
      else if (*mode == 'R')
              {if (!ManList)
                  {eDest->Emsg("Config", "Remote manager not specified.");
                   NoGo=1;
                  }
              }

   if (!OLBPath) OLBPath = strdup("/tmp/.olb/olbd.admin");
   RepWaitMS = RepWait * 1000;

// All done
//
   temp = (NoGo ? (char *)"failed." : (char *)"completed.");
   eDest->Emsg("Config", "Distributed cache initialization", temp);
   return NoGo;
}

/******************************************************************************/
/*                     P r i v a t e   F u n c t i o n s                      */
/******************************************************************************/
/******************************************************************************/
/*                            C o n f i g P r o c                             */
/******************************************************************************/
  
int XrdOdcConfig::ConfigProc(char *ConfigFN)
{
  char *var;
  int  cfgFD, retc, NoGo = 0;
  XrdOucStream Config(eDest);

// Make sure we have a config file
//
   if (!ConfigFN || !*ConfigFN)
      {eDest->Emsg("Config", "odc configuration file not specified.");
       return 1;
      }

// Try to open the configuration file.
//
   if ( (cfgFD = open(ConfigFN, O_RDONLY, 0)) < 0)
      {eDest->Emsg("Config", errno, "open config file", ConfigFN);
       return 1;
      }
   Config.Attach(cfgFD);

// Now start reading records until eof.
//
   while((var = Config.GetFirstWord()))
        {if (!strncmp(var, ODC_Prefix, ODC_PrefLen))
            {var += ODC_PrefLen;
             NoGo |= ConfigXeq(var, Config);
            }
            else if (!strcmp(var, "olb.adminpath"))
                    NoGo |= xapath(eDest, Config);
        }

// Now check if any errors occured during file i/o
//
   if ((retc = Config.LastError()))
      NoGo = eDest->Emsg("Config", retc, "read config file", ConfigFN);
   Config.Close();

// Return final return code
//
   return NoGo;
}

/******************************************************************************/
/*                             C o n f i g X e q                              */
/******************************************************************************/

int XrdOdcConfig::ConfigXeq(char *var, XrdOucStream &Config)
{

   // Process items. for either a local or a remote configuration
   //
   TS_Xeq("conwait",       xconw);
   TS_Xeq("manager",       xmang);
   TS_Xeq("msgkeep",       xmsgk);
   TS_Xeq("olbapath",      xapath);
   TS_Xeq("portbal",       xpbal);
   TS_Xeq("portsel",       xpsel);
   TS_Xeq("request",       xreqs);
   TS_Xeq("trace",         xtrac);

   // No match found, complain.
   //
   eDest->Emsg("Config", "Warning, unknown directive", var);
   return 0;
}


/******************************************************************************/
/*                                x a p a t h                                 */
/******************************************************************************/

/* Function: xapath

   Purpose:  To parse the directive: adminpath <path> [ group ]

             <path>    the path of the named socket to use for admin requests.
             group     allow group access to the path.

   Type: Manager only, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/
  
int XrdOdcConfig::xapath(XrdOucError *errp, XrdOucStream &Config)
{
    struct sockaddr_un USock;
    char *pval;

// Get the path
//
   pval = Config.GetWord();
   if (!pval || !pval[0])
      {errp->Emsg("Config", "olb admin path not specified"); return 1;}

// Make sure it's an absolute path
//
   if (*pval != '/')
      {errp->Emsg("Config", "olb admin path not absolute"); return 1;}

// Make sure path is not too long
//
   if (strlen(pval) > sizeof(USock.sun_path))
      {errp->Emsg("Config", "olb admin path is too long.");
       return 1;
      }

// Record the path
//
   if (OLBPath) free(OLBPath);
   OLBPath = strdup(pval);
   return 0;
}

/******************************************************************************/
/*                                 x c o n w                                  */
/******************************************************************************/

/* Function: xconw

   Purpose:  To parse the directive: conwait <sec>

             <sec>   number of seconds to wait for a manager connection

   Type: Remote server only, dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOdcConfig::xconw(XrdOucError *errp, XrdOucStream &Config)
{
    char *val;
    int cw;

    if (!(val = Config.GetWord()))
       {errp->Emsg("Config", "conwait value not specified."); return 1;}

    if (XrdOuca2x::a2tm(*errp,"conwait value",val,&cw,1)) return 1;

    ConWait = cw;
    return 0;
}
  
/******************************************************************************/
/*                                 x m a n g                                  */
/******************************************************************************/

/* Function: xmang

   Purpose:  To parse directive: manager [proxy] [all|any] <host>[+] [<port>]

             proxy  This is a proxy service manager not cache space manager.
             all    Distribute requests across all managers.
             any    Choose different manager only when necessary (default).
             <host> The dns name of the host that is the cache manager.
                    If the host name ends with a plus, all addresses that are
                    associated with the host are treated as managers.

             <port> The port number to use for this host.

   Notes:   Any number of manager directives can be given. The finder will
            load balance amongst all of them.

   Type: Remote server only, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOdcConfig::xmang(XrdOucError *errp, XrdOucStream &Config)
{
    struct sockaddr_in InetAddr[8];
    XrdOucTList *tp = 0;
    char *val, *bval = 0, *mval = 0;
    int i, port, isProxy = 0, smode = ODC_FAILOVER;

    SMode = 0;
    do {if (!(val = Config.GetWord()))
           {errp->Emsg("Config","manager host name not specified"); return 1;}
             if (!isProxy && !strcmp("proxy", val)) isProxy = 1;
        else if (!SMode && !strcmp("any", val)) smode = ODC_FAILOVER;
        else if (!SMode && !strcmp("all", val)) smode = ODC_ROUNDROB;
        else mval = strdup(val);
       } while(!mval);
    if (isProxy) SModeP = smode;
       else      SMode  = smode;

    if ((val = Config.GetWord()))
       if (isdigit(*val))
           {if (XrdOuca2x::a2i(*errp,"manager port",val,&port,1,65535))
               port = 0;
           }
           else if (!(port = XrdOucNetwork::findPort(val, "tcp")))
                   {errp->Emsg("Config", "unable to find tcp service", val);
                    port = 0;
                   }
       else errp->Emsg("Config","manager port not specified for",mval);

    if (lclPort) return 0; // Local configs don't need a manager

    if (!port) {free(mval); return 1;}

    i = strlen(mval);
    if (mval[i-1] != '+') i = 0;
        else {bval = strdup(mval); mval[i-1] = '\0';
              if (!(i = XrdOucNetwork::getHostAddr(mval, InetAddr, 8)))
                 {errp->Emsg("Config","Manager host", mval,
                             (char *)"not found");
                  free(bval); free(mval); return 1;
                 }
             }

    do {if (i)
           {i--; free(mval);
            mval = XrdOucNetwork::getHostName(InetAddr[i]);
            errp->Emsg("Config", (const char *)bval,
                       (char *)"-> odc.manager", mval);
           }
        tp = (isProxy ? PanList : ManList);
        while(tp) 
             if (strcmp(tp->text, mval) || tp->val != port) tp = tp->next;
                else {errp->Emsg("Config","Duplicate manager",mval);
                      break;
                     }
        if (tp) break;
        if (isProxy) PanList = new XrdOucTList(mval, port, PanList);
           else      ManList = new XrdOucTList(mval, port, ManList);
       } while(i);

    if (bval) free(bval);
    free(mval);
    return tp != 0;
}

/******************************************************************************/
/*                                 x m s g k                                  */
/******************************************************************************/

/* Function: xmsgk

   Purpose:  To parse the directive: msgkeep <num>

             <num>   number of msg blocks to keep for re-use

   Type: Server only, dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOdcConfig::xmsgk(XrdOucError *errp, XrdOucStream &Config)
{
    char *val;
    int mk;

    if (!(val = Config.GetWord()))
       {errp->Emsg("Config", "msgkeep value not specified."); return 1;}

    if (XrdOuca2x::a2i(*errp, "msgkeep value", val, &mk, 60)) return 1;

    XrdOdcMsg::setKeep(mk);
    return 0;
}

/******************************************************************************/
/*                                 x p b a l                                  */
/******************************************************************************/

/* Function: xpbal

   Purpose:  To parse the directive: portbal <port1> <port2> [<port3> [...]]

             <portx> number of a port to balance. There must be at least two.

   Type: Local server only, dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOdcConfig::xpbal(XrdOucError *errp, XrdOucStream &Config)
{
    char *val;
    int pv, xport = 0, pi = 0, pimax = sizeof(portVec)/sizeof(portVec[0]);

    if (!lclPort) return 0;

    while((val = Config.GetWord()))
         {if (XrdOuca2x::a2i(*errp,"portbal port value",val,&pv,1,65535))
             return 1;
          if (pi >= pimax)
             {errp->Emsg("Config","too many portbal ports specified.");
              return 1;
             }
          portVec[pi++] = pv;
          if (pv == lclPort) xport = 1;
         };

    if (pi < 2) {errp->Emsg("Config","portbal needs two or more ports.");
                 return 1;
                }
    if (!xport) {errp->Emsg("Config", "portbal does not include this server.");
                 return 1;
                }

    portVec[pi] = 0;
    return 0;
}
  
/******************************************************************************/
/*                                 x p s e l                                  */
/******************************************************************************/

/* Function: xpsel

   Purpose:  To parse the directive: portsel <sched>
                                             [cpu <cpu>] [int <int>] [key <key>]

             <sched>  is the selection algorithm to be used:
                      fd - selects port whose server has the least files open
                      ld - uses a combination of cpu usage and fd
                      rr - round robbin scheduling

             <cpu>    the nummber of cpu's in this configuration. If not
                      specified, the value comes from variable xrd_NUMCPU.
                      if not set then the value is assumed to be 1.
             <int>    The monitoring interval in seconds. The default is 60.
             <key>    is the shared memory key to be used to hold the data.
                      The default is 1312.

   Output: 0 upon success or !0 upon failure.

   Type: Local server only, dynamic.
*/

int XrdOdcConfig::xpsel(XrdOucError *Eroute, XrdOucStream &Config)
{
    const char *emsg = "invalid portsel value";
    char *val;
    int  i, ppp, retc;;
    struct selopts {const char *opname; int *oploc; int opval;} Sopts[] =
       {
        {"key",      &pselSkey,   0},
        {"int",      &pselMint,   1}
       };
    int numopts = sizeof(Sopts)/sizeof(struct selopts);

    if (!(val = Config.GetWord()))
       {Eroute->Emsg("Config", "portsel arguments not specified"); return 1;}

         if (!strcmp("fd", val)) pselType = selByFD;
    else if (!strcmp("ld", val)) pselType = selByLD;
    else if (!strcmp("rr", val)) pselType = selByRR;
    else    {Eroute->Emsg("Config", "invalid portsel scheduling -", val);
             return 1;
            }

    while (val)
          {for (i = 0; i < numopts; i++)
               if (!strcmp(val, Sopts[i].opname))
                  {if (!(val = Config.GetWord()))
                      {Eroute->Emsg("Config","monitor ",(char *)Sopts[i].opname,
                                   (char *)" argument not specified.");
                       return 1;
                      }
                   if (Sopts[i].opval)
                           retc = XrdOuca2x::a2tm(*Eroute,emsg,val,&ppp,1);
                      else retc = XrdOuca2x::a2i( *Eroute,emsg,val,&ppp,1);
                   if (retc) return 1;
                   *Sopts[i].oploc = ppp;
                   break;
                  }
           if (i >= numopts)
              Eroute->Emsg("Config", "Warning, invalid portsel option", val);
           val = Config.GetWord();
          }
   return 0;
}
  
/******************************************************************************/
/*                                 x r e q s                                  */
/******************************************************************************/

/* Function: xreqs

   Purpose:  To parse the directive: request [repwait <sec1>] [delay <sec2>]

             <sec1>  number of seconds to wait for a locate reply
             <sec2>  number of seconds to delay a retry upon failure

   Type: Remote server only, dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOdcConfig::xreqs(XrdOucError *errp, XrdOucStream &Config)
{
    char *val;
    static struct reqsopts {const char *opname; int *oploc;} rqopts[] =
       {
        {"delay",    &RepDelay},
        {"repwait",  &RepWait}
       };
    int i, ppp, numopts = sizeof(rqopts)/sizeof(struct reqsopts);

    if (!(val = Config.GetWord()))
       {errp->Emsg("Config", "request arguments not specified"); return 1;}

    while (val)
    do {for (i = 0; i < numopts; i++)
            if (!strcmp(val, rqopts[i].opname))
               { if (!(val = Config.GetWord()))
                  {errp->Emsg("Config", 
                      "request argument value not specified"); 
                   return 1;}
                   if (XrdOuca2x::a2i(*errp,"request value",val,&ppp,1))
                   return 1;
                   else *rqopts[i].oploc = ppp;
                break;
               }
        if (i >= numopts) errp->Emsg("Config","invalid request option",val);
       } while((val = Config.GetWord()));
     return 0;
}
  
/******************************************************************************/
/*                                x t r a c e                                 */
/******************************************************************************/

/* Function: xtrace

   Purpose:  To parse the directive: trace <events>

             <events> the blank separated list of events to trace. Trace
                      directives are cummalative.

   Output: retc upon success or -EINVAL upon failure.
*/

int XrdOdcConfig::xtrac(XrdOucError *Eroute, XrdOucStream &Config)
{
  
    extern XrdOucTrace OdcTrace;
    char  *val;
    static struct traceopts {const char *opname; int opval;} tropts[] =
       {
        {"all",      TRACE_ALL},
        {"debug",    TRACE_Debug},
        {"forward",  TRACE_Forward},
        {"redirect", TRACE_Redirect}
       };
    int i, neg, trval = 0, numopts = sizeof(tropts)/sizeof(struct traceopts);

    if (!(val = Config.GetWord()))
       {Eroute->Emsg("config", "trace option not specified"); return 1;}
    while (val)
         {if (!strcmp(val, "off")) trval = 0;
             else {if ((neg = (val[0] == '-' && val[1]))) val++;
                   for (i = 0; i < numopts; i++)
                       {if (!strcmp(val, tropts[i].opname))
                           {if (neg) trval &= ~tropts[i].opval;
                               else  trval |=  tropts[i].opval;
                            break;
                           }
                       }
                   if (i >= numopts)
                      Eroute->Emsg("config", "invalid trace option", val);
                  }
          val = Config.GetWord();
         }
    OdcTrace.What = trval;
    return 0;
}
