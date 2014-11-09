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

#ifndef __XRD_CL_FORK_HANDLER_HH__
#define __XRD_CL_FORK_HANDLER_HH__

#include <XrdSys/XrdSysPthread.hh>
#include <set>

namespace XrdCl
{
  class FileStateHandler;
  class FileSystem;
  class PostMaster;
  class FileTimer;

  //----------------------------------------------------------------------------
  // Helper class for handling forking
  //----------------------------------------------------------------------------
  class ForkHandler
  {
    public:
      ForkHandler();

      //------------------------------------------------------------------------
      //! Register a file object
      //------------------------------------------------------------------------
      void RegisterFileObject( FileStateHandler *file )
      {
        XrdSysMutexHelper scopedLock( pMutex );
        pFileObjects.insert( file );
      }

      //------------------------------------------------------------------------
      // Un-register a file object
      //------------------------------------------------------------------------
      void UnRegisterFileObject( FileStateHandler *file )
      {
        XrdSysMutexHelper scopedLock( pMutex );
        pFileObjects.erase( file );
      }

      //------------------------------------------------------------------------
      // Register a file system object
      //------------------------------------------------------------------------
      void RegisterFileSystemObject( FileSystem *fs )
      {
        XrdSysMutexHelper scopedLock( pMutex );
        pFileSystemObjects.insert( fs );
      }

      //------------------------------------------------------------------------
      //! Un-register a file system object
      //------------------------------------------------------------------------
      void UnRegisterFileSystemObject( FileSystem *fs )
      {
        XrdSysMutexHelper scopedLock( pMutex );
        pFileSystemObjects.erase( fs );
      }

      //------------------------------------------------------------------------
      //! Register a post master object
      //------------------------------------------------------------------------
      void RegisterPostMaster( PostMaster *postMaster )
      {
        XrdSysMutexHelper scopedLock( pMutex );
        pPostMaster = postMaster;
      }

      void RegisterFileTimer( FileTimer *fileTimer )
      {
        XrdSysMutexHelper scopedLock( pMutex );
        pFileTimer = fileTimer;
      }

      //------------------------------------------------------------------------
      //! Handle the preparation part of the forking process
      //------------------------------------------------------------------------
      void Prepare();

      //------------------------------------------------------------------------
      //! Handle the parent post-fork
      //------------------------------------------------------------------------
      void Parent();

      //------------------------------------------------------------------------
      //! Handler the child post-fork
      //------------------------------------------------------------------------
      void Child();

    private:
      std::set<FileStateHandler*>  pFileObjects;
      std::set<FileSystem*>        pFileSystemObjects;
      PostMaster                  *pPostMaster;
      FileTimer                   *pFileTimer;
      XrdSysMutex                  pMutex;
  };
}

#endif // __XRD_CL_FORK_HANDLER_HH__
