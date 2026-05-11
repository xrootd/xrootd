//------------------------------------------------------------------------------
// Test-only XrdXrootdRedirPI plugin loaded by tests/XRootD/tpcredir.cfg.
//
// Two sentinels, one per redirect form:
//
//   * Redirect()    -> "rdlplugin-rewritten:9999"
//        Exercised by both HTTP-TPC's COPY redirect (RedirectTransfer) and
//        the XRootD protocol's host:port-form SFS_REDIRECT path
//        (XrdXrootdProtocol::fsRedirPI()).
//
//   * RedirectURL() -> "root://rdlplugin-rewritten-url:9997//destfile"
//        Exercised only by the XRootD protocol's URL-form SFS_REDIRECT path
//        (paired with the OSS wrapper in oss_url_redir_plugin.cc, which
//        causes XrdOfs::open to emit URL-form SFS_REDIRECT for paths under
//        /urlredir/).
//
// Issue #2767.  Not for production use.
//------------------------------------------------------------------------------

#include <cstdint>
#include <string>

#include "XrdNet/XrdNetAddrInfo.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdVersion.hh"
#include "XrdXrootd/XrdXrootdRedirPI.hh"

namespace
{
   class TestRedirPI : public XrdXrootdRedirPI
   {
   public:
      std::string Redirect(const char * /*Target*/, uint16_t &port,
                           const char *TCgi,
                           XrdNetAddrInfo & /*TNetInfo*/,
                           XrdNetAddrInfo & /*CNetInfo*/) override
      {
         port = 9999;
         std::string out("rdlplugin-rewritten");
         if (TCgi && *TCgi) out.append(TCgi);
         return out;
      }

      std::string RedirectURL(const char * /*urlHead*/,
                              const char * /*Target*/,
                              const char * /*port*/,
                              const char * /*urlTail*/,
                              int &        /*rdrOpts*/,
                              XrdNetAddrInfo & /*TNetInfo*/,
                              XrdNetAddrInfo & /*CNetInfo*/) override
      {
         return std::string("root://rdlplugin-rewritten-url:9997//destfile");
      }
   };
}

XrdVERSIONINFO(XrdXrootGetdRedirPI, RedirTestPI);

extern "C"
XrdXrootdRedirPI *XrdXrootGetdRedirPI(XrdXrootdRedirPI * /*prevPI*/,
                                      XrdSysLogger     * /*Logger*/,
                                      const char       * /*parms*/,
                                      const char       * /*configFn*/,
                                      XrdOucEnv        * /*envP*/)
{
   static TestRedirPI gPI;
   return &gPI;
}
