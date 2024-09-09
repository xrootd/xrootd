/******************************************************************************/
/*                                                                            */
/*                   X r d S e c t e s t C l i e n t . c c                    */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

/* Syntax: testClient [-b] [-d] [-h host] [-l] [sectoken]

   See the help() function for an explanation of the above.
*/
  
#include <unistd.h>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <sys/param.h>

#include "XrdNet/XrdNetAddr.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSec/XrdSecInterface.hh"
  
/******************************************************************************/
/*                    G l o b a l   D e f i n i t i o n s                     */
/******************************************************************************/

extern "C"
{
extern XrdSecProtocol *XrdSecGetProtocol(const char             *hostname,
                                               XrdNetAddrInfo   &endPoint,
                                               XrdSecParameters &parms,
                                               XrdOucErrInfo    *einfo=0);
}
  
/******************************************************************************/
/*                    L O C A L   D E F I N I T I O N S                       */
/******************************************************************************/

#define H(x)         fprintf(stderr,x); fprintf(stderr, "\n");
#define I(x)         fprintf(stderr, "\n"); H(x)

/******************************************************************************/
/*                                  m a i n                                   */
/******************************************************************************/
  
int main(int argc, char **argv)
{
char *tohex(char *inbuff, int inlen, char *outbuff);

char *protocols=0, *hostspec=0;

XrdNetAddr theAddr;

int putbin = 0, putlen = 0;
char kbuff[8192];
char c;

XrdSecCredentials *cred;
XrdSecParameters   SecToken;
XrdSecProtocol    *pp;
int DebugON = 0;
void help(int);


   /*Get all of the options.
    */
    while ((c=getopt(argc,argv,"bdlh:")) != (char)EOF)
      { switch(c)
        {
        case 'b': putbin = 1;                         break;
        case 'd': DebugON = 1;                        break;
        case 'h': hostspec = optarg;                  break;
        case 'l': putlen = 1;                         break;
        default:  help(1);
        }
      }

// Check if the security token is the last argument
//
   if (optind < argc) protocols = argv[optind++];

/*Make sure no more parameters exist.
*/
   if (optind < argc) 
      {std::cerr <<"testClient: Extraneous parameter, '" <<argv[optind] <<"'." <<std::endl;
       help(2);
      }

// Determine protocol string
//
   if (!protocols && !(protocols = getenv("XrdSecSECTOKEN")))
      {std::cerr <<"testClient: Security protocol string not specified." <<std::endl;
       help(2);
      }
   SecToken.size = strlen(protocols);
   SecToken.buffer = protocols;

// if hostname given, get the hostname address
//
   if (hostspec && (eText = theAddr(hostspec,0)))
      {std::cerr <<"testServer: Unable to resolve '" <<hostspec <<"'; " <<eText <<std::endl;
       exit(1);
      } else theAddr.Set("localhost",0);

// Do debug processing
//
   if (DebugON)
      {putenv((char *)"XrdSecDEBUG=1");
       std::cerr <<"testClient: security token='" <<protocols <<"'" <<std::endl;
      }

// Get the protocol
//
   pp = XrdSecGetProtocol(hostspec, theAddr, SecToken, 0);
   if (!pp) {std::cerr << "Unable to get protocol." <<std::endl; exit(1);}

// Get credentials using this context
//
   pp->addrInfo = &theAddr;
   cred = pp->getCredentials();
   if (!cred)
      {std::cerr << "Unable to get credentials," <<std::endl;
       exit(1);
      }
   if (DebugON)
      std::cerr << "testClient: credentials size=" <<cred->size <<std::endl;

// Write out the credentials
//
   if (putbin)
      {if (putlen)
          {if (fwrite(&cred->size, sizeof(cred->size), 1, stdout) != sizeof(cred->size))
	      {std::cerr << "Unable to write credentials length" <<std::endl; 
	       exit(1);}}
       if (fwrite((char *) cred->buffer, cred->size, 1, stdout) != (size_t) cred->size)
          {std::cerr << "Unable to write credentials" <<std::endl; 
           exit(1);}
      } else {
       if (putlen) printf("%s",
                tohex((char *)&cred->size, sizeof(cred->size), kbuff));
       printf("%s\n", tohex((char *) cred->buffer, cred->size, kbuff));
      }

// All done.
//
   pp->Delete();
}

char *tohex(char *inbuff, int inlen, char *outbuff) {
     static char hv[] = "0123456789abcdef";
     int i, j = 0;
     for (i = 0; i < inlen; i++) {
         outbuff[j++] = hv[(inbuff[i] >> 4) & 0x0f];
         outbuff[j++] = hv[ inbuff[i]       & 0x0f];
         }
     outbuff[j] = '\0';
     return outbuff;
     }

/*help prints hout the obvious.
*/
void help(int rc) {
/* Use H macro to avoid Sun string catenation bug. */
I("Syntax:   testClient [ options ] [sectoken]")
I("Options:  -b -d -l -h host")
I("Function: Request for credentials relative to an operation.")

if (rc > 1) exit(rc);
I("options:  (defaults: -o 01")
I("-b        output the ticket in binary format (i.e., not hexchar).")
I("-d        turns on debugging.")
I("-l        prefixes the ticket with its 4-byte length.")
I("-h host   the requesting hostname (default is localhost).")
I("Notes:    1. Variable XrdSecSECTOKEN must contain the security token,")
H("             sectoken, if it is not specified on the command line.")
exit(rc);
}
