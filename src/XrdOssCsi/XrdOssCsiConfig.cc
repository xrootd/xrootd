/******************************************************************************/
/*                                                                            */
/*                   X r d O s s C s i C o n f i g . c c                      */
/*                                                                            */
/* (C) Copyright 2021 CERN.                                                   */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* In applying this licence, CERN does not waive the privileges and           */
/* immunities granted to it by virtue of its status as an Intergovernmental   */
/* Organization or submit itself to any jurisdiction.                         */
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

#include "XrdOssCsiConfig.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOssCsiTrace.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sstream>
#include <string>
#include <vector>

extern XrdOucTrace  OssCsiTrace;

#define TS_Xeq(x,m)    if (!strcmp(x,var)) return m(Config, Eroute);

int XrdOssCsiConfig::Init(XrdSysError &Eroute, const char *config_fn, const char *parms, XrdOucEnv * /* envP */)
{
   int NoGo = XrdOssOK;
   Eroute.Say("++++++ OssCsi plugin initialization started.");

   std::stringstream ss(parms ? parms : "");
   std::string item;

   while(std::getline(ss, item, ' '))
   {
      std::string value;
      const auto idx = item.find('=');
      if (idx != std::string::npos)
      {
         value = item.substr(idx+1, std::string::npos);
         item.erase(idx, std::string::npos);
      }
      if (item == "nofill")
      {
         fillFileHole_ = false;
      }
      else if (item == "space" && !value.empty())
      {
         xrdtSpaceName_ = value;
      }
      else if (item == "nomissing")
      {
         allowMissingTags_ = false;
      }
      else if (item == "prefix")
      {
         if (tagParam_.SetPrefix(Eroute, value)) NoGo = 1;
      }
      else if (item == "nopgextend")
      {
         disablePgExtend_ = true;
      }
      else if (item == "noloosewrites")
      {
         disableLooseWrite_ = true;
      }
   }

   if (NoGo) return NoGo;

   OssCsiTrace.What = TRACE_Warn;
   if (getenv("XRDDEBUG")) OssCsiTrace.What = TRACE_ALL;
   if (readConfig(Eroute, config_fn)) return 1;

   Eroute.Say("       compute file holes      : ", fillFileHole_ ? "yes" : "no");
   Eroute.Say("       space name              : ", xrdtSpaceName_.c_str());
   Eroute.Say("       allow files without CRCs: ", allowMissingTags_ ? "yes" : "no");
   Eroute.Say("       pgWrite can extend      : ", disablePgExtend_ ? "no" : "yes");
   Eroute.Say("       loose writes            : ", disableLooseWrite_ ? "no" : "yes");
   Eroute.Say("       trace level             : ", std::to_string((long long int)OssCsiTrace.What).c_str());
   Eroute.Say("       prefix                  : ", tagParam_.prefix_.empty() ? "[empty]" : tagParam_.prefix_.c_str());

   Eroute.Say("++++++ OssCsi plugin initialization completed.");


   return XrdOssOK;
}

int XrdOssCsiConfig::readConfig(XrdSysError &Eroute, const char *ConfigFN)
{
   char *var;
   int cfgFD, retc, NoGo = XrdOssOK;
   XrdOucEnv myEnv;
   XrdOucStream Config(&Eroute, getenv("XRDINSTANCE"), &myEnv, "=====> ");

   if( !ConfigFN || !*ConfigFN)
   {
      Eroute.Say("Config warning: config file not specified; defaults assumed.");
      return XrdOssOK;
   }

   if ( (cfgFD = open(ConfigFN, O_RDONLY, 0)) < 0)
   {
      Eroute.Emsg("Config", errno, "open config file", ConfigFN);
      return 1;
   }

   Config.Attach(cfgFD);
   static const char *cvec[] = { "*** osscsi plugin config:", 0 };
   Config.Capture(cvec);

   while((var = Config.GetMyFirstWord()))
   {
      if (!strncmp(var, "csi.", 4))
      {
         if (ConfigXeq(var+4, Config, Eroute))
         {
            Config.Echo(); NoGo = 1;
         }
      }
   }

   if ((retc = Config.LastError()))
      NoGo = Eroute.Emsg("Config", retc, "read config file", ConfigFN);

   Config.Close();

   return NoGo;
}

int XrdOssCsiConfig::ConfigXeq(char *var, XrdOucStream &Config, XrdSysError &Eroute)
{
   TS_Xeq("trace",         xtrace);
   return 0;
}

int XrdOssCsiConfig::xtrace(XrdOucStream &Config, XrdSysError &Eroute)
{
    char *val;
    static struct traceopts {const char *opname; int opval;} tropts[] =
       {
        {"all",      TRACE_ALL},
        {"debug",    TRACE_Debug},
        {"warn",     TRACE_Warn},
        {"info",     TRACE_Info}
       };
    int i, neg, trval = 0, numopts = sizeof(tropts)/sizeof(struct traceopts);

    if (!(val = Config.GetWord()))
       {Eroute.Emsg("Config", "trace option not specified"); return 1;}
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
                      Eroute.Say("Config warning: ignoring invalid trace option '",val,"'.");
                  }
          val = Config.GetWord();
         }
    OssCsiTrace.What = trval;
    return 0;
}
