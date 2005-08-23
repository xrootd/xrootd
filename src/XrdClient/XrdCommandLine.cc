//////////////////////////////////////////////////////////////////////////
//                                                                      //
// xrdcp                                                                //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
//                                                                      //
// A cp-like command line tool for xrootd environments                  //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//       $Id$

#include "XrdClient/XrdClientUrlInfo.hh"
#include "XrdClient/XrdClient.hh"
#include "XrdClient/XrdClientAdmin.hh"
#include "XrdClient/XrdClientDebug.hh"
#include "XrdClient/XrdClientEnv.hh"
#include "XrdOuc/XrdOucTokenizer.hh"

#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <stdarg.h>

/////////////////////////////////////////////////////////////////////
// function + macro to allow formatted print via cout,cerr
/////////////////////////////////////////////////////////////////////
extern "C" {

   void cout_print(const char *format, ...) {
      char cout_buff[4096];
      va_list args;
      va_start(args, format);
      vsprintf(cout_buff, format,  args);
      va_end(args);
      cout << cout_buff;
   }

   void cerr_print(const char *format, ...) {
      char cerr_buff[4096];
      va_list args;
      va_start(args, format);
      vsprintf(cerr_buff, format,  args);
      va_end(args);
      cerr << cerr_buff;
   }

#define COUT(s) do {				\
      cout_print s;				\
   } while (0)

#define CERR(s) do {				\
      cerr_print s;				\
   } while (0)

}

//////////////////////////////////////////////////////////////////////





#define XRDCLI_VERSION            "(C) 2004 SLAC INFN xrd 0.1 beta"


///////////////////////////////////////////////////////////////////////
// Coming from parameters on the cmd line

char *opaqueinfo = 0;   // opaque info to be added to urls

// Default open flags for opening a file (xrd)
kXR_unt16 xrd_wr_flags=kXR_async | kXR_mkpath | kXR_open_updt | kXR_new;

char *initialhost;

///////////////////////

///////////////////////////////////////////////////////////////////////
// Generic instances used throughout the code

XrdClient *genclient = 0;
XrdClientAdmin *genadmin = 0;
XrdClientString currentpath;

///////////////////////

void PrintUsage() {
   cerr << "usage: xrd [host] "
     "[-DSparmname stringvalue] ... [-DIparmname intvalue]  [-O<opaque info>]" << endl;
   cerr << " -DSparmname stringvalue     :         override the default value of an internal"
      " XrdClient setting (of string type)" << endl;
   cerr << " -DIparmname intvalue        :         override the default value of an internal"
      " XrdClient setting (of int type)" << endl;
   cerr << " -O     :         adds some opaque information to any used xrootd url" << endl;
   cerr << " -h     :         this help screen" << endl;

   cerr << endl << " where:" << endl;
   cerr << "   parmname     is the name of an internal parameter" << endl;
   cerr << "   stringvalue  is a string to be assigned to an internal parameter" << endl;
   cerr << "   intvalue     is an int to be assigned to an internal parameter" << endl;
}

void PrintPrompt() {

   if (genadmin)
      cout << "root://" << genadmin->GetCurrentUrl().Host << 
	 ":" << genadmin->GetCurrentUrl().Port <<
	 "/" << currentpath;
   
   cout << ">";

}

void PrintHelp() {

   cout << endl <<
      "List of available commands:" << endl <<
      " cd <dir name>" << endl <<
      "  changes the current directory" << endl <<
      "  Note: no existence check is performed." << endl <<
      " envputint <varname> <intval>" << endl <<
      "  puts an integer in the internal environment." << endl <<
      " envputstring <varname> <stringval>" << endl <<
      "  puts a string in the internal environment." << endl <<
      " help" << endl <<
      "  this help screen." << endl <<
      " exit" << endl <<
      "  exits from the program." << endl << endl <<
      " connect [hostname[:port]]" << endl <<
      "  connects to the specified host." << endl <<
      " dirlist [dirname]" << endl <<
      "  gets the requested directory listing." << endl;


   cout << endl;
}

bool CheckAnswer(XrdClientAbs *gencli) {
   if (!gencli->LastServerResp()) return false;

   switch (gencli->LastServerResp()->status) {
   case kXR_ok:
      return true;

   case kXR_error:

      cout << "Error " << gencli->LastServerError()->errnum <<
	 ": " << gencli->LastServerError()->errmsg << endl << endl;
      return false;

   default:
      cout << "Server response: " << gencli->LastServerResp()->status << endl;
      return true;

   }
}


// Main program
int main(int argc, char**argv) {

   int retval = 0;

   DebugSetLevel(0);

   // We want this tool to be able to connect everywhere
   // Note that the side effect of these calls here is to initialize the
   // XrdClient environment.
   // This is crucial if we want to later override its default values
   EnvPutString( NAME_REDIRDOMAINALLOW_RE, "*" );
   EnvPutString( NAME_CONNECTDOMAINALLOW_RE, "*" );
   EnvPutString( NAME_REDIRDOMAINDENY_RE, "" );
   EnvPutString( NAME_CONNECTDOMAINDENY_RE, "" );

   EnvPutInt( NAME_DEBUG, -1);

   for (int i=1; i < argc; i++) {


      if ( (strstr(argv[i], "-O") == argv[i])) {
	 opaqueinfo=argv[i]+2;
	 continue;
      }

      if ( (strstr(argv[i], "-h") == argv[i])) {
	 PrintUsage();
	 exit(0);
	 continue;
      }

      if ( (strstr(argv[i], "-DS") == argv[i]) &&
	   (argc >= i+2) ) {
	cerr << "Overriding " << argv[i]+3 << " with value " << argv[i+1] << ". ";
	 EnvPutString( argv[i]+3, argv[i+1] );
	 cerr << " Final value: " << EnvGetString(argv[i]+3) << endl;
	 i++;
	 continue;
      }

      if ( (strstr(argv[i], "-DI") == argv[i]) &&
	   (argc >= i+2) ) {
	cerr << "Overriding '" << argv[i]+3 << "' with value " << argv[i+1] << ". ";
	 EnvPutInt( argv[i]+3, atoi(argv[i+1]) );
	 cerr << " Final value: " << EnvGetLong(argv[i]+3) << endl;
	 i++;
	 continue;
      }

      // Any other par is ignored
      if ( (strstr(argv[i], "-") == argv[i]) && (strlen(argv[i]) > 1) ) {
	 cerr << "Unknown parameter " << argv[i] << endl;
	 continue;
      }

      if (!initialhost) initialhost = argv[i];

   }

   DebugSetLevel(EnvGetLong(NAME_DEBUG));

   cout << endl << "Welcome to the xrootd command line interface." << endl <<
      "Type 'help' for a list of available commands." << endl;



   if (initialhost)
      genadmin = new XrdClientAdmin(initialhost);

   while(1) {
      char linebuf[4096];
      linebuf[0] = 0;
      XrdOucTokenizer tkzer(linebuf);

      
      PrintPrompt();

      // Now we get a line of input from the console
      if (!fgets(linebuf, 4096, stdin) || !strlen(linebuf))
	 continue;

      // And the simple parsing starts
      tkzer.Attach(linebuf);
      if (!tkzer.GetLine()) continue;
      
      char *cmd = tkzer.GetToken(0, 1);
      
      if (!cmd) continue;
      
      // -------------------------- cd ---------------------------
      if (!strcmp(cmd, "cd")) {
	 char *parmname = tkzer.GetToken(0, 0);

	 if (!parmname || !strlen(parmname)) {
	    cout << "A directory name is needed." << endl << endl;
	    continue;
	 }

	 // Quite trivial directory processing
	 if (!strcmp(parmname, "..")) {
	    int pos = currentpath.RFind((char *)"/");

	    if (pos != STR_NPOS)
	       currentpath.EraseToEnd(pos);

	    continue;
	 }

	 if (!strcmp(parmname, "."))
	    continue;
	    
	 currentpath += "/";
	 currentpath += parmname;
	 continue;
      }


      // -------------------------- envputint ---------------------------
      if (!strcmp(cmd, "envputint")) {
	 char *parmname = tkzer.GetToken(0, 0),
	    *val = tkzer.GetToken(0, 1);

	 if (!parmname || !val) {
	    cout << "A parameter name and an integer value are needed." << endl << endl;
	    continue;
	 }

	 EnvPutInt(parmname, atoi(val));
	 DebugSetLevel(EnvGetLong(NAME_DEBUG));
	 continue;
      }

      // -------------------------- envputstring ---------------------------
      if (!strcmp(cmd, "envputstring")) {
	 char *parmname = tkzer.GetToken(0, 0),
	    *val = tkzer.GetToken(0, 1);

	 if (!parmname || !val) {
	    cout << "A parameter name and a string value are needed." << endl << endl;
	    continue;
	 }

	 EnvPutString(parmname, val);
	 continue;
      }

      // -------------------------- help ---------------------------
      if (!strcmp(cmd, "help")) {
	 PrintHelp();
	 continue;
      }

      // -------------------------- exit ---------------------------
      if (!strcmp(cmd, "exit")) {
	 cout << "Goodbye." << endl << endl;
	 retval = 0;
	 break;
      }

      // -------------------------- connect ---------------------------
      if (!strcmp(cmd, "connect")) {
	 char *host = initialhost;

	 // If no host was given, then pretend one
	 if (!host) {

	    host = tkzer.GetToken(0, 1);
	    if (!host || !strlen(host)) {
	       cout << "A hostname is needed to connect somewhere." << endl;
	       continue;
	    }

	 }

	 // Init the instance
	 if (genadmin) delete genadmin;
	 XrdClientString h(host);
	 h  = "root://" + h;
	 h += "//dummy";

	 genadmin = new XrdClientAdmin(h.c_str());

	 // Then connect
	 if (!genadmin->Connect()) {
	    delete genadmin;
	    genadmin = 0;
	 }

	 continue;
      }

      // -------------------------- dirlist ---------------------------
      if (!strcmp(cmd, "dirlist")) {

	 if (!genadmin) {
	    cout << "Not connected to any server." << endl;
	    continue;
	 }

	 char *dirname = tkzer.GetToken(0, 0);
	 XrdClientString path;

	 if (dirname)
	    path = currentpath + "/" + dirname;
	 else path = currentpath;

	 // Now try to issue the request
	 vecString vs;
	 genadmin->DirList(path.c_str(), vs);

	 // Now check the answer
	 if (!CheckAnswer(genadmin))
	    continue;
      
	 for (int i = 0; i < vs.GetSize(); i++)
	    cout << vs[i] << endl;

	 cout << endl;
	 continue;
      }


      // ---------------------------------------------------------------------
      // -------------------------- not recognized ---------------------------
      cout << "Command not recognized." << endl <<
	 "Type \"help\" for a list of commands." << endl << endl;
      

      
	
   } // while (1)


   if (genadmin) delete genadmin;
   return retval;

}
