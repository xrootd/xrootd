//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <michal.simon@cern.ch>
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

#include "XrdEc/XrdEcReader.hh"
#include "XrdEc/XrdEcUtilities.hh"
#include "XrdEc/XrdEcConfig.hh"
#include "XrdEc/XrdEcObjCfg.hh"
#include "XrdEc/XrdEcThreadPool.hh"

#include "XrdZip/XrdZipLFH.hh"
#include "XrdZip/XrdZipCDFH.hh"
#include "XrdZip/XrdZipUtils.hh"

#include "XrdCl/XrdClMessageUtils.hh"


#include "XrdOuc/XrdOucCRC32C.hh"

#include "XrdCl/XrdClParallelOperation.hh"
#include "XrdCl/XrdClZipOperations.hh"
#include "XrdCl/XrdClFileOperations.hh"
#include "XrdCl/XrdClFinalOperation.hh"

#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"

#include <algorithm>
#include <iterator>
#include <numeric>
#include <tuple>
#include <set>

namespace XrdEc
{

  //---------------------------------------------------------------------------
  // Destructor (we need it in the source file because block_t is defined in
  // here)
  //---------------------------------------------------------------------------
  Reader::~Reader()
  {
  }

  //---------------------------------------------------------------------------
  // Open the erasure coded / striped object
  //---------------------------------------------------------------------------
  void Reader::Open( XrdCl::ResponseHandler *handler, uint16_t timeout )
  {
	auto log = XrdCl::DefaultEnv::GetLog();
    const size_t size = objcfg.plgr.size();
    std::vector<XrdCl::Pipeline> opens; opens.reserve( size );
    std::vector<XrdCl::Pipeline> healthRead; healthRead.reserve(size);
    for( size_t i = 0; i < size; ++i )
    {
      // generate the URL
      std::string url = objcfg.GetDataUrl( i );
      archiveIndices.emplace(url, i);
      // create the file object
      dataarchs.emplace( url, std::make_shared<XrdCl::ZipArchive>(
          Config::Instance().enable_plugins ) );
      // open the archive
      if( objcfg.nomtfile )
      {
        opens.emplace_back( XrdCl::OpenArchive( *dataarchs[url], url, XrdCl::OpenFlags::Read ) );
      }
      else
        opens.emplace_back( OpenOnly( *dataarchs[url], url, false ) );

      healthRead.emplace_back(CheckHealthExists(i));
    }

    auto pipehndl = [=]( const XrdCl::XRootDStatus &st )
                    		{ // set the central directories in ZIP archives (if we use metadata files)
    	auto itr = dataarchs.begin();
    	for (; itr != dataarchs.end(); ++itr) {
    		const std::string &url = itr->first;
    		auto &zipptr = itr->second;
    		if (zipptr->openstage == XrdCl::ZipArchive::NotParsed)
    			zipptr->SetCD(metadata[url]);
    		else if (zipptr->openstage != XrdCl::ZipArchive::Done
    				&& !metadata.empty())
    			AddMissing(metadata[url]);
    		auto itr = zipptr->cdmap.begin();
    		for (; itr != zipptr->cdmap.end(); ++itr) {
    			try {
    				size_t blknb = fntoblk(itr->first);
    				urlmap.emplace(itr->first, url);
    				if (blknb > lstblk)
    					lstblk = blknb;
    			} catch (std::invalid_argument&) {
    				log->Error(XrdCl::XRootDMsg, "EC Reader Open: Invalid file name found");
    			}
    		}
    	}
                      metadata.clear();
                      // call user handler
                      if( handler )
                        handler->HandleResponse( new XrdCl::XRootDStatus( st ), nullptr );
                    };
    // in parallel open the data files and read the metadata
    XrdCl::Pipeline p = objcfg.nomtfile
                      ? XrdCl::Parallel( opens ).AtLeast( objcfg.nbdata )
                    		  | XrdCl::Parallel(healthRead).AtLeast(0)
                    		  | ReadSize( 0 ) | XrdCl::Final( pipehndl )
                      : XrdCl::Parallel( ReadMetadata( 0 ),
                                         XrdCl::Parallel( opens ).AtLeast( objcfg.nbdata ) ) >> pipehndl;
    XrdCl::Async( std::move( p ), timeout );
  }

  //-----------------------------------------------------------------------
  // Read data from the data object
  //-----------------------------------------------------------------------
  void Reader::Read( uint64_t                offset,
                     uint32_t                length,
                     void                   *buffer,
                     XrdCl::ResponseHandler *handler,
                     uint16_t                timeout )
  {
    if( objcfg.nomtfile )
    {
      if( offset >= filesize )
        length = 0;
      else if( offset + length > filesize )
        length = filesize - offset;
    }

    if( length == 0 )
    {
      ScheduleHandler( offset, 0, buffer, handler );
      return;
    }

    char *usrbuff = reinterpret_cast<char*>( buffer );
    typedef std::tuple<uint64_t, uint32_t,
                       void*, uint32_t,
                       XrdCl::ResponseHandler*,
                       XrdCl::XRootDStatus> rdctx_t;
    auto rdctx = std::make_shared<rdctx_t>( offset, 0, buffer,
                                            length, handler,
                                            XrdCl::XRootDStatus() );
    auto rdmtx = std::make_shared<std::mutex>();

    while( length > 0 )
    {
      size_t   blkid  = offset / objcfg.datasize;                                     //< ID of the block from which we will be reading
      size_t   strpid = ( offset % objcfg.datasize ) / objcfg.chunksize;              //< ID of the stripe from which we will be reading
      uint64_t rdoff  = offset - blkid * objcfg.datasize - strpid * objcfg.chunksize; //< relative read offset within the stripe
      uint32_t rdsize = objcfg.chunksize - rdoff;                                     //< read size within the stripe
      if( rdsize > length ) rdsize = length;
      //-------------------------------------------------------------------
      // Make sure we operate on a valid block
      //-------------------------------------------------------------------
      std::unique_lock<std::mutex> lck( blkmtx );
      if( !block || block->blkid != blkid )
        block = std::make_shared<block_t>( blkid, *this, objcfg );
      //-------------------------------------------------------------------
      // Prepare the callback for reading from single stripe
      //-------------------------------------------------------------------
      auto blk = block;
      lck.unlock();
      auto callback = [blk, rdctx, rdsize, rdmtx]( const XrdCl::XRootDStatus &st, uint32_t nbrd )
      {
        std::unique_lock<std::mutex> lck( *rdmtx );
        //---------------------------------------------------------------------
        // update number of bytes left to be read (bytes requested not actually
        // read)
        //---------------------------------------------------------------------
        std::get<3>( *rdctx ) -= rdsize;
        //---------------------------------------------------------------------
        // Handle failure ...
        //---------------------------------------------------------------------
        if( !st.IsOK() )
          std::get<5>( *rdctx ) = st; // the error
        //---------------------------------------------------------------------
        // Handle success ...
        //---------------------------------------------------------------------
        else
          std::get<1>( *rdctx ) += nbrd; // number of bytes read
        //---------------------------------------------------------------------
        // Are we done?
        //---------------------------------------------------------------------
        if( std::get<3>( *rdctx ) == 0 )
        {
          //-------------------------------------------------------------------
          // Check if the read operation was successful ...
          //-------------------------------------------------------------------
          XrdCl::XRootDStatus &status = std::get<5>( *rdctx );
          if( !status.IsOK() )
            ScheduleHandler( std::get<4>( *rdctx ), status );
          else
            ScheduleHandler( std::get<0>( *rdctx ), std::get<1>( *rdctx ),
                             std::get<2>( *rdctx ), std::get<4>( *rdctx ) );
        }
      };
      //-------------------------------------------------------------------
      // Read data from a stripe
      //-------------------------------------------------------------------
      block_t::read( blk, strpid, rdoff, rdsize, usrbuff, callback, timeout );
      //-------------------------------------------------------------------
      // Update absolute offset, read length, and user buffer
      //-------------------------------------------------------------------
      offset  += rdsize;
      length  -= rdsize;
      usrbuff += rdsize;
    }
  }

  //-----------------------------------------------------------------------
  // Close the data object
  //-----------------------------------------------------------------------
  void Reader::Close( XrdCl::ResponseHandler *handler, uint16_t timeout )
  {
    //---------------------------------------------------------------------
    // prepare the pipelines ...
    //---------------------------------------------------------------------
    std::vector<XrdCl::Pipeline> closes;
    closes.reserve( dataarchs.size() );
    auto itr = dataarchs.begin();
    for( ; itr != dataarchs.end() ; ++itr )
    {
      auto &zipptr = itr->second;
      if( zipptr->IsOpen() )
      {
        zipptr->SetProperty( "BundledClose", "true");
        closes.emplace_back( XrdCl::CloseArchive( *zipptr ) >>
                             [zipptr]( XrdCl::XRootDStatus& ){ } );
      }
    }

    // if there is nothing to close just schedule the handler
    if( closes.empty() ) ScheduleHandler( handler );
    // otherwise close the archives
    else XrdCl::Async( XrdCl::Parallel( closes ) >> handler, timeout );
  }

  //-------------------------------------------------------------------------
  // on-definition is not allowed here beforeiven stripes from given block
  //-------------------------------------------------------------------------
  void Reader::Read( size_t blknb, size_t strpnb, buffer_t &buffer, callback_t cb, uint16_t timeout )
  {
    // generate the file name (blknb/strpnb)
    std::string fn = objcfg.GetFileName( blknb, strpnb );
    // if the block/stripe does not exist it means we are reading passed the end of the file
    auto itr = urlmap.find( fn );
    if( itr == urlmap.end() )
    {
      auto st = XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errNotFound );
      ThreadPool::Instance().Execute( cb, st, 0 );
      return;
    }
    // get the URL of the ZIP archive with the respective data
    const std::string &url = itr->second;
    // get the ZipArchive object
    auto &zipptr = dataarchs[url];
    // check the size of the data to be read
    XrdCl::StatInfo *info = nullptr;
    auto st = zipptr->Stat( fn, info );
    if( !st.IsOK() )
    {
      ThreadPool::Instance().Execute( cb, st, 0 );
      return;
    }
    uint32_t rdsize = info->GetSize();
    delete info;
    // create a buffer for the data
    buffer.resize( objcfg.chunksize );
    // issue the read request
    XrdCl::Async( XrdCl::ReadFrom(*zipptr, fn, 0, rdsize, buffer.data()) >>
    		[zipptr, fn, cb, &buffer, this, url, timeout] (XrdCl::XRootDStatus &st, XrdCl::ChunkInfo &ch)
			{
    	//---------------------------------------------------
    	// If read failed there's nothing to do, just pass the
    	// status to user callback
    	//---------------------------------------------------
    	if( !st.IsOK() )
    	{
    		cb( XrdCl::XRootDStatus(st.status, XrdCl::errNotFound, 0, "Read failed"), 0 );
    		return;
    	}
    	//---------------------------------------------------
    	// Get the checksum for the read data
    	//---------------------------------------------------
    	uint32_t orgcksum = 0;
    	auto s = zipptr->GetCRC32(fn, orgcksum);
    	//---------------------------------------------------
    	// If we cannot extract the checksum assume the data
    	// are corrupted
    	//---------------------------------------------------
    	if( !s.IsOK() )
    	{
    		cb( XrdCl::XRootDStatus(s.status, s.code, s.errNo, "Chksum fail"), 0 );
    		return;
    	}
    	//---------------------------------------------------
    	// Verify data integrity
    	//---------------------------------------------------
    	uint32_t cksum = objcfg.digest( 0, ch.buffer, ch.length );
    	if( orgcksum != cksum )
    	{
    		cb( XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errCheckSumError, 0, "Chksum of data and cdfh not equal" ), 0 );
    		return;
    	}
    	// checksums identical, call with positive response
    	cb(XrdCl::XRootDStatus(), ch.length);
    	return;
			}, timeout);
  }

  //-----------------------------------------------------------------------
  // Read metadata for the object
  //-----------------------------------------------------------------------
  XrdCl::Pipeline Reader::ReadMetadata( size_t index )
  {
    const size_t size = objcfg.plgr.size();
    // create the File object
    auto file = std::make_shared<XrdCl::File>( Config::Instance().enable_plugins );
    // prepare the URL for Open operation
    std::string url = objcfg.GetMetadataUrl( index );
    // arguments for the Read operation
    XrdCl::Fwd<uint32_t> rdsize;
    XrdCl::Fwd<void*>    rdbuff;

    return XrdCl::Open( *file, url, XrdCl::OpenFlags::Read ) >>
             [=]( XrdCl::XRootDStatus &st, XrdCl::StatInfo &info ) mutable
             {
               if( !st.IsOK() )
               {
                 if( index + 1 < size )
                   XrdCl::Pipeline::Replace( ReadMetadata( index + 1 ) );
                 return;
               }
               // prepare the args for the subsequent operation
               rdsize = info.GetSize();
               rdbuff = new char[info.GetSize()];
             }
         | XrdCl::Read( *file, 0, rdsize, rdbuff ) >>
             [=]( XrdCl::XRootDStatus &st, XrdCl::ChunkInfo &ch )
             {
               if( !st.IsOK() )
               {
                 if( index + 1 < size )
                   XrdCl::Pipeline::Replace( ReadMetadata( index + 1 ) );
                 return;
               }
               // now parse the metadata
               if( !ParseMetadata( ch ) )
               {
                 if( index + 1 < size )
                   XrdCl::Pipeline::Replace( ReadMetadata( index + 1 ) );
                 return;
               }
             }
         | XrdCl::Close( *file ) >>
             []( XrdCl::XRootDStatus &st )
             {
               if( !st.IsOK() )
                 XrdCl::Pipeline::Ignore(); // ignore errors, we don't really care
             }
         | XrdCl::Final(
             [rdbuff, file]( const XrdCl::XRootDStatus& )
             {
               // deallocate the buffer if necessary
               if( rdbuff.Valid() )
               {
                 char* buffer = reinterpret_cast<char*>( *rdbuff );
                 delete[] buffer;
               }
             } );
  }

  //-----------------------------------------------------------------------
  //! Read size from xattr
  //!
  //! @param index : placement's index
  //-----------------------------------------------------------------------
  XrdCl::Pipeline Reader::ReadSize( size_t index )
  {
    std::string url = objcfg.GetDataUrl( index );
    return XrdCl::GetXAttr( dataarchs[url]->GetFile(), "xrdec.filesize" ) >>
        [index, this, url]( XrdCl::XRootDStatus &st, std::string &size)
        {
          if( !st.IsOK() || this->dataarchs[url]->openstage != XrdCl::ZipArchive::Done)
          {
            //-------------------------------------------------------------
            // Check if we can recover the error or a diffrent location
            //-------------------------------------------------------------
            if( index + 1 < objcfg.plgr.size() )
              XrdCl::Pipeline::Replace( ReadSize( index + 1 ) );
            return;
          }
          try{
          	filesize = std::stoull( size );
          }
          catch(std::invalid_argument &){
        	  if( index + 1 < objcfg.plgr.size() )
        	  XrdCl::Pipeline::Replace( ReadSize( index + 1 ) );
          }
        };
  }

  XrdCl::Pipeline Reader::CheckHealthExists(size_t index){
	  std::string url = objcfg.GetDataUrl( index );
	  		  return XrdCl::ListXAttr(dataarchs[url]->GetFile()) >>
	  				  [index, url, this] (XrdCl::XRootDStatus &st, std::vector<XrdCl::XAttr> attrs){
	  			  	  	 for(auto it = attrs.begin(); it != attrs.end(); it++){
	  			  	  		 if(it->name == "xrdec.corrupted"){
	  			  	  			 XrdCl::Pipeline::Replace(ReadHealth(index));
	  			  	  		 }
	  			  	  	 }
	  		  };
  }

  XrdCl::Pipeline Reader::ReadHealth(size_t index){
		  std::string url = objcfg.GetDataUrl( index );
		  return XrdCl::GetXAttr( dataarchs[url]->GetFile(), "xrdec.corrupted" ) >>
	          [index, url, this]( XrdCl::XRootDStatus &st, std::string &damage)
	          {
				if (st.IsOK()) {
					try {
						int damaged = std::stoi(damage);
						if (damaged > 0)
							this->dataarchs[url]->openstage = XrdCl::ZipArchive::Error;
					} catch (std::invalid_argument&) {
						XrdCl::DefaultEnv::GetLog()->Error(XrdCl::XRootDMsg, "Read non-integer from xrdec.corrupted XAttr");
						return;
					}
				}
				return;
	          };
  }

  //-----------------------------------------------------------------------
  // Parse metadata from chunk info object
  //-----------------------------------------------------------------------
  bool Reader::ParseMetadata( XrdCl::ChunkInfo &ch )
  {
    const size_t mincnt = objcfg.nbdata + objcfg.nbparity;
    const size_t maxcnt = objcfg.plgr.size();

    char   *buffer = reinterpret_cast<char*>( ch.buffer );
    size_t  length = ch.length;

    for( size_t i = 0; i < maxcnt; ++i )
    {
      uint32_t signature = XrdZip::to<uint32_t>( buffer );
      if( signature != XrdZip::LFH::lfhSign )
      {
        if( i + 1 < mincnt ) return false;
        break;
      }
      XrdZip::LFH lfh( buffer );
      // check if we are not reading passed the end of the buffer
      if( lfh.lfhSize + lfh.uncompressedSize > length ) return false;
      buffer += lfh.lfhSize;
      length -= lfh.lfhSize;
      // verify the checksum
      uint32_t crc32val = objcfg.digest( 0, buffer, lfh.uncompressedSize );
      if( crc32val != lfh.ZCRC32 ) return false;
      // keep the metadata
      try{
    	  std::string url = objcfg.GetDataUrl( std::stoull( lfh.filename ) );
    	  metadata.emplace( url, buffer_t( buffer, buffer + lfh.uncompressedSize ) );
      }
      catch(const std::invalid_argument &){
		  XrdCl::DefaultEnv::GetLog()->Error(XrdCl::XRootDMsg, "Found invalid filename in lfh");
      }
      buffer += lfh.uncompressedSize;
      length -= lfh.uncompressedSize;
    }

    return true;
  }

  //-----------------------------------------------------------------------
  // Add all the entries from given Central Directory to missing
  //-----------------------------------------------------------------------
  void Reader::AddMissing( const buffer_t &cdbuff )
  {
    const char *buff = cdbuff.data();
    size_t      size = cdbuff.size();
    // parse Central Directory records
    XrdZip::cdvec_t cdvec;
    XrdZip::cdmap_t cdmap;
    std::tie(cdvec, cdmap ) = XrdZip::CDFH::Parse( buff, size );
    auto itr = cdvec.begin();
    for( ; itr != cdvec.end() ; ++itr )
    {
      XrdZip::CDFH &cdfh = **itr;
      missing.insert( cdfh.filename );
    }
  }

  //-----------------------------------------------------------------------
  //! Check if chunk file name is missing
  //-----------------------------------------------------------------------
  bool Reader::IsMissing( const std::string &fn )
  {
    // if the chunk is in the missing set return true
    if( missing.count( fn ) ) return true;
    // if we don't have a metadata file and the chunk exceeds last chunk
    // also return true
    try{
    	if( objcfg.nomtfile && fntoblk( fn ) <= lstblk ) return true;
    	}
    catch(std::invalid_argument &){
    	XrdCl::DefaultEnv::GetLog()->Error(XrdCl::XRootDMsg, "File name couldn't be parsed to block id");
    }
    // otherwise return false
    return false;
  }


  inline callback_t Reader::ErrorCorrected(Reader *reader, std::shared_ptr<block_t> &self, size_t blkid, size_t strpid){
	  return [reader, self, strpid, blkid]( const XrdCl::XRootDStatus &st, uint32_t ) mutable
	                 {
	                   std::unique_lock<std::mutex> readerLock(reader->missingChunksMutex);
	                   reader->missingChunksVectorRead.erase(std::remove(reader->missingChunksVectorRead.begin(), reader->missingChunksVectorRead.end(), std::make_tuple(blkid, strpid)));
	                   reader->waitMissing.notify_all();
	                 };
  }

  void Reader::MissingVectorRead(std::shared_ptr<block_t> &currentBlock, size_t blkid, size_t strpid, uint16_t timeout){
	  {
		std::unique_lock<std::mutex> lk(missingChunksMutex);
		missingChunksVectorRead.emplace_back(
			std::make_tuple(blkid,strpid));
	  }
	currentBlock->state[strpid] = block_t::Missing;
	currentBlock->read(currentBlock, strpid, 0, objcfg.chunksize,
			nullptr,
			ErrorCorrected(this, currentBlock, blkid, strpid),
			timeout);
  }


  void Reader::VectorRead(const XrdCl::ChunkList &chunks, void *buffer, XrdCl::ResponseHandler *handler, uint16_t timeout){
	  if(chunks.size() > 1024) {
		  if(handler) handler->HandleResponse(new XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidArgs, XrdCl::errInvalidArgs), nullptr);
		  return;
	  }

	  std::vector<XrdCl::ChunkList> hostLists;
	  for(size_t dataHosts = 0; dataHosts < objcfg.plgr.size(); dataHosts++){
		  hostLists.emplace_back(XrdCl::ChunkList());
	  }

	  auto log = XrdCl::DefaultEnv::GetLog();

	  //bool useGlobalBuffer = buffer != nullptr;
	  char* globalBuffer = (char*)buffer;

	  // host index, blkid, strpid
	  std::set<std::tuple<size_t, size_t, size_t>> requestedChunks;
	  // create block_ts for any requested block index
	  std::map<size_t, std::shared_ptr<block_t>> blockMap;

	  // go through the requested lists of chunks and assign them to fitting hosts
	  for(size_t index = 0; index < chunks.size(); index++){
		  uint32_t remainLength = chunks[index].length;
		  uint64_t currentOffset = chunks[index].offset;

		  while(remainLength > 0){
		      size_t   blkid  = currentOffset / objcfg.datasize;                                     //< ID of the block from which we will be reading
		      size_t   strpid = ( currentOffset % objcfg.datasize ) / objcfg.chunksize;              //< ID of the stripe from which we will be reading
		      uint64_t rdoff  = currentOffset - blkid * objcfg.datasize - strpid * objcfg.chunksize ; //< relative read offset within the stripe
		      //uint64_t offsetInFile = rdoff + blkid * objcfg.chunksize;							// relative offset within the file
		      uint32_t rdsize = objcfg.chunksize - rdoff;                                     //< read size within the stripe
		      if( rdsize > remainLength ) rdsize = remainLength;
		      if(currentOffset + rdsize >= filesize) {
		    	  rdsize = filesize - currentOffset;
		    	  remainLength = rdsize;
		      }


		      std::string fn = objcfg.GetFileName(blkid, strpid);

		      auto itr = urlmap.find( fn );
		      if( itr == urlmap.end() )
		      {
		        log->Error(XrdCl::XRootDMsg, "EC Vector Read: No mapping of file to host found.");
		        break;
		      }
		      // get the URL of the ZIP archive with the respective data
		      const std::string &url = itr->second;
		      auto itr2 = archiveIndices.find(url);
		      if(itr2 == archiveIndices.end())
		      {
		    	  log->Error(XrdCl::XRootDMsg, "EC Vector Read: Couldn't find host for file.");
		    	  break;
		      }
		      size_t indexOfArchive = archiveIndices[url];

			if (blockMap.find(blkid) == blockMap.end())
			{
				blockMap.emplace(blkid,
						std::make_shared<block_t>(blkid, *this, objcfg));
			}

			blockMap[blkid]->state[strpid] = block_t::Loading;
			XrdCl::StatInfo* info;
			if(dataarchs[url]->Stat(objcfg.GetFileName(blkid, strpid), info).IsOK())
				blockMap[blkid]->stripes[strpid].resize( info ->GetSize() );

		      auto requestChunk = std::make_tuple(indexOfArchive, blkid, strpid);
		      if(requestedChunks.find(requestChunk) == requestedChunks.end())
		    	  {
		    	  uint64_t off = 0;
		    	  dataarchs[url]->GetOffset(objcfg.GetFileName(blkid, strpid), off);
		    	  hostLists[indexOfArchive].emplace_back(XrdCl::ChunkInfo(
		    			  off,
						  info ->GetSize(),
						  blockMap[blkid]->stripes[strpid].data()));

		    	  // fill list of requested chunks by block and stripe id
		    	  requestedChunks.emplace(requestChunk);

		    	  }
		      remainLength -= rdsize;
		      currentOffset += rdsize;

		  }
	  }

	  std::vector<XrdCl::Pipeline> hostPipes;
	  hostPipes.reserve(hostLists.size());
	  for(size_t i = 0; i < hostLists.size(); i++){
		  while(hostLists[i].size() > 0){
			  uint32_t range = hostLists[i].size() > 1024 ? 1024 : hostLists[i].size();
			  XrdCl::ChunkList partList(hostLists[i].begin(), hostLists[i].begin() + range);
			  hostLists[i].erase(hostLists[i].begin(), hostLists[i].begin() + range);
		hostPipes.emplace_back(
				XrdCl::VectorRead(XrdCl::Ctx<XrdCl::File>(dataarchs[objcfg.GetDataUrl(i)]->archive),
						partList, nullptr, timeout)
						>> [=](const XrdCl::XRootDStatus &st, XrdCl::VectorReadInfo ch) mutable
						{
								auto it = requestedChunks.begin();
								while(it!=requestedChunks.end())
								{
									auto &args = *it;
									size_t host = std::get<0>(args);
									size_t blkid = std::get<1>(args);
									size_t strpid = std::get<2>(args);
									it++;
									if(host == i)
									{
										std::shared_ptr<block_t> currentBlock = blockMap[blkid];


										if(!st.IsOK())
										{
											log->Error(XrdCl::XRootDMsg, "EC Vector Read of host %d failed entirely.", i);
											MissingVectorRead(currentBlock, blkid, strpid, timeout);
										}
										else{
											uint32_t orgcksum = 0;
											auto s = dataarchs[objcfg.GetDataUrl(i)]->GetCRC32( objcfg.GetFileName(blkid, strpid), orgcksum );
											//---------------------------------------------------
											// If we cannot extract the checksum assume the data
											// are corrupted
											//---------------------------------------------------
											if( !st.IsOK() )
											{
												log->Error(XrdCl::XRootDMsg, "EC Vector Read: Couldn't read CRC32 from CD.");
												MissingVectorRead(currentBlock, blkid, strpid, timeout);
												continue;
											}
											//---------------------------------------------------
											// Verify data integrity
											//---------------------------------------------------
											uint32_t cksum = objcfg.digest( 0, currentBlock->stripes[strpid].data(), currentBlock->stripes[strpid].size() );
											if( orgcksum != cksum )
											{
												log->Error(XrdCl::XRootDMsg, "EC Vector Read: Wrong checksum for block %d stripe %d.", blkid, strpid);
												MissingVectorRead(currentBlock, blkid, strpid, timeout);
												continue;
											}
											else{
												currentBlock->state[strpid] = block_t::Valid;
												bool recoverable = currentBlock->error_correction( currentBlock );
												if(!recoverable)
													log->Error(XrdCl::XRootDMsg, "EC Vector Read: Couldn't recover block %d.", blkid);
											}
										}
									}
								}
							}
							);
	  }
	  }

	  auto finalPipehndl = [=] (const XrdCl::XRootDStatus &st) mutable {
		  // wait until all missing chunks are corrected (uses single reads to get parity stripes)
		  std::unique_lock<std::mutex> lk(missingChunksMutex);
		  waitMissing.wait(lk, [=] { return missingChunksVectorRead.size() == 0;});

		  bool failed = false;
		  for(size_t index = 0; index < chunks.size(); index++){
			  uint32_t remainLength = chunks[index].length;
			  uint64_t currentOffset = chunks[index].offset;

					char *localBuffer;
					if (globalBuffer)
						localBuffer = globalBuffer;
					else
						localBuffer = (char*)(chunks[index].buffer);

			  while(remainLength > 0){
			      size_t   blkid  = currentOffset / objcfg.datasize;                                     //< ID of the block from which we will be reading
			      size_t   strpid = ( currentOffset % objcfg.datasize ) / objcfg.chunksize;              //< ID of the stripe from which we will be reading
			      uint64_t rdoff  = currentOffset - blkid * objcfg.datasize - strpid * objcfg.chunksize ; //< relative read offset within the stripe
			      uint32_t rdsize = objcfg.chunksize - rdoff;                                     //< read size within the stripe
			      if( rdsize > remainLength ) rdsize = remainLength;

				  // put received data into given buffers
			      if(blockMap.find(blkid) == blockMap.end() || blockMap[blkid] == nullptr){
			    	  log->Error(XrdCl::XRootDMsg, "EC Vector Read: Missing block %d.", blkid);
			    	  failed = true;
			    	  break;
			      }
			      if(blockMap[blkid]->state[strpid] != block_t::Valid){
			    	  log->Error(XrdCl::XRootDMsg, "EC Vector Read: Invalid stripe in block %d stripe %d.", blkid, strpid);
			    	  failed = true;
			    	  break;
			      }

			      memcpy(localBuffer, blockMap[blkid]->stripes[strpid].data() + rdoff, rdsize);

			      remainLength -= rdsize;
			      currentOffset += rdsize;
			      localBuffer += rdsize;
			  }
			  if(globalBuffer) globalBuffer = localBuffer;
		  }
		  if(handler){
			  if(failed) log->Error(XrdCl::XRootDMsg, "EC Vector Read failed (at least in part).");
			  if(failed) handler->HandleResponse(new XrdCl::XRootDStatus(XrdCl::stError, "Couldn't read all segments"), nullptr);
			  else handler->HandleResponse(new XrdCl::XRootDStatus(), nullptr);
		  }
	  };

	  XrdCl::Pipeline p = XrdCl::Parallel(hostPipes) |
			  XrdCl::Final(finalPipehndl);

	  XrdCl::Async(std::move(p), timeout);

  }

} /* namespace XrdEc */
