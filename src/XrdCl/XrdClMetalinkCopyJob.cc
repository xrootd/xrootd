/*
 * XrdClMetalinkCopy.cpp
 *
 *  Created on: Sep 2, 2015
 *      Author: simonm
 */

#include "XrdCl/XrdClMetalinkCopyJob.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClClassicCopyJob.hh"
#include "XrdCl/XrdClTPFallBackCopyJob.hh"
#include "XrdCl/XrdClUglyHacks.hh"
#include "XrdCl/XrdClFileSystem.hh"

#include "XrdXml/XrdXmlMetaLink.hh"

#include <iostream>
#include <memory>

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

namespace XrdCl
{

  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  MetalinkCopyJob::MetalinkCopyJob( uint16_t      jobId,
                                  PropertyList *jobProperties,
                                  PropertyList *jobResults ):
    CopyJob( jobId, jobProperties, jobResults ), pJobId(jobId), pFileInfos(0), size(0), pMetalinkFile("/tmp/XXXXXX"), pLocalFile( false )
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( UtilityMsg, "Creating a metalink copy job, from %s to %s",
                GetSource().GetURL().c_str(), GetTarget().GetURL().c_str() );
  }

  MetalinkCopyJob::~MetalinkCopyJob()
  {
    if( pFileInfos )
      XrdXmlMetaLink::DeleteAll(pFileInfos, size);

    if( !pMetalinkFile.empty() && !pLocalFile)
      remove(pMetalinkFile.c_str());
  }

  //----------------------------------------------------------------------------
  // Run the copy job
  //----------------------------------------------------------------------------
  XRootDStatus MetalinkCopyJob::Run( CopyProgressHandler *progress )
  {
    XRootDStatus ret;
    // download the metalink file
    ret = DownloadMetalink( progress );
    if( !ret.IsOK() ) return ret;
    // parse the metalink file
    ret = ParseMetalink();
    if( !ret.IsOK() ) return ret;
    // copy files retrieved from metalink
    return CopyFiles( progress );
  }

  XRootDStatus MetalinkCopyJob::DownloadMetalink( CopyProgressHandler *progress )
  {
    Log *log = DefaultEnv::GetLog();
    // check if it is a local file
    if (pSource.GetProtocol() == "file")
    {
      // if yes use that one
      pMetalinkFile = pSource.GetPath();
      pLocalFile = true;
      return XRootDStatus();
    }
    // otherwise download the file with a tmp name
    log->Info( UtilityMsg, "Downloading the metalink file." );
    // generate the tmp name
    mktemp( &*pMetalinkFile.begin() );
    if( pMetalinkFile.empty() )
      return XRootDStatus(stError, errRetry, 0);

    // set copy job parameters
    PropertyList res;
    PropertyList props;
    props.Set( "source",         pSource.GetURL()        );
    props.Set( "target",         pMetalinkFile           );
    props.Set( "force",          false                   );
    props.Set( "posc",           false                   );
    props.Set( "coerce",         false                   );
    props.Set( "makeDir",        false                   );
    props.Set( "dynamicSource",  false                   );
    props.Set( "thirdParty",     "none"                  );
    props.Set( "checkSumMode",   "none"                  );
    props.Set( "checkSumType",   ""                      );
    props.Set( "checkSumPreset", ""                      );
    props.Set( "chunkSize",      DefaultCPChunkSize      );
    props.Set( "parallelChunks", DefaultCPParallelChunks );

    // do a classic copy job
    ClassicCopyJob mlJob( pJobId, &props, &res );
    return mlJob.Run( progress );
  }

  XRootDStatus MetalinkCopyJob::RemoveFile(const URL & url)
  {
    // local file
    if( url.GetProtocol() == "file" )
    {
      int rc = remove( url.GetPath().c_str() );
      if( rc != 0 )
        return XRootDStatus( stError, errOSError, errno, "MetalinkCopyJob: failed to cleanup a replica with a wrong checksum." );
      return XRootDStatus();
    }
    // standard output
    else if( url.GetProtocol() == "stdio" )
      return XRootDStatus(); // nothing to do ...
    // remote file
    else
    {
      FileSystem fs( url );
      return fs.Rm( url.GetPath() );
    }
  }

  XRootDStatus MetalinkCopyJob::ParseMetalink()
  {
    XrdXmlMetaLink metalink;
    pFileInfos = metalink.ConvertAll( pMetalinkFile.c_str(), size, 0 );

    if( !pFileInfos )
    {
      int ecode;
      const char * etxt = metalink.GetStatus( ecode );
      Log *log = DefaultEnv::GetLog();
      log->Error( UtilityMsg, "Failed to parse the metalink file: %s (error code: %d)", etxt, ecode );
      return XRootDStatus( stError, errDataError );
    }

    if( size != 1 )
    {
      Log *log = DefaultEnv::GetLog();
      log->Error( UtilityMsg, "Expected only one file per metalink." );
      return XRootDStatus( stError, errDataError );
    }

    return XRootDStatus();
  }

  XRootDStatus MetalinkCopyJob::CopyFiles( CopyProgressHandler *progress )
  {
    Log *log = DefaultEnv::GetLog();
    // get the metalink filename
    std::string metalink = GetSource().GetURL();
    size_t pos = metalink.rfind( '/' );
    metalink = metalink.substr( pos + 1 );
    // the target file name specified in metalink
    std::string sufix =  pFileInfos[0]->GetTargetName();
    // get the target
    std::string trg = GetTarget().GetURL(), target, tmp;
    // if trg is empty the destination was not specified and an absolute path is expected in the metalink file
    if( GetTarget().GetPath().empty() )
    {
      if( sufix[0] != '/' )
      {
        log->Error( UtilityMsg, "MetalinkCopyJob: metalink target is not an absolute path and no destination has been specified." );
        XRootDStatus st = XRootDStatus( stError, errInvalidArgs );
        return st;
      }
      target = trg + sufix;
    }
    else
    {
      // check if a directory is the destination (make sure the metalink file name is not used as destination file name)
      pos = trg.rfind( '/' );
      tmp = trg.substr( pos + 1 );
      // check if that's a path to directory or a file
      // (if it's a directory we will find the metalink name appended to the end)
      // (this holds for remote files)
      bool isDir = ( tmp == metalink );
      // if the metalink name was appended remove it
      target = isDir ? trg.substr( 0, pos + 1 ) : trg;
      // set target filename
      if( isDir )
      {
        if( sufix[0] == '/' )
        {
          log->Error( UtilityMsg, "MetalinkCopyJob: metalink target is an absolute path and a destination has been specified." );
          XRootDStatus st = XRootDStatus( stError, errInvalidArgs );
          return st;
        }
        target += "/" + sufix;
      }
    }
    URL trgUrl = target;
    if( !trgUrl.IsValid() )
      return XRootDStatus( stError, errInvalidArgs, 0, "invalid target" );

    pProperties->Set("target", target);
    // check if it is a third party copy
    bool tpc = false;
    pProperties->Get("thirdParty", tmp);
    tpc = tmp != "none";
    // transfer status
    XRootDStatus ret;
    // take care of checksumming
    // check what the user specified
    std::string usrmode, usrtype, usrval;
    pProperties->Get( "checkSumMode",   usrmode );
    pProperties->Get( "checkSumType",   usrtype );
    if( usrtype == "adler32" ) usrtype = "a32";
    pProperties->Get( "checkSumPreset", usrval  );
    // if the value was defined by user we leave it as it is
    if( usrval.empty() )
    {
      // check if the value for the checksum type was given in the metalink
      const char *chvalue = 0, *chtype = 0;
      do
      {
        chtype = pFileInfos[0]->GetDigest( chvalue );
      }
      while( !(chtype == 0 || usrtype == chtype) );
      // if the checksum value has been defined in metalink use it
      if( chtype )
      {
        log->Info( UtilityMsg, "Using checksum value specified in metalink: %s:%s", chtype, chvalue );
        pProperties->Set( "checkSumPreset", chvalue   );
      }
    }
    // try the different replicas
    bool cleanup = false;
    const char * url = 0;
    char cntry[3];
    int prty;
    while( (url = pFileInfos[0]->GetUrl(cntry, &prty)) )
    {
      log->Info( UtilityMsg, "Try URL: %s", url );

      if( cleanup )
      {
        ret = RemoveFile( target );
        if( !ret.IsOK() )
        {
          log->Error( UtilityMsg, "MetalinkCopyJob: could not cleanup a replica with a wrong checksum." );
          return ret;
        }
        cleanup = false;
      }

      URL srcUrl = std::string( url );
      if( !srcUrl.IsValid() )
      {
        log->Error( UtilityMsg, "MetalinkCopyJob: invalid source URL." );
        ret = XRootDStatus( stError, errInvalidArgs, 0, "invalid source" );
        continue;
      }

      // create the copy job
      pProperties->Set("source", url);
      XRDCL_SMART_PTR_T<CopyJob> job;
      if( tpc ) job.reset( new TPFallBackCopyJob( pJobId, pProperties, pResults ) );
      else job.reset( new ClassicCopyJob( pJobId, pProperties, pResults ) );
      ret = job->Run( progress );
      // if the transfer was successful we are done
      if( ret.IsOK() ) break;
      else
      {
        // if this is a OS error (e.g. file already exists)
        // trying another replica doesn't make sense
        if( ret.code == errOSError ) return ret;
        // print the error msg for the given replica
        log->Error( UtilityMsg, "%s", ret.ToStr().c_str() );
        // in case there are some left overs after previous transfer due to
        // checksum failure set the force flag so the file can be overwritten
        if( ret.code == errCheckSumError )
        {
          cleanup = true;
        }
      }
    }

    return ret;
  }

}
