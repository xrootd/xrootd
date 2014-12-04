/******************************************************************************/
/*                                                                            */
/*                     X r d C l i e n t E n v . c c                          */
/*                                                                            */
/* Author: Fabrizio Furano (INFN Padova, 2004)                                */
/* Adapted from TXNetFile (root.cern.ch) originally done by                   */
/*  Alvise Dorigo, Fabrizio Furano                                            */
/*          INFN Padova, 2003                                                 */
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

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// Singleton used to handle the default parameter values                //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "XrdSys/XrdSysHeaders.hh"
#include "XrdClient/XrdClientConst.hh"
#include "XrdClient/XrdClientEnv.hh"
#include "XrdClient/XrdClientConn.hh"
#include "XrdClient/XrdClientConnMgr.hh"
#include <string>
#include <algorithm>
#include <ctype.h>

XrdClientEnv *XrdClientEnv::fgInstance = 0;

XrdClientEnv *XrdClientEnv::Instance() {
   // Create unique instance

   if (!fgInstance) {
      fgInstance = new XrdClientEnv;
      if (!fgInstance) {
	 std::cerr << "XrdClientEnv::Instance: fatal - couldn't create XrdClientEnv" << std::endl;
         abort();
      }
   }
   return fgInstance;
}

//_____________________________________________________________________________
XrdClientEnv::XrdClientEnv() {
   // Constructor
   fOucEnv   = new XrdOucEnv();
   fShellEnv = new XrdOucEnv();

   PutInt(NAME_CONNECTTIMEOUT, DFLT_CONNECTTIMEOUT);
   PutInt(NAME_REQUESTTIMEOUT, DFLT_REQUESTTIMEOUT);
   PutInt(NAME_MAXREDIRECTCOUNT, DFLT_MAXREDIRECTCOUNT);
   PutInt(NAME_DEBUG, DFLT_DEBUG);
   PutInt(NAME_RECONNECTWAIT, DFLT_RECONNECTWAIT);
   PutInt(NAME_REDIRCNTTIMEOUT, DFLT_REDIRCNTTIMEOUT);
   PutInt(NAME_FIRSTCONNECTMAXCNT, DFLT_FIRSTCONNECTMAXCNT);
   PutInt(NAME_READCACHESIZE, DFLT_READCACHESIZE);
   PutInt(NAME_READCACHEBLKREMPOLICY, DFLT_READCACHEBLKREMPOLICY);
   PutInt(NAME_READAHEADSIZE, DFLT_READAHEADSIZE);
   PutInt(NAME_MULTISTREAMCNT, DFLT_MULTISTREAMCNT);
   PutInt(NAME_DFLTTCPWINDOWSIZE, DFLT_DFLTTCPWINDOWSIZE);
   PutInt(NAME_DATASERVERCONN_TTL, DFLT_DATASERVERCONN_TTL);
   PutInt(NAME_LBSERVERCONN_TTL, DFLT_LBSERVERCONN_TTL);
   PutInt(NAME_PURGEWRITTENBLOCKS, DFLT_PURGEWRITTENBLOCKS);
   PutInt(NAME_READAHEADSTRATEGY, DFLT_READAHEADSTRATEGY);
   PutInt(NAME_READTRIMBLKSZ, DFLT_READTRIMBLKSZ);
   PutInt(NAME_TRANSACTIONTIMEOUT, DFLT_TRANSACTIONTIMEOUT);
   PutInt(NAME_REMUSEDCACHEBLKS, DFLT_REMUSEDCACHEBLKS);
   PutInt(NAME_ENABLE_FORK_HANDLERS, DFLT_ENABLE_FORK_HANDLERS);
   PutInt(NAME_ENABLE_TCP_KEEPALIVE, DFLT_ENABLE_TCP_KEEPALIVE);
   PutInt(NAME_TCP_KEEPALIVE_TIME,     DFLT_TCP_KEEPALIVE_TIME);
   PutInt(NAME_TCP_KEEPALIVE_INTERVAL, DFLT_TCP_KEEPALIVE_INTERVAL);
   PutInt(NAME_TCP_KEEPALIVE_PROBES,   DFLT_TCP_KEEPALIVE_PROBES);
   PutInt(NAME_XRDCP_SIZE_HINT,        DFLT_XRDCP_SIZE_HINT);
   PutInt(NAME_PRINT_REDIRECTS,        DFLT_PRINT_REDIRECTS);

   ImportInt( NAME_CONNECTTIMEOUT );
   ImportInt( NAME_REQUESTTIMEOUT );
   ImportInt( NAME_MAXREDIRECTCOUNT );
   ImportInt( NAME_DEBUG );
   ImportInt( NAME_RECONNECTWAIT );
   ImportInt( NAME_REDIRCNTTIMEOUT );
   ImportInt( NAME_FIRSTCONNECTMAXCNT );
   ImportInt( NAME_READCACHESIZE );
   ImportInt( NAME_READCACHEBLKREMPOLICY );
   ImportInt( NAME_READAHEADSIZE );
   ImportInt( NAME_MULTISTREAMCNT );
   ImportInt( NAME_DFLTTCPWINDOWSIZE );
   ImportInt( NAME_DATASERVERCONN_TTL );
   ImportInt( NAME_LBSERVERCONN_TTL );
   ImportInt( NAME_PURGEWRITTENBLOCKS );
   ImportInt( NAME_READAHEADSTRATEGY );
   ImportInt( NAME_READTRIMBLKSZ );
   ImportInt( NAME_TRANSACTIONTIMEOUT );
   ImportInt( NAME_REMUSEDCACHEBLKS );
   ImportInt( NAME_ENABLE_FORK_HANDLERS );
   ImportInt( NAME_ENABLE_TCP_KEEPALIVE );
   ImportInt( NAME_TCP_KEEPALIVE_TIME );
   ImportInt( NAME_TCP_KEEPALIVE_INTERVAL );
   ImportInt( NAME_TCP_KEEPALIVE_PROBES );
   ImportInt( NAME_XRDCP_SIZE_HINT );
   ImportInt( NAME_PRINT_REDIRECTS );
}

//------------------------------------------------------------------------------
// Import a string variable from the shell environment
//------------------------------------------------------------------------------
bool XrdClientEnv::ImportStr( const char *varname )
{
  std::string name = "XRD_";
  name += varname;
  std::transform( name.begin(), name.end(), name.begin(), ::toupper );

  char *value;
  if( !XrdOucEnv::Import( name.c_str(), value ) )
     return false;

  fShellEnv->Put( varname, value );
  return true;
}

//------------------------------------------------------------------------------
// Import an int variable from the shell environment
//------------------------------------------------------------------------------
bool XrdClientEnv::ImportInt( const char *varname )
{
  std::string name = "XRD_";
  name += varname;
  std::transform( name.begin(), name.end(), name.begin(), ::toupper );

  long value;
  if( !XrdOucEnv::Import( name.c_str(), value ) )
     return false;

  fShellEnv->PutInt( varname, value );
  return true;
}

//------------------------------------------------------------------------------
// Get a string from the shell environment
//------------------------------------------------------------------------------
const char *XrdClientEnv::ShellGet(const char *varname)
{
  XrdSysMutexHelper m( fMutex );
  const char *res = fShellEnv->Get( varname );
  if( res )
    return res;

  res = fOucEnv->Get( varname );
  return res;
}

//------------------------------------------------------------------------------
// Get an integer from the shell environment
//------------------------------------------------------------------------------
long XrdClientEnv::ShellGetInt(const char *varname)
{
  XrdSysMutexHelper m( fMutex );
  const char *res = fShellEnv->Get( varname );

  if( res )
    return fShellEnv->GetInt( varname );

  return fOucEnv->GetInt( varname );
}


//_____________________________________________________________________________
XrdClientEnv::~XrdClientEnv() {
   // Destructor
   delete fOucEnv;
   delete fShellEnv;
   delete fgInstance;

   fgInstance = 0;
}

//------------------------------------------------------------------------------
// The fork handlers need to have C linkage (no symbol name mangling)
//------------------------------------------------------------------------------
extern "C"
{

//------------------------------------------------------------------------------
// To be called prior to forking
//------------------------------------------------------------------------------
static void prepare()
{
  if( EnvGetLong( NAME_ENABLE_FORK_HANDLERS ) && ConnectionManager )
  {
    ConnectionManager->ShutDown();
    XrdClientConn::DelSessionIDRepo();
  }
  XrdClientEnv::Instance()->Lock();
}

//------------------------------------------------------------------------------
// To be called in the parent just after forking
//------------------------------------------------------------------------------
static void parent()
{
  XrdClientEnv::Instance()->UnLock();
  if( EnvGetLong( NAME_ENABLE_FORK_HANDLERS ) && ConnectionManager )
  {
    ConnectionManager->BootUp();
  }
}

//------------------------------------------------------------------------------
// To be called in the child just after forking
//------------------------------------------------------------------------------
static void child()
{
  XrdClientEnv::Instance()->ReInitLock();
  if( EnvGetLong( NAME_ENABLE_FORK_HANDLERS ) && ConnectionManager )
  {
    ConnectionManager->BootUp();
  }
}

} // extern "C"

//------------------------------------------------------------------------------
// Install the fork handlers on application startup and set IPV4 mode
//------------------------------------------------------------------------------
namespace
{
  static struct Initializer
  {
    Initializer()
    {
      //------------------------------------------------------------------------
      // Install the fork handlers
      //------------------------------------------------------------------------
#ifndef WIN32
      if( pthread_atfork( prepare, parent, child ) != 0 )
      {
        std::cerr << "Unable to install the fork handlers - safe forking not ";
        std::cerr << "possible" << std::endl;
      }
#endif
    }
  } initializer;
}
