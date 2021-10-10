#ifndef __XRDFILECACHE_PRINT_HH__
#define __XRDFILECACHE_PRINT_HH__
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

namespace XrdPfc
{
class Print {
public:
   //------------------------------------------------------------------------
   //! Constructor.
   //------------------------------------------------------------------------
   Print(XrdOss* oss, char u, bool v, bool j, int i, const char* path);

private:
   XrdOss*     m_oss;      //! file system
   XrdOucEnv   m_env;      //! env used by file system
   int         m_unit_shift; //! bitshift for byte-size fields
   int         m_unit_width; //! width of byte-size fields
   char        m_unit[3];    //! unit for regular printout (not JSON)
   bool        m_verbose;  //! print each block
   bool        m_json;     //! print in json format
   int         m_indent;   //! indent for json dump
   const char* m_ossUser;  //! file system user

   //---------------------------------------------------------------------
   //! Check file ends with *.cinfo suffix
   //---------------------------------------------------------------------
   bool isInfoFile(const char* path);

   //---------------------------------------------------------------------
   //! Print information in meta-data file in json format
   //---------------------------------------------------------------------
   void printFileJson(const std::string& path);

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

#endif
