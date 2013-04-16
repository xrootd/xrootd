//------------------------------------------------------------------------------
// Copyright (c) 2013 by European Organization for Nuclear Research (CERN)
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

#ifndef __XRD_CL_FILE_TIMER_HH__
#define __XRD_CL_FILE_TIMER_HH__

#include "XrdSys/XrdSysPthread.hh"
#include "XrdCl/XrdClTaskManager.hh"

namespace XrdCl
{
  class FileStateHandler;

  //----------------------------------------------------------------------------
  //! Task generating timeout events for FileStateHandlers in recovery mode
  //----------------------------------------------------------------------------
  class FileTimer: public Task
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      FileTimer()
      {
        SetName( "FileTimer task" );
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~FileTimer()
      {
      }

      //------------------------------------------------------------------------
      //! Register a file state handler
      //------------------------------------------------------------------------
      void RegisterFileObject( FileStateHandler *file )
      {
        XrdSysMutexHelper scopedLock( pMutex );
        pFileObjects.insert( file );
      }

      //------------------------------------------------------------------------
      //! Un-register a file state handler
      //------------------------------------------------------------------------
      void UnRegisterFileObject( FileStateHandler *file )
      {
        XrdSysMutexHelper scopedLock( pMutex );
        pFileObjects.erase( file );
      }

      //------------------------------------------------------------------------
      //! Lock the task
      //------------------------------------------------------------------------
      void Lock()
      {
        pMutex.Lock();
      }

      //------------------------------------------------------------------------
      //! Un-lock the task
      //------------------------------------------------------------------------
      void UnLock()
      {
        pMutex.UnLock();
      }

      //------------------------------------------------------------------------
      //! Perform the task's action
      //------------------------------------------------------------------------
      virtual time_t Run( time_t now );

    private:
      std::set<FileStateHandler*> pFileObjects;
      XrdSysMutex                 pMutex;
  };
}

#endif // __XRD_CL_FILE_TIMER_HH__
