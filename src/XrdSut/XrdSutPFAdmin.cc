// $Id$
//
// Testing PFile ... 
//
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <XrdOuc/XrdOucString.hh>

#include <XrdSut/XrdSutAux.hh>
#include <XrdSut/XrdSutPFEntry.hh>
#include <XrdSut/XrdSutPFile.hh>
#include <XrdSut/XrdSutRndm.hh>
//
// Globals 
XrdOucString gFileRef = "etc/testpfile";
#define PRINT(x) {cerr <<x <<endl;}

int main( int argc, char **argv )
{
   // Manipulate PFiles

   XrdSutSetTrace(sutTRACE_Debug);

   // The file
   XrdOucString File = gFileRef;

   char *onam = argv[0];
   PRINT("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
   PRINT("++++++++++++++++ Welcome to XrdSutPFAdmin ++++++++++++++++++");
   PRINT("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");

   if (argc == 1) {
      PRINT(onam<<":+ At least one keyword and its options is needed!");
      PRINT(onam<<":+ Recognized keywords:");
      PRINT(onam<<":+   add     <name> [<file>]");
      PRINT(onam<<":+   read    <name> [<file>]");
      PRINT(onam<<":+   remove  <name> [<file>]");
      PRINT(onam<<":+   browse         [<file>]");
      PRINT(onam<<":+   trim           [<file>]");
      exit(1);
   }

   if (!strcmp(argv[1],"add")) {
      if (argc == 2) {
         PRINT(onam<<":+ keyword 'add' requires a name as argument");
         exit(1);
      }
      // Define file
      if (argc > 4)
         File = argv[3];
      XrdSutPFile ff(File.c_str());
      // Prepare Entry
      XrdSutPFEntry ent;
      ent.SetName(argv[2]);
      ent.status = 0;
      ent.cnt    = 0;
      // Init Random machinery
      XrdSutRndm::Init();
      XrdOucString salt;
      XrdSutRndm::GetString(3,8,salt);
      ent.buf1.SetBuf(salt.c_str(),salt.length());
      // Save (or update) entry
      ff.WriteEntry(ent);
      ff.Browse();
   } else if (!strcmp(argv[1],"read")) {
      if (argc == 2) {
         PRINT(onam<<":+ keyword 'read' requires a name as argument");
         exit(1);
      }
      // Define file
      if (argc > 4)
         File = argv[3];
      XrdSutPFile ff(File.c_str());
      // Get related entry, if any
      XrdSutPFEntry ent;
      int nr = ff.ReadEntry(argv[2],ent);
      if (nr > 0) {
         PRINT(" Found entry for '"<<argv[2]<<"' in file: "<<ff.Name());
         PRINT(" Details: "<<ent.AsString());
      } else {
         PRINT(" Entry for '"<<argv[2]<<"' not found in file: "<<ff.Name());
      }
   } else if (!strcmp(argv[1],"remove")) {
      if (argc == 2) {
         PRINT(onam<<":+ keyword 'remove' requires a name as argument");
         exit(1);
      }
      // Define file
      if (argc > 4)
         File = argv[3];
      XrdSutPFile ff(File.c_str());
      // Get related entry, if any
      XrdSutPFEntry ent;
      int nr = ff.RemoveEntry(argv[2]);
      if (nr == 0) {
         PRINT(" Entry for '"<<argv[2]<<"' removed from file: "<<ff.Name());
      } else {
         PRINT(" Entry for '"<<argv[2]<<"' not found in file: "<<ff.Name());
      }
   } else if (!strcmp(argv[1],"browse")) {
      // Define file
      if (argc > 3)
         File = argv[2];
      XrdSutPFile ff(File.c_str());
      ff.Browse();
   } else if (!strcmp(argv[1],"trim")) {
      // Define file
      if (argc > 3)
         File = argv[2];
      XrdSutPFile ff(File.c_str());
      ff.Trim();
      ff.Browse();
   } else {
      PRINT(onam<<":+ Unknown keyword ("<<argv[1]<<")");
   }

   exit(0);
}
