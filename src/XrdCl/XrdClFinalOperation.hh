//------------------------------------------------------------------------------
// Copyright (c) 2011-2017 by European Organization for Nuclear Research (CERN)
// Author: Krzysztof Jamrog <krzysztof.piotr.jamrog@cern.ch>,
//         Michal Simon <michal.simon@cern.ch>
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

#ifndef SRC_XRDCL_XRDCLFINALOPERATION_HH_
#define SRC_XRDCL_XRDCLFINALOPERATION_HH_

#include <functional>

namespace XrdCl
{
  class XRootDStatus;

  //---------------------------------------------------------------------------
  //! Final operation in the pipeline, always executed, no matter if the
  //! pipeline failed or not.
  //!
  //! Used to manage resources.
  //---------------------------------------------------------------------------
  class FinalOperation
  {
    //declare friendship with other operations
    template<template<bool> class Derived, bool HasHndl, typename HdlrFactory, typename ... Args>
    friend class ConcreteOperation;

    public:

      //-----------------------------------------------------------------------
      //! Constructor
      //!
      //! @param final : the routine that should be called in order to finalize
      //!                the pipeline
      //-----------------------------------------------------------------------
      FinalOperation( std::function<void(const XRootDStatus&)> final ) : final( std::move( final ) )
      {
      }

    private:

      //-----------------------------------------------------------------------
      //! finalization routine
      //-----------------------------------------------------------------------
      std::function<void(const XRootDStatus&)> final;
  };

  typedef FinalOperation Final;
}

#endif /* SRC_XRDCL_XRDCLFINALOPERATION_HH_ */
