/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d L o a d L i b . c c                    */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

#include "XrdVersion.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlugin.hh"

/******************************************************************************/
/*              V e r s i o n   I n f o r m a t i o n   L i n k               */
/******************************************************************************/
  
XrdVERSIONINFOREF(XrdgetProtocol);

/******************************************************************************/
/*                 x r o o t d _ l o a d F i l e s y s t e m                  */
/******************************************************************************/

XrdSfsFileSystem *XrdXrootdloadFileSystem(XrdSysError *eDest, 
                                          char *fslib, const char *cfn)
{
   XrdSysPlugin ofsLib(eDest,fslib,"fslib",&XrdVERSIONINFOVAR(XrdgetProtocol));
   XrdSfsFileSystem *(*ep)(XrdSfsFileSystem *, XrdSysLogger *, const char *);
   XrdSfsFileSystem *FS;

// Record the library path in the environment
//
   XrdOucEnv::Export("XRDOFSLIB", fslib);

// Get the file system object creator
//
   if (!(ep = (XrdSfsFileSystem *(*)(XrdSfsFileSystem *,XrdSysLogger *,const char *))
                                    ofsLib.getPlugin("XrdSfsGetFileSystem")))
       return 0;

// Get the file system object
//
   if (!(FS = (*ep)(0, eDest->logger(), cfn)))
      {eDest->Emsg("Config", "Unable to create file system object via",fslib);
       return 0;
      }

// All done
//
   ofsLib.Persist();
   return FS;
}
  
/******************************************************************************/
/*                   x r o o t d _ l o a d S e c u r i t y                    */
/******************************************************************************/

XrdSecService *XrdXrootdloadSecurity(XrdSysError *eDest, char *seclib, 
                                     char *cfn, void **secGetProt)
{
   XrdSysPlugin secLib(eDest, seclib, "seclib",
                       &XrdVERSIONINFOVAR(XrdgetProtocol), 1);
   XrdSecService *(*ep)(XrdSysLogger *, const char *cfn);
   XrdSecService *CIA;

// Get the server object creator
//
   if (!(ep = (XrdSecService *(*)(XrdSysLogger *, const char *cfn))
              secLib.getPlugin("XrdSecgetService")))
       return 0;

// Get the server object
//
   if (!(CIA = (*ep)(eDest->logger(), cfn)))
      {eDest->Emsg("Config", "Unable to create security service object via",seclib);
       return 0;
      }

// Get the client object creator (in case we are acting as a client). We return
// the function pointer as a (void *) to the caller so that it can be passed
// onward via an environment object.
//
   if (!(*secGetProt = (void *)secLib.getPlugin("XrdSecGetProtocol"))) return 0;

// All done
//
   secLib.Persist();
   return CIA;
}
