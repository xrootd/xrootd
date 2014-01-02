//
// Get a fragment of a file and write it out to stdout.
//
// Note that one should have a valid GSI proxy, error messages from
//
//
// Example usage:
//   ./xrdfragcp root://xrootd.unl.edu//store/mc/Summer12/WJetsToLNu_TuneZ2Star_8TeV-madgraph-tarball/AODSIM/PU_S7_START52_V9-v2/00000/E47B9F8B-42EF-E111-A3A4-003048FFD756.root 0 1024 > xxx

#include "XrdClient/XrdClient.hh"
#include <unistd.h>

#include <memory>

#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char *argv[])
{
  if (argc < 4 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)
  {
    fprintf(stderr,
	    "Usage: %s file-name offset length\n"
	    "   Maximum supported length is 1GByte (all is read in one chunk).\n"
	    "   Requested data is put to standard output.\n",
	    argv[0]);
  }


  long long offset = atoll(argv[2]);
  long long sizell = atoi (argv[3]);
  int numCopies = 1;
  if (argc > 4)
     numCopies = atoi (argv[4]);

  if (offset < 0)
  {
    fprintf(stderr, "Error: offset '%lld' must be non-negative.\n", offset);
    exit(1);
  }

  if (sizell <= 0 || sizell > 1024*1024*1024)
  {
    fprintf(stderr, "Error: size '%lld' must be larger than zero and smaller than 1GByte.\n", sizell);
    exit(1);
  }

  int size = sizell;


  std::auto_ptr<XrdClient> c( new XrdClient(argv[1]) );

  if ( ! c->Open(0, kXR_async) || c->LastServerResp()->status != kXR_ok)
  {
    fprintf(stderr, "Error opening file '%s'.", argv[1]);
    exit(1);
  }

  XrdClientStatInfo si;
  c->Stat(&si);

  if (offset + size > si.size)
  {
    fprintf(stderr, "Error: requested chunk not in file, file-size=%lld.", si.size);
    exit(1);
  }

  std::vector<char> buf;
  buf.resize(size);

  printf("reading buff \n");
  for (int i = 0 ; i < numCopies; ++i)
     c->Read(&buf[0], offset + size*i, size);

  

  return 0;
}
