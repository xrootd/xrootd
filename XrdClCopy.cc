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
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClLog.hh"

#include <iostream>
#include <iomanip>

//------------------------------------------------------------------------------
// Progress notifier
//------------------------------------------------------------------------------
class ProgressDisplay: public XrdCl::CopyProgressHandler
{
  public:
    //--------------------------------------------------------------------------
    //! Constructor
    //--------------------------------------------------------------------------
    ProgressDisplay():
      pAllJobs( 0 ), pCurrentJob( 0 )
    {
    }

    //--------------------------------------------------------------------------
    //! Begin job
    //--------------------------------------------------------------------------
    virtual void BeginJob( uint16_t          jobNum,
                           uint16_t          jobTotal,
                           const XrdCl::URL */*source*/,
                           const XrdCl::URL */*destination*/ )
    {
      pAllJobs    = jobTotal;
      pCurrentJob = jobNum;
    }

    //--------------------------------------------------------------------------
    //! End job
    //--------------------------------------------------------------------------
    virtual void EndJob( const XrdCl::XRootDStatus &/*status*/ )
    {
      std::cout << std::endl;
    }

    //--------------------------------------------------------------------------
    //! Job progress
    //--------------------------------------------------------------------------
    virtual void JobProgress( uint64_t bytesProcessed,
                              uint64_t bytesTotal )
    {
      std::string bar;
      int prog = (int)((double)bytesProcessed/bytesTotal)*50;
      int proc = (int)((double)bytesProcessed/bytesTotal)*100;
      bar.append( prog, '=' );
      if( prog < 50 )
        bar += ">";

      std::cout << "\r";
      std::cout << "[" << pCurrentJob << "/" << pAllJobs << "] ";
      std::cout << "[" << std::setw(50) << std::left;
      std::cout << bar;
      std::cout << "] ";
      std::cout << "[" << std::setw(3) << std::right << proc << "%]";
      std::cout << std::flush;
    }
  private:
    uint16_t pAllJobs;
    uint16_t pCurrentJob;
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
    std::string source;
    if( sourceFile->Protocol == XrdCpFile::isDir ||
        sourceFile->Protocol == XrdCpFile::isFile )
      source = "file://";
    source += sourceFile->Path;

    if( !process.AddSource( source ) )
    {
      std::cerr << "Invalid source path: " << sourceFile->Path << std::endl;
      return 2;
    }
    process.SetRootOffset( sourceFile->Doff );
    sourceFile = sourceFile->Next;
  }

  //----------------------------------------------------------------------------
  // Set other options
  //----------------------------------------------------------------------------
  ProgressDisplay display;
  if( !config.Want(XrdCpConfig::DoNoPbar) )
    process.SetProgressHandler( &display );

  if( config.Want( XrdCpConfig::DoPosc ) )
    process.SetThirdPartyCopy( true );
  if( config.Want( XrdCpConfig::DoForce ) )
    process.SetForce( true );
  if( config.Want( XrdCpConfig::DoTpc ) )
    process.SetThirdPartyCopy( true );

  if( config.Dlvl )
  {
    Log *log = DefaultEnv::GetLog();
    if( config.Dlvl == 1 ) log->SetLevel( Log::InfoMsg );
    else if( config.Dlvl == 2 ) log->SetLevel( Log::DebugMsg );
    else if( config.Dlvl == 3 ) log->SetLevel( Log::DumpMsg );
  }

  //----------------------------------------------------------------------------
  // Prepare and run the copy process
  //----------------------------------------------------------------------------
  XRootDStatus st = process.Prepare();
  if( !st.IsOK() )
  {
    std::cout << st.ToStr() << std::endl;
    return st.GetShellCode();
  }


  st = process.Run();
  if( !st.IsOK() )
  {
    std::cout << st.ToStr() << std::endl;
    return st.GetShellCode();
  }

  return 0;
}
