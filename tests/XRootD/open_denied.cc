//------------------------------------------------------------------------------
// Minimal XrdCl driver used by the moncollect end-to-end test: open an existing
// file for write and keep it open, then open the SAME file for write again on a
// second handle. The server's file-lock manager denies the second writer with
// kXR_FileLocked *before* the filesystem open is attempted, a path that bypassed
// the f-stream terminal-error report (it sends the error directly rather than
// through fsError). This provokes a failed-open report for that bypass.
//
// Exits 0 iff the second open failed as expected, so the caller can tell a
// genuine denial from a connection problem.
//
// Usage: xrdopen-denied <url>
//------------------------------------------------------------------------------

#include "XrdCl/XrdClFile.hh"

#include <cstdio>

int main(int argc, char *argv[])
{
  if (argc != 2)
    { std::fprintf(stderr, "usage: %s <url>\n", argv[0]); return 2; }

  // First writer takes the lock and holds it open.
  XrdCl::File first;
  XrdCl::XRootDStatus st = first.Open(argv[1], XrdCl::OpenFlags::Update);
  if (!st.IsOK())
    { std::fprintf(stderr, "first open failed: %s\n", st.ToString().c_str());
      return 2; }

  // Second writer of the same path is denied (kXR_FileLocked) by the server's
  // lock manager before the open reaches the filesystem.
  XrdCl::File second;
  XrdCl::XRootDStatus st2 = second.Open(argv[1], XrdCl::OpenFlags::Update);

  bool denied = !st2.IsOK();
  if (!denied)
    { std::fprintf(stderr, "second open unexpectedly succeeded\n");
      XrdCl::XRootDStatus s = second.Close(); (void)s; }
  else
    std::fprintf(stderr, "second open denied as expected: %s\n",
                 st2.ToString().c_str());

  XrdCl::XRootDStatus cst = first.Close(); (void)cst;
  return denied ? 0 : 1;
}
