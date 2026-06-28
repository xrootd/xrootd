#ifndef __XRDHTTPMETRICSEXPORTERCONFIG_HH__
#define __XRDHTTPMETRICSEXPORTERCONFIG_HH__
/******************************************************************************/
/*                                                                            */
/*    X r d H t t p M e t r i c s E x p o r t e r C o n f i g . h h            */
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
#include <string>

#include "XrdOuc/XrdOucGatherConf.hh"

//-----------------------------------------------------------------------------
//! Parsing helpers for XrdHttpMetricsExporter, factored out of the plugin so
//! they can be unit tested without constructing the handler (which registers
//! metrics and starts background threads). The logic is header-only and pure:
//! it does not touch curl, threads, or the metrics registry.
//-----------------------------------------------------------------------------

namespace XrdHttpMetricsExporterCfg
{
//! The metrics.* directives the plugin recognises, as one space-separated blob
//! suitable for the XrdOucGatherConf constructor.
inline const char *DirectiveList()
{
   return "metrics.path metrics.instance "
          "metrics.pushurl metrics.pushinterval metrics.pushjob "
          "metrics.otelurl metrics.otelinterval";
}

//! The plugin's configurable state, with the same defaults as the handler.
struct Config
{
   std::string path     = "/metrics"; // served request path
   std::string instance;              // instance label (empty => hostname)

   std::string pushURL;               // Pushgateway base URL ("" => disabled)
   std::string pushJob  = "xrootd";   // Pushgateway job label
   int         pushEvery = 30;        // Pushgateway push period (seconds)

   std::string otelURL;               // OTLP/HTTP metrics URL ("" => disabled)
   int         otelEvery = 30;        // OTLP push period (seconds)
};

//! Walk an already-gathered XrdOucGatherConf (full_lines) and fold the
//! recognised metrics.* directives into cfg. Unknown keys and valueless lines
//! are ignored; a non-positive interval is rejected (default kept).
inline void Parse(XrdOucGatherConf &conf, Config &cfg)
{
   char *key;
   while(conf.GetLine() && (key = conf.GetToken()))
        {char *val = conf.GetToken();
         if (!val || !*val) continue;
              if (!strcmp(key, "metrics.path"))     cfg.path     = val;
         else if (!strcmp(key, "metrics.instance")) cfg.instance = val;
         else if (!strcmp(key, "metrics.pushurl"))  cfg.pushURL  = val;
         else if (!strcmp(key, "metrics.pushjob"))  cfg.pushJob  = val;
         else if (!strcmp(key, "metrics.otelurl"))  cfg.otelURL  = val;
         else if (!strcmp(key, "metrics.pushinterval"))
                 {int n = atoi(val); if (n > 0) cfg.pushEvery = n;}
         else if (!strcmp(key, "metrics.otelinterval"))
                 {int n = atoi(val); if (n > 0) cfg.otelEvery = n;}
        }
}

//! Build the per-instance Pushgateway URL:
//!   <base>/metrics/job/<job>/instance/<instance>
//! A single trailing slash on the base is trimmed so the path is well formed.
inline std::string PushgatewayURL(const std::string &base,
                                  const std::string &job,
                                  const std::string &instance)
{
   std::string url = base;
   if (!url.empty() && url.back() == '/') url.pop_back();
   url += "/metrics/job/" + job + "/instance/" + instance;
   return url;
}
}
#endif
