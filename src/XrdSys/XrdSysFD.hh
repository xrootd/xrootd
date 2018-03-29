#ifndef __XRDSYS_FD_H__
#define __XRDSYS_FD_H__
/******************************************************************************/
/*                                                                            */
/*                           X r d S y s F D . h h                            */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

//-----------------------------------------------------------------------------
//! XrdSysFD defines a set of alternate functions that make sure that the
//! CLOEXEC attribute is associated with any new file descriptors returned by
//! by commonly used functions. This is platform sensitive as some platforms
//! allow atomic setting of the attribute while others do not. These functions
//! are used to provide platform portability.
//-----------------------------------------------------------------------------

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace
{
#if defined(__linux__) && defined(SOCK_CLOEXEC) && defined(O_CLOEXEC)
inline int  XrdSysFD_Accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
                 {return accept4(sockfd, addr, addrlen, SOCK_CLOEXEC);}

inline int  XrdSysFD_Dup(int oldfd)
                 {return fcntl(oldfd, F_DUPFD_CLOEXEC, 0);}

inline int  XrdSysFD_Dup1(int oldfd, int minfd)
                 {return fcntl(oldfd, F_DUPFD_CLOEXEC, minfd);}

inline int  XrdSysFD_Dup2(int oldfd, int newfd)
                 {return dup3(oldfd, newfd, O_CLOEXEC);}

inline int  XrdSysFD_Open(const char *path, int flags)
                 {return open(path, flags|O_CLOEXEC);}

inline int  XrdSysFD_Open(const char *path, int flags, mode_t mode)
                 {return open(path, flags|O_CLOEXEC, mode);}

inline int  XrdSysFD_Pipe(int pipefd[2])
                 {return pipe2(pipefd, O_CLOEXEC);}

inline int  XrdSysFD_Socket(int domain, int type, int protocol)
                 {return socket(domain, type|SOCK_CLOEXEC, protocol);}

inline int  XrdSysFD_Socketpair(int domain, int type, int protocol, int sfd[2])
                 {return socketpair(domain, type|SOCK_CLOEXEC, protocol, sfd);}
#else
inline int  XrdSysFD_Accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
                 {int newfd = accept(sockfd, addr, addrlen);
                  if (newfd >= 0) fcntl(newfd, F_SETFD, FD_CLOEXEC);
                  return newfd;
                 }

inline int  XrdSysFD_Dup(int oldfd)
                 {int newfd = dup(oldfd);
                  if (newfd >= 0) fcntl(newfd, F_SETFD, FD_CLOEXEC);
                  return newfd;
                 }

inline int  XrdSysFD_Dup1(int oldfd, int minfd)
                 {int newfd = fcntl(oldfd, F_DUPFD, minfd);
                  if (newfd >= 0) fcntl(newfd, F_SETFD, FD_CLOEXEC);
                  return newfd;
                 }

inline int  XrdSysFD_Dup2(int oldfd, int newfd)
                 {int rc = dup2(oldfd, newfd);
                  if (!rc) fcntl(newfd, F_SETFD, FD_CLOEXEC);
                  return rc;
                 }

inline int  XrdSysFD_Open(const char *path, int flags)
                 {int newfd = open(path, flags);
                  if (newfd >= 0) fcntl(newfd, F_SETFD, FD_CLOEXEC);
                  return newfd;
                 }

inline int  XrdSysFD_Open(const char *path, int flags, mode_t mode)
                 {int newfd = open(path, flags, mode);
                  if (newfd >= 0) fcntl(newfd, F_SETFD, FD_CLOEXEC);
                  return newfd;
                 }

inline int  XrdSysFD_Pipe(int pipefd[2])
                 {int rc = pipe(pipefd);
                  if (!rc) {fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
                            fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);
                           }
                  return rc;
                 }

inline int  XrdSysFD_Socket(int domain, int type, int protocol)
                 {int newfd = socket(domain, type, protocol);
                  if (newfd >= 0) fcntl(newfd, F_SETFD, FD_CLOEXEC);
                  return newfd;
                 }

inline int  XrdSysFD_Socketpair(int domain, int type, int protocol, int sfd[2])
                 {int rc = socketpair(domain, type, protocol, sfd);
                  if (!rc) {fcntl(sfd[0], F_SETFD, FD_CLOEXEC);
                            fcntl(sfd[1], F_SETFD, FD_CLOEXEC);
                           }
                  return rc;
                 }
#endif

inline bool XrdSysFD_Yield(int fd)
                  {int fdFlags = fcntl(fd, F_GETFD);
                   if (fdFlags < 0) return false;
                   return   0 == fcntl(fd, F_SETFD, fdFlags & ~FD_CLOEXEC);
                  }
}
#endif
