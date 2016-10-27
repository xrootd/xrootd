//----------------------------------------------------------------------------------
// Copyright (c) 2014 by Board of Trustees of the Leland Stanford, Jr., University
// Author: Alja Mrak-Tadel
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

#include "XrdOuc/XrdOucEnv.hh"

class XrdOss;
class XrdOssDF;

namespace XrdFileCache
{
class Print {
public:
   //------------------------------------------------------------------------
   //! Constructor.
   //------------------------------------------------------------------------
   Print(XrdOss* oss, bool v, const char* path);

private:
   XrdOss*     m_oss;      //! file system
   XrdOucEnv   m_env;      //! env used by file system
   bool        m_verbose;  //! print each block
   const char* m_ossUser;  //! file system user

   //---------------------------------------------------------------------
   //! Check file ends with *.cinfo suffix
   //---------------------------------------------------------------------
   bool isInfoFile(const char* path);

   //---------------------------------------------------------------------
   //! Print information in meta-data file
   //---------------------------------------------------------------------
   void printFile(const std::string& path);

   //---------------------------------------------------------------------
   //! Print information in meta-data file recursivly
   //---------------------------------------------------------------------
   void printDir(XrdOssDF* iOssDF, const std::string& path);
};
}
