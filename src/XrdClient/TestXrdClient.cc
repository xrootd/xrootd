//       $Id$

#include "XrdClient/XrdClient.hh"
#include "XrdClient/XrdClientAdmin_c.hh"
#include <iostream>

int main(int argc, char **argv) {


//    XrdClient *x = new XrdClient(argv[1]);
//    if (x->Open(0, 0)) {

//       for (int i = 0; i < 1000; i++)
// 	 x->Copy("/tmp/testcopy");

//       x->Close();
//    }
//    delete x;



   XrdInitialize(argv[1], "DebugLevel 3\nConnectTimeout 5");
   bool ans;
   char *strans;

   ans = XrdExistFiles("/store/PR/R14/AllEvents/0004/35/14.2.0b/AllEvents_00043511_14.2.0bV00.02E.root");
   cout << "\nThe answer of XTNetAdmin_ExistFiles is:" << ans << endl;

   ans = XrdExistDirs("/prod");
   cout << "\nThe answer of XTNetAdmin_ExistDirs is:" << ans << endl;

   ans = XrdExistFiles("/prod\n/store/PR/R14/AllEvents/0004/35/14.2.0b/AllEvents_00043511_14.2.0bV00.02E.root\n/tmp");
   cout << "\nThe answer of XTNetAdmin_ExistFiles is:" << ans << endl;

   ans = XrdExistDirs("/prod\n/store\n/store/PR");
   cout << "\nThe answer of XTNetAdmin_ExistDirs is:" << ans << endl;

   ans = XrdIsFileOnline("/store/PR/R14/AllEvents/0004/35/14.2.0b/AllEvents_00043511_14.2.0bV00.02E.root");
   cout << "\nThe answer of XTNetAdmin_IsFileOnline is:" << ans << endl;

   strans = XrdGetChecksum("/prod/store/PRskims/R14/16.0.1a/AllEvents/23/AllEvents_2301.01.root");
   cout << "\nThe answer of XTNetAdmin_Getchecksum is:'" << strans << "'" << endl;

   XrdTerminate();

      


}
