#include "XrdClient.hh"


int main(int argc, char **argv) {


   XrdClient *x = new XrdClient(argv[1]);
   x->Open(0, 0);
   for (int i = 0; i < 1000; i++)
      x->Copy("/tmp/testcopy");
   x->Close();
   SafeDelete(x);


}
