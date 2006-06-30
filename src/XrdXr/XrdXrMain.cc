/*****************************************************************************/
/*                                                                           */
/*                          X r d X r C l i e n t . c c                      */
/*                                                                           */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*            Produced by Heinz Stockinger for Stanford University           */
/*****************************************************************************/

//         $Id$

const char *XrdXrMainCVSID = "$Id$";

/**
 * Client for the XRootd.
 *
 * The following program uses the XrdXrClient class that provides a subset of 
 * the xrootd procol.
 *
 */

/*****************************************************************************/
/*                        i n c l u d e   f i l e s                          */
/*****************************************************************************/

#include <iostream.h>
#include <unistd.h>
#include <pwd.h>
#include <string>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "XProtocol/XProtocol.hh"
#include "XrdXr/XrdXrClient.hh"
#include "XrdXr/XrdXrTrace.hh"
#include "XrdOuc/XrdOucLogger.hh"
#include "XrdSec/XrdSecInterface.hh"

/*****************************************************************************/
/*                          g l o b a l   v a r i a b l e s                  */
/*****************************************************************************/

using namespace std;

extern XrdOucTrace  XrTrace; 
       XrdXrClient *client;  

enum executionValues {
  xr_error = -1,
  xr_exit  = 1,
  xr_help,
  xr_test,
  xr_out
};

bool login = false;    // check if login was successful
int  fd;               // file descriptor for outfile

extern "C"
{
XrdSecProtocol *(*XrdXrootdSecGetProtocol)(const char *,
                                           const struct sockaddr  &,
                                           const XrdSecParameters &,
                                           XrdOucErrInfo    *)
                                           = XrdSecGetProtocol;
}

/*****************************************************************************/
/*                           g e t T o k e n                                 */
/*****************************************************************************/

/**
 * Search for blanks (white space) in the buffer and return 
 * a single token (item) in this buffer identified by number
 *
 * Input:   buf    - string buffer that contains white space separated tokens
 *          number - position of the token to be returned. Start position is 0.
 *
 * Output:  Return a string to the token at the requested position or ""
 *          if the token does not exist.
 */
string getToken(string buffer, int number)
{
  int pos;
  int found=0;

  // Find the start of the token
  //
  for (int i = 0; i < number; i++) {
    pos = buffer.find(' ');
    if (pos > -1) {
      buffer = buffer.substr(pos+1);
      found++;
    }
  }
  
  // Determine the end of the token
  //
  pos = buffer.find(' ');
  if (pos > -1) {
    buffer = buffer.substr(0,pos);
  }

  // We need to find 'number' white spaces. If not, return "" since the 
  // requested position does not exist 
  //
  if (found != number) {
    return "";
  }
  return buffer;
} // getToken

/*****************************************************************************/
/*                          n u m b e r O f T o k e n s                      */
/*****************************************************************************/

/**
 * Return the number of white space separated tokens in a char buffer
 *
 * Input:   buf    - char buffer that contains white space separated tokens
 *
 * Outpt:   Return the number of tokens
 */
int numberOfTokens(char *buf) 
{

  string buffer(buf);
  int pos;
  int found=1;

  if (strlen(buf) == 0) {
    return 0;
  }

  do {
    pos = buffer.find(' ');
    if (pos > -1) {
      buffer = buffer.substr(pos+1);
      found++;
    }
  } while (pos != -1);

  return found;

} // numberOfTokens

/*****************************************************************************/
/*                           p r i n t H e l p                               */
/*****************************************************************************/

void printHelp() 
{
  cout << "XrClient help: " << endl;
  cout << "auth <credtype> <cred>" << endl;
  cout << "close          close the currently open file" << endl;
  cout << "exit           exit program" << endl;
  cout << "login [<username>] [<role>]" << endl;
  cout << "    <username> is the Unix UID that is used to login " << endl;
  cout << "    <role>     can be either kXR_useruser (default) or kXR_useradmin" << endl;
  cout << "open <path> [a|c|...] [<mode>]" << endl;
  cout << "    <path>     is the absolute file path on the remote host " << endl; 
  cout << "    a ...      open flags - see protocol specification for details " << endl;
  cout << "    <mode>     access permission mode - see protocol specification for details " << endl;
  cout << "outfile <filename> " << endl;
  cout << "               a read request will write to the file <filename>" << endl;
  cout << "quit           exit program" << endl;
  cout << "read <offset> <length>" << endl;
  cout << "stat <path>    status for a file identified by path" << endl;
  cout << "test           run a few test commands" << endl; 
 

} // printHelp

/*****************************************************************************/
/*                          d o O u t f i l e                                */
/*****************************************************************************/

void doOutfile(char* buffer)
{
  string filename = getToken(buffer, 1);

  if (filename == "") {
    cerr << "Filepath is missing." << endl;
    return;
  }

  // Open/create a new file
  //
  fd = open(filename.c_str(), O_CREAT | O_RDWR, 
	    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  
  if (fd == -1) {
    if (errno == 13) {
      cerr << "Error: cannot create the file " << filename << endl;
      return;
    }
    cerr << "Error in creating file: " << errno << endl;
  } 

} // doOutfile


/*****************************************************************************/
/*                            d o L o g i n                                  */
/*****************************************************************************/

void doLogin(char *buffer)
{
  string  username;
  string  role;

  // If no arguments are supplied, use the default user id of the process
  // as well as the default role (kXR_useruser).
  //
  int args = numberOfTokens(buffer) - 1;
  
  if (args == 0) {
    username = getpwuid(getuid())->pw_name;
    role = kXR_useruser;
  } else {
    
    // The first argument corresponds to the user name
    //
    username = getToken(buffer, 1).c_str();

    // Check for further arguments and see if we need to assign a role
    //
    if (args == 2) {
      string dummyRole = getToken(buffer, 2);
      if (dummyRole.find("kXR_useradmin") == 0) {
	role  = kXR_useradmin;
      } else if (dummyRole.find("kXR_useruser") == 0) {
	role  = kXR_useruser;
      } else {
	cerr << "Incorrect role: " 
	     << " can only be kXR_useradmin or kXR_useruser. " << endl;
      } 
    }
    
  }

  if (client->login((kXR_char*) username.c_str(), 
		    (kXR_char*) role.c_str()     ) == 0) {
    login = true;
    client->setMaxWaitTime(120); // Was 10
  } else {
    cerr << "Error: login not done correctly. " << endl;
  }
  
} // doLogin()


/*****************************************************************************/
/*                              d o A u t h                                  */
/*****************************************************************************/

void doAuth(char *buffer) 
{
  if (login == false) {
    cerr << "Not logged in to a remote server.\n";
    return;
  }

  kXR_char  *credtype = (kXR_char*) "krb5";

  // If the user supplys a credential type, use it. If not, use Kerberos 5 
  //
  string credType = getToken(buffer, 1);
  if (credType != "") {
    credtype = (kXR_char*) credType.c_str();
  }

  if (client->auth(credtype)) {
    cerr << "Error: authentication not done correctly. " << endl;
  } 
} // doAuth


/*****************************************************************************/
/*                              d o O p e n                                  */
/*****************************************************************************/

void doOpen(char *buffer)
{
  int    oflags = O_RDONLY;  
  mode_t mode   = 0;                        // only used if O_CREATE is used
  string filename = getToken(buffer, 1);
  
  if (filename == "") {
    cerr << "Filename required for open." << endl;
    return;
  }

  // Check if open flags are used. If not, set a default value.
  // We do not support bitwise ORed values here
  // 
  if (numberOfTokens(buffer) >= 3) {
    string flag = getToken(buffer, 2);

    if      ( flag == "a") {oflags = kXR_async;         }
    else if ( flag == "c") {oflags = kXR_compress;      }
    else if ( flag == "d") {oflags = kXR_delete;        }
    else if ( flag == "f") {oflags = kXR_force;         }
    else if ( flag == "n") {oflags = kXR_new;           }
    else if ( flag == "r") {oflags = kXR_open_read;     }
    else if ( flag == "s") {oflags = kXR_refresh;       }
    else if ( flag == "u") {oflags = kXR_open_updt;     }
    else                   {cerr << "Invalid open flag" << endl;}

  } 

  // Check if open mode has been set 
  //
  if (numberOfTokens(buffer) == 4) {
    string modes = getToken(buffer, 3);

    int modeNumber = atoi(modes.c_str());
    mode = modeNumber;
  } 

  // If login has not yet been called, login to the remote machine
  //
  if (login == false) {
    kXR_char role[1] = {kXR_useruser};
    if (client->login((kXR_char*) getpwuid(getuid())->pw_name, 
		      (kXR_char*) role) == 0) {
      login = true;
      client->setMaxWaitTime(120); // Was 10
    }
    else {
      return; 
    }
  }

  if (client->open((kXR_char*) filename.c_str(), 
		   (kXR_unt16) oflags, 
		   (kXR_unt16) mode              ) != 0) {
    cerr << "Error: file not opened correctly. " << endl;
  }

} // doOpen


/*****************************************************************************/
/*                              d o R e a d                                  */
/*****************************************************************************/


void doRead(char* buffer) 

{
  if (login == false) {
    cerr << "Not logged in to a remote server.\n";
    return;
  }

  string offset = getToken(buffer, 1);
  if (offset == "") {
    cerr << "Offset required for read." << endl;
    return;
  } 
  kXR_int64 loffset = atol(offset.c_str());

  string len = getToken(buffer, 2);
  if (len == "") {
    cerr << "Length (i.e. number of bytes) required for read." << endl;
    return;
  }
  kXR_int32 ilen = atoi(len.c_str());

  char *buf = (char*) malloc(ilen); 
  ssize_t rc = client->read((void*) buf, loffset, ilen);
  if (rc < 0) {
    cerr << "Error: file not read correctly. " << endl;
  } else {

    // If an outfile was used, write the buffer into the file
    //
    if (fd > 0) {
      cout << write(fd, buf, rc) 
	   << " bytes have been written to the outfile." << endl;
    }
  }

  free(buf);
} // doRead

/*****************************************************************************/
/*                              d o S t a t                                  */
/*****************************************************************************/


void doStat(char *buffer) 
{
  if (login == false) {
    cerr << "Not logged in to a remote server.\n";
    return;
  }

  if (numberOfTokens(buffer) < 2) {
    cerr << "Filepath is missing." << endl;
    return;
  }

  struct stat buf;

  // Get the status information and print it to the screen if available
  //
  if (client->stat(&buf, (kXR_char*) getToken(buffer, 1).c_str()) != 0) {
    cerr << "Error: file status not obtained correctly. " << endl;
  } else {
    cout << "id="   << buf.st_dev   << "," << buf.st_ino << ", "
	 << "size=" << buf.st_size  << ", "
	 << "mode=" << buf.st_mode  << ", "
	 << "stat=" << buf.st_mtime << endl;
  }

} // doStat

/*****************************************************************************/
/*                              d o C l o s e                                */
/*****************************************************************************/


void doClose() 
{
  if (login == false) {
    cerr << "Not logged in to a remote server.\n";
    return;
  }

  if (client->close() != 0) {
    cerr << "Error: file not closed correctly. " << endl;
  }
} // doClose


/*****************************************************************************/
/*                              d o T e s t                                  */
/*****************************************************************************/

void doTest() 
{
  // For future extension. Currently, only a hardcoded test
  //
  cout << "Test open /tmp/heinz/testfile\n";
  doOpen((char*) "open /tmp/heinz/testfile");

} // doTest


/*****************************************************************************/
/*                          p a r s e I n p u t                              */
/*****************************************************************************/



/**
 * Parse the input buffer, extract a command to execute and return a command
 * number as well as the arguments for the command.
 *
 * Input:   buffer - char string that hold the command given by the user
 *                   It includes the command as well as arguments and options.
 *
 * Output:  int    - command number 
 */
int parseInput(char *buffer) 
{
  // Check for the command to be executed
  //
  if   (strncmp(buffer, "open",     4) == 0 )   { doOpen(buffer);   
                                                  return kXR_auth;  }
  if   (strncmp(buffer, "read",     4) == 0 )   { doRead(buffer);
                                                  return kXR_read;  }
  if   (strncmp(buffer, "close",    5) == 0)    { doClose();
                                                  return kXR_close; }
  if   (strncmp(buffer, "login",    5) == 0)    { doLogin(buffer);  
                                                  return kXR_login; }
  if   (strncmp(buffer, "auth",     4) == 0 )   { doAuth(buffer);   
                                                  return kXR_auth;  }
  if   (strncmp(buffer, "stat",     4) == 0 )   { doStat(buffer);   
                                                  return kXR_stat;  }
  if ( (strncmp(buffer, "exit",     4) == 0) || 
       (strncmp(buffer, "quit",     4) == 0))   { return xr_exit;   }
  if   (strncmp(buffer, "help",     4) == 0)    { printHelp();
                                                  return xr_help;   }
  if   (strncmp(buffer, "test",     4) == 0)    { doTest();
                                                  return xr_test;   }
  if   (strncmp(buffer, "outfile",  7) == 0)    { doOutfile(buffer);
                                                  return xr_out;    }

  // If none of the above is true, it must be an unknown command
  //
  cerr << "Unknown command. Enter 'help' for help." << endl;
  return xr_error;

} // parseInput


/*****************************************************************************/
/*                                   m a i n                                 */
/*****************************************************************************/


int main(int argc, char *argv[])
{ 

  const char* hostname;
  int         port     = 1094;
  const int   len      = 300;  // length of input characters read from stdin
  char        input[len];

  int command = 0;
  int debug   = 0;

  // Check for arguments. The hostname is required. If no port name is set,
  // we select the default port 1094
  //
  if (argc < 2) {
    cerr << "usage: xrclient [-d] hostname [port] " << endl;
    _exit(-1);
  } else {

    if (strcmp(argv[1],"-d") == 0) { 
      XrTrace.What |= TRACE_All;  
      debug = 1;

      // For security libraries, set the environment variable XrdSecDEBUG
      // to switch on more debugging
      //
      putenv((char *)"XrdSecDEBUG=1");
    }

    hostname = argv[1+debug];

    // If 2nd (or 3rd in case debugging mode is used) argument is applied, 
    // assume that it is the port number
    //
    if (argc == 3+debug) {
      port = atoi(argv[2+debug]);
    }

  }
 
  XrdOucLogger *logger = new XrdOucLogger(0);
  client = new XrdXrClient(hostname, port, logger);

  // Turn off sigpipe inorder to avoid a program crash in case the connection
  // to the server is broken and we still send requests
  //
  signal(SIGPIPE, SIG_IGN);

  do {
    if (login == true && strncmp(hostname, "NULL", 4)) {
      cout << hostname << ":" << port <<"> ";
    } else {
      cout << "hostname:port> ";
      login = false;
    }
    if (!cin.getline(input, len)) {cout <<endl; break;} // At eof
 
    command = parseInput(input);

    // Obtain the current host since we might have been redirected to a 
    // different server.
    //
    hostname = client->getHost();
    port = client->getPort();

  } while (command != xr_exit);

  // If an outfile was used, close the file descriptor
  //
  if (fd > 0) {
    close(fd);
  }

  // Delete the object only if the login was successful. Otherwise the
  // detele results in a seg. fault
  //
  if (login == true) {  
    delete client;
  }
  delete logger;
  _exit(0);
  return 0;
}
