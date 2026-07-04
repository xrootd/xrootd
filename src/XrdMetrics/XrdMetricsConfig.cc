/******************************************************************************/
/*                                                                            */
/*                  X r d M e t r i c s C o n f i g . c c                     */
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
/******************************************************************************/

#include <cstdlib>
#include <cstring>

#include "XrdMetrics/XrdMetricsConfig.hh"
#include "XrdMetrics/XrdMetricsRegistry.hh"
#include "XrdOuc/XrdOucGatherConf.hh"
#include "XrdSys/XrdSysError.hh"

namespace XrdMetrics
{
namespace
{
// A yes/no directive value. Anything not clearly affirmative is treated as off.
//
bool parseBool(const char *v)
{
   return v && (!strcasecmp(v, "yes")  || !strcasecmp(v, "true")
             || !strcasecmp(v, "on")   || !strcmp(v, "1"));
}
}

/******************************************************************************/
/*                              I n s t a n c e                              */
/******************************************************************************/

Config& Config::Instance()
{
   static Config instance;
   return instance;
}

/******************************************************************************/
/*                         D i r e c t i v e L i s t                         */
/******************************************************************************/

const char *Config::DirectiveList()
{
// all.role is gathered too: it is a source for the auto-seeded "role" global
// label, letting one daemon's series be told apart by cluster role.
//
   return "metrics.enable metrics.label metrics.subsystems "
          "metrics.path metrics.instance "
          "metrics.pushurl metrics.pushinterval metrics.pushjob "
          "metrics.otelurl metrics.otelinterval "
          "metrics.scrapettl metrics.scraperatelimit all.role";
}

/******************************************************************************/
/*                           s e t U s e r L a b e l                         */
/******************************************************************************/

void Config::setUserLabel(const std::string &key, const std::string &val)
{
   for (auto &kv : userLabels)
       if (kv.first == key) {kv.second = val; return;}
   userLabels.emplace_back(key, val);
}

/******************************************************************************/
/*                                 p a r s e                                 */
/******************************************************************************/

void Config::parse(XrdOucGatherConf &conf)
{
   char *d;
   while (conf.GetLine() && (d = conf.GetToken()))
        {     if (!strcmp(d, "metrics.enable"))
                 {char *v = conf.GetToken(); enabled = parseBool(v);}
         else if (!strcmp(d, "metrics.label"))
                 {char *k = conf.GetToken();
                  char *v = conf.GetToken();
                  if (k && *k && v) setUserLabel(k, v);
                 }
         else if (!strcmp(d, "metrics.subsystems"))
                 {char *t;
                  while ((t = conf.GetToken()))
                        {     if (*t == '-') {if (t[1]) disabledSubsys.insert(t+1);}
                         else if (*t == '+') {if (t[1]) enabledSubsys.insert(t+1);}
                         else if (*t)        enabledSubsys.insert(t);
                        }
                 }
         else if (!strcmp(d, "metrics.path"))
                 {char *v = conf.GetToken(); if (v && *v) path = v;}
         else if (!strcmp(d, "metrics.instance"))
                 {char *v = conf.GetToken(); if (v && *v) instance = v;}
         else if (!strcmp(d, "metrics.pushurl"))
                 {char *v = conf.GetToken(); if (v && *v) pushURL = v;}
         else if (!strcmp(d, "metrics.pushjob"))
                 {char *v = conf.GetToken(); if (v && *v) pushJob = v;}
         else if (!strcmp(d, "metrics.otelurl"))
                 {char *v = conf.GetToken(); if (v && *v) otelURL = v;}
         else if (!strcmp(d, "metrics.pushinterval"))
                 {char *v = conf.GetToken(); int n = v ? atoi(v) : 0;
                  if (n > 0) pushEvery = n;
                 }
         else if (!strcmp(d, "metrics.otelinterval"))
                 {char *v = conf.GetToken(); int n = v ? atoi(v) : 0;
                  if (n > 0) otelEvery = n;
                 }
         else if (!strcmp(d, "metrics.scrapettl"))
                 {char *v = conf.GetToken(); int n = v ? atoi(v) : -1;
                  if (n >= 0) scrapeTTL = n;
                 }
         else if (!strcmp(d, "metrics.scraperatelimit"))
                 {char *v = conf.GetToken(); int n = v ? atoi(v) : -1;
                  if (n >= 0) scrapeRateLimit = n;
                 }
         else if (!strcmp(d, "all.role"))
                 {char *v = conf.GetToken(); if (v && *v) role = v;}
        }
}

/******************************************************************************/
/*                     b u i l d G l o b a l L a b e l s                     */
/******************************************************************************/

std::vector<ConstLabel> Config::buildGlobalLabels() const
{
   std::vector<ConstLabel> out;
   auto set = [&](const std::string &k, const std::string &v)
             {for (auto &kv : out) if (kv.first == k) {kv.second = v; return;}
              out.emplace_back(k, v);
             };

// Auto-seed the cross-daemon differentiators, then let explicit metrics.label
// directives override or extend them. The role prefers the daemon's resolved
// role (XRDROLE, which cmsd expands to e.g. "supervisor"); the all.role
// directive gathered from the file is the fallback for daemons that do not
// export one.
//
   const char *prog = getenv("XRDPROG");
   if (prog && *prog)  set("program", prog);

   const char *envRole = getenv("XRDROLE");
   if      (envRole && *envRole) set("role", envRole);
   else if (!role.empty())       set("role", role);

   for (auto &kv : userLabels) set(kv.first, kv.second);

   return out;
}

/******************************************************************************/
/*                       s u b s y s t e m E n a b l e d                     */
/******************************************************************************/

bool Config::subsystemEnabled(const std::string &subsystem) const
{
   if (!enabled)                         return false;
   if (disabledSubsys.count(subsystem))  return false;
   if (!enabledSubsys.empty())           return enabledSubsys.count(subsystem) != 0;
   return true;
}

/******************************************************************************/
/*                       P u s h g a t e w a y U R L                         */
/******************************************************************************/

std::string Config::PushgatewayURL(const std::string &base,
                                   const std::string &job,
                                   const std::string &instance)
{
   std::string url = base;
   if (!url.empty() && url.back() == '/') url.pop_back();
   url += "/metrics/job/" + job + "/instance/" + instance;
   return url;
}

/******************************************************************************/
/*                                  L o a d                                  */
/******************************************************************************/

void Config::Load(const char *cfgfn, XrdSysError *log)
{
   if (loaded_) return;
   loaded_ = true;

   if (cfgfn)
      {XrdOucGatherConf conf(DirectiveList(), log);
       if (conf.Gather(cfgfn, XrdOucGatherConf::full_lines) >= 0 && conf.hasData())
          parse(conf);
      }

   globalLabels = buildGlobalLabels();

// Freeze the global labels onto the default registry. This must land before any
// family is created; when it does not (e.g. a late standalone load) the labels
// are simply absent from already-created series, so warn rather than fail.
//
   if (!globalLabels.empty() && !Default().setGlobalLabels(globalLabels) && log)
      log->Say("Config warning: metrics global labels ignored; metrics were "
               "already registered before the configuration was loaded");
}
}
