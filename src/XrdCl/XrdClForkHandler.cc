//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClForkHandler.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClPostMaster.hh"
#include "XrdCl/XrdClFileTimer.hh"
#include "XrdCl/XrdClFileStateHandler.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  ForkHandler::ForkHandler():
    pPostMaster(0), pFileTimer(0)
  {
  }

  //----------------------------------------------------------------------------
  // Handle the preparation part of the forking process
  //----------------------------------------------------------------------------
  void ForkHandler::Prepare()
  {
    Log *log = DefaultEnv::GetLog();
    pid_t pid = getpid();
    log->Debug( UtilityMsg, "Running the prepare fork handler for process %d",
                pid );

    pMutex.Lock();
    if( pPostMaster )
      pPostMaster->Stop();
    pFileTimer->Lock();

    //--------------------------------------------------------------------------
    // Lock the user-level objects
    //--------------------------------------------------------------------------
    log->Debug( UtilityMsg, "Locking File and FileSystem objects for process: "
                "%d", pid );

    std::set<FileStateHandler*>::iterator itFile;
    for( itFile = pFileObjects.begin(); itFile != pFileObjects.end();
         ++itFile )
      (*itFile)->Lock();

    std::set<FileSystem*>::iterator itFs;
    for( itFs = pFileSystemObjects.begin(); itFs != pFileSystemObjects.end();
         ++itFs )
      (*itFs)->Lock();
  }

  //----------------------------------------------------------------------------
  // Handle the parent post-fork
  //----------------------------------------------------------------------------
  void ForkHandler::Parent()
  {
    Log *log = DefaultEnv::GetLog();
    pid_t pid = getpid();
    log->Debug( UtilityMsg, "Running the parent fork handler for process %d",
                pid );

    log->Debug( UtilityMsg, "Unlocking File and FileSystem objects for "
                "process:  %d", pid );

    std::set<FileStateHandler*>::iterator itFile;
    for( itFile = pFileObjects.begin(); itFile != pFileObjects.end();
         ++itFile )
      (*itFile)->UnLock();

    std::set<FileSystem*>::iterator itFs;
    for( itFs = pFileSystemObjects.begin(); itFs != pFileSystemObjects.end();
         ++itFs )
      (*itFs)->UnLock();

    pFileTimer->UnLock();
    if( pPostMaster )
      pPostMaster->Start();

    pMutex.UnLock();
  }

  //------------------------------------------------------------------------
  //! Handler the child post-fork
  //------------------------------------------------------------------------
  void ForkHandler::Child()
  {
    Log *log = DefaultEnv::GetLog();
    pid_t pid = getpid();
    log->Debug( UtilityMsg, "Running the child fork handler for process %d",
                pid );

    log->Debug( UtilityMsg, "Unlocking File and FileSystem objects for "
                "process:  %d", pid );

    std::set<FileStateHandler*>::iterator itFile;
    for( itFile = pFileObjects.begin(); itFile != pFileObjects.end();
         ++itFile )
    {
      (*itFile)->AfterForkChild();
      (*itFile)->UnLock();
    }

    std::set<FileSystem*>::iterator itFs;
    for( itFs = pFileSystemObjects.begin(); itFs != pFileSystemObjects.end();
         ++itFs )
      (*itFs)->UnLock();

    pFileTimer->UnLock();
    if( pPostMaster )
    {
      pPostMaster->Finalize();
      pPostMaster->Initialize();
      pPostMaster->Start();
      pPostMaster->GetTaskManager()->RegisterTask( pFileTimer, time(0), false );
    }

    pMutex.UnLock();
  }
}
