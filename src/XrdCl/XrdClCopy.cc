//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
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

#include "XrdApps/XrdCpConfig.hh"
#include "XrdApps/XrdCpFile.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClCopyProcess.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClDlgEnv.hh"
#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysPthread.hh"

#include <cstdio>
#include <iostream>
#include <iomanip>
#include <limits>

//------------------------------------------------------------------------------
// Progress notifier
//------------------------------------------------------------------------------
class ProgressDisplay: public XrdCl::CopyProgressHandler
{
  public:
    //--------------------------------------------------------------------------
    //! Constructor
    //--------------------------------------------------------------------------
    ProgressDisplay(): pPrevious(0), pPrintProgressBar(true),
      pPrintSourceCheckSum(false), pPrintTargetCheckSum(false),
      pPrintAdditionalCheckSum(false)
    {}

    //--------------------------------------------------------------------------
    //! Begin job
    //--------------------------------------------------------------------------
    virtual void BeginJob( uint16_t          jobNum,
                           uint16_t          jobTotal,
                           const XrdCl::URL *source,
                           const XrdCl::URL *destination )
    {
      XrdSysMutexHelper scopedLock( pMutex );
      if( pPrintProgressBar )
      {
        if( jobTotal > 1 )
        {
          std::cerr << "Job: "    << jobNum << "/" << jobTotal << std::endl;
          std::cerr << "Source: " << source->GetURL() << std::endl;
          std::cerr << "Target: " << destination->GetURL() << std::endl;
        }
      }
      pPrevious = 0;

      JobData d;
      d.started = time(0);
      d.source  = source;
      d.target  = destination;
      pOngoingJobs[jobNum] = d;
    }

    //--------------------------------------------------------------------------
    //! End job
    //--------------------------------------------------------------------------
    virtual void EndJob( uint16_t jobNum, const XrdCl::PropertyList *results )
    {
      XrdSysMutexHelper scopedLock( pMutex );

      std::map<uint16_t, JobData>::iterator it = pOngoingJobs.find( jobNum );
      if( it == pOngoingJobs.end() )
        return;

      JobData &d = it->second;

      // make sure the last available status was printed, which may not be
      // the case when processing stdio since we throttle printing and don't
      // know the total size
      JobProgress( jobNum, d.bytesProcessed, d.bytesTotal );

      if( pPrintProgressBar )
      {
        if( pOngoingJobs.size() > 1 )
          std::cerr << "\r" << std::string(70, ' ') << "\r";
        else
          std::cerr << std::endl;
      }

      XrdCl::XRootDStatus st;
      results->Get( "status", st );
      if( !st.IsOK() )
      {
        pOngoingJobs.erase(it);
        return;
      }

      std::string checkSum;
      uint64_t    size;
      results->Get( "size", size );
      if( pPrintSourceCheckSum )
      {
        results->Get( "sourceCheckSum", checkSum );
        PrintCheckSum( d.source, checkSum, size );
      }

      if( pPrintTargetCheckSum )
      {
        results->Get( "targetCheckSum", checkSum );
        PrintCheckSum( d.target, checkSum, size );
      }

      if( pPrintAdditionalCheckSum )
      {
        std::vector<std::string> addcksums;
        results->Get( "additionalCkeckSum", addcksums );
        for( auto &cks : addcksums )
          PrintCheckSum( d.source, cks, size );
      }

      pOngoingJobs.erase(it);
    }

    //--------------------------------------------------------------------------
    //! Get progress bar
    //--------------------------------------------------------------------------
    std::string GetProgressBar( time_t now )
    {
      JobData &d = pOngoingJobs.begin()->second;

      uint64_t speed = 0;
      if( now-d.started )
        speed = d.bytesProcessed/(now-d.started);
      else
        speed = d.bytesProcessed;

      std::string bar;
      int prog = 0;
      int proc = 0;

      if( d.bytesTotal )
      {
        prog = (int)((double)d.bytesProcessed/d.bytesTotal*50);
        proc = (int)((double)d.bytesProcessed/d.bytesTotal*100);
      }
      else
      {
        prog = 50;
        proc = 100;
      }
      bar.append( prog, '=' );
      if( prog < 50 )
        bar += ">";

      std::ostringstream o;
      o << "[" << XrdCl::Utils::BytesToString(d.bytesProcessed) << "B/";
      o << XrdCl::Utils::BytesToString(d.bytesTotal) << "B]";
      o << "[" << std::setw(3) << std::right << proc << "%]";
      o << "[" << std::setw(50) << std::left;
      o << bar;
      o << "]";
      o << "[" << XrdCl::Utils::BytesToString(speed) << "B/s]  ";
      return o.str();
    }

    //--------------------------------------------------------------------------
    //! Get sumary bar
    //--------------------------------------------------------------------------
    std::string GetSummaryBar( time_t now )
    {
      std::map<uint16_t, JobData>::iterator it;
      std::ostringstream o;

      for( it = pOngoingJobs.begin(); it != pOngoingJobs.end(); ++it )
      {
        JobData  &d      = it->second;
        uint16_t  jobNum = it->first;

        uint64_t speed = 0;
        if( now-d.started )
          speed = d.bytesProcessed/(now-d.started);

        int proc = 0;
        if( d.bytesTotal )
          proc = (int)((double)d.bytesProcessed/d.bytesTotal*100);
        else
          proc = 100;

        o << "[#" << jobNum << ": ";
        o << proc << "% ";
        o << XrdCl::Utils::BytesToString(speed) << "B/s] ";
      }
      o << "        ";
      return o.str();
    }

    //--------------------------------------------------------------------------
    //! Job progress
    //--------------------------------------------------------------------------
    virtual void JobProgress( uint16_t jobNum,
                              uint64_t bytesProcessed,
                              uint64_t bytesTotal )
    {
      XrdSysMutexHelper scopedLock( pMutex );

      if( pPrintProgressBar )
      {
        time_t now = time(0);
        if( (now - pPrevious < 1) && (bytesProcessed != bytesTotal) )
          return;
        pPrevious = now;

        std::map<uint16_t, JobData>::iterator it = pOngoingJobs.find( jobNum );
        if( it == pOngoingJobs.end() )
          return;

        JobData &d = it->second;

        d.bytesProcessed = bytesProcessed;
        d.bytesTotal     = bytesTotal;

        std::string progress;
        if( pOngoingJobs.size() == 1 )
          progress = GetProgressBar( now );
        else
          progress = GetSummaryBar( now );

        std::cerr << "\r" << progress << std::flush;
      }
    }

    //--------------------------------------------------------------------------
    //! Print the checksum
    //--------------------------------------------------------------------------
    void PrintCheckSum( const XrdCl::URL  *url,
                        const std::string &checkSum,
                        uint64_t size )
    {
      if( checkSum.empty() )
        return;
      std::string::size_type i = checkSum.find( ':' );
      std::cerr << checkSum.substr( 0, i+1 ) << " ";
      std::cerr << checkSum.substr( i+1, checkSum.length()-i ) << " ";

      if( url->IsLocalFile() )
        std::cerr << url->GetPath() << " ";
      else
      {
        std::cerr << url->GetProtocol() << "://" << url->GetHostId();
        std::cerr << url->GetPath() << " ";
      }

      std::cerr << size;
      std::cerr << std::endl;
    }

    //--------------------------------------------------------------------------
    // Printing flags
    //--------------------------------------------------------------------------
    void PrintProgressBar( bool print )    { pPrintProgressBar    = print; }
    void PrintSourceCheckSum( bool print ) { pPrintSourceCheckSum = print; }
    void PrintTargetCheckSum( bool print ) { pPrintTargetCheckSum = print; }
    void PrintAdditionalCheckSum( bool print ) { pPrintAdditionalCheckSum = print; }

  private:
    struct JobData
    {
      JobData(): bytesProcessed(0), bytesTotal(0),
        started(0), source(0), target(0) {}
      uint64_t          bytesProcessed;
      uint64_t          bytesTotal;
      time_t            started;
      const XrdCl::URL *source;
      const XrdCl::URL *target;
    };

    time_t                      pPrevious;
    bool                        pPrintProgressBar;
    bool                        pPrintSourceCheckSum;
    bool                        pPrintTargetCheckSum;
    bool                        pPrintAdditionalCheckSum;
    std::map<uint16_t, JobData> pOngoingJobs;
    XrdSysRecMutex              pMutex;
};

//------------------------------------------------------------------------------
// Check if we support all the specified user options
//------------------------------------------------------------------------------
bool AllOptionsSupported( XrdCpConfig *config )
{
  if( config->pHost )
  {
    std::cerr << "SOCKS Proxies are not yet supported" << std::endl;
    return false;
  }

  return true;
}

//------------------------------------------------------------------------------
// Append extra cgi info to existing URL
//------------------------------------------------------------------------------
void AppendCGI( std::string &url, const char *newCGI )
{
  if( !newCGI || !(*newCGI) )
    return;

  if( *newCGI == '&' )
    ++newCGI;

  if( url.find( '?' ) == std::string::npos )
    url += "?";

  if( url.find( '&' ) == std::string::npos )
    url += "&";

  url += newCGI;
}

//------------------------------------------------------------------------------
// Process commandline environment settings
//------------------------------------------------------------------------------
void ProcessCommandLineEnv( XrdCpConfig *config )
{
  XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();

  XrdCpConfig::defVar *cursor = config->intDefs;
  while( cursor )
  {
    env->PutInt( cursor->vName, cursor->intVal );
    cursor = cursor->Next;
  }

  cursor = config->strDefs;
  while( cursor )
  {
    env->PutString( cursor->vName, cursor->strVal );
    cursor = cursor->Next;
  }
}

//------------------------------------------------------------------------------
// Translate file type to a string for diagnostics purposes
//------------------------------------------------------------------------------
const char *FileType2String( XrdCpFile::PType type )
{
  switch( type )
  {
    case XrdCpFile::isDir:   return "directory";
    case XrdCpFile::isFile:  return "local file";
    case XrdCpFile::isXroot: return "xroot";
    case XrdCpFile::isHttp:  return "http";
    case XrdCpFile::isHttps: return "https";
    case XrdCpFile::isStdIO: return "stdio";
    default: return "other";
  };
}

//------------------------------------------------------------------------------
// Count the sources
//------------------------------------------------------------------------------
uint32_t CountSources( XrdCpFile *file )
{
  uint32_t count;
  for( count = 0; file; file = file->Next, ++count ) {};
  return count;
}

//------------------------------------------------------------------------------
// Adjust file information for the cases when XrdCpConfig cannot do this
//------------------------------------------------------------------------------
void AdjustFileInfo( XrdCpFile *file )
{
  //----------------------------------------------------------------------------
  // If the file is url and the directory offset is not set we set it
  // to the last slash
  //----------------------------------------------------------------------------
  if( file->Doff == 0 )
  {
    char *slash = file->Path;
    for( ; *slash; ++slash ) {};
    for( ; *slash != '/' && slash > file->Path; --slash ) {};
    file->Doff = slash - file->Path;
  }
};

//------------------------------------------------------------------------------
// Recursively index all files and directories inside a remote directory
//------------------------------------------------------------------------------
XrdCpFile *IndexRemote( XrdCl::FileSystem *fs,
                        std::string        basePath,
                        uint16_t           dirOffset )
{
  using namespace XrdCl;

  Log *log = DefaultEnv::GetLog();
  log->Debug( AppMsg, "Indexing %s", basePath.c_str() );

  DirectoryList *dirList = 0;
  XRootDStatus st = fs->DirList( URL( basePath ).GetPath(), DirListFlags::Recursive
      | DirListFlags::Locate | DirListFlags::Merge, dirList );
  if( !st.IsOK() )
  {
    log->Info( AppMsg, "Failed to get directory listing for %s: %s",
                       basePath.c_str(),
                       st.GetErrorMessage().c_str() );
    return 0;
  }

  XrdCpFile start, *current = 0;
  XrdCpFile *end   = &start;
  int       badUrl = 0;
  for( auto itr = dirList->Begin(); itr != dirList->End(); ++itr )
  {
    DirectoryList::ListEntry *e = *itr;
    if( e->GetStatInfo()->TestFlags( StatInfo::IsDir ) )
      continue;
    std::string path = basePath + '/' + e->GetName();
    current = new XrdCpFile( path.c_str(), badUrl );
    if( badUrl )
    {
      delete current;
      log->Error( AppMsg, "Bad URL: %s", current->Path );
      return 0;
    }

    current->Doff = dirOffset;
    end->Next     = current;
    end           = current;
  }

  delete dirList;

  return start.Next;
}

//------------------------------------------------------------------------------
// Clean up the copy job descriptors
//------------------------------------------------------------------------------
void CleanUpResults( std::vector<XrdCl::PropertyList *> &results )
{
  std::vector<XrdCl::PropertyList *>::iterator it;
  for( it = results.begin(); it != results.end(); ++it )
    delete *it;
}

//--------------------------------------------------------------------------
// Let the show begin
//------------------------------------------------------------------------------
int main( int argc, char **argv )
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Configure the copy command, if it returns then everything went well, ugly
  //----------------------------------------------------------------------------
  XrdCpConfig config( argv[0] );
  config.Config( argc, argv, XrdCpConfig::optRmtRec );
  if( !AllOptionsSupported( &config ) )
    return 50; // generic error
  ProcessCommandLineEnv( &config );

  //----------------------------------------------------------------------------
  // Set options
  //----------------------------------------------------------------------------
  CopyProcess process;
  Log *log = DefaultEnv::GetLog();
  if( config.Dlvl )
  {
    if( config.Dlvl == 1 ) log->SetLevel( Log::InfoMsg );
    else if( config.Dlvl == 2 ) log->SetLevel( Log::DebugMsg );
    else if( config.Dlvl == 3 ) log->SetLevel( Log::DumpMsg );
  }

  ProgressDisplay progress;
  if( config.Want(XrdCpConfig::DoNoPbar) )
    progress.PrintProgressBar( false );

  bool         posc          = false;
  bool         force         = false;
  bool         coerce        = false;
  bool         makedir       = false;
  bool         dynSrc        = false;
  bool         delegate      = false;
  bool         preserveXAttr = false;
  bool         rmOnBadCksum  = false;
  bool         continue_     = false;
  bool         recurse       = false;
  bool         zipappend     = false;
  std::string thirdParty = "none";

  if( config.Want( XrdCpConfig::DoPosc ) )      posc       = true;
  if( config.Want( XrdCpConfig::DoForce ) )     force      = true;
  if( config.Want( XrdCpConfig::DoCoerce ) )    coerce     = true;
  if( config.Want( XrdCpConfig::DoTpc ) )       thirdParty = "first";
  if( config.Want( XrdCpConfig::DoTpcOnly ) )   thirdParty = "only";
  if( config.Want( XrdCpConfig::DoZipAppend ) ) zipappend  = true;
  if( config.Want( XrdCpConfig::DoTpcDlgt ) )
  {
    // the env var is being set already here (we are issuing a stat
    // inhere and we need the env var when we are establishing the
    // connection and authenticating), but we are also setting a delegate
    // parameter for CopyJob so it can be used on its own.
    DlgEnv::Instance().Enable();
    delegate = true;
  }
  else
    DlgEnv::Instance().Disable();

  if( config.Want( XrdCpConfig::DoRecurse ) )
  {
    makedir = true;
    recurse = true;
  }
  if( config.Want( XrdCpConfig::DoPath    ) )      makedir       = true;
  if( config.Want( XrdCpConfig::DoDynaSrc ) )      dynSrc        = true;
  if( config.Want( XrdCpConfig::DoXAttr ) )        preserveXAttr = true;
  if( config.Want( XrdCpConfig::DoRmOnBadCksum ) ) rmOnBadCksum  = true;
  if( config.Want( XrdCpConfig::DoContinue ) )     continue_     = true;

  if( force && continue_ )
  {
    std::cerr << "Invalid argument combination: continue + force." << std::endl;
    return 50;
  }

  //----------------------------------------------------------------------------
  // Checksums
  //----------------------------------------------------------------------------
  std::string checkSumType;
  std::string checkSumPreset;
  std::string checkSumMode  = "none";
  if( config.Want( XrdCpConfig::DoCksum ) )
  {
    checkSumMode = "end2end";
    std::vector<std::string> ckSumParams;
    Utils::splitString( ckSumParams, config.CksVal, ":" );
    if( ckSumParams.size() > 1 )
    {
      if( ckSumParams[1] == "print" )
      {
        checkSumMode = "target";
        progress.PrintTargetCheckSum( true );
      }
      else
        checkSumPreset = ckSumParams[1];
    }
    checkSumType = ckSumParams[0];
  }

  if( config.Want( XrdCpConfig::DoCksrc ) )
  {
    checkSumMode = "source";
    std::vector<std::string> ckSumParams;
    Utils::splitString( ckSumParams, config.CksVal, ":" );
    if( ckSumParams.size() == 2 )
    {
      checkSumMode = "source";
      checkSumType = ckSumParams[0];
      progress.PrintSourceCheckSum( true );
    }
    else
    {
      std::cerr << "Invalid parameter: " << config.CksVal << std::endl;
      return 50; // generic error
    }
  }

  if( !config.AddCksVal.empty() )
    progress.PrintAdditionalCheckSum( true );

  //----------------------------------------------------------------------------
  // ZIP archive
  //----------------------------------------------------------------------------
  std::string zipFile;
  bool        zip = false;
  if( config.Want( XrdCpConfig::DoZip ) )
  {
    zipFile = config.zipFile;
    zip = true;
  }

  //----------------------------------------------------------------------------
  // Extreme Copy
  //----------------------------------------------------------------------------
  int nbSources = 0;
  bool xcp      = false;
  if( config.Want( XrdCpConfig::DoSources ) )
  {
    nbSources = config.nSrcs;
    xcp       = true;
  }

  //----------------------------------------------------------------------------
  // Environment settings
  //----------------------------------------------------------------------------
  XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
  if( config.nStrm != 0 )
    env->PutInt( "SubStreamsPerChannel", config.nStrm + 1 /*stands for the control stream*/ );

  if( config.Retry != -1 )
  {
    env->PutInt( "CpRetry", config.Retry );
    env->PutString( "CpRetryPolicy", config.RetryPolicy );
  }

  if( config.Want( XrdCpConfig::DoNoTlsOK ) )
    env->PutInt( "NoTlsOK", 1 );

  if( config.Want( XrdCpConfig::DoTlsNoData ) )
    env->PutInt( "TlsNoData", 1 );

  if( config.Want( XrdCpConfig::DoTlsMLF ) )
    env->PutInt( "TlsMetalink", 1 );

  if( config.Want( XrdCpConfig::DoZipMtlnCksum ) )
    env->PutInt( "ZipMtlnCksum", 1 );

  int chunkSize = DefaultCPChunkSize;
  env->GetInt( "CPChunkSize", chunkSize );

  int blockSize = DefaultXCpBlockSize;
  env->GetInt( "XCpBlockSize", blockSize );

  int parallelChunks = DefaultCPParallelChunks;
  env->GetInt( "CPParallelChunks", parallelChunks );
  if( parallelChunks < 1 ||
      parallelChunks > std::numeric_limits<uint8_t>::max() )
  {
    std::cerr << "Can only handle between 1 and ";
    std::cerr << (int)std::numeric_limits<uint8_t>::max();
    std::cerr << " chunks in parallel. You asked for " << parallelChunks;
    std::cerr << "." << std::endl;
    return 50; // generic error
  }

  if( !preserveXAttr )
  {
    int val = DefaultPreserveXAttrs;
    env->GetInt( "PreserveXAttrs", val );
    if( val ) preserveXAttr = true;
  }

  log->Dump( AppMsg, "Chunk size: %d, parallel chunks %d, streams: %d",
             chunkSize, parallelChunks, config.nStrm + 1 );

  //----------------------------------------------------------------------------
  // Build the URLs
  //----------------------------------------------------------------------------
  std::vector<XrdCl::PropertyList*> resultVect;

  std::string dest;
  if( config.dstFile->Protocol == XrdCpFile::isDir ||
      config.dstFile->Protocol == XrdCpFile::isFile )
  {
    dest = "file://";

    // if it is not an absolute path append cwd
    if( config.dstFile->Path[0] != '/' )
    {
      char buf[FILENAME_MAX];
      char *cwd = getcwd( buf, FILENAME_MAX );
      if( !cwd )
      {
        XRootDStatus st( stError, XProtocol::mapError( errno ), errno, XrdSysE2T( errno ) );
        std::cerr <<  st.GetErrorMessage() << std::endl;
        return st.GetShellCode();
      }
      dest += cwd;
      dest += '/';
    }
  }
  dest += config.dstFile->Path;

  //----------------------------------------------------------------------------
  // We need to check whether our target is a file or a directory:
  // 1) it's a file, so we can accept only one source
  // 2) it's a directory, so:
  //    * we can accept multiple sources
  //    * we need to append the source name
  //----------------------------------------------------------------------------
  bool targetIsDir  = false;
  bool targetExists = false;
  if( config.dstFile->Protocol == XrdCpFile::isDir )
    targetIsDir = true;
  else if( config.dstFile->Protocol == XrdCpFile::isXroot ||
           config.dstFile->Protocol == XrdCpFile::isXroots )
  {
    URL target( dest );
    FileSystem fs( target );
    StatInfo *statInfo = 0;
    XRootDStatus st = fs.Stat( target.GetPath(), statInfo );
    if( st.IsOK() )
    {
      if( statInfo->TestFlags( StatInfo::IsDir ) )
        targetIsDir = true;
      targetExists = true;
    }
    else if( st.errNo == kXR_NotFound && config.Want( XrdCpConfig::DoPath ) )
    {
      int n = strlen(config.dstFile->Path);
      if( config.dstFile->Path[n-1] == '/' )
        targetIsDir = true;
    }

    delete statInfo;
  }

  if( !targetIsDir && targetExists && !force && !recurse && !zipappend )
  {
    XRootDStatus st( stError, errInvalidOp, EEXIST );
    // Unable to create /tmp/test.txt; file exists
    log->Error( AppMsg, "%s (destination)", st.ToString().c_str() );
    std::cerr << "Run: " << st.ToStr() << std::endl;
    return st.GetShellCode();
  }

  //----------------------------------------------------------------------------
  // If we have multiple sources and target is neither a directory nor stdout
  // then we cannot proceed
  //----------------------------------------------------------------------------
  if( CountSources(config.srcFile) > 1 && !targetIsDir &&
      config.dstFile->Protocol != XrdCpFile::isStdIO )
  {
    std::cerr << "Multiple sources were given but target is not a directory.";
    std::cerr << std::endl;
    return 50; // generic error
  }

  //----------------------------------------------------------------------------
  // If we're doing remote recursive copy, chain all the files (if it's a
  // directory)
  //----------------------------------------------------------------------------
  bool remoteSrcIsDir = false;
  if( config.Want( XrdCpConfig::DoRecurse ) &&
      config.srcFile->Protocol == XrdCpFile::isXroot )
  {
    URL          source( config.srcFile->Path );
    FileSystem  *fs       = new FileSystem( source );
    StatInfo    *statInfo = 0;

    XRootDStatus st = fs->Stat( source.GetPath(), statInfo );
    if( st.IsOK() && statInfo->TestFlags( StatInfo::IsDir ) )
    {
      remoteSrcIsDir = true;
      //------------------------------------------------------------------------
      // Recursively index the remote directory
      //------------------------------------------------------------------------
      delete config.srcFile;
      std::string url = source.GetURL();
      config.srcFile = IndexRemote( fs, url, url.size() );
      if ( !config.srcFile )
      {
        std::cerr << "Error indexing remote directory.";
        return 50; // generic error
      }
    }

    delete fs;
    delete statInfo;
  }

  XrdCpFile *sourceFile = config.srcFile;
  //----------------------------------------------------------------------------
  // Process the sources
  //----------------------------------------------------------------------------
  while( sourceFile )
  {
    AdjustFileInfo( sourceFile );

    //--------------------------------------------------------------------------
    // Create a job for every source
    //--------------------------------------------------------------------------
    PropertyList  properties;
    PropertyList *results = new PropertyList;
    std::string source = sourceFile->Path;
    if( sourceFile->Protocol == XrdCpFile::isFile )
    {
      // make sure it is an absolute path
      if( source[0] == '/' )
        source = "file://" + source;
      else
      {
        char buf[FILENAME_MAX];
        char *cwd = getcwd( buf, FILENAME_MAX );
        if( !cwd )
        {
          XRootDStatus st( stError, XProtocol::mapError( errno ), errno, XrdSysE2T( errno ) );
          std::cerr <<  st.GetErrorMessage() << std::endl;
          return st.GetShellCode();
        }
        source = "file://" + std::string( cwd ) + '/' + source;
      }
    }

    AppendCGI( source, config.srcOpq );

    log->Dump( AppMsg, "Processing source entry: %s, type %s, target file: %s",
               sourceFile->Path, FileType2String( sourceFile->Protocol ),
               dest.c_str() );

    //--------------------------------------------------------------------------
    // Set up the job
    //--------------------------------------------------------------------------
    std::string target = dest;


    bool srcIsDir = false;
    // if this is local file, for a directory Dlen + Doff will overlap with path size
    if( strncmp( sourceFile->ProtName, "file", 4 ) == 0 )
      srcIsDir = std::string( sourceFile->Path ).size() == size_t( sourceFile->Doff + sourceFile->Dlen );
    // otherwise we are handling a remote file
    else
      srcIsDir = remoteSrcIsDir;
    // if this is a recursive copy make sure we preserve the directory structure
    if( config.Want( XrdCpConfig::DoRecurse ) && srcIsDir )
    {
      // get the source directory
      std::string srcDir( sourceFile->Path, sourceFile->Doff );
      // remove the trailing slash
      if( srcDir[srcDir.size() - 1] == '/' )
        srcDir = srcDir.substr( 0, srcDir.size() - 1 );
      size_t diroff = srcDir.rfind( '/' );
      // if there is no '/' it means a directory name has been given as relative path
      if( diroff == std::string::npos ) diroff = 0;
      target += '/';
      target += sourceFile->Path + diroff;
      // remove the filename from destination path as it will be appended later anyway
      target = target.substr( 0 , target.rfind('/') );
    }
    AppendCGI( target, config.dstOpq );

    properties.Set( "source",          source                 );
    properties.Set( "target",          target                 );
    properties.Set( "force",           force                  );
    properties.Set( "posc",            posc                   );
    properties.Set( "coerce",          coerce                 );
    properties.Set( "makeDir",         makedir                );
    properties.Set( "dynamicSource",   dynSrc                 );
    properties.Set( "thirdParty",      thirdParty             );
    properties.Set( "checkSumMode",    checkSumMode           );
    properties.Set( "checkSumType",    checkSumType           );
    properties.Set( "checkSumPreset",  checkSumPreset         );
    properties.Set( "chunkSize",       chunkSize              );
    properties.Set( "parallelChunks",  parallelChunks         );
    properties.Set( "zipArchive",      zip                    );
    properties.Set( "xcp",             xcp                    );
    properties.Set( "xcpBlockSize",    blockSize              );
    properties.Set( "delegate",        delegate               );
    properties.Set( "targetIsDir",     targetIsDir            );
    properties.Set( "preserveXAttr",   preserveXAttr          );
    properties.Set( "xrate",           config.xRate           );
    properties.Set( "xrateThreashold", config.xRateThreashold );
    properties.Set( "rmOnBadCksum",    rmOnBadCksum           );
    properties.Set( "continue",        continue_              );
    properties.Set( "zipAppend",       zipappend              );
    properties.Set( "addcksums",       config.AddCksVal );

    if( zip )
      properties.Set( "zipSource",     zipFile                );

    if( xcp )
      properties.Set( "nbXcpSources",  nbSources              );


    XRootDStatus st = process.AddJob( properties, results );
    if( !st.IsOK() )
    {
      std::cerr << "AddJob " << source << " -> " << target << ": ";
      std::cerr << st.ToStr() << std::endl;
    }
    resultVect.push_back( results );
    sourceFile = sourceFile->Next;
  }

  //----------------------------------------------------------------------------
  // Configure the copy process
  //----------------------------------------------------------------------------
  PropertyList processConfig;
  processConfig.Set( "jobType", "configuration" );
  processConfig.Set( "parallel", config.Parallel );
  process.AddJob( processConfig, 0 );

  //----------------------------------------------------------------------------
  // Prepare and run the copy process
  //----------------------------------------------------------------------------
  XRootDStatus st = process.Prepare();
  if( !st.IsOK() )
  {
    CleanUpResults( resultVect );
    std::cerr << "Prepare: " << st.ToStr() << std::endl;
    return st.GetShellCode();
  }

  st = process.Run( &progress );
  if( !st.IsOK() )
  {
    if( resultVect.size() == 1 )
      std::cerr << "Run: " << st.ToStr() << std::endl;
    else
    {
      std::vector<XrdCl::PropertyList*>::iterator it;
      uint16_t i = 1;
      uint16_t jobsRun = 0;
      uint16_t errors  = 0;
      for( it = resultVect.begin(); it != resultVect.end(); ++it, ++i )
      {
        if( !(*it)->HasProperty( "status" ) )
          continue;

        XRootDStatus st = (*it)->Get<XRootDStatus>("status");
        if( !st.IsOK() )
        {
          std::cerr << "Job #" << i << ": " << st.ToStr();
          ++errors;
        }
        ++jobsRun;
      }
      std::cerr << "Jobs total: " << resultVect.size();
      std::cerr << ", run: " << jobsRun;
      std::cerr << ", errors: " << errors << std::endl;
    }
    CleanUpResults( resultVect );
    return st.GetShellCode();
  }
  CleanUpResults( resultVect );

  return 0;
}

