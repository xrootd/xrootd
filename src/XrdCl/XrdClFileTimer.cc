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

#include "XrdCl/XrdClFileTimer.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClFileStateHandler.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Perform the task's action
  //----------------------------------------------------------------------------
  time_t FileTimer::Run( time_t now )
  {
    pMutex.Lock();
    std::set<FileStateHandler*>::iterator it;
    for( it = pFileObjects.begin(); it != pFileObjects.end(); ++it )
      (*it)->Tick(now);
    pMutex.UnLock();
    Env *env = DefaultEnv::GetEnv();
    int timeoutResolution = DefaultTimeoutResolution;
    env->GetInt( "TimeoutResolution", timeoutResolution );
    return now+timeoutResolution;
  }
}
