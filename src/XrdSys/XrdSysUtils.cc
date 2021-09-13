/******************************************************************************/
/*                                                                            */
/*                        X r d S y s U t i l s . c c                         */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <signal.h>
#include <cstring>
#include <unistd.h>
#include <sys/param.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#ifdef WIN32
#include <direct.h>
#include "XrdSys/XrdWin32.hh"
#else
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#endif
#include "XrdSysUtils.hh"
  
/******************************************************************************/
/*                              E x e c N a m e                               */
/******************************************************************************/

const char *XrdSysUtils::ExecName()
{
   static const char *myEname = 0;

// If we have been here before, simply return what we discovered. This is
// relatively thread-safe as we might loose some memory but it will work.
// Anyway, this method is unlikely to be called by multiple threads. Also,
// according to gthe Condor team, this code will not be able to return the
// program name if the program is under the control of valgrind!
//
   if (myEname) return myEname;

// Get the exec name based on platform
//
#if defined(__linux__) || defined(__GNU__) || (defined(__FreeBSD_kernel__) && defined(__GLIBC__))
  {char epBuff[2048];
   int  epLen;
   if ((epLen = readlink("/proc/self/exe", epBuff, sizeof(epBuff)-1)) > 0)
      {epBuff[epLen] = 0;
       myEname = strdup(epBuff);
       return myEname;
      }
  }
#elif defined(__APPLE__)
  {char epBuff[2048];
   uint32_t epLen = sizeof(epBuff)-1;
   if (!_NSGetExecutablePath(epBuff, &epLen))
      {epBuff[epLen] = 0;
       myEname = strdup(epBuff);
       return myEname;
      }
  }
#elif defined(__solaris__)
  {const char *epBuff = getexecname();
   if (epBuff)
      {if (*epBuff == '/') myEname = strdup(epBuff);
          else {char *ename, *cwd = getcwd(0, MAXPATHLEN);
                ename = (char *)malloc(strlen(cwd)+1+strlen(epBuff)+1);
                sprintf(ename, "%s/%s", cwd, epBuff);
                myEname = ename;
                free(cwd);
               }
       return myEname;
      }
  }
#else
#endif

// If got here then we don't have a valid program name. Return a null string.
//
   return "";
}

/******************************************************************************/
/*                              F m t U n a m e                               */
/******************************************************************************/
  
int XrdSysUtils::FmtUname(char *buff, int blen)
{
#if defined(WINDOWS)
    return snprintf(buff, blen, "%s", "windows");
#else
   struct utsname uInfo;

// Obtain the uname inofmormation
//
   if (uname(&uInfo) < 0) return snprintf(buff, blen, "%s", "unknown OS");

// Format appropriate for certain platforms
// Linux and MacOs do not add usefull version information
//
#if   defined(__linux__)
   return snprintf(buff, blen, "%s %s",       uInfo.sysname, uInfo.release);
#elif defined(__APPLE__) || defined(__FreeBSD__) || (defined(__FreeBSD__) || defined(__GLIBC__))
   return snprintf(buff, blen, "%s %s %s",    uInfo.sysname, uInfo.release,
                               uInfo.machine);
#else
   return snprintf(buff, blen, "%s %s %s %s", uInfo.sysname, uInfo.release,
                              uInfo.version, uInfo.machine);
#endif
#endif
}
  
/******************************************************************************/
/*                             G e t S i g N u m                              */
/******************************************************************************/

namespace
{
   static struct SigTab {const char *sname; int snum;} sigtab[] =
                        {{"hup",     SIGHUP},     {"HUP",     SIGHUP},
#ifdef SIGRTMIN
                         {"rtmin",   SIGRTMIN},   {"RTMIN",   SIGRTMIN},
                         {"rtmin+1", SIGRTMIN+1}, {"RTMIN+1", SIGRTMIN+1},
                         {"rtmin+2", SIGRTMIN+2}, {"RTMIN+2", SIGRTMIN+2},
#endif
                         {"ttou",    SIGTTOU},    {"TTOU",    SIGTTOU},
//                       {"usr1",    SIGUSR1},    {"USR1",    SIGUSR1},
//                       {"usr2",    SIGUSR2},    {"USR2",    SIGUSR2},
                         {"winch",   SIGWINCH},   {"WINCH",   SIGWINCH},
                         {"xfsz",    SIGXFSZ},    {"XFSZ",    SIGXFSZ}
                        };
   static int snum = sizeof(sigtab)/sizeof(struct SigTab);
};
  
int XrdSysUtils::GetSigNum(const char *sname)
{
   int i;

// Trim off the "sig" in sname
//
   if (!strncmp(sname, "sig", 3) || !strncmp(sname, "SIG", 3)) sname += 3;

// Convert to signal number
//
   for (i = 0; i < snum; i++)
       {if (!strcmp(sname, sigtab[i].sname)) return sigtab[i].snum;}
   return 0;
}

/******************************************************************************/
/*                              S i g B l o c k                               */
/******************************************************************************/
  
bool XrdSysUtils::SigBlock()
{
   sigset_t  myset;

// Ignore pipe signals and prepare to blocks others
//
   signal(SIGPIPE, SIG_IGN);  // Solaris optimization

// Add the standard signals we normally always block
//
   sigemptyset(&myset);
   sigaddset(&myset, SIGPIPE);
   sigaddset(&myset, SIGCHLD);

// Block a couple of real-time signals if they are supported (async I/O)
//
#ifdef SIGRTMAX
   sigaddset(&myset, SIGRTMAX);
   sigaddset(&myset, SIGRTMAX-1);
#endif

// Now turn off these signals
//
   return pthread_sigmask(SIG_BLOCK, &myset, NULL) == 0;
}

/******************************************************************************/
  
bool XrdSysUtils::SigBlock(int numsig)
{
   sigset_t  myset;

// Ignore pipe signals and prepare to blocks others
//
   if (sigemptyset(&myset) == -1 || sigaddset(&myset, numsig) == -1)
      return false;

// Now turn off these signals
//
   return pthread_sigmask(SIG_BLOCK, &myset, NULL) == 0;
}
