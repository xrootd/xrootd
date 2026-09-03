//------------------------------------------------------------------------------
// Copyright (c) 2026 by European Organization for Nuclear Research (CERN)
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
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
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClThirdPartyCopyPlugIn.hh"

#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClPlugInInterface.hh"
#include "XrdCl/XrdClPlugInManager.hh"
#include "XrdCl/XrdClURL.hh"

#include <memory>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Destructor anchors the vtable and typeinfo in libXrdCl so that the
  // dynamic_cast below works on plug-in objects from dlopened libraries.
  //----------------------------------------------------------------------------
  ThirdPartyCopyPlugIn::~ThirdPartyCopyPlugIn() = default;

  //----------------------------------------------------------------------------
  // Run a third party copy through the plug-in registered for the source URL
  //----------------------------------------------------------------------------
  XRootDStatus ThirdPartyCopy( const URL          &source,
                               const URL          &dest,
                               const PropertyList *properties,
                               ProgressHandler    *progress_handler,
                               time_t              timeout )
  {
    PlugInFactory *fact = DefaultEnv::GetPlugInManager()->GetFactory( source.GetURL() );
    if( !fact )
      return XRootDStatus( stError, errNotSupported );

    std::unique_ptr<FileSystemPlugIn> fs( fact->CreateFileSystem( source.GetURL() ) );
    auto *tpc = dynamic_cast<ThirdPartyCopyPlugIn*>( fs.get() );
    if( !tpc )
      return XRootDStatus( stError, errNotSupported );

    return tpc->ThirdPartyCopy( source.GetURL(), dest.GetURL(), properties,
                                progress_handler, timeout );
  }
}
