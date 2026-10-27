//------------------------------------------------------------------------------
// Range-reading client used by the XCache tests.
//
// xrdcp can only fetch whole files, which is not enough to exercise the cache:
// the interesting behaviour is block-granular. This reads exactly the ranges it
// is told to, so a test can leave a file deliberately incomplete, come back for
// the rest, drive the ReadV path, or have several readers land on the same
// blocks at once.
//
// Data is written to <outfile> in the order the ranges were given, so the test
// can rebuild the same bytes locally with dd and cmp the two.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClXRootDResponses.hh"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace
{

struct Range { uint64_t offset; uint32_t length; };

int fail(const std::string &what, const XrdCl::XRootDStatus &st)
{
   fprintf(stderr, "xrdpfc-read: %s failed: %s\n", what.c_str(), st.ToStr().c_str());
   return 1;
}

// "<offset>:<length>"
bool parse_range(const char *s, Range &r)
{
   char *end = nullptr;
   r.offset = strtoull(s, &end, 0);
   if (end == s || *end != ':') return false;
   const char *l = end + 1;
   r.length = (uint32_t) strtoul(l, &end, 0);
   return end != l && *end == '\0' && r.length > 0;
}

bool write_out(const char *path, const std::vector<char> &buf, size_t n)
{
   FILE *f = fopen(path, "wb");
   if ( ! f) { perror("xrdpfc-read: fopen"); return false; }
   bool ok = fwrite(buf.data(), 1, n, f) == n;
   if (fclose(f) != 0) ok = false;
   if ( ! ok) fprintf(stderr, "xrdpfc-read: short write to %s\n", path);
   return ok;
}

int cmd_read(const std::vector<std::string> &args)
{
   // read <url> <outfile> <off:len> [<off:len> ...]
   // Each range is a separate Read() on one open file.
   if (args.size() < 3) return 2;

   std::vector<Range> ranges;
   size_t total = 0;
   for (size_t i = 2; i < args.size(); ++i)
   {
      Range r;
      if ( ! parse_range(args[i].c_str(), r))
      {
         fprintf(stderr, "xrdpfc-read: bad range '%s'\n", args[i].c_str());
         return 2;
      }
      ranges.push_back(r);
      total += r.length;
   }

   XrdCl::File f;
   XrdCl::XRootDStatus st = f.Open(args[0], XrdCl::OpenFlags::Read);
   if ( ! st.IsOK()) return fail("open", st);

   std::vector<char> buf(total);
   size_t pos = 0;
   for (const Range &r : ranges)
   {
      uint32_t got = 0;
      st = f.Read(r.offset, r.length, buf.data() + pos, got);
      if ( ! st.IsOK()) return fail("read", st);
      if (got != r.length)
      {
         fprintf(stderr, "xrdpfc-read: short read at %llu: wanted %u got %u\n",
                 (unsigned long long) r.offset, r.length, got);
         return 1;
      }
      pos += got;
   }

   st = f.Close();
   if ( ! st.IsOK()) return fail("close", st);

   return write_out(args[1].c_str(), buf, pos) ? 0 : 1;
}

int cmd_readv(const std::vector<std::string> &args)
{
   // readv <url> <outfile> <off:len> [<off:len> ...]
   // All ranges in a single VectorRead(), which is the path a scattered
   // analysis read takes and which the cache coalesces into block runs.
   if (args.size() < 3) return 2;

   std::vector<Range> ranges;
   size_t total = 0;
   for (size_t i = 2; i < args.size(); ++i)
   {
      Range r;
      if ( ! parse_range(args[i].c_str(), r))
      {
         fprintf(stderr, "xrdpfc-read: bad range '%s'\n", args[i].c_str());
         return 2;
      }
      ranges.push_back(r);
      total += r.length;
   }

   XrdCl::File f;
   XrdCl::XRootDStatus st = f.Open(args[0], XrdCl::OpenFlags::Read);
   if ( ! st.IsOK()) return fail("open", st);

   std::vector<char> buf(total);
   XrdCl::ChunkList chunks;
   size_t pos = 0;
   for (const Range &r : ranges)
   {
      chunks.push_back(XrdCl::ChunkInfo(r.offset, r.length, buf.data() + pos));
      pos += r.length;
   }

   XrdCl::VectorReadInfo *vinfo = nullptr;
   st = f.VectorRead(chunks, nullptr, vinfo);
   if ( ! st.IsOK()) { delete vinfo; return fail("vectorread", st); }

   uint32_t got = vinfo ? vinfo->GetSize() : 0;
   delete vinfo;
   if (got != total)
   {
      fprintf(stderr, "xrdpfc-read: short vector read: wanted %zu got %u\n", total, got);
      return 1;
   }

   st = f.Close();
   if ( ! st.IsOK()) return fail("close", st);

   return write_out(args[1].c_str(), buf, pos) ? 0 : 1;
}

int cmd_multi(const std::vector<std::string> &args)
{
   // multi <url> <outfile> <nreaders> <length>
   // nreaders independent opens reading the same first <length> bytes at the
   // same time, so they contend for the same blocks. Every reader must return
   // identical data; the cache should fetch those bytes from the origin once.
   if (args.size() != 4) return 2;

   const std::string url = args[0];
   const int      nread  = atoi(args[2].c_str());
   const uint32_t length = (uint32_t) strtoul(args[3].c_str(), nullptr, 0);
   if (nread < 1 || length < 1) return 2;

   std::vector<std::vector<char>> bufs(nread, std::vector<char>(length));
   std::vector<int> rc(nread, 0);
   std::vector<std::thread> threads;

   for (int i = 0; i < nread; ++i)
   {
      threads.emplace_back([&, i]() {
         XrdCl::File f;
         XrdCl::XRootDStatus st = f.Open(url, XrdCl::OpenFlags::Read);
         if ( ! st.IsOK()) { rc[i] = fail("open", st); return; }
         uint32_t got = 0;
         st = f.Read(0, length, bufs[i].data(), got);
         if ( ! st.IsOK()) { rc[i] = fail("read", st); return; }
         if (got != length)
         {
            fprintf(stderr, "xrdpfc-read: reader %d short read: %u of %u\n", i, got, length);
            rc[i] = 1;
            return;
         }
         st = f.Close();
         if ( ! st.IsOK()) rc[i] = fail("close", st);
      });
   }
   for (auto &t : threads) t.join();

   for (int i = 0; i < nread; ++i)
      if (rc[i]) return rc[i];

   for (int i = 1; i < nread; ++i)
      if (memcmp(bufs[0].data(), bufs[i].data(), length) != 0)
      {
         fprintf(stderr, "xrdpfc-read: reader %d returned different data than reader 0\n", i);
         return 1;
      }

   return write_out(args[1].c_str(), bufs[0], length) ? 0 : 1;
}

const char *usage =
   "Usage: xrdpfc-read read  <url> <outfile> <off:len> [<off:len> ...]\n"
   "       xrdpfc-read readv <url> <outfile> <off:len> [<off:len> ...]\n"
   "       xrdpfc-read multi <url> <outfile> <nreaders> <length>\n";

}

int main(int argc, char *argv[])
{
   if (argc < 2) { fputs(usage, stderr); return 2; }

   const std::string cmd = argv[1];
   std::vector<std::string> args(argv + 2, argv + argc);

   int rc;
   if      (cmd == "read")  rc = cmd_read(args);
   else if (cmd == "readv") rc = cmd_readv(args);
   else if (cmd == "multi") rc = cmd_multi(args);
   else { fputs(usage, stderr); return 2; }

   if (rc == 2) fputs(usage, stderr);
   return rc;
}
