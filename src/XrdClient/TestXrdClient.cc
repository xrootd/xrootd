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



   XrdCA_Initialize(argv[1], 2);
   bool ans;
   ans = XrdCA_ExistFiles("/store");
   cout << "\nThe answer of XTNetAdmin_ExistFiles is:" << ans << endl;

   ans = XrdCA_ExistDirs("/data/babar/kanga\n/etc\n/mydir");
   cout << "\nThe answer of XTNetAdmin_ExistDirs is:" << ans << endl;

   ans = XrdCA_IsFileOnline("/store/PR/R14/AllEvents/0004/35/14.2.0b/AllEvents_00043511_14.2.0bV00.02E.root");
   cout << "\nThe answer of XTNetAdmin_IsFileOnline is:" << ans << endl;

   XrdCA_Terminate();

      


}
