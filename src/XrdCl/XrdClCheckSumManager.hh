//------------------------------------------------------------------------------
// Copyright (c) 2012-2014 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
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

#ifndef __XRD_CL_CHECK_SUM_MANAGER_HH__
#define __XRD_CL_CHECK_SUM_MANAGER_HH__

#include <map>
#include <string>
#include "XrdSys/XrdSysPthread.hh"
#include "XrdCks/XrdCksData.hh"

class XrdCksLoader;
class XrdCksCalc;

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! Manage the checksum calc objects
  //----------------------------------------------------------------------------
  class CheckSumManager
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      CheckSumManager();

      //------------------------------------------------------------------------
      // Destructor
      //------------------------------------------------------------------------
      virtual ~CheckSumManager();

      //------------------------------------------------------------------------
      //! Get the check sum calc object for a given checksum type
      //!
      //! @param  algName name of the checksumming algorithm
      //! @return the appropriate calc object (must be deleted by the user)
      //!          or 0 if a calculator cannot be obtained
      //------------------------------------------------------------------------
      XrdCksCalc *GetCalculator( const std::string &algName );

      //------------------------------------------------------------------------
      //! Calculate a checksum of for a given file
      //------------------------------------------------------------------------
      bool Calculate( XrdCksData        &result,
                      const std::string &algName,
                      const std::string &filePath );

    private:
      CheckSumManager(const CheckSumManager &other);
      CheckSumManager &operator = (const CheckSumManager &other);

      typedef std::map<std::string, XrdCksCalc*> CalcMap;
      CalcMap       pCalculators;
      XrdCksLoader *pLoader;
      XrdSysMutex   pMutex;
  };
}

#endif // __XRD_CL_CHECK_SUM_MANAGER_HH__
