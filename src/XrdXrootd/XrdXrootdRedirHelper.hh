#ifndef __XRDXROOTDREDIRHELPER__
#define __XRDXROOTDREDIRHELPER__
/******************************************************************************/
/*                                                                            */
/*               X r d X r o o t d R e d i r H e l p e r . h h                */
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

#include <cstdint>
#include <ctime>
#include <functional>
#include <string>

class XrdNetAddrInfo;
class XrdSysError;
class XrdXrootdRedirPI;

//------------------------------------------------------------------------------
//! @class XrdXrootdRedirHelper
//!
//! Process-wide adapter around the redirect plugin (XrdXrootdRedirPI) so the
//! plugin can be invoked uniformly from the XRootD protocol and from any
//! other subsystem that produces redirect targets (e.g. the HTTP TPC handler
//! addressed by issue #2767).
//!
//! Responsibilities centralised here so they do not need to be reproduced at
//! every call site:
//!  - the static plugin pointer plus the logger used for diagnostics;
//!  - a cache of XrdNetAddrInfo objects for previously-resolved redirect
//!    targets (each plugin call needs the target's resolved netaddr, which
//!    is comparatively expensive to obtain);
//!  - the empty / "<new target>" / "!<error>" return-string contract of the
//!    plugin, normalised into a tri-state Outcome.
//!
//! The class is non-instantiable; it is initialised once by Init() (called
//! from XrdXrootdConfig.cc after the redirect plugin has been loaded) and
//! its Redirect() static method is then invoked concurrently from any number of
//! worker threads.  When no plugin is configured IsActive() returns false and
//! Redirect() is an inexpensive no-op.
//------------------------------------------------------------------------------

class XrdXrootdRedirHelper
{
public:

   //----------------------------------------------------------------------------
   //! Tri-state outcome returned by Redirect().
   //!
   //! Mirrors the three cases of the plugin's return-string contract:
   //!   - empty string    -> Unchanged: caller keeps the original target
   //!   - "<new target>"  -> Replaced: caller emits the new target
   //!   - "!<message>"    -> Error: caller fails the request, surfaces the
   //!                        message
   //----------------------------------------------------------------------------

   enum class Outcome { Unchanged, Replaced, Error };

   //----------------------------------------------------------------------------
   //! Initialise the helper.  Must be called exactly once, after the redirect
   //! plugin has been loaded and before any worker thread invokes Redirect().
   //! Calling Init(nullptr, ...) is a valid no-op that simply disarms the
   //! helper.
   //!
   //! @param  pi      pointer to the loaded redirect plugin, or nullptr if no
   //!                 plugin was loaded.  When nullptr, IsActive() returns
   //!                 false and Redirect() always returns Unchanged.
   //! @param  eDest   logger used to report cache-population failures.  Must
   //!                 not be nullptr when @p pi is non-null.
   //! @param  ipHold  number of seconds for which a successfully-resolved
   //!                 target XrdNetAddrInfo is reused before being refreshed.
   //!                 Mirrors XrdXrootdProtocol::redirIPHold.
   //----------------------------------------------------------------------------

   static void Init(XrdXrootdRedirPI *pi, XrdSysError *eDest, int ipHold);

   //----------------------------------------------------------------------------
   //! @return true iff a redirect plugin has been registered via Init().
   //----------------------------------------------------------------------------

   static bool IsActive();

   //----------------------------------------------------------------------------
   //! Run a redirect target through the redirect plugin.
   //!
   //! Single entry point for both redirect forms; @p port selects which one,
   //! mirroring the calling convention of XrdXrootdProtocol::fsRedirPI():
   //!
   //!   - @p port >= 0 : @p trg is a host[?cgi] target and @p port is its
   //!                    numeric port.  The target is split internally (via
   //!                    splitHostCgi) and the plugin's Redirect() entry point
   //!                    is invoked; @p port is rewritten in place when the
   //!                    plugin asks for a different port.
   //!
   //!   - @p port <  0 : @p trg is a full scheme://host[:port][/tail] URL.  It
   //!                    is parsed internally (via ParseURL) and the plugin's
   //!                    RedirectURL() entry point is invoked.  Here @p port
   //!                    doubles as the plugin's rdrOpts argument; because it
   //!                    is also the protocol's URL marker it is passed to the
   //!                    plugin but left unmodified on return.  A URL that
   //!                    cannot be parsed is logged and yields
   //!                    Outcome::Unchanged without invoking the plugin.
   //!
   //! Used by the XRootD protocol (fsRedirPI) for both forms and by the HTTP
   //! TPC handler (issue #2767), which only ever passes the host[?cgi] form.
   //!
   //! @param  trg          redirect target: host[?cgi] when @p port >= 0, or a
   //!                      scheme://host[:port][/tail] URL when @p port < 0.
   //!                      A nullptr is tolerated defensively and yields
   //!                      Outcome::Unchanged without invoking the plugin.
   //! @param  port         in/out, doubling as the host-vs-URL selector; see
   //!                      the per-form description above.
   //! @param  clientAddr   network info of the client being redirected.
   //! @param  outTarget    output: when Replaced, holds the new redirect
   //!                      target.  Untouched otherwise.
   //! @param  errMsg       output: when Error, holds the plugin's error
   //!                      message with the contract's leading '!' already
   //!                      stripped.  Untouched otherwise.
   //!
   //! @return Unchanged if no plugin is configured, the target's network
   //!         address could not be resolved, a URL target could not be parsed,
   //!         or the plugin returned an empty string.  Replaced if the plugin
   //!         returned a new target.  Error if the plugin returned a fatal
   //!         error.
   //!
   //! @note   Thread-safe.  Internal state is protected by a mutex; the
   //!         caller may invoke this from any thread once Init() has
   //!         returned.
   //----------------------------------------------------------------------------

   static Outcome Redirect(const char *trg, int &port,
                           XrdNetAddrInfo &clientAddr,
                           std::string &outTarget, std::string &errMsg);

   //----------------------------------------------------------------------------
   //! Parse a scheme://host[:port][/tail] redirect URL into its components.
   //!
   //! Exposed as a static method, rather than kept private to the .cc, so the
   //! URL grammar Redirect() relies on for URL-form targets can be unit-tested
   //! directly without a plugin, the netaddr cache or DNS.  Redirect() is its
   //! only production caller.
   //!
   //! @param  url      URL to parse.  Must not be nullptr.
   //! @param  urlHead  output: scheme prefix up to and including "://".
   //! @param  host     output: host name, with any ":port" suffix removed.
   //! @param  port     output: port string, or "" when no port is present.
   //! @param  urlTail  output: path/query tail including the leading '/', or
   //!                  "" when the URL has no path.
   //!
   //! @return true on success.  false when @p url is not a parseable
   //!         scheme://host[:port][/tail] URL; the output parameters are
   //!         then left in an unspecified state.
   //----------------------------------------------------------------------------

   static bool ParseURL(const char *url, std::string &urlHead,
                        std::string &host, std::string &port,
                        std::string &urlTail);

   //----------------------------------------------------------------------------
   //! ====================== ONLY FOR TESTING, DO NOT USE ======================
   //!
   //! Override the clock function used by the internal target-netaddr cache so
   //! unit tests can fast-forward past the ipHold expiry and exercise the
   //! refresh path of LookupTarget.  Pass nullptr to restore the default clock
   //! (`time(nullptr)`).
   //!
   //! @warning  This function exists solely to make XrdXrootdRedirHelper unit-
   //!           testable.  **It must never be called from production code.**
   //!           Calling it from anywhere other than tests would replace the
   //!           wall-clock with a fake clock for every redirect resolution in
   //!           the whole process and silently break the cache TTL semantics.
   //!
   //! @param  nowFn  function returning a `time_t` to be used in place of
   //!                `time(nullptr)`.  Pass an empty function to restore the
   //!                default.
   //----------------------------------------------------------------------------
   static void SetClockForTesting(std::function<time_t()> nowFn);

private:

   XrdXrootdRedirHelper()  = delete;
   ~XrdXrootdRedirHelper() = delete;
};

#endif
