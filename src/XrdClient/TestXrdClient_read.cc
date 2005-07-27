#include "XrdClient/XrdClient.hh"
#include "XrdClient/XrdClientEnv.hh"
#include <iostream>
#include <fstream>

int main(int argc, char **argv) {
   void *buf;
   if (argc < 3) {
      cout <<
	 "This program gets from the standard input a sequence of" << endl <<
	 " <length> <offset>             (one for each line, with <length> less than 16M)" << endl <<
	 " and performs the corresponding read requests towards the given xrootd URL." << endl <<
	 endl <<
	 "Usage: TestXrdClient_read <xrootd url> <rasize> <cachesize> <debuglevel>" << 
	 endl << endl <<
	 " Where:" << endl <<
	 "  <xrootd url> is the xrootd URL of a remote file " << endl <<
	 "  <rasize> is the cache line/readahead block size. Can be 0." << endl <<
	 "  <cachesize> is the size of the internal cache, in bytes. Can be 0." << endl <<
	 "  <debuglevel can be an integer from -1 to 3." << endl << endl;

      exit(1);
   }

   EnvPutInt( NAME_READAHEADSIZE, atol(argv[2]));
   EnvPutInt( NAME_READCACHESIZE, atol(argv[3]));
   
   EnvPutInt( NAME_DEBUG, atol(argv[4]));

   buf = malloc(16*1024*1024);
       	  
   XrdClient *cli = new XrdClient(argv[1]);
   EnvPutInt( NAME_READAHEADTYPE, 0);
   cli->Open(0, 0);

   while (!cin.eof()) {
      int sz;
      long long offs;
      int retval;

      cin >> sz >> offs;

      retval = cli->Read(buf, offs, sz);

      cout << endl << "Read returned " << retval << endl;

      if (retval <= 0) exit(1);

   }
  

   free(buf);
   return 0;

}
