//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------

#include "XrdApps/XrdCpConfig.hh"
#include "XrdApps/XrdCpFile.hh"
#include "XrdCl/XrdClCopyProcess.hh"

#include <iostream>

//------------------------------------------------------------------------------
// Progress notifier
//------------------------------------------------------------------------------
class ProgressDisplay: public XrdCl::CopyProgressHandler
{
  public:
    //--------------------------------------------------------------------------
    //! Begin job
    //--------------------------------------------------------------------------
    virtual void BeginJob( uint16_t          jobNum,
                           uint16_t          jobTotal,
                           const XrdCl::URL */*source*/,
                           const XrdCl::URL */*destination*/ )
    {
      std::cout << "Job: " << jobNum << "/" << jobTotal << std::endl;
    }

    //--------------------------------------------------------------------------
    //! End job
    //--------------------------------------------------------------------------
    virtual void EndJob( const XrdCl::XRootDStatus &status )
    {
      std::cout << std::endl;
      if( status.IsOK() )
        std::cout << "Done." << std::endl;
      else
        std::cout << "Error: " << status.ToStr() << std::endl;
    }

    //--------------------------------------------------------------------------
    //! Job progress
    //--------------------------------------------------------------------------
    virtual void JobProgress( uint64_t bytesProcessed,
                              uint64_t bytesTotal )
    {
      std::cout << "\r" << bytesProcessed << "/" << bytesTotal << std::flush;
    }
};

//------------------------------------------------------------------------------
// Let the show begin
//------------------------------------------------------------------------------
int main( int argc, char **argv )
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Configure the copy command, if it returns then everything went well, ugly
  //----------------------------------------------------------------------------
  XrdCpConfig config( argv[0] );
  config.Config( argc, argv, 0 );

  //----------------------------------------------------------------------------
  // Add the sources and the destination
  //----------------------------------------------------------------------------
  CopyProcess process;
  std::string dest;
  if( config.dstFile->Protocol == XrdCpFile::isDir ||
      config.dstFile->Protocol == XrdCpFile::isFile )
    dest = "file://";
  dest += config.dstFile->Path;
  if( !process.SetDestination( dest ) )
  {
    std::cerr << "Invalid destination path" << std::endl;
    return 2;
  }

  XrdCpFile *sourceFile = config.srcFile;
  while( sourceFile )
  {
    if( !process.AddSource( sourceFile->Path ) )
    {
      std::cerr << "Invalid source path: " << sourceFile->Path << std::endl;
      return 2;
    }
    sourceFile = sourceFile->Next;
  }

  XRootDStatus st = process.Prepare();
  if( !st.IsOK() )
    return st.GetShellCode();

  ProgressDisplay display;
  process.SetProgressHandler( &display );

  st = process.Run();
  if( !st.IsOK() )
    return st.GetShellCode();

  return 0;
}
