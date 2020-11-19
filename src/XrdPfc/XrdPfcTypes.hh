#ifndef __XRDPFC_TYPES_HH__
#define __XRDPFC_TYPES_HH__
//----------------------------------------------------------------------------------
// Copyright (c) 2014 by Board of Trustees of the Leland Stanford, Jr., University
// Author: Alja Mrak-Tadel, Matevz  Tadel, Brian Bockelman
//----------------------------------------------------------------------------------
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
//----------------------------------------------------------------------------------

namespace XrdPfc
{
enum CkSumCheck_e { CSChk_Unknown = -1, CSChk_None = 0, CSChk_Cache = 1, CSChk_Net = 2, CSChk_Both = 3,
                    CSChk_TLS = 4 // Only used during configuration parsing.
};

typedef std::vector<uint32_t> vCkSum_t;
}

// #define XRDPFC_CKSUM_TEST

#endif
