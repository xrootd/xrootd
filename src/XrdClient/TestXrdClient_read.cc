#include "XrdClient/XrdClient.hh"
#include "XrdClient/XrdClientEnv.hh"
#include <iostream>
#include <fstream>
#include <vector>

int main(int argc, char **argv) {
   void *buf;
   if (argc < 3) {
      cout <<
	 "This program gets from the standard input a sequence of" << endl <<
	 " <length> <offset>             (one for each line, with <length> less than 16M)" << endl <<
	 " and performs the corresponding read requests towards the given xrootd URL or to ALL" << endl <<
	 " the xrootd URLS contained in the given file." << endl <<
	 endl <<
	 "Usage: TestXrdClient_read <xrootd url or file name> <rasize> <cachesize> <debuglevel>" << 
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
   
   // Check if we have a file or a root:// url
   bool isrooturl = (strstr(argv[1], "root://"));

   if (isrooturl) {
     XrdClient *cli = new XrdClient(argv[1]);
     EnvPutInt( NAME_READAHEADTYPE, 0);
     cli->Open(0, 0);

     while (!cin.eof()) {
       int sz = 0;
       long long offs = 0;
       int retval = 0;
       
       cin >> sz >> offs;

       if (offs || sz) {
          retval = cli->Read(buf, offs, sz);
          cout << endl << "---Read returned " << retval << endl;
       }
       else break;
       
       
       if (retval <= 0) {
          cout << "------ A read failed" << endl << endl;
          exit(1);
       }
     }
   }
   else {
     vector<XrdClient *> xrdcvec;
     ifstream filez(argv[1]);
     int i = 0;
     XrdClientUrlInfo u;

     // Open all the files (in parallel man!)
     while (!filez.eof()) {
       string s;
       XrdClient * cli;

       filez >> s;
       if (s != "") {
	 cli = new XrdClient(s.c_str());
         u.TakeUrl(s.c_str());

         cout << "Mytest " << time(0) << " File: " << u.File << " - Opening." << endl;
	 if (cli->Open(0, 0)) {
	   cout << "--- Open of " << s << " in progress." << endl;
	   xrdcvec.push_back(cli);
	 }
	 else delete cli;
       }

       i++;
     }

     filez.close();
     cout << "--- All the open requests have been submitted" << endl;
     
     i = 0;

     // Now fire the read trace to all the instances
     while (!cin.eof()) {
       int sz = 0;
       long long offs = 0;
       int retval = 0;
       
       cin >> sz >> offs;
    
       if (offs || sz) {
          cout << "-----<<<<<< Trying to read " << sz << "@" << offs << endl;
          for(int i = 0; i < (int) xrdcvec.size(); i++) {
             retval = xrdcvec[i]->Read(buf, offs, sz);
             cout << "Mytest " << time(0) << " File: " << xrdcvec[i]->GetCurrentUrl().File << " - Finished read " << sz << "@" << offs << endl;
             if (retval <= 0) {
                cout << "------ A read failed:" << xrdcvec[i]->GetCurrentUrl().GetUrl() << endl << endl;
             }
          }
       }
       else break;
       
       cout << endl << "Last Read (of " << xrdcvec.size() << ") returned " << retval << endl;
       
       i++;
     }

     cout << "--- Closing all instances" << endl;
     for(int i = 0; i < (int) xrdcvec.size(); i++) {
        xrdcvec[i]->Close();
        cout << "Mytest " << time(0) << " File: " << xrdcvec[i]->GetCurrentUrl().File << " - Closed." << endl;
     }
    
     cout << "--- Deleting all instances" << endl;
     for(int i = 0; i < (int) xrdcvec.size(); i++) delete xrdcvec[i];
     
     cout << "--- Clearing pointer vector" << endl; 
     xrdcvec.clear();
   }
  

   cout << "--- Freeing buffer" << endl;
   free(buf);

   cout << "--- bye bye" << endl;
   return 0;

}
