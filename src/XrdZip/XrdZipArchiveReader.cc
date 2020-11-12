/*
 * XrdZipArchiveReader.cc
 *
 *  Created on: 10 Nov 2020
 *      Author: simonm
 */

#include "XrdZip/XrdZipArchiveReader.hh"
#include "XrdZip/XrdZipZIP64EOCDL.hh"
#include "XrdCl/XrdClFileOperations.hh"

namespace XrdZip
{

  ArchiveReader::ArchiveReader() : archsize( 0 ), isopen( false ), openstage( None )
  {
  }

  ArchiveReader::~ArchiveReader()
  {
  }

  XrdCl::XRootDStatus ArchiveReader::OpenArchive( const std::string      &url,
                                                  XrdCl::ResponseHandler *handler,
                                                  uint16_t                timeout )
  {
    using namespace XrdCl;

    Fwd<uint32_t> rdsize;
    Fwd<uint64_t> rdoff;
    Fwd<void*>    rdbuff;
    uint32_t      maxrdsz = EOCD::maxCommentLength + EOCD::eocdBaseSize +
                            ZIP64_EOCDL::zip64EocdlSize;

    Pipeline open_archive = // open the archive
                            XrdCl::Open( archive, url, OpenFlags::Read ) >>
                              [=]( XRootDStatus &status, StatInfo &info )
                                {
                                   if( !status.IsOK() )
                                     return handler->HandleResponse( new XrdCl::XRootDStatus( status ), nullptr );
                                   archsize = info.GetSize();
                                   rdsize = ( archsize <= maxrdsz ? archsize : maxrdsz );
                                   rdoff  = archsize - *rdsize;
                                   buffer.reset( new char[*rdsize] );
                                   rdbuff = buffer.get();
                                   openstage = HaveEocdBlk;
                                 }
                            // read the Central Directory (in several stages if necessary)
                          | XrdCl::Read( archive, rdoff, rdsize, rdbuff ) >>
                              [=]( XRootDStatus &status, ChunkInfo &chunk )
                                {
                                  if( !status.IsOK() )
                                    return handler->HandleResponse( new XrdCl::XRootDStatus( status ), nullptr );;

                                  const char *buff = reinterpret_cast<char*>( chunk.buffer );

                                  while( true )
                                  {
                                    switch( openstage )
                                    {
                                      case HaveEocdBlk:
                                      {
                                        // Parse the EOCD record
                                        const char *eocdBlock = EOCD::Find( buff, chunk.length );
                                        if( !eocdBlock )
                                        {
                                          XRootDStatus error( stError, errDataError, 0,
                                                              "End-of-central-directory signature not found." );
                                          handler->HandleResponse( new XrdCl::XRootDStatus( error ), nullptr );;
                                          Pipeline::Stop( error );
                                        }
                                        eocd.reset( new EOCD( eocdBlock ) );

                                        // Do we have the whole archive?
                                        if( chunk.length == archsize )
                                        {
                                          // If we managed to download the whole archive we don't need to
                                          // worry about zip64, it is so small that standard EOCD will do
                                          buff = buff + eocd->cdOffset;
                                          openstage = HaveCdRecords;
                                          continue;
                                        }

                                        // Let's see if it is ZIP64 (if yes, the EOCD will be preceded with ZIP64 EOCD locator)
                                        const char *zip64EocdlBlock = eocdBlock - ZIP64_EOCDL::zip64EocdlSize;
                                        // make sure there is enough data to assume there's a ZIP64 EOCD locator
                                        if( zip64EocdlBlock > buffer.get() )
                                        {
                                          uint32_t signature = to<uint32_t>( zip64EocdlBlock );
                                          if( signature == ZIP64_EOCDL::zip64EocdlSign )
                                          {
                                            buff = zip64EocdlBlock;
                                            openstage = HaveZip64EocdlBlk;
                                            continue;
                                          }
                                        }

                                        // It's not ZIP64, we already know where the CD records are
                                        // we need to read more data
                                        rdoff  = eocd->cdOffset;
                                        rdsize = eocd->cdSize;
                                        buffer.reset( new char[*rdsize] );
                                        rdbuff = buffer.get();
                                        openstage = HaveCdRecords;
                                        Pipeline::Repeat();
                                      }

                                      case HaveZip64EocdlBlk:
                                      {
                                        std::unique_ptr<ZIP64_EOCDL> eocdl( new ZIP64_EOCDL( buff ) );
                                        if( chunk.offset > eocdl->zip64EocdOffset )
                                        {
                                          // we need to read more data
                                          rdsize = archsize - eocdl->zip64EocdOffset;
                                          rdoff  = eocdl->zip64EocdOffset;
                                          buffer.reset( new char[*rdsize] );
                                          rdbuff = buffer.get();
                                          openstage = HaveZip64EocdBlk;
                                          Pipeline::Repeat();
                                        }

                                        buff = buffer.get() + ( eocdl->zip64EocdOffset - chunk.offset );
                                        openstage = HaveZip64EocdBlk;
                                        continue;
                                      }

                                      case HaveZip64EocdBlk:
                                      {
                                        uint32_t signature = to<uint32_t>( buff );
                                        if( signature != ZIP64_EOCD::zip64EocdSign )
                                        {
                                          XrdCl::XRootDStatus error( XrdCl::stError, XrdCl::errDataError, 0,
                                                              "ZIP64 End-of-central-directory signature not found." );
                                          handler->HandleResponse( new XrdCl::XRootDStatus( error ), nullptr );
                                          Pipeline::Stop( error );
                                        }
                                        zip64eocd.reset( new ZIP64_EOCD( buff ) );

                                        // now we can read the CD records
                                        rdoff  = zip64eocd->cdOffset;
                                        rdsize = zip64eocd->cdSize;
                                        buffer.reset( new char[*rdsize] );
                                        rdbuff = buffer.get();
                                        openstage = HaveCdRecords;
                                        Pipeline::Repeat();
                                      }

                                      case HaveCdRecords:
                                      {
                                        try
                                        {
                                          cdRecords = CDFH::Parse( buff, eocd->cdSize, eocd->nbCdRec );
                                        }
                                        catch( const bad_data &ex )
                                        {
                                          XrdCl::XRootDStatus error( XrdCl::stError, XrdCl::errDataError, 0,
                                                                     "ZIP Central Directory corrupted." );
                                          handler->HandleResponse( new XrdCl::XRootDStatus( error ), nullptr );
                                          Pipeline::Stop( error );
                                        }
                                        openstage = Done;
                                        handler->HandleResponse( new XrdCl::XRootDStatus( status ), nullptr );
                                        if( chunk.length != archsize ) buffer.reset();
                                        break;
                                      }

                                      default:
                                      {
                                        Pipeline::Stop( XRootDStatus( stError, errInvalidOp ) );
                                      }
                                    }

                                    break;
                                  }
                                };


    Async( std::move( open_archive ), timeout );
    return XRootDStatus();
  }

} /* namespace XrdZip */
