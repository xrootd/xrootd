/******************************************************************************/
/*                                                                            */
/*                       X r d X r o o t d X e q . c c                        */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

const char *XrdXrootdXeqCVSID = "$Id$";

#include <stdio.h>

#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucPlatform.hh"
#include "XrdOuc/XrdOucProg.hh"
#include "XrdOuc/XrdOucReqID.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "Xrd/XrdBuffer.hh"
#include "Xrd/XrdLink.hh"
#include "XrdXrootd/XrdXrootdAio.hh"
#include "XrdXrootd/XrdXrootdFile.hh"
#include "XrdXrootd/XrdXrootdFileLock.hh"
#include "XrdXrootd/XrdXrootdMonitor.hh"
#include "XrdXrootd/XrdXrootdPrepare.hh"
#include "XrdXrootd/XrdXrootdProtocol.hh"
#include "XrdXrootd/XrdXrootdStats.hh"
#include "XrdXrootd/XrdXrootdTrace.hh"
#include "XrdXrootd/XrdXrootdXPath.hh"
  
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

extern XrdOucTrace *XrdXrootdTrace;

/******************************************************************************/
/*                      L o c a l   S t r u c t u r e s                       */
/******************************************************************************/
  
struct XrdXrootdFHandle
       {kXR_int32 handle;

        XrdXrootdFHandle() {}
        XrdXrootdFHandle(kXR_char *ch)
            {memcpy((void *)&handle, (const void *)ch, sizeof(handle));}
       ~XrdXrootdFHandle() {}
       };

/******************************************************************************/
/*                         L o c a l   D e f i n e s                          */
/******************************************************************************/

#define CRED (const XrdSecClientName *)&Client

#define TRACELINK Link

#define UPSTATS(x) SI->statsMutex.Lock(); SI->x++; SI->statsMutex.UnLock()
 
/******************************************************************************/
/*                              d o _ A d m i n                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Admin()
{
   return Response.Send(kXR_Unsupported, "admin request is not supported");
}
  
/******************************************************************************/
/*                               d o _ A u t h                                */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Auth()
{
    XrdSecCredentials cred;
    XrdSecParameters *parm = 0;
    XrdOucErrInfo     eMsg;
    char *eText;
    int rc;

// Ignore authenticate requests if security turned off
//
    if (!CIA) return Response.Send();

// Attempt to authenticate this person
//
   cred.size   = Request.header.dlen;
   cred.buffer = argp->buff;
   if (!(rc = CIA->Authenticate(&cred, &parm, Client, &eMsg)))
      {rc = Response.Send(); Status &= ~XRD_NEED_AUTH;
       eDest.Emsg("Xeq", "User authenticated as", Client.name);
       return rc;
      }

// If we need to continue authentication, tell the client as much
//
   if (rc > 0)
      {TRACEP(LOGIN, "more auth requested; sz=" <<(parm ? parm->size : 0));
       if (parm) {rc = Response.Send(kXR_authmore, parm->buffer, parm->size);
                  delete parm;
                  return rc;
                 }
       eDest.Emsg("Xeq", "Security requested additional auth w/o parms!");
       return Response.Send(kXR_ServerError,"invalid authentication exchange");
      }

// We got an error, bail out
//
   eText = (char *)eMsg.getErrText(rc);
   eDest.Emsg("Xeq", "User authentication failed;", eText);
   return Response.Send(kXR_NotAuthorized, eText);
}

/******************************************************************************/
/*                              d o _ c h m o d                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Chmod()
{
   int mode, rc;
   XrdOucErrInfo myError;

// Unmarshall the data
//
   mode = mapMode((int)ntohs(Request.chmod.mode));
   if (rpCheck(argp->buff)) return rpEmsg("Modifying", argp->buff);
   if (!Squash(argp->buff)) return vpEmsg("Modifying", argp->buff);

// Preform the actual function
//
   rc = osFS->chmod((const char *)argp->buff, (XrdSfsMode)mode, myError, CRED);
   TRACEP(FS, "rc=" <<rc <<" chmod " <<std::oct <<mode <<std::dec <<' ' <<argp->buff);
   if (SFS_OK == rc) return Response.Send();

// An error occured
//
   return fsError(rc, myError);
}

/******************************************************************************/
/*                              d o _ C K s u m                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_CKsum()
{
   struct iovec ckResp[4];
   char *lp;
   int dlen;
   XrdOucStream cks;

// Check if we support this operation
//
   if (!ProgCKS)
      return Response.Send(kXR_Unsupported, "query chksum is not supported");

// Prescreen the path
//
   if (rpCheck(argp->buff)) return rpEmsg("Check summing", argp->buff);
   if (!Squash(argp->buff)) return vpEmsg("Check summing", argp->buff);

// Preform the actual function
//
   if (ProgCKS->Run(&cks, argp->buff) || !(lp = cks.GetLine()))
      return Response.Send(kXR_ServerError, (char *)"Checksum failed");

// Send back the checksum
//
   ckResp[1].iov_base = ProgCKT;     dlen  = ckResp[1].iov_len = strlen(ProgCKT);
   ckResp[2].iov_base = (char *)" "; dlen += ckResp[2].iov_len = 1;
   ckResp[3].iov_base = lp;          dlen += ckResp[3].iov_len = strlen(lp);
   return Response.Send(ckResp, 4, dlen);
}

/******************************************************************************/
/*                              d o _ C l o s e                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Close()
{
   XrdXrootdFile *fp;
   XrdXrootdFHandle fh(Request.close.fhandle);

// Keep statistics
//
   UPSTATS(miscCnt);

// Find the file object
//
   if (!FTab || !(fp = FTab->Get(fh.handle)))
      return Response.Send(kXR_FileNotOpen, 
                          "close does not refer to an open file");

// If we are in async mode, serialize the link to make sure any in-flight
// operations on this handle have completed
//
   if (fp->AsyncMode) Link->Serialize();

// If we are monitoring, insert a close entry
//
   if (monFILE && Monitor) Monitor->Close(fp->FileID,fp->readCnt,fp->writeCnt);

// Delete the file from the file table; this will unlock/close the file
//
   FTab->Del(fh.handle);

// Respond that all went well
//
   TRACEP(FS, "close fh=" <<fh.handle);
   return Response.Send();
}

/******************************************************************************/
/*                            d o _ D i r l i s t                             */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Dirlist()
{
   int bleft, rc = 0, dlen, cnt = 0;
   char *buff, *dname, ebuff[4096];
   XrdSfsDirectory *dp;

// Prescreen the path
//
   if (rpCheck(argp->buff)) return rpEmsg("Listing", argp->buff);
   if (!Squash(argp->buff)) return vpEmsg("Listing", argp->buff);

// Get a directory object
//
   if (!(dp = osFS->newDir()))
      {snprintf(ebuff,sizeof(ebuff)-1,"Insufficient memory to open %s",argp->buff);
       eDest.Emsg("Xeq", ebuff);
       return Response.Send(kXR_NoMemory, ebuff);
      }

// First open the directory
//
   if ((rc = dp->open((const char *)argp->buff, CRED)))
      {rc = fsError(rc, dp->error); delete dp; return rc;}

// Start retreiving each entry and place in a local buffer with a trailing new
// line character (the last entry will have a null byte). If we cannot fit a
// full entry in the buffer, send what we have with an OKSOFAR and continue.
// This code depends on the fact that a directory entry will never be longer
// than sizeof( ebuff)-1; otherwise, an infinite loop will result. No errors
// are allowed to be reflected at this point.
//
  dname = 0;
  do {buff = ebuff; bleft = sizeof(ebuff);
      while(dname || (dname = (char *)dp->nextEntry()))
           {dlen = strlen(dname);
            if (dlen > 2 || dname[0] != '.' || (dlen == 2 && dname[1] != '.'))
               {if ((bleft -= (dlen+1)) < 0) break;
                strcpy(buff, dname); buff += dlen; *buff = '\n'; buff++; cnt++;
               }
            dname = 0;
           }
       if (dname) rc = Response.Send(kXR_oksofar, ebuff, buff-ebuff);
     } while(!rc && dname);

// Send the ending packet if we actually have one to send
//
   if (!rc) 
      {if (ebuff == buff) rc = Response.Send();
          else {*(buff-1) = '\0';
                rc = Response.Send((void *)ebuff, buff-ebuff);
               }
      }

// Close the directory
//
   dp->close();
   delete dp;
   if (!rc) {TRACEP(FS, "dirlist entries=" <<cnt <<" path=" <<argp->buff);}
   return rc;
}

/******************************************************************************/
/*                            d o   G e t f i l e                             */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Getfile()
{
   int gopts, buffsz;

// Keep Statistics
//
   UPSTATS(getfCnt);

// Unmarshall the data
//
   gopts  = int(ntohl(Request.getfile.options));
   buffsz = int(ntohl(Request.getfile.buffsz));

   return Response.Send(kXR_Unsupported, "getfile request is not supported");
}

/******************************************************************************/
/*                              d o _ L o g i n                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Login()
{
   int i, pid, rc;
   char uname[9];

// Keep Statistics
//
   UPSTATS(miscCnt);

// Unmarshall the data
//
   pid = (int)ntohl(Request.login.pid);
   for (i = 0; i < (int)sizeof(uname); i++)
      {if (Request.login.username[i] == '\0' ||
           Request.login.username[i] == ' ') break;
       uname[i] = Request.login.username[i];
      }
   uname[i] = '\0';

// Make sure the user is not already logged in
//
   if (Status) return Response.Send(kXR_InvalidRequest,
                                    "duplicate login; already logged in");

// Establish the ID for this link
//
   Link->setID((const char *)uname, pid);
   Client.tident = Link->ID;
   CapVer = Request.login.capver[0];

// Check if this is an admin login
//
   if (*(Request.login.role) & (kXR_char)kXR_useradmin)
      Status = XRD_ADMINUSER;

// Get the security token for this link
//
   if (CIA)
      {const char *pp=CIA->getParms(i, (const char *)&Client.host);
       if (pp && i ) {rc = Response.Send((void *)pp, i);
                      Status |= (XRD_LOGGEDIN | XRD_NEED_AUTH);
                     }
          else {rc = Response.Send(); Status = XRD_LOGGEDIN;}
      }
      else     {rc = Response.Send(); Status = XRD_LOGGEDIN;}

// Allocate a monitoring object, if needed for this connection
//
   if ((Monitor = XrdXrootdMonitor::Alloc()))
      {monFILE = XrdXrootdMonitor::monFILE;
       monIO   = XrdXrootdMonitor::monIO;
       if (XrdXrootdMonitor::monUSER)
          monUID = XrdXrootdMonitor::Map(XROOTD_MON_MAPUSER,
                                        (const char *)Link->ID, 0);
      }

// Document this login
//
   eDest.Log(OUC_LOG_01, "Xeq", Link->ID, (char *)"login",
                    (Status & XRD_ADMINUSER ? (char *)"as admin" : 0));
   return rc;
}

/******************************************************************************/
/*                              d o _ M k d i r                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Mkdir()
{
   int mode, rc;
   XrdOucErrInfo myError;

// Unmarshall the data
//
   mode = mapMode((int)ntohs(Request.mkdir.mode)) | S_IRWXU;
   if (Request.mkdir.options[0] & static_cast<unsigned char>(kXR_mkdirpath))
      mode |= SFS_O_MKPTH;
   if (rpCheck(argp->buff)) return rpEmsg("Creating", argp->buff);
   if (!Squash(argp->buff)) return vpEmsg("Creating", argp->buff);

// Preform the actual function
//
   rc = osFS->mkdir((const char *)argp->buff, (XrdSfsMode)mode, myError, CRED);
   TRACEP(FS, "rc=" <<rc <<" mkdir " <<std::oct <<mode <<std::dec <<' ' <<argp->buff);
   if (SFS_OK == rc) return Response.Send();

// An error occured
//
   return fsError(rc, myError);
}

/******************************************************************************/
/*                                 d o _ M v                                  */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Mv()
{
   int rc;
   char *oldp, *newp;
   XrdOucErrInfo myError;

// Find the space separator between the old and new paths
//
   oldp = newp = argp->buff;
   while(*newp && *newp != ' ') newp++;
   if (*newp) {*newp = '\0'; newp++;
               while(*newp && *newp == ' ') newp++;
              }

// Get rid of relative paths and multiple slashes
//
   if (rpCheck(oldp)) return rpEmsg("Renaming", oldp);
   if (rpCheck(newp)) return rpEmsg("Renaming to", newp);
   if (!Squash(oldp)) return vpEmsg("Renaming", oldp);
   if (!Squash(newp)) return vpEmsg("Renaming to", newp);

// Check if new path actually specified here
//
   if (*newp == '\0')
      Response.Send(kXR_ArgMissing, "new path specfied for mv");

// Preform the actual function
//
   rc = osFS->rename((const char *)oldp, (const char *)newp, myError, CRED);
   TRACEP(FS, "rc=" <<rc <<" mv " <<oldp <<' ' <<newp);
   if (SFS_OK == rc) return Response.Send();

// An error occured
//
   return fsError(rc, myError);
}

/******************************************************************************/
/*                               d o _ O p e n                                */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Open()
{
   int fhandle;
   int rc, mode, opts, openopts, mkpath = 0, doforce = 0, compchk = 0;
   char usage, ebuff[2048];
   char *fn = argp->buff, opt[16], *opaque, *op=opt, isAsync = '\0';
   XrdSfsFile *fp;
   XrdXrootdFile *xp;
   struct ServerResponseBody_Open myResp;
   int resplen = sizeof(myResp.fhandle);

// Keep Statistics
//
   UPSTATS(openCnt);

// Unmarshall the data
//
   mode = (int)ntohs(Request.open.mode);
   opts = (int)ntohs(Request.open.options);

// Map the mode and options
//
   mode = mapMode(mode) | S_IRUSR | S_IWUSR; usage = 'w';
        if (opts & kXR_open_read)  
           {openopts  = SFS_O_RDONLY;  *op++ = 'r'; usage = 'r';}
   else if (opts & kXR_new)         
           {openopts  = SFS_O_CREAT;   *op++ = 'n';
            if (opts & kXR_mkdir)     {*op++ = 'm'; mkpath = 1;
                                       mode |= SFS_O_MKPTH;
                                      }
           }
   else if (opts & kXR_delete)     
           {openopts  = SFS_O_TRUNC;   *op++ = 'd';}
   else if (opts & kXR_open_updt)   
           {openopts  = SFS_O_RDWR;    *op++ = 'u';}
   else    {openopts  = SFS_O_RDONLY;  *op++ = 'r'; usage = 'r';}
   if (opts & kXR_compress)        
           {openopts |= SFS_O_RAWIO;   *op++ = 'c'; compchk = 1;}
   if (opts & kXR_force)              {*op++ = 'f'; doforce = 1;}
   if ((opts & kXR_async || as_force) && !as_noaio)
                                      {*op++ = 'a'; isAsync = '1';}
   if (opts & kXR_refresh)            {*op++ = 's'; openopts |= SFS_O_RESET;
                                       UPSTATS(Refresh);
                                      }
   *op = '\0';

// Check if opaque data has been provided
//
   if ((opaque = index((const char *)fn, (int)'?')))
      {*opaque++ = '\0'; if (!*opaque) opaque = 0;}
   if (rpCheck(fn)) return rpEmsg("Opening", fn);
   if (!Squash(fn)) return vpEmsg("Opening", fn);

// Get a file object
//
   if (!(fp = osFS->newFile()))
      {snprintf(ebuff, sizeof(ebuff)-1,"Insufficient memory to open %s",fn);
       eDest.Emsg("Xeq", ebuff);
       return Response.Send(kXR_NoMemory, ebuff);
      }

// Open the file
//
   if ((rc = fp->open((const char *)fn, (XrdSfsFileOpenMode)openopts,
                     (mode_t)mode, CRED, (const char *)opaque)))
      {rc = fsError(rc, fp->error); delete fp; return rc;}

// Obtain a hyper file object
//
   if (!(xp = new XrdXrootdFile(Link->ID, fp, usage, isAsync)))
      {delete fp;
       snprintf(ebuff, sizeof(ebuff)-1, "Insufficient memory to open %s", fn);
       eDest.Emsg("Xeq", ebuff);
       return Response.Send(kXR_NoMemory, ebuff);
      }

// Serialize the link
//
   Link->Serialize();
   *ebuff = '\0';

// Lock this file
//
   if ((rc = Locker->Lock(xp, doforce)))
      {char *who;
       if (rc > 0) who = (rc > 1 ? (char *)"readers" : (char *)"reader");
          else {   rc = -rc;
                   who = (rc > 1 ? (char *)"writers" : (char *)"writer");
               }
       snprintf(ebuff, sizeof(ebuff)-1,
                "%s file %s is already opened by %d %s; open denied.",
                ('r' == usage ? "Input" : "Output"), fn, rc, who);
       delete fp; xp->XrdSfsp = 0; delete xp;
       eDest.Emsg("Xeq", ebuff);
       return Response.Send(kXR_FileLocked, ebuff);
      }

// Create a file table for this link if it does not have one
//
   if (!FTab) FTab = new XrdXrootdFileTable();

// Insert this file into the link's file table
//
   if (!FTab || (fhandle = FTab->Add(xp)) < 0)
      {delete xp;
       snprintf(ebuff, sizeof(ebuff)-1, "Insufficient memory to open %s", fn);
       eDest.Emsg("Xeq", ebuff);
       return Response.Send(kXR_NoMemory, ebuff);
      }

// Document forced opens
//
   if (doforce)
      {int rdrs, wrtrs;
       Locker->numLocks(xp, rdrs, wrtrs);
       if (('r' == usage && wrtrs) || ('w' == usage && rdrs) || wrtrs > 1)
          {snprintf(ebuff, sizeof(ebuff)-1,
             "%s file %s forced opened with %d reader(s) and %d writer(s).",
             ('r' == usage ? "Input" : "Output"), fn, rdrs, wrtrs);
           eDest.Emsg("Xeq", ebuff);
          }
      }

// Determine if file is compressed
//
   if (!compchk) resplen = sizeof(myResp.fhandle);
      else {int cpsize;
            fp->getCXinfo((char *)myResp.cptype, cpsize);
            if (cpsize) {myResp.cpsize = static_cast<kXR_int32>(htonl(cpsize));
                         resplen = sizeof(myResp);
                        }
           }

// If we are monitoring, send off a path to dictionary mapping
//
   if (monFILE && Monitor) 
      {xp->FileID = Monitor->Map(XROOTD_MON_MAPPATH,
                                (const char *)Link->ID,(const char *)fn);
       Monitor->Open(xp->FileID);
      }

// Marshall the file handle (this works on all but alpha platforms)
//
   memcpy((void *)myResp.fhandle,(const void *)&fhandle,sizeof(myResp.fhandle));

// Respond
//
   TRACEP(FS, "open " <<opt <<' ' <<fn <<" fh=" <<fhandle);
   return Response.Send((void *)&myResp, resplen);
}

/******************************************************************************/
/*                               d o _ P i n g                                */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Ping()
{

// Keep Statistics
//
   UPSTATS(miscCnt);

// This is a basic nop
//
   return Response.Send();
}

/******************************************************************************/
/*                            d o _ P r e p a r e                             */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Prepare()
{
   int rc, hport, pathnum = 0;
   char opts, hname[32], reqid[64], nidbuff[512], *path;
   XrdOucErrInfo myError;
   XrdOucTokenizer pathlist(argp->buff);
   XrdOucTList *pathp = 0;
   XrdXrootdPrepArgs pargs(0, 1);
   XrdSfsPrep fsprep;

// Grab the options
//
   opts = Request.prepare.options;

// Get a request ID for this prepare
//
   if (opts & kXR_stage && !(opts & kXR_cancel)) 
      XrdOucReqID::ID(reqid, sizeof(reqid));
      else {reqid[0] = '*'; reqid[1] = '\0';}

// Initialize the fsile system prepare arg list
//
   fsprep.reqid   = reqid;
   fsprep.paths   = 0;
   fsprep.opts    = Prep_PRTY0;
   fsprep.notify  = 0;

// Check if this is a cancel request
//
   if (opts & kXR_cancel)
      {if (!(path = pathlist.GetLine()))
          return Response.Send(kXR_ArgMissing, "Prepare requestid not specified");
       if (!XrdOucReqID::isMine(path, hport, hname, sizeof(hname)))
          {if (!hport) return Response.Send(kXR_ArgInvalid,
                             "Prepare requestid owned by an unknown server");
           TRACEI(REDIR, Response.ID() <<"redirecting to " << hname <<':' <<hport);
           return Response.Send(kXR_redirect, hport, hname);
          }
       fsprep.reqid = path;
       if (SFS_OK != (rc = osFS->prepare(fsprep, myError, CRED)))
          return fsError(rc, myError);
       rc = Response.Send();
       XrdXrootdPrepare::Logdel(path);
       return rc;
      }

// Cycle through all of the paths in the list
//
   while((path = pathlist.GetLine()))
        {if (rpCheck(path)) return rpEmsg("Preparing", path);
         if (!Squash(path)) return vpEmsg("Preparing", path);
         pathp = new XrdOucTList(path, pathnum, pathp);
         pathnum++;
        }

// Make sure we have at least one path
//
   if (!pathp)
      return Response.Send(kXR_ArgMissing, "No prepare paths specified");

// Issue the prepare
//
   if (opts & kXR_notify)
      {fsprep.notify  = nidbuff;
       sprintf(nidbuff, Notify, Link->FDnum(), Client.tident);
       fsprep.opts = (opts & kXR_noerrs ? Prep_SENDAOK : Prep_SENDACK);
      }
   if (opts & kXR_wmode) fsprep.opts |= Prep_WMODE;
   fsprep.paths = pathp;
   if (SFS_OK != (rc = osFS->prepare(fsprep, myError, CRED)))
      return fsError(rc, myError);

// Perform final processing
//
   if (!(opts & kXR_stage)) rc = Response.Send();
      else {rc = Response.Send(reqid, strlen(reqid));
            pargs.reqid=reqid;
            pargs.user=Client.tident;
            pargs.paths=pathp;
            XrdXrootdPrepare::Log(pargs);
           }
   return rc;
}
  
/******************************************************************************/
/*                           d o _ P r o t o c o l                            */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Protocol()
{
    static ServerResponseBody_Protocol Resp
                 = {static_cast<kXR_int32>(htonl(XROOTD_VERSBIN)),
                    static_cast<kXR_int32>(htonl(kXR_DataServer))};

// Keep Statistics
//
   UPSTATS(miscCnt);

// Return info
//
    if (isRedir) Resp.flags = static_cast<kXR_int32>(htonl(kXR_LBalServer));
    return Response.Send((void *)&Resp, sizeof(Resp));
}

/******************************************************************************/
/*                            d o _ P u t f i l e                             */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Putfile()
{
   int popts, buffsz;

// Keep Statistics
//
   UPSTATS(putfCnt);

// Unmarshall the data
//
   popts  = int(ntohl(Request.putfile.options));
   buffsz = int(ntohl(Request.putfile.buffsz));

   return Response.Send(kXR_Unsupported, "putfile request is not supported");
}

/******************************************************************************/
/*                              d o _ Q u e r y                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Query()
{
    short qopt = (short)ntohs(Request.query.infotype);

// Perform the appropriate query
//
   switch(qopt)
         {case kXR_QStats: return SI->Stats(Response,
                              (Request.header.dlen ? argp->buff : (char *)"a"));
          case kXR_Qcksum: return do_CKsum();
          default:         break;
         }

// Whatever we have, it's not valid
//
   return Response.Send(kXR_ArgInvalid, 
                        "Invalid information query type code");
}

/******************************************************************************/
/*                               d o _ R e a d                                */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Read()
{
   int retc;
   XrdXrootdFHandle fh(Request.read.fhandle);
   numReads++;

// We first handle the pre-read list, if any. We do it this was because of
// a historical glitch in the protocol. One should really not piggy back a
// pre-read on top of a read, though it is allowed.
//
   if (Request.header.dlen && do_ReadNone(retc)) return retc;

// Unmarshall the data
//
   myIOLen  = ntohl(Request.read.rlen);
              n2hll(Request.read.offset, myOffset);

// Find the file object
//
   if (!FTab || !(myFile = FTab->Get(fh.handle)))
      return Response.Send(kXR_FileNotOpen,
                           "read does not refer to an open file");

// Short circuit processing is read length is zero
//
   TRACEP(FS, "fh=" <<fh.handle <<" read " <<myIOLen <<'@' <<myOffset);
   if (!myIOLen) return Response.Send();

// If we are monitoring, insert a read entry
//
   if (monIO && Monitor) Monitor->Add_rd(myFile->FileID, Request.read.rlen,
                                         Request.read.offset);

// If we are in async mode, schedule the read to ocur asynchronously
//
   if (myFile->AsyncMode)
      {if (myIOLen >= as_miniosz && Link->UseCnt() < as_maxperlnk)
          if ((retc = aio_Read()) != -EAGAIN) return retc;
       SI->AsyncRej++;
      }

// Now read all of the data (do pre-reads first)
//
   return do_ReadAll();
}

/******************************************************************************/
/*                            d o _ R e a d A l l                             */
/******************************************************************************/

// myFile   = file to be read
// myOffset = Offset at which to read
// myIOLen  = Number of bytes to read from file and write to socket
  
int XrdXrootdProtocol::do_ReadAll()
{
   int xframt, Quantum = (myIOLen > maxBuffsz ? maxBuffsz : myIOLen);
   int iolen = myIOLen;
   char *buff;

// Make sure we have a large enough buffer
//
   if (!argp || Quantum > argp->bsize)
      {if (argp) BPool->Release(argp);
       if (!(argp = BPool->Obtain(Quantum)))
          return Response.Send(kXR_NoMemory,"insufficient memory to read file");
      }
   buff = argp->buff;

// Now read all of the data
//
   do {if ((xframt = myFile->XrdSfsp->read(myOffset, buff, Quantum)) <= 0) break;
       if (xframt >= myIOLen) return Response.Send(buff, xframt);
       if (Response.Send(kXR_oksofar, buff, xframt) < 0) return -1;
       myOffset += xframt; myIOLen -= xframt;
       if (myIOLen < Quantum) Quantum = myIOLen;
      } while(myIOLen);
   myFile->readCnt += (iolen - myIOLen); // Somewhat accurate

// Determine why we ended here
//
   if (xframt == 0) return Response.Send();
   return Response.Send(kXR_FSError,(char *)myFile->XrdSfsp->error.getErrText());
}

/******************************************************************************/
/*                           d o _ R e a d N o n e                            */
/******************************************************************************/
  
int XrdXrootdProtocol::do_ReadNone(int &retc)
{
   XrdXrootdFHandle fh;
   int ralsz = Request.header.dlen;
   struct readahead_list *ralsp=(struct readahead_list *)(argp->buff);

// Run down the pre-read list
//
   while(ralsz > 0)
        {myIOLen  = ntohl(ralsp->rlen);
                    n2hll(ralsp->offset, myOffset);
         memcpy((void *)&fh.handle, (const void *)ralsp->fhandle,
                  sizeof(fh.handle));
         TRACEP(FS, "fh=" <<fh.handle <<" read " <<myIOLen <<'@' <<myOffset);
         if (!FTab || !(myFile = FTab->Get(fh.handle)))
            {retc = Response.Send(kXR_FileNotOpen,
                             "preread does not refer to an open file");
             return 1;
            }
         myFile->XrdSfsp->read(myOffset, myIOLen);
         ralsz -= sizeof(struct readahead_list);
         ralsp++;
         numReads++;
        };

// All done
//
   return 0;
}
  
/******************************************************************************/
/*                                 d o _ R m                                  */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Rm()
{
   int rc;
   XrdOucErrInfo myError;

// Prescreen the path
//
   if (rpCheck(argp->buff)) return rpEmsg("Removing", argp->buff);
   if (!Squash(argp->buff)) return vpEmsg("Removing", argp->buff);

// Preform the actual function
//
   rc = osFS->rem((const char *)argp->buff, myError, CRED);
   TRACEP(FS, "rc=" <<rc <<" rm " <<argp->buff);
   if (SFS_OK == rc) return Response.Send();

// An error occured
//
   return fsError(rc, myError);
}

/******************************************************************************/
/*                              d o _ R m d i r                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Rmdir()
{
   int rc;
   XrdOucErrInfo myError;

// Prescreen the path
//
   if (rpCheck(argp->buff)) return rpEmsg("Removing", argp->buff);
   if (!Squash(argp->buff)) return vpEmsg("Removing", argp->buff);

// Preform the actual function
//
   rc = osFS->remdir((const char *)argp->buff, myError, CRED);
   TRACEP(FS, "rc=" <<rc <<" rmdir " <<argp->buff);
   if (SFS_OK == rc) return Response.Send();

// An error occured
//
   return fsError(rc, myError);
}

/******************************************************************************/
/*                                d o _ S e t                                 */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Set()
{
   XrdOucTokenizer setargs(argp->buff);
   char *val, *rest;

// Get the first argument
//
   if (!setargs.GetLine() || !(val = setargs.GetToken(&rest)))
      return Response.Send(kXR_ArgMissing, "set argument not specified.");

// Trace this set
//
   TRACEP(DEBUG, "set " <<val <<' ' <<rest);

// Now determine what the user wants to set
//
        if (!strcmp("appid", val))
           {while(*rest && *rest == ' ') rest++;
            eDest.Emsg("Xeq", (const char *)Link->ID, (char *)"appid", rest);
            return Response.Send();
           }
   else if (!strcmp("monitor", val)) return do_Set_Mon(setargs);

// All done
//
   return Response.Send(kXR_ArgInvalid, "invalid set parameter");
}

/******************************************************************************/
/*                            d o _ S e t _ M o n                             */
/******************************************************************************/

// Process: set monitor {off | on} [appid] | info [info]}

int XrdXrootdProtocol::do_Set_Mon(XrdOucTokenizer &setargs)
{
  char *val, *appid;
  kXR_unt32 myseq = 0;

// Get the first argument
//
   if (!(val = setargs.GetToken(&appid)))
      return Response.Send(kXR_ArgMissing,"set monitor argument not specified.");

// For info requests, nothing changes. However, info events must have been
// enabled for us to record them. Route the information via the static
// monitor entry, since it knows how to forward the information.
//
   if (!strcmp(val, "info"))
      {if (appid && XrdXrootdMonitor::monINFO)
          {while(*appid && *appid == ' ') appid++;
           if (strlen(appid) > 1024) appid[1024] = '\0';
           if (*appid) myseq = XrdXrootdMonitor::Map(XROOTD_MON_MAPINFO,
                               (const char *)Link->ID, (const char *)appid);
          }
       return Response.Send((void *)&myseq, sizeof(myseq));
      }

// Determine if on do appropriate processing
//
   if (!strcmp(val, "on"))
      {if (Monitor || (Monitor = XrdXrootdMonitor::Alloc(1)))
          {if (appid && XrdXrootdMonitor::monIO)
              {while(*appid && *appid == ' ') appid++;
               if (*appid) Monitor->appID(appid);
              }
           monIO   =  XrdXrootdMonitor::monIO;
           monFILE =  XrdXrootdMonitor::monFILE;
           if (XrdXrootdMonitor::monUSER && !monUID)
              monUID = XrdXrootdMonitor::Map(XROOTD_MON_MAPUSER,
                               (const char *)Link->ID, 0);
          }
       return Response.Send();
      }

// Determine if off and do appropriate processing
//
   if (!strcmp(val, "off"))
      {if (Monitor)
          {if (appid && XrdXrootdMonitor::monIO)
              {while(*appid && *appid == ' ') appid++;
               if (*appid) Monitor->appID(appid);
              }
           Monitor->unAlloc(Monitor); Monitor = 0; monIO = monFILE = 0;
          }
       return Response.Send();
      }

// Improper request
//
   return Response.Send(kXR_ArgInvalid, "invalid set monitor argument");
}
  
/******************************************************************************/
/*                               d o _ S t a t                                */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Stat()
{
   int len, rc, flags = 0;
   long long fsz;
   char xxBuff[256];
   struct stat buf;
   XrdOucErrInfo myError;
   union {long long uuid; struct {int hi; int lo;} id;} Dev;

// Prescreen the path
//
   if (rpCheck(argp->buff)) return rpEmsg("Stating", argp->buff);
   if (!Squash(argp->buff)) return vpEmsg("Stating", argp->buff);

// Preform the actual function
//
   rc = osFS->stat((const char *)argp->buff, &buf, myError, CRED);
   TRACEP(FS, "rc=" <<rc <<" stat " <<argp->buff);
   if (rc != SFS_OK) return fsError(rc, myError);

// Compute the unique id
//
   Dev.id.lo = buf.st_ino;
   Dev.id.hi = buf.st_dev;

// Compute the flag settings
//
   if (buf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) flags |= kXR_xset;
   if (S_ISDIR(buf.st_mode))                        flags |= kXR_isDir;
      else if (!S_ISREG(buf.st_mode))               flags |= kXR_other;
   if (!Dev.uuid)                                   flags |= kXR_offline;
   fsz = (long long)buf.st_size;

// Format the results and return them
//
   len = sprintf(xxBuff,"%lld %lld %d %ld",Dev.uuid,fsz,flags,buf.st_mtime)+1;
   return Response.Send(xxBuff, len);
}

/******************************************************************************/
/*                                                                            */
/*                              d o _ S t a t x                               */
/*                                                                            */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Statx()
{
   int rc;
   char *path, *respinfo = argp->buff;
   mode_t mode;
   XrdOucErrInfo myError;
   XrdOucTokenizer pathlist(argp->buff);

// Cycle through all of the paths in the list
//
   while((path = pathlist.GetLine()))
        {if (rpCheck(path)) return rpEmsg("Stating", path);
         if (!Squash(path)) return vpEmsg("Stating", path);
         rc = osFS->stat((const char *)path, mode, myError, CRED);
         TRACEP(FS, "rc=" <<rc <<" stat " <<path);
         if (rc != SFS_OK)                   *respinfo = (char)kXR_other;
            else {if (mode == (mode_t)-1)    *respinfo = (char)kXR_offline;
                     else if (S_ISDIR(mode)) *respinfo = (char)kXR_isDir;
                             else            *respinfo = (char)kXR_file;
                 }
         respinfo++;
        }

// Return result
//
   return Response.Send(argp->buff, respinfo-argp->buff);
}

/******************************************************************************/
/*                               d o _ S y n c                                */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Sync()
{
   int rc;
   XrdXrootdFile *fp;
   XrdXrootdFHandle fh(Request.sync.fhandle);

// Keep Statistics
//
   UPSTATS(syncCnt);

// Find the file object
//
   if (!FTab || !(fp = FTab->Get(fh.handle)))
      return Response.Send(kXR_FileNotOpen,"sync does not refer to an open file");

// Sync the file
//
   rc = fp->XrdSfsp->sync();
   TRACEP(FS, "sync rc=" <<rc <<" fh=" <<fh.handle);
   if (SFS_OK != rc)
      return Response.Send(kXR_FSError,(char *)fp->XrdSfsp->error.getErrText());

// Respond that all went well
//
   return Response.Send();
}

/******************************************************************************/
/*                              d o _ W r i t e                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Write()
{
   int retc;
   XrdXrootdFHandle fh(Request.write.fhandle);
   numWrites++;

// Unmarshall the data
//
   myIOLen  = Request.header.dlen;
              n2hll(Request.write.offset, myOffset);

// Find the file object
//
   if (!FTab || !(myFile = FTab->Get(fh.handle)))
      return Response.Send(kXR_FileNotOpen,"write does not refer to an open file");

// If we are monitoring, insert a write entry
//
   if (monIO && Monitor) Monitor->Add_wr(myFile->FileID, Request.write.dlen,
                                         Request.write.offset);

// If zero length write, simply return
//
   TRACEP(FS, "fh=" <<fh.handle <<" write " <<myIOLen <<'@' <<myOffset);
   if (myIOLen <= 0) return Response.Send();

// If we are in async mode, schedule the write to occur asynchronously
//
   if (myFile->AsyncMode && !as_syncw)
      {if (myStalls > as_maxstalls) myStalls--;
          else if (myIOLen >= as_miniosz && Link->UseCnt() < as_maxperlnk)
                  if ((retc = aio_Write()) != -EAGAIN) return retc;
       SI->AsyncRej++;
      }

// Just to the i/o now
//
   myFile->writeCnt += myIOLen; // Optimistically correct
   return do_WriteAll();
}
  
/******************************************************************************/
/*                           d o _ W r i t e A l l                            */
/******************************************************************************/

// myFile   = file to be written
// myOffset = Offset at which to write
// myIOLen  = Number of bytes to read from socket and write to file
  
int XrdXrootdProtocol::do_WriteAll()
{
   int rc, Quantum = (myIOLen > maxBuffsz ? maxBuffsz : myIOLen);

// Make sure we have a large enough buffer
//
   if (!argp || Quantum > argp->bsize)
      {if (argp) BPool->Release(argp);
       if (!(argp = BPool->Obtain(Quantum)))
          return Response.Send(kXR_NoMemory,"insufficient memory to write file");
      }

// Now write all of the data (XrdXrootdProtocol.C defines getData())
//
   while(myIOLen > 0)
        {if ((rc = getData("data", argp->buff, Quantum)))
            {if (rc > 0) 
                {Resume = &XrdXrootdProtocol::do_WriteCont;
                 myBlast = Quantum;
                 myStalls++;
                }
             return rc;
            }
         if (myFile->XrdSfsp->write(myOffset, argp->buff, Quantum) < 0)
            {myIOLen  = myIOLen-Quantum;
             return do_WriteNone();
            }
         myOffset += Quantum; myIOLen -= Quantum;
         if (myIOLen < Quantum) Quantum = myIOLen;
        }

// All done
//
   return Response.Send();
}

/******************************************************************************/
/*                          d o _ W r i t e C o n t                           */
/******************************************************************************/

// myFile   = file to be written
// myOffset = Offset at which to write
// myIOLen  = Number of bytes to read from socket and write to file
// myBlast  = Number of bytes already read from the socket
  
int XrdXrootdProtocol::do_WriteCont()
{

// Write data that was finaly finished comming in
//
   if (myFile->XrdSfsp->write(myOffset, argp->buff, myBlast) < 0)
      {myIOLen  = myIOLen-myBlast;
       return do_WriteNone();
      }
    myOffset += myBlast; myIOLen -= myBlast;

// See if we need to finish this request in the normal way
//
   if (myIOLen > 0) return do_WriteAll();
   return Response.Send();
}
  
/******************************************************************************/
/*                          d o _ W r i t e N o n e                           */
/******************************************************************************/
  
int XrdXrootdProtocol::do_WriteNone()
{
   int rlen, blen = (myIOLen > argp->bsize ? argp->bsize : myIOLen);

// Discard any data being transmitted
//
   TRACEP(REQ, "discarding " <<myIOLen <<" bytes");
   while(myIOLen > 0)
        {rlen = Link->Recv(argp->buff, blen, readWait);
         if (rlen  < 0) return Link->setEtext("link read error");
         myIOLen -= rlen;
         if (rlen < blen) 
            {myBlen   = 0;
             Resume   = &XrdXrootdProtocol::do_WriteNone;
             return 1;
            }
         if (myIOLen < blen) blen = myIOLen;
        }

// Send our the error message and return
//
   return Response.Send(kXR_FSError,(char *)myFile->XrdSfsp->error.getErrText());
}
  
/******************************************************************************/
/*                       U t i l i t y   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                               f s E r r o r                                */
/******************************************************************************/
  
int XrdXrootdProtocol::fsError(int rc, XrdOucErrInfo &myError)
{
   int ecode;
   const char *eMsg = myError.getErrText(ecode);

// Process standard errors
//
   if (rc == SFS_ERROR)
      {SI->errorCnt++;
       rc = mapError(ecode);
       return Response.Send((XErrorCode)rc, eMsg);
      }

// Process the redirection (error msg is host:port)
//
   if (rc == SFS_REDIRECT)
      {SI->redirCnt++;
       if (ecode <= 0) ecode = (ecode ? -ecode : Port);
       TRACEI(REDIR, Response.ID() <<"redirecting to " << eMsg <<':' <<ecode);
       return Response.Send(kXR_redirect, ecode, (char *)eMsg);
      }

// Process the deferal
//
   if (rc >= SFS_STALL)
      {SI->stallCnt++;
       TRACEI(STALL, Response.ID() <<"stalling client for " <<rc <<" sec");
       return Response.Send(kXR_wait, rc, (char *)eMsg);
      }

// Unknown conditions, report it
//
   {char buff[32];
    SI->errorCnt++;
    sprintf(buff, "%d", rc);
    eDest.Emsg("Xeq", "Unknown error code", buff, (char *)eMsg);
    return Response.Send(kXR_ServerError, eMsg);
   }
}
  
/******************************************************************************/
/*                              m a p E r r o r                               */
/******************************************************************************/
  
int XrdXrootdProtocol::mapError(int rc)
{
    if (rc < 0) rc = -rc;
    switch(rc)
       {case ENOENT:       return kXR_NotFound;
        case EPERM:        return kXR_NotAuthorized;
        case EACCES:       return kXR_NotAuthorized;
        case EIO:          return kXR_IOError;
        case ENOMEM:       return kXR_NoMemory;
        case ENOBUFS:      return kXR_NoMemory;
        case ENOSPC:       return kXR_NoSpace;
        case ENAMETOOLONG: return kXR_ArgTooLong;
        case ENETUNREACH:  return kXR_noserver;
        case ENOTBLK:      return kXR_NotFile;
        case EISDIR:       return kXR_isDir;
        default:           return kXR_FSError;
       }
}

/******************************************************************************/
/*                               m a p M o d e                                */
/******************************************************************************/

#define Map_Mode(x,y) if (Mode & kXR_ ## x) newmode |= S_I ## y

int XrdXrootdProtocol::mapMode(int Mode)
{
   int newmode = 0;

// Map the mode in the obvious way
//
   Map_Mode(ur, RUSR); Map_Mode(uw, WUSR);  Map_Mode(ux, XUSR);
   Map_Mode(gr, RGRP); Map_Mode(gw, WGRP);  Map_Mode(gx, XGRP);
   Map_Mode(or, ROTH);                      Map_Mode(ox, XOTH);

// All done
//
   return newmode;
}
  
/******************************************************************************/
/*                               r p C h e c k                                */
/******************************************************************************/
  
int XrdXrootdProtocol::rpCheck(char *fn)
{
   char *cp;

   if (*fn != '/') return 1;

   while ((cp = index((const char *)fn, '/')))
         {fn = cp+1;
          if (fn[0] == '.' && fn[1] == '.' && fn[2] == '/') return 1;
         }
   return 0;
}
  
/******************************************************************************/
/*                                r p E m s g                                 */
/******************************************************************************/
  
int XrdXrootdProtocol::rpEmsg(const char *op, char *fn)
{
   char buff[2048];
   snprintf(buff,sizeof(buff)-1,"%s relative path '%s' is disallowed.",op,fn);
   buff[sizeof(buff)-1] = '\0';
   return Response.Send(kXR_NotAuthorized, buff);
}
 
/******************************************************************************/
/*                                S q u a s h                                 */
/******************************************************************************/
  
int XrdXrootdProtocol::Squash(char *fn)
{
   char *ofn, *ifn = fn;

   while(*ifn)
        {if (*ifn == '/')
            if (*(ifn+1) == '/'
            || (*(ifn+1) == '.' && *(ifn+1) && *(ifn+2) == '/')) break;
         ifn++;
        }

   if (!*ifn) return XPList.Validate(fn, ifn-fn);

   ofn = ifn;
   while(*ifn) {*ofn = *ifn++;
                while(*ofn == '/')
                   {while(*ifn == '/') ifn++;
                    if (ifn[0] == '.' && ifn[1] == '/') ifn += 2;
                       else break;
                   }
                ofn++;
               }
   *ofn = '\0';

   return XPList.Validate(fn, ofn-fn);
}
  
/******************************************************************************/
/*                                v p E m s g                                 */
/******************************************************************************/
  
int XrdXrootdProtocol::vpEmsg(const char *op, char *fn)
{
   char buff[2048];
   snprintf(buff,sizeof(buff)-1,"%s path '%s' is disallowed.",op,fn);
   buff[sizeof(buff)-1] = '\0';
   return Response.Send(kXR_NotAuthorized, buff);
}
