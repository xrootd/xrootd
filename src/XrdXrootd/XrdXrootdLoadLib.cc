/******************************************************************************/
/*                                                                            */
/*                      x r o o t d _ L o a d L i b . C                       */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*      All Rights Reserved. See XrdVersion.cc for complete License Terms     */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//       $Id$ 

const char *XrdXrootdLoadLibCVSID = "$Id$";

// Bypass Solaris ELF madness
//
#if (defined(SUNCC) || defined(SUN)) 
#include <sys/isa_defs.h>
#if defined(_ILP32) && (_FILE_OFFSET_BITS != 32)
#undef  _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 32
#undef  _LARGEFILE_SOURCE
#endif
#endif
  
#include <dlfcn.h>
#include <link.h>

#include "Experiment/Experiment.hh"

#include "XrdSec/XrdSecInterface.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdOuc/XrdOucError.hh"
  
/******************************************************************************/
/*                 x r o o t d _ l o a d F i l e s y s t e m                  */
/******************************************************************************/

XrdSfsFileSystem *XrdXrootdloadFileSystem(XrdOucError *eDest, 
                                          char *fslib, const char *cfn)
{
   void *libhandle;
   XrdSfsFileSystem *(*ep)(XrdSfsFileSystem *, XrdOucLogger *, const char *);
   XrdSfsFileSystem *FS;

// Open the security library
//
   if (!(libhandle = dlopen(fslib, RTLD_NOW)))
      {eDest->Emsg("Config",dlerror(),(char *)"opening shared library",fslib);
       return 0;
      }

// Get the file system object creator
//
   if (!(ep = (XrdSfsFileSystem *(*)(XrdSfsFileSystem *,XrdOucLogger *,const char *))
                                  dlsym(libhandle,"XrdSfsGetFileSystem")))
      {eDest->Emsg("Config", dlerror(),
                   (char *)"finding XrdSfsGetFileSystem() in", fslib);
       return 0;
      }

// Get the file system object
//
   if (!(FS = (*ep)(0, eDest->logger(), cfn)))
      {eDest->Emsg("Config", "Unable to create file system object via",fslib);
       return 0;
      }

// All done
//
   return FS;
}
  
/******************************************************************************/
/*                   x r o o t d _ l o a d S e c u r i t y                    */
/******************************************************************************/

XrdSecProtocol *XrdXrootdloadSecurity(XrdOucError *eDest, char *seclib)
{
   void *libhandle;
   XrdSecProtocol *(*ep)(XrdOucError &);
   static XrdOucError secDest(eDest->logger());  // Passed only once!
   XrdSecProtocol *CIA;

// Open the security library
//
   if (!(libhandle = dlopen(seclib, RTLD_NOW)))
      {eDest->Emsg("Config",dlerror(),(char *)"opening shared library",seclib);
       return 0;
      }

// Get the server object creator
//
   if (!(ep = (XrdSecProtocol *(*)(XrdOucError &))dlsym(libhandle,
              "XrdSecProtocolsrvrObject")))
      {eDest->Emsg("Config", dlerror(),
                   (char *)"finding XrdSecProtocolsrvrObject() in", seclib);
       return 0;
      }

// Get the server object
//
   if (!(CIA = (*ep)(secDest)))
      {eDest->Emsg("Config", "Unable to create security protocol object via",seclib);
       return 0;
      }

// All done
//
   return CIA;
}
