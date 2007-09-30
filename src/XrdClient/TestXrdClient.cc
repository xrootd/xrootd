//       $Id$
// Simple keleton for simple tests of Xrdclient and XrdClientAdmin
//

#include "XrdClient/XrdClient.hh"
#include "XrdClient/XrdClientAdmin.hh"
#include "XrdClient/XrdClientEnv.hh"
#include <iostream>

int main(int argc, char **argv) {

  EnvPutInt(NAME_DEBUG, -1);
  //EnvPutInt(NAME_READCACHESIZE, 100000000);


//   XrdClient *x = new XrdClient(argv[1]);
//   XrdClient *y = new XrdClient(argv[2]);
//   x->Open(0, 0);
    
//      for (int i = 0; i < 1000; i++)
//        x->Copy("/tmp/testcopy");
  
//   x->Close();

//   delete x;
//   x = 0;
   
//   y->Open(0, 0);
  
//      for (int i = 0; i < 1000; i++)
//        x->Copy("/tmp/testcopy");
  
//   y->Close();
  
//   delete y;

  XrdClientUrlInfo u;
  XrdClientAdmin::SetAdminConn(false);
  XrdClientAdmin *adm = new XrdClientAdmin(argv[1]);

  adm->Connect();

  string s;
  int i = 0;
  while (!cin.eof()) {
    cin >> s;

    if (!s.size()) continue;

    adm->Locate((kXR_char*)s.c_str(), u, false);

    if (!(i % 100)) cout << i << "...";
    i++;

  }


  delete adm;

      


}
