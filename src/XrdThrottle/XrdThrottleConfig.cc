/******************************************************************************/
/*                                                                            */
/* (c) 2025 by the Morgridge Institute for Research                           */
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

#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdThrottle/XrdThrottleConfig.hh"
#include "XrdThrottle/XrdThrottleTrace.hh"

#include <cstring>
#include <string>
#include <fcntl.h>

using namespace XrdThrottle;

#define TS_Xeq(key, func) NoGo = (strcmp(key, var) == 0) ? func(Config) : 0
int
Configuration::Configure(const std::string &config_file)
{
    XrdOucEnv myEnv;
    XrdOucStream Config(&m_log, getenv("XRDINSTANCE"), &myEnv, "(Throttle Config)> ");
    int cfgFD;
    if (config_file.empty()) {
        m_log.Say("No filename specified.");
        return 1;
    }
    if ((cfgFD = open(config_file.c_str(), O_RDONLY)) < 0) {
        m_log.Emsg("Config", errno, "Unable to open configuration file", config_file.c_str());
        return 1;
    }
    Config.Attach(cfgFD);
    static const char *cvec[] = { "*** throttle (ofs) plugin config:", 0 };
    Config.Capture(cvec);

    char *var, *val;
    int NoGo = 0;
    while( (var = Config.GetMyFirstWord()) )
    {
        if (!strcmp("throttle.fslib", var)) {
            val = Config.GetWord();
            if (!val || !val[0]) {m_log.Emsg("Config", "fslib not specified."); continue;}
            m_fslib = val;
        }
        TS_Xeq("throttle.max_open_files", xmaxopen);
        TS_Xeq("throttle.max_active_connections", xmaxconn);
        TS_Xeq("throttle.throttle", xthrottle);
        TS_Xeq("throttle.loadshed", xloadshed);
        TS_Xeq("throttle.max_wait_time", xmaxwait);
        TS_Xeq("throttle.trace", xtrace);
        if (NoGo)
        {
            m_log.Emsg("Config", "Throttle configuration failed.");
            return 1;
        }
    }
    return 0;
}

/******************************************************************************/
/*                            x m a x o p e n                                 */
/******************************************************************************/

/* Function: xmaxopen

   Purpose:  Parse the directive: throttle.max_open_files <limit>

             <limit>   maximum number of open file handles for a unique entity.

  Output: 0 upon success or !0 upon failure.
*/
int
Configuration::xmaxopen(XrdOucStream &Config)
{
    auto val = Config.GetWord();
    if (!val || val[0] == '\0')
       {m_log.Emsg("Config", "Max open files not specified!  Example usage: throttle.max_open_files 16000");}
    long long max_open = -1;
    if (XrdOuca2x::a2sz(m_log, "max open files value", val, &max_open, 1)) return 1;

    m_max_open = max_open;
    return 0;
}


/******************************************************************************/
/*                            x m a x c o n n                                 */
/******************************************************************************/

/* Function: xmaxconn

   Purpose:  Parse the directive: throttle.max_active_connections <limit>

             <limit>   maximum number of connections with at least one open file for a given entity

  Output: 0 upon success or !0 upon failure.
*/
int
Configuration::xmaxconn(XrdOucStream &Config)
{
    auto val = Config.GetWord();
    if (!val || val[0] == '\0')
       {m_log.Emsg("Config", "Max active connections not specified!  Example usage: throttle.max_active_connections 4000");}
    long long max_conn = -1;
    if (XrdOuca2x::a2sz(m_log, "max active connections value", val, &max_conn, 1)) return 1;

    m_max_conn = max_conn;
    return 0;
}

/******************************************************************************/
/*                            x m a x w a i t                                 */
/******************************************************************************/

/* Function: xmaxwait

   Purpose:  Parse the directive: throttle.max_wait_time <limit>

             <limit>   maximum wait time, in seconds, before an operation should fail

   If the directive is not provided, the default is 30 seconds.

  Output: 0 upon success or !0 upon failure.
*/
int
Configuration::xmaxwait(XrdOucStream &Config)
{
    auto val = Config.GetWord();
    if (!val || val[0] == '\0')
       {m_log.Emsg("Config", "Max waiting time not specified (must be in seconds)!  Example usage: throttle.max_wait_time 20");}
    long long max_wait = -1;
    if (XrdOuca2x::a2sz(m_log, "max waiting time value", val, &max_wait, 1)) return 1;

    return 0;
}

/******************************************************************************/
/*                            x t h r o t t l e                               */
/******************************************************************************/

/* Function: xthrottle

   Purpose:  To parse the directive: throttle [data <drate>] [iops <irate>] [concurrency <climit>] [interval <rint>]

             <drate>    maximum bytes per second through the server.
             <irate>    maximum IOPS per second through the server.
             <climit>   maximum number of concurrent IO connections.
             <rint>     minimum interval in milliseconds between throttle re-computing.

   Output: 0 upon success or !0 upon failure.
*/
int
Configuration::xthrottle(XrdOucStream &Config)
{
    long long drate = -1, irate = -1, rint = 1000, climit = -1;
    char *val;

    while ((val = Config.GetWord()))
    {
       if (strcmp("data", val) == 0)
       {
          if (!(val = Config.GetWord()))
             {m_log.Emsg("Config", "data throttle limit not specified."); return 1;}
          if (XrdOuca2x::a2sz(m_log,"data throttle value",val,&drate,1)) return 1;
       }
       else if (strcmp("iops", val) == 0)
       {
          if (!(val = Config.GetWord()))
             {m_log.Emsg("Config", "IOPS throttle limit not specified."); return 1;}
          if (XrdOuca2x::a2sz(m_log,"IOPS throttle value",val,&irate,1)) return 1;
       }
       else if (strcmp("rint", val) == 0)
       {
          if (!(val = Config.GetWord()))
             {m_log.Emsg("Config", "recompute interval not specified (in ms)."); return 1;}
          if (XrdOuca2x::a2sp(m_log,"recompute interval value (in ms)",val,&rint,10)) return 1;
       }
       else if (strcmp("concurrency", val) == 0)
       {
          if (!(val = Config.GetWord()))
             {m_log.Emsg("Config", "Concurrency limit not specified."); return 1;}
          if (XrdOuca2x::a2sz(m_log,"Concurrency limit value",val,&climit,1)) return 1;
       }
       else
       {
          m_log.Emsg("Config", "Warning - unknown throttle option specified", val, ".");
       }
    }

    m_throttle_data_rate = drate;
    m_throttle_iops_rate = irate;
    m_throttle_concurrency_limit = climit;
    m_throttle_recompute_interval_ms = rint;

    return 0;
}

/******************************************************************************/
/*                            x l o a d s h e d                               */
/******************************************************************************/

/* Function: xloadshed

   Purpose:  To parse the directive: loadshed host <hostname> [port <port>] [frequency <freq>]

             <hostname> hostname of server to shed load to.  Required
             <port>     port of server to shed load to.  Defaults to 1094
             <freq>     A value from 1 to 100 specifying how often to shed load
                        (1 = 1% chance; 100 = 100% chance; defaults to 10).

   Output: 0 upon success or !0 upon failure.
*/
int Configuration::xloadshed(XrdOucStream &Config)
{
    long long port = 0, freq = 0;
    char *val;
    std::string hostname;

    while ((val = Config.GetWord()))
    {
       if (strcmp("host", val) == 0)
       {
          if (!(val = Config.GetWord()))
             {m_log.Emsg("Config", "loadshed hostname not specified."); return 1;}
          hostname = val;
       }
       else if (strcmp("port", val) == 0)
       {
          if (!(val = Config.GetWord()))
             {m_log.Emsg("Config", "Port number not specified."); return 1;}
          if (XrdOuca2x::a2sz(m_log,"Port number",val,&port,1, 65536)) return 1;
       }
       else if (strcmp("frequency", val) == 0)
       {
           if (!(val = Config.GetWord()))
              {m_log.Emsg("Config", "Loadshed frequency not specified."); return 1;}
           if (XrdOuca2x::a2sz(m_log,"Loadshed frequency",val,&freq,1,100)) return 1;
       }
       else
       {
           m_log.Emsg("Config", "Warning - unknown loadshed option specified", val, ".");
       }
    }

    if (hostname.empty())
    {
        m_log.Emsg("Config", "must specify hostname for loadshed parameter.");
        return 1;
    }

    m_loadshed_freq = freq;
    m_loadshed_hostname = hostname;
    m_loadshed_port = port;

    return 0;
}

/******************************************************************************/
/*                                x t r a c e                                 */
/******************************************************************************/

/* Function: xtrace

   Purpose:  To parse the directive: trace <events>

             <events> the blank separated list of events to trace. Trace
                      directives are cummalative.

   Output: 0 upon success or 1 upon failure.
*/

int Configuration::xtrace(XrdOucStream &Config)
{
   char *val;
   static const struct traceopts {const char *opname; int opval;} tropts[] =
   {
      {"all",       TRACE_ALL},
      {"off",       TRACE_NONE},
      {"none",      TRACE_NONE},
      {"debug",     TRACE_DEBUG},
      {"iops",      TRACE_IOPS},
      {"bandwidth", TRACE_BANDWIDTH},
      {"ioload",    TRACE_IOLOAD},
      {"files",     TRACE_FILES},
      {"connections",TRACE_CONNS},
   };
   int i, neg, trval = 0, numopts = sizeof(tropts)/sizeof(struct traceopts);

   if (!(val = Config.GetWord()))
   {
      m_log.Emsg("Config", "trace option not specified");
      return 1;
   }
   while (val)
   {
      if (!strcmp(val, "off"))
      {
         trval = 0;
      }
      else
      {
         if ((neg = (val[0] == '-' && val[1])))
         {
            val++;
         }
         for (i = 0; i < numopts; i++)
         {
            if (!strcmp(val, tropts[i].opname))
            {
               if (neg)
               {
                  if (tropts[i].opval) trval &= ~tropts[i].opval;
                  else trval = TRACE_ALL;
               }
               else if (tropts[i].opval) trval |= tropts[i].opval;
               else trval = TRACE_NONE;
               break;
            }
         }
         if (i >= numopts)
         {
            m_log.Say("Config warning: ignoring invalid trace option '", val, "'.");
         }
      }
      val = Config.GetWord();
   }
   m_trace_levels = trval;
   return 0;
}
