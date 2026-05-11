/******************************************************************************/
/*                                                                            */
/*               X r d X r o o t d R e d i r H e l p e r . c c                */
/*                                                                            */
/* (c) 2026 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdXrootd/XrdXrootdRedirHelper.hh"

#include <ctime>
#include <functional>
#include <map>
#include <memory>

#include "XrdNet/XrdNetAddr.hh"
#include "XrdNet/XrdNetAddrInfo.hh"
#include "XrdOuc/XrdOucPrivateUtils.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysRAtomic.hh"
#include "XrdXrootd/XrdXrootdRedirPI.hh"

/******************************************************************************/
/*                       S t a t i c   h e l p e r   s t a t e                */
/******************************************************************************/

// All three globals below are written exactly once, by Init(), before any
// worker thread can observe them; they are therefore read without locking.

namespace
{
   XrdXrootdRedirPI *gPlugin = nullptr;  // owned by the redirlib loader
   XrdSysError      *gLog    = nullptr;  // borrowed; logger lives forever
   int               gIPHold = 8 * 60 * 60;

   // ==========================================================================
   // ====================== ONLY FOR TESTING, DO NOT MODIFY====================
   // ==========================================================================
   //
   // Test-only clock seam.  Production code leaves gNow empty, in which case
   // Now() falls back to time(nullptr) on every call.  The override is installed
   // exclusively by XrdXrootdRedirHelper::SetClockForTesting() so unit tests can
   // fast-forward past gIPHold and exercise the cache-refresh branch in
   // LookupTarget.
   //
   // DO NOT call SetClockForTesting() from anywhere other than a test binary,
   // and DO NOT add new code that reads or writes gNow directly.  Replacing
   // the wall-clock at runtime would silently break the netaddr cache TTL for
   // every redirect in the whole process.
   //
   // ==========================================================================
   std::function<time_t()> gNow;
   inline time_t Now() { return gNow ? gNow() : time(nullptr); }

   //--------------------------------------------------------------------------
   // Cached resolution of one redirect target.  The cache is intentionally
   // unbounded: a deployment has only a handful of redirect targets (one per
   // data server) so the table never grows large enough to need eviction.
   //--------------------------------------------------------------------------

   struct netInfo
   {
      XrdNetAddr           netAddr;
      XrdSysMutex          niMutex;
      std::string          netID;
      time_t               expTime = 0;
      RAtomic_uint         refs    = {0};

      explicit netInfo(const char *id) : netID(id) {}
   };

   //--------------------------------------------------------------------------
   // Scope guard that decrements netInfo::refs on destruction so that early
   // returns (e.g. plugin error) do not leak the borrowed reference.
   //--------------------------------------------------------------------------

   struct THandle
   {
      netInfo *Info = nullptr;
      THandle()                          = default;
      ~THandle()                         { if (Info) Info->refs--; }
      THandle(const THandle&)            = delete;
      THandle& operator=(const THandle&) = delete;
   };

   //--------------------------------------------------------------------------
   //! Look up (or initialise / refresh) the cached netInfo for @p netID.
   //!
   //! @return  pointer with refs already incremented, or nullptr on a hard
   //!          first-time resolution failure.
   //--------------------------------------------------------------------------

   netInfo *LookupTarget(const char *netID, int port)
   {
      static std::map<std::string, std::unique_ptr<netInfo>, std::less<>> niMap;
      static XrdSysMutex niMapMtx;

      // Locate or insert the entry.  Entries are immortal once created, so
      // it is safe to drop the map lock as soon as we have the pointer.
      niMapMtx.Lock();
      netInfo *niP;
      if (auto it = niMap.find(netID); it == niMap.end())
         {auto newInfo = std::make_unique<netInfo>(netID);
          niP = newInfo.get();
          niMap.try_emplace(niP->netID, std::move(newInfo));
         } else niP = it->second.get();
      niMapMtx.UnLock();

      // Hold the per-entry mutex while we resolve / refresh the netaddr so
      // that two callers do not race each other while the address is being
      // populated.
      niP->niMutex.Lock();
      time_t nowT = Now();
      niP->refs++;
      // First-time init or expired entry: (re)resolve.  refs == 1 means we
      // are the only outstanding caller, so writing into netAddr cannot
      // trample a concurrent reader; anyone else takes the cached address.
      if (niP->expTime > nowT || niP->refs != 1)
         {niP->niMutex.UnLock();
          return niP;
         }

      if (const char *eTxt = niP->netAddr.Set(netID, port); eTxt)
         {if (niP->expTime == 0)
             {// First-time resolution failed: drop the borrowed ref and
              // tell the caller to fall back on the original target.
              if (gLog) gLog->Emsg("RedirIP", "Unable to init NetInfo for",
                                   netID, eTxt);
              niP->refs--;
              niP->niMutex.UnLock();
              return nullptr;
             }
          // Refresh failed but we still have the previous good address;
          // keep using it and back off the next refresh attempt.
          if (gLog) gLog->Emsg("RedirIP", "Unable to refresh NetInfo for",
                               netID, eTxt);
          niP->expTime += 60;
         } else niP->expTime = nowT + gIPHold;
      niP->niMutex.UnLock();
      return niP;
   }
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/

void XrdXrootdRedirHelper::Init(XrdXrootdRedirPI *pi, XrdSysError *eDest,
                                int ipHold)
{
   gPlugin = pi;
   gLog    = eDest;
   gIPHold = ipHold;
}

/******************************************************************************/
/*                              I s A c t i v e                               */
/******************************************************************************/

bool XrdXrootdRedirHelper::IsActive() { return gPlugin != nullptr; }

/******************************************************************************/
/*  =================== ONLY FOR TESTING, DO NOT USE ====================      */
/*                                                                            */
/*                  S e t C l o c k F o r T e s t i n g                       */
/*                                                                            */
/*  =================== ONLY FOR TESTING, DO NOT USE ====================      */
/******************************************************************************/

// The only legal way to write gNow; see the header for full warnings.
// DO NOT CALL FROM PRODUCTION CODE.

void XrdXrootdRedirHelper::SetClockForTesting(std::function<time_t()> nowFn)
{
   gNow = std::move(nowFn);
}

/******************************************************************************/
/*                              R e d i r e c t                               */
/******************************************************************************/

XrdXrootdRedirHelper::Outcome
XrdXrootdRedirHelper::Redirect(const char *trg, int &port,
                               XrdNetAddrInfo &clientAddr,
                               std::string &outTarget, std::string &errMsg)
{
   if (!gPlugin || !trg) return Outcome::Unchanged;

   std::string pluginReply;   // raw plugin reply, filled by the branch below
   uint16_t    hostPort = 0;  // host form: the plugin's port (in/out)

   if (port >= 0)
      {// Host[?cgi] form: split the target and invoke the plugin's host+port
       // Redirect() entry point, which may rewrite the port in place.
       if (port > UINT16_MAX)
          {if (gLog) gLog->Emsg("RedirPI", "Redirect port out of range -",
                                std::to_string(port).c_str());
           return Outcome::Unchanged;
          }
       std::string host;
       std::string cgi;
       splitHostCgi(trg, host, cgi);

       // The plugin call needs the target's resolved netaddr.  If we cannot
       // produce one the safest fallback is to leave the redirect alone
       // (Unchanged) so the caller emits the original target unmodified.
       THandle T;
       T.Info = LookupTarget(host.c_str(), port);
       if (!T.Info) return Outcome::Unchanged;

       hostPort    = static_cast<uint16_t>(port);
       pluginReply = gPlugin->Redirect(host.c_str(), hostPort, cgi.c_str(),
                                       T.Info->netAddr, clientAddr);
      } else {
       // URL form: parse scheme://host[:port][/tail] and invoke the plugin's
       // RedirectURL() entry point.  port doubles as the rdrOpts argument and
       // is the protocol's URL marker, so it is passed in but left unmodified.
       // A URL we cannot parse is not salvageable: skip the plugin and report
       // Unchanged so the caller emits the original target unmodified.
       std::string urlHead;
       std::string host;
       std::string urlPort;
       std::string urlTail;
       if (!ParseURL(trg, urlHead, host, urlPort, urlTail))
          {if (gLog) gLog->Emsg("RedirPI", "Invalid redirect URL -", trg);
           return Outcome::Unchanged;
          }

       // The cache is keyed by host alone (no scheme, no port suffix); pass
       // -1 so the netaddr resolution does not bind to any specific port.  A
       // resolution failure falls back to Unchanged, as in the host form.
       THandle T;
       T.Info = LookupTarget(host.c_str(), -1);
       if (!T.Info) return Outcome::Unchanged;

       int rdrOpts = port;
       pluginReply = gPlugin->RedirectURL(urlHead.c_str(), host.c_str(),
                                          urlPort.c_str(), urlTail.c_str(),
                                          rdrOpts, T.Info->netAddr,
                                          clientAddr);
      }

   // Translate the plugin's "" / "<target>" / "!<msg>" return-string contract.
   if (pluginReply.empty())        return Outcome::Unchanged;
   if (pluginReply.front() == '!') { errMsg.assign(pluginReply, 1);
                                     return Outcome::Error; }

   // Replaced: commit the plugin's (possibly rewritten) port.  Host form only;
   // the URL form leaves port untouched as the protocol's URL marker.
   if (port >= 0) port = hostPort;
   outTarget = std::move(pluginReply);
   return Outcome::Replaced;
}

/******************************************************************************/
/*                              P a r s e U R L                               */
/******************************************************************************/

bool XrdXrootdRedirHelper::ParseURL(const char *url, std::string &urlHead,
                                    std::string &host, std::string &port,
                                    std::string &urlTail)
{
   const char *hBeg = strstr(url, "://");
   if (!hBeg) return false;
   hBeg += 3;
   urlHead.assign(url, hBeg - url);

   // Split off the path/query tail; require the host[:port] authority that
   // precedes it to be at least two characters long.
   if (const char *tail = strstr(hBeg, "/"); !tail)
      {urlTail.clear(); host = hBeg;}
      else {if (tail - hBeg < 3) return false;
            host.assign(hBeg, tail - hBeg);
            urlTail = tail;
           }

   // Separate an optional ":port" suffix from the host.
   port.clear();
   if (size_t colon = host.find(':'); colon != std::string::npos)
      {port.assign(host, colon + 1, std::string::npos);
       host.erase(colon);
      }
   return true;
}
