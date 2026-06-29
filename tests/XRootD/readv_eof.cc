//------------------------------------------------------------------------------
// Minimal XrdCl driver used by the moncollect end-to-end test: open a file and
// issue a single vector read whose one chunk extends past end-of-file. The
// server treats a readv that cannot be fully satisfied as a terminal error
// ("readv past EOF"), which exercises the mid-transfer read-error reporting on
// the f-stream (XrdXrootdProtocol::monIOErr -> close record with hasERR). The
// program exits non-zero on the (expected) readv error so the caller can tell a
// genuine failure from a connection problem.
//
// Usage: xrdreadv-eof <url>
//------------------------------------------------------------------------------

#include "XrdCl/XrdClFile.hh"

#include <cstdint>
#include <cstdio>
#include <vector>

int main(int argc, char *argv[])
{
  if (argc != 2)
    { std::fprintf(stderr, "usage: %s <url>\n", argv[0]); return 2; }

  XrdCl::File file;
  XrdCl::XRootDStatus st = file.Open(argv[1], XrdCl::OpenFlags::Read);
  if (!st.IsOK())
    { std::fprintf(stderr, "open failed: %s\n", st.ToString().c_str()); return 2; }

  // One chunk that starts far beyond any plausible file size: the server cannot
  // satisfy it and reports a readv error.
  const uint64_t farOffset = 1024ULL * 1024ULL * 1024ULL; // 1 GiB
  const uint32_t length    = 65536;
  std::vector<char> buffer(length);

  XrdCl::ChunkList chunks;
  chunks.push_back(XrdCl::ChunkInfo(farOffset, length, buffer.data()));

  XrdCl::VectorReadInfo *info = nullptr;
  st = file.VectorRead(chunks, buffer.data(), info);
  delete info;

  // We expect the vector read to fail; report that as the program's "success".
  bool readvFailed = !st.IsOK();
  if (!readvFailed)
    std::fprintf(stderr, "vector read unexpectedly succeeded\n");
  else
    std::fprintf(stderr, "vector read failed as expected: %s\n",
                 st.ToString().c_str());

  XrdCl::XRootDStatus cst = file.Close();
  (void)cst;
  return readvFailed ? 0 : 1;
}
