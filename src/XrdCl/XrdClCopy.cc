//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
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

#include "XrdApps/XrdCpConfig.hh"
#include "XrdApps/XrdCpFile.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClCopyProcess.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClUtils.hh"

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
    ProgressDisplay(): pBytesProcessed(0), pBytesTotal(0), pPrevious(0) {}

    //--------------------------------------------------------------------------
    //! Begin job
    //--------------------------------------------------------------------------
    virtual void BeginJob( uint16_t          jobNum,
                           uint16_t          jobTotal,
                           const XrdCl::URL *source,
                           const XrdCl::URL *destination )
    {
      if( jobTotal > 1 )
      {
        std::cerr << "Job: "    << jobNum << "/" << jobTotal << std::endl;
        std::cerr << "Source: " << source->GetURL() << std::endl;
        std::cerr << "Target: " << destination->GetURL() << std::endl;
      }
      pPrevious = 0;
    }

    //--------------------------------------------------------------------------
    //! End job
    //--------------------------------------------------------------------------
    virtual void EndJob( const XrdCl::XRootDStatus &/*status*/ )
    {
      // make sure the last available status was printed, which may not be
      // the case when processing stdio since we throttle printing and don't
      // know the total size
      JobProgress( pBytesProcessed, pBytesTotal );
      std::cerr << std::endl;
    }

    //--------------------------------------------------------------------------
    //! Job progress
    //--------------------------------------------------------------------------
    virtual void JobProgress( uint64_t bytesProcessed,
                              uint64_t bytesTotal )
    {
      pBytesProcessed = bytesProcessed;
      pBytesTotal     = bytesTotal;

      time_t now = time(0);
      if( (now - pPrevious < 1) && (bytesProcessed != bytesTotal) )
        return;
      pPrevious = now;

      std::string bar;
      int prog = (int)((double)bytesProcessed/bytesTotal*50);
      int proc = (int)((double)bytesProcessed/bytesTotal*100);

      if( bytesTotal )
      {
        prog = (int)((double)bytesProcessed/bytesTotal*50);
        proc = (int)((double)bytesProcessed/bytesTotal*100);
      }
      else
      {
        prog = 50;
        proc = 100;
      }
      bar.append( prog, '=' );
      if( prog < 50 )
        bar += ">";

      std::cerr << "\r";
      std::cerr << "[" << std::setw(3) << std::right << proc << "%]";
      std::cerr << "[" << std::setw(50) << std::left;
      std::cerr << bar;
      std::cerr << "] ";
      std::cerr << "[" << XrdCl::Utils::BytesToString(bytesProcessed) << "/";
      std::cerr << XrdCl::Utils::BytesToString(bytesTotal) << "]   ";
      std::cerr << std::flush;
    }
  private:
    uint64_t pBytesProcessed;
    uint64_t pBytesTotal;
    time_t   pPrevious;
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

  if( config->xRate )
  {
    std::cerr << "Limiting transfer rate is not yet supported" << std::endl;
    return false;
  }

  if( config->nSrcs != 1 )
  {
    std::cerr << "Multiple sources are not yet supported" << std::endl;
    return false;
  }

  if( config->Want( XrdCpConfig::DoServer ) )
  {
    std::cerr << "Running in server mode is not yet supported" << std::endl;
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
// Get a list of files and a list of directories inside a remote directory
//------------------------------------------------------------------------------
XrdCl::XRootDStatus GetDirList( XrdCl::FileSystem        *fs,
                                const XrdCl::URL         &url,
                                std::vector<std::string> *&files,
                                std::vector<std::string> *&directories )
{
  using namespace XrdCl;
  DirectoryList *list;
  XRootDStatus   status;
  Log *log = DefaultEnv::GetLog();

  status = fs->DirList( url.GetPath(), DirListFlags::Stat, list );
  if( !status.IsOK() )
  {
    log->Error( AppMsg, "Error listing directory: %s",
                        status.ToStr().c_str());
    return status;
  }

  for ( DirectoryList::Iterator it = list->Begin(); it != list->End(); ++it )
  {
    if ( (*it)->GetStatInfo()->TestFlags( StatInfo::IsDir ) )
    {
      std::string directory = (*it)->GetName();
      directories->push_back( directory );
    }
    else
    {
      std::string file = (*it)->GetName();
      files->push_back( file );
    }
  }

  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Recursively index all files and directories inside a remote directory
//------------------------------------------------------------------------------
XrdCpFile* IndexRemote( XrdCl::FileSystem *fs,
                        std::string        basePath,
                        uint16_t           dirOffset )
{
  using namespace XrdCl;

  XrdCpFile  *start = new XrdCpFile();
  XrdCpFile  *end   = start;
  XrdCpFile  *current;
  URL         source( basePath );
  int         badUrl;

  std::vector<std::string> *files       = new std::vector<std::string>();
  std::vector<std::string> *directories = new std::vector<std::string>();

  Log *log = DefaultEnv::GetLog();
  log->Debug( AppMsg, "Indexing %s", basePath.c_str() );

  XRootDStatus status = GetDirList( fs, source, files, directories );
  if( !status.IsOK() )
  {
    log->Info( AppMsg, "Failed to get directory listing for %s: %s",
                       source.GetURL().c_str(),
                       status.GetErrorMessage().c_str() );
  }

  std::vector<std::string>::iterator it;
  for( it = files->begin(); it != files->end(); ++it )
  {
    std::string file = basePath + "/" + (*it);
    log->Dump( AppMsg, "Found file %s", file.c_str() );

    current = new XrdCpFile( file.c_str(), badUrl );
    if( badUrl )
    {
      log->Error( AppMsg, "Bad URL: %s", current->Path );
      return 0;
    }

    current->Doff = dirOffset;
    end->Next     = current;
    end           = current;
  }

  for( it = directories->begin(); it != directories->end(); ++it )
  {
    std::string directory = basePath + "/" + (*it);
    log->Dump( AppMsg, "Found directory %s", directory.c_str() );

    end->Next = IndexRemote( fs, directory, dirOffset );
    while( end->Next ) end = end->Next;
  }

  delete files;
  delete directories;
  return start->Next;
}

//------------------------------------------------------------------------------
// Clean up the copy job descriptors
//------------------------------------------------------------------------------
void CleanUpJobs( std::vector<XrdCl::JobDescriptor *> &jobs )
{
  std::vector<XrdCl::JobDescriptor *>::iterator it;
  for( it = jobs.begin(); it != jobs.end(); ++it )
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
    return 254;
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

  ProgressDisplay progressHandler, *progress = 0;
  if( !config.Want(XrdCpConfig::DoNoPbar) )
    progress = &progressHandler;

  bool posc               = false;
  bool thirdParty         = false;
  bool thirdPartyFallBack = true;
  bool force              = false;
  bool coerce             = false;
  bool makedir            = false;

  if( config.Want( XrdCpConfig::DoPosc ) )     posc                = true;
  if( config.Want( XrdCpConfig::DoForce ) )    force               = true;
  if( config.Want( XrdCpConfig::DoCoerce ) )   coerce              = true;
  if( config.Want( XrdCpConfig::DoTpc ) )      thirdParty          = true;
  if( config.Want( XrdCpConfig::DoTpcOnly ) )  thirdPartyFallBack  = false;
  if( config.Want( XrdCpConfig::DoRecurse ) )  makedir             = true;

  std::string checkSumType;
  std::string checkSumPreset;
  bool        checkSumPrint  = false;
  if( config.Want( XrdCpConfig::DoCksum ) )
  {
    std::vector<std::string> ckSumParams;
    Utils::splitString( ckSumParams, config.CksVal, ":" );
    if( ckSumParams.size() > 1 )
    {
      if( ckSumParams[1] == "print" )
        checkSumPrint = true;
      else
        checkSumPreset = ckSumParams[1];
    }
    checkSumType = ckSumParams[0];
  }

  XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
  if( config.nStrm != 1 )
    env->PutInt( "SubStreamsPerChannel", config.nStrm );

  int chunkSize = DefaultCPChunkSize;
  env->GetInt( "CPChunkSize", chunkSize );

  int parallelChunks = DefaultCPParallelChunks;
  env->GetInt( "CPParallelChunks", parallelChunks );

  log->Dump( AppMsg, "Chunk size: %d, parallel chunks %d, streams: %d",
             config.nStrm, chunkSize, parallelChunks );

  //----------------------------------------------------------------------------
  // Build the URLs
  //----------------------------------------------------------------------------
  std::vector<JobDescriptor *> jobs;

  std::string dest;
  if( config.dstFile->Protocol == XrdCpFile::isDir ||
      config.dstFile->Protocol == XrdCpFile::isFile )
    dest = "file://";
  dest += config.dstFile->Path;

  //----------------------------------------------------------------------------
  // We need to check whether our target is a file or a directory:
  // 1) it's a file, so we can accept only one source
  // 2) it's a directory, so:
  //    * we can accept multiple sources
  //    * we need to append the source name
  //----------------------------------------------------------------------------
  bool targetIsDir = false;
  if( config.dstFile->Protocol == XrdCpFile::isDir )
    targetIsDir = true;
  else if( config.dstFile->Protocol == XrdCpFile::isXroot )
  {
    URL target( dest );
    FileSystem fs( target );
    StatInfo *statInfo = 0;
    XRootDStatus st = fs.Stat( target.GetPath(), statInfo );
    if( st.IsOK() && statInfo->TestFlags( StatInfo::IsDir ) )
      targetIsDir = true;

    delete statInfo;
  }

  //----------------------------------------------------------------------------
  // If we have multiple sources and target is not a directory then we cannot
  // proceed
  //----------------------------------------------------------------------------
  if( CountSources(config.srcFile) > 1 && !targetIsDir )
  {
    std::cerr << "Multiple sources were given but target is not a directory.";
    return 255;
  }

  //----------------------------------------------------------------------------
  // If we're doing remote recursive copy, chain all the files (if it's a
  // directory)
  //----------------------------------------------------------------------------
  if( config.DoRecurse && config.srcFile->Protocol == XrdCpFile::isXroot )
  {
    URL          source( config.srcFile->Path );
    FileSystem  *fs       = new FileSystem( source );
    StatInfo    *statInfo = 0;

    XRootDStatus st = fs->Stat( source.GetPath(), statInfo );
    if( st.IsOK() && statInfo->TestFlags( StatInfo::IsDir ) )
    {
      //------------------------------------------------------------------------
      // Recursively index the remote directory
      //------------------------------------------------------------------------
      delete config.srcFile;
      config.srcFile = IndexRemote( fs, source.GetURL(),
                                    source.GetURL().size() );
      if ( !config.srcFile )
      {
        std::cerr << "Error indexing remote directory.";
        return 255;
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
    JobDescriptor *job = new JobDescriptor();
    std::string source = sourceFile->Path;
    if( sourceFile->Protocol == XrdCpFile::isFile )
      source = "file://" + source;

    AppendCGI( source, config.srcOpq );

    log->Dump( AppMsg, "Processing source entry: %s, type %s, target file: %s",
               sourceFile->Path, FileType2String( sourceFile->Protocol ),
               dest.c_str() );

    //--------------------------------------------------------------------------
    // Set up the job
    //--------------------------------------------------------------------------
    std::string target = dest;
    if( targetIsDir )
    {
      target = dest + "/";
      target += (sourceFile->Path+sourceFile->Doff);
    }

    AppendCGI( target, config.dstOpq );

    job->source               = source;
    job->target               = target;
    job->force                = force;
    job->posc                 = posc;
    job->coerce               = coerce;
    job->makedir              = makedir;
    job->thirdParty           = thirdParty;
    job->thirdPartyFallBack   = thirdPartyFallBack;
    job->checkSumType         = checkSumType;
    job->checkSumPreset       = checkSumPreset;
    job->checkSumPrint        = checkSumPrint;
    job->chunkSize            = chunkSize;
    job->parallelChunks       = parallelChunks;
    process.AddJob( job );
    jobs.push_back( job );

    sourceFile = sourceFile->Next;
  }

  //----------------------------------------------------------------------------
  // Prepare and run the copy process
  //----------------------------------------------------------------------------
  XRootDStatus st = process.Prepare();
  if( !st.IsOK() )
  {
    CleanUpJobs( jobs );
    std::cerr << "Prepare: " << st.ToStr() << std::endl;
    return st.GetShellCode();
  }

  st = process.Run( progress );
  if( !st.IsOK() )
  {
    CleanUpJobs( jobs );
    std::cerr << "Run: " << st.ToStr() << std::endl;
    return st.GetShellCode();
  }
  CleanUpJobs( jobs );
  return 0;
}

