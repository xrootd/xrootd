/******************************************************************************/
/*                                                                            */
/*                        X r d F r c U t i l s . c c                         */
/*                                                                            */
/* (c) 2009 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
  
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "XrdFrc/XrdFrcTrace.hh"
#include "XrdFrc/XrdFrcUtils.hh"
#include "XrdFrc/XrdFrcXAttr.hh"

#include "XrdOuc/XrdOucSxeq.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdOuc/XrdOucXAttr.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPlatform.hh"

using namespace XrdFrc;
  
/******************************************************************************/
/*                                   A s k                                    */
/******************************************************************************/
  
char XrdFrcUtils::Ask(char dflt, const char *Msg1, const char *Msg2,
                                 const char *Msg3)
{
   const char *Hint;
   char Answer[8];
   int n;

   Hint = (dflt == 'y' ? " (y | n | a): " : " (n | y | a): ");

   do {cerr <<"frm_admin: " <<Msg1 <<Msg2 <<Msg3 <<Hint;
       cin.getline(Answer, sizeof(Answer));
       if (!*Answer) return dflt;

       n = strlen(Answer);
       if (!strncmp("yes",  Answer, n)) return 'y';
       if (!strncmp("no",   Answer, n)) return 'n';
       if (!strncmp("abort",Answer, n)) return 'a';
      } while(1);
   return 'a';
}
  
/******************************************************************************/
/*                                c h k U R L                                 */
/******************************************************************************/
  
int XrdFrcUtils::chkURL(const char *Url)
{
   const char *Elem;

// Verify that this is a valid url and return offset to the lfn
//
   if (!(Elem = index(Url, ':'))) return 0;
   if (Elem[1] != '/' || Elem[2] != '/') return 0;
   if (!(Elem = index(Elem+3, '/')) || Elem[1] != '/') return 0;
   Elem++;

// At this point ignore all leading slashes but one
//
   while(Elem[1] == '/') Elem++;
   return Elem - Url;
}

/******************************************************************************/
/*                              m a k e P a t h                               */
/******************************************************************************/
  
char *XrdFrcUtils::makePath(const char *iName, const char *Path, int Mode)
{
   char *bPath;
   int rc;

// Generate an frm-specific admin path
//
   bPath = XrdOucUtils::genPath(Path, iName, "frm");

// Create the admin directory if it does not exists and a mode supplied
//
   if (Mode > 0 && (rc = XrdOucUtils::makePath(bPath, Mode)))
      {Say.Emsg("makePath", rc, "create directory", bPath);
       return 0;
      }

// Return the actual adminpath we are to use (this has been strduped).
//
   return bPath;
}

/******************************************************************************/
/*                              m a k e Q D i r                               */
/******************************************************************************/
  
char *XrdFrcUtils::makeQDir(const char *Path, int Mode)
{
   char qPath[1040], qLink[1032];
   int n, lksz, rc;

// Generate an frm-specific queue path
//
   strcpy(qPath, Path);
   n = strlen(qPath);
   if (qPath[n-1] != '/') qPath[n++] = '/';
   strcpy(qPath+n, "Queues/");

// If the target is a symlink, optimize the path
//
   if ((lksz = readlink(qPath, qLink, sizeof(qLink)-1)) > 0)
      {qLink[lksz] = '\0';
       if (qLink[lksz-1] != '/') {qLink[lksz++] = '/'; qLink[lksz++] = '\0';}
       if (*qLink == '/') strcpy(qPath, qLink);
          else strcpy(qPath+n, qLink);
      }

// Create the queue directory if it does not exists
//
   if (Mode > 0 && (rc = XrdOucUtils::makePath(qPath, Mode)))
      {Say.Emsg("makeQDir", rc, "create directory", qPath);
       return 0;
      }

// Return the actual adminpath we are to use
//
   return strdup(qPath);
}

/******************************************************************************/
/*                                M a p M 2 O                                 */
/******************************************************************************/
  
int XrdFrcUtils::MapM2O(const char *Nop, const char *Pop)
{
   int Options = 0;

// Map processing options to request options
//
   if (index(Pop, 'w')) Options |= XrdFrcRequest::makeRW;
      if (*Nop != '-')
         {if (index(Pop, 's') ||  index(Pop, 'n'))
             Options |= XrdFrcRequest::msgSucc;
          if (index(Pop, 'f') || !index(Pop, 'q'))
             Options |= XrdFrcRequest::msgFail;
         }

// All done
//
   return Options;
}
  
/******************************************************************************/
/*                                M a p R 2 Q                                 */
/******************************************************************************/
  
int XrdFrcUtils::MapR2Q(char Opc, int *Flags)
{

// Simply map the request code to the relevant queue
//
   switch(Opc)
         {case 0  :
          case '+': return XrdFrcRequest::stgQ;
          case '^': if (Flags) *Flags = XrdFrcRequest::Purge;
                    return XrdFrcRequest::migQ;
          case '&': return XrdFrcRequest::migQ;
          case '<': return XrdFrcRequest::getQ;
          case '=': if (Flags) *Flags |= XrdFrcRequest::Purge;
                    return XrdFrcRequest::putQ;
          case '>': return XrdFrcRequest::putQ;
          default:  break;
         }
   return XrdFrcRequest::nilQ;
}
  
/******************************************************************************/
/*                                M a p V 2 I                                 */
/******************************************************************************/
  
int XrdFrcUtils::MapV2I(const char *vName, XrdFrcRequest::Item &ICode)
{
   static struct ITypes {const char *IName; XrdFrcRequest::Item ICode;}
                 ITList[] = {{"lfn",    XrdFrcRequest::getLFN},
                             {"lfncgi", XrdFrcRequest::getLFNCGI},
                             {"mode",   XrdFrcRequest::getMODE},
                             {"obj",    XrdFrcRequest::getOBJ},
                             {"objcgi", XrdFrcRequest::getOBJCGI},
                             {"op",     XrdFrcRequest::getOP},
                             {"prty",   XrdFrcRequest::getPRTY},
                             {"qwt",    XrdFrcRequest::getQWT},
                             {"rid",    XrdFrcRequest::getRID},
                             {"tod",    XrdFrcRequest::getTOD},
                             {"note",   XrdFrcRequest::getNOTE},
                             {"tid",    XrdFrcRequest::getUSER}};
   static const int ITNum = sizeof(ITList)/sizeof(struct ITypes);
   int i;

// Simply map the variable name to the item code
//
   for (i = 0; i < ITNum; i++)
       if (!strcmp(vName, ITList[i].IName))
          {ICode = ITList[i].ICode; return 1;}
   return 0;
}
  
/******************************************************************************/
/*                                U n i q u e                                 */
/******************************************************************************/
  
int XrdFrcUtils::Unique(const char *lkfn, const char *myProg)
{
   static const int Mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
   FLOCK_t lock_args;
   int myFD, rc;

// Open the lock file first in r/w mode
//
   if ((myFD = open(lkfn, O_RDWR|O_CREAT, Mode)) < 0)
      {Say.Emsg("Unique",errno,"open",lkfn); return 0;}

// Establish locking options
//
   bzero(&lock_args, sizeof(lock_args));
   lock_args.l_type =  F_WRLCK;

// Perform action.
//
   do {rc = fcntl(myFD,F_SETLK,&lock_args);}
       while(rc < 0 && errno == EINTR);
   if (rc < 0) 
      {Say.Emsg("Unique", errno, "obtain the run lock on", lkfn);
       Say.Emsg("Unique", "Another", myProg, "may already be running!");
       close(myFD);
       return 0;
      }

// All done
//
   return 1;
}
  
/******************************************************************************/
/*                               u p d t C p y                                */
/******************************************************************************/
  
int XrdFrcUtils::updtCpy(const char *Pfn, int Adj)
{
   XrdOucXAttr<XrdFrcXAttrCpy> cpyInfo;
   struct stat Stat;

// Make sure the base file exists
//
   if (stat(Pfn, &Stat)) {Say.Emsg("updCpy", errno,"stat pfn ",Pfn); return 0;}

// Set correct copy time based on need
//
   cpyInfo.Attr.cpyTime = static_cast<long long>(Stat.st_mtime + Adj);
   return cpyInfo.Set(Pfn) == 0;
}

/******************************************************************************/
/*                                 U t i m e                                  */
/******************************************************************************/
  
int XrdFrcUtils::Utime(const char *Path, time_t tVal)
{
   struct utimbuf tbuf = {tVal, tVal};
   int rc;

// Set the time
//
   do {rc = utime(Path, &tbuf);} while(rc && errno == EINTR);
   if (rc) Say.Emsg("Utils", errno, "set utime for pfn", Path);

// All done
//
   return rc == 0;
}
