#ifndef __XRDNETCONNECT__
#define __XRDNETCONNECT__
/******************************************************************************/
/*                                                                            */
/*                      X r d N e t C o n n e c t . h h                       */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <sys/types.h>
#ifndef WIN32
#include <sys/socket.h>
#else
#include <Winsock2.h>
#endif
  
class XrdNetConnect
{
public:

// Connect() performs the same function as sycall connect() however, it
//           can optionally apply a thread-safe timeout of tsec seconds.
//           It returns 0 upon success or errno upon failure.
//
static int  Connect(             int       fd,      // Open socket descriptor
                    const struct sockaddr *name,    // Address to connect to
                                 int       namelen, // Size of address
                                 int       tsec=-1);// Optional timeout

private:
        // Only this class is allowed to create and delete this object
        //
        XrdNetConnect() {}
       ~XrdNetConnect() {}
};
#endif
