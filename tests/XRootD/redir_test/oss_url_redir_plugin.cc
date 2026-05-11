//------------------------------------------------------------------------------
// Test-only OSS wrapper loaded by tests/XRootD/tpcredir.cfg to exercise the
// URL-form SFS_REDIRECT branch of fsRedirPI().
//
// For any path beginning with "/urlredir/" the Open() implementation returns
// -EDESTADDRREQ and stores a fully-formed URL in the open env under the
// "FileURL" key.  XrdOfs::open translates that pair into SFS_REDIRECT with
// the URL placed in the SfsError; XrdXrootdProtocol::fsRedirPI() then takes
// its URL-form branch and routes the redirect through the URL branch of
// XrdXrootdRedirHelper::Redirect(), which is the path the system test
// asserts on.  All other paths pass through unchanged.
//------------------------------------------------------------------------------

#include "XrdOss/XrdOssWrapper.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdVersion.hh"

#include <cerrno>
#include <cstring>
#include <memory>

namespace {

// Sentinel target this plugin advertises to OFS.  The redirect plugin
// (loaded via xrootd.redirlib) then rewrites this URL via RedirectURL();
// the values here only need to survive far enough for the helper to call
// the plugin, after which the rewritten URL is what reaches the client.
//
// The host MUST be DNS-resolvable: XrdXrootdRedirHelper::Redirect() calls
// LookupTarget() (which calls XrdNetAddr::Set()) before invoking the
// plugin, and a resolution failure short-circuits the helper to
// Outcome::Unchanged -- skipping the plugin entirely and letting the
// original URL reach the client.  "localhost" is guaranteed to resolve.
// The port and path do not need to be reachable; they are placeholders
// the plugin overwrites.
constexpr const char *kRedirURL =
    "root://localhost:9996//urlredir/destfile";

constexpr const char *kTriggerPrefix = "/urlredir/";

class File final : public XrdOssWrapDF
{
public:
   explicit File(std::unique_ptr<XrdOssDF> wrapped)
      : XrdOssWrapDF(*wrapped), m_wrapped(std::move(wrapped)) {}

   int Open(const char *path, int Oflag, mode_t Mode,
            XrdOucEnv &env) override
   {
      if (path && strstr(path, kTriggerPrefix))
         {env.Put("FileURL", kRedirURL);
          return -EDESTADDRREQ;
         }
      return wrapDF.Open(path, Oflag, Mode, env);
   }

private:
   std::unique_ptr<XrdOssDF> m_wrapped;
};

class FileSystem final : public XrdOssWrapper
{
public:
   FileSystem(XrdOss *oss, XrdSysLogger *, XrdOucEnv *)
      : XrdOssWrapper(*oss), m_oss(oss) {}

   XrdOssDF *newFile(const char *user) override
   {
      std::unique_ptr<XrdOssDF> wrapped(wrapPI.newFile(user));
      return new File(std::move(wrapped));
   }

   // XrdOfs::open invokes Create() before the per-file Open() for write
   // requests, and the underlying Create() would fail with -ENOENT because
   // /urlredir/ does not exist on disk.  Returning -ENOTSUP makes XrdOfs
   // "promote creation to subsequent open" (XrdOfs.cc), which lets our
   // Open() above fire and emit the URL-form SFS_REDIRECT.
   int Create(const char *tid, const char *path, mode_t mode,
              XrdOucEnv &env, int opts = 0) override
   {
      if (path && strstr(path, kTriggerPrefix)) return -ENOTSUP;
      return wrapPI.Create(tid, path, mode, env, opts);
   }

private:
   std::unique_ptr<XrdOss> m_oss;
};

} // namespace

extern "C" {

XrdOss *XrdOssAddStorageSystem2(XrdOss *curr_oss, XrdSysLogger *logger,
                                const char * /*config_fn*/,
                                const char * /*parms*/,
                                XrdOucEnv *envP)
{
   return new FileSystem(curr_oss, logger, envP);
}

XrdVERSIONINFO(XrdOssAddStorageSystem2, urlredirfs);

} // extern "C"
