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

#include "XrdEcRepairTool.hh"

#include "XrdEc/XrdEcReader.hh"
#include "XrdEc/XrdEcUtilities.hh"
#include "XrdEc/XrdEcConfig.hh"
#include "XrdEc/XrdEcObjCfg.hh"
#include "XrdEc/XrdEcThreadPool.hh"

#include "XrdZip/XrdZipLFH.hh"
#include "XrdZip/XrdZipCDFH.hh"
#include "XrdZip/XrdZipEOCD.hh"
#include "XrdZip/XrdZipUtils.hh"

#include "XrdOuc/XrdOucCRC32C.hh"

#include "XrdCl/XrdClParallelOperation.hh"
#include "XrdCl/XrdClZipOperations.hh"
#include "XrdCl/XrdClFileOperations.hh"
#include "XrdCl/XrdClFinalOperation.hh"
#include "XrdCl/XrdClMessageUtils.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"

#include <algorithm>
#include <iterator>
#include <numeric>
#include <tuple>
#include <fstream>

namespace XrdEc {


//-----------------------------------------------------------------------
// Get a callback for read operation
//-----------------------------------------------------------------------
callback_t RepairTool::read_callback(std::shared_ptr<ThreadEndSemaphore> sem, size_t blkid, size_t strpid, RepairTool *tool) {
	return [tool, sem, blkid, strpid](const XrdCl::XRootDStatus &st, const uint32_t &length) mutable {
		if(sem != nullptr && !st.IsOK()){
			std::stringstream stream;
			if(tool->urlmap.find(tool->objcfg.GetFileName(blkid, strpid))!=tool->urlmap.end())
				stream << "Corruption in block " << blkid << " and stripe " << strpid << "\nHost: "
					<< tool->urlmap[tool->objcfg.GetFileName(blkid, strpid)] << "\n" << std::flush;
			else stream << "Corruption in block " << blkid << " and stripe " << strpid << "\nHost not found\n" << std::flush;
			XrdCl::DefaultEnv::GetLog()->Error(XrdCl::XRootDMsg, &(stream.str()[0]));
			tool->st->status = XrdCl::stError;
		}
		std::unique_lock<std::mutex> lk(tool->bufferCountMutex);
		tool->currentBuffers--;
		lk.unlock();
		tool->waitBuffers.notify_all();
	};
}

void RepairTool::CheckFile(XrdCl::ResponseHandler *handler, uint16_t timeout){
	auto log = XrdCl::DefaultEnv::GetLog();

	XrdCl::SyncResponseHandler handler1;
	TryOpen(&handler1, XrdCl::OpenFlags::Read, timeout);
	handler1.WaitForResponse();
	st = handler1.GetStatus();
	if (handler1.GetStatus()->IsOK())
	{
		auto itr = redirectionMap.begin();
		for (; itr != redirectionMap.end(); ++itr)
		{
			const std::string &oldUrl = itr->first;
			log->Error(XrdCl::XRootDMsg, "Archive %s contains some damaged metadata", oldUrl);
			InvalidateReplaceArchive(oldUrl, readDataarchs[oldUrl], timeout);
			st->status = XrdCl::stError;
		}
	}
	// do the read for each strpid and blkid but with different callback func
	uint64_t numBlocks = ceil((filesize / (float) objcfg.chunksize) / objcfg.nbdata);
	std::vector<std::shared_ptr<buffer_t>> buffers;
	auto sem = std::make_shared<XrdSysSemaphore>(0);
	{
		// switching out of this context will remove the own reference to ptr
		std::shared_ptr<ThreadEndSemaphore> ptr = std::make_shared<
				ThreadEndSemaphore>(sem);

		for (size_t blkid = 0; blkid < numBlocks; blkid++)
		{
			for (size_t strpid = 0; strpid < objcfg.nbdata + objcfg.nbparity;
					strpid++)
			{
				std::unique_lock<std::mutex> lk(bufferCountMutex);
				waitBuffers.wait(lk, [this] {return this->currentBuffers < this->bufferLimit;});
				std::shared_ptr<buffer_t> buffer = std::make_shared<buffer_t>();
				currentBuffers++;
				buffer->reserve(objcfg.chunksize);
				buffers.push_back(buffer);
				Read(blkid, strpid, *buffer,
						RepairTool::read_callback(ptr, blkid, strpid, this), timeout);
			}
		}
	}
	sem->Wait();
	for(size_t u = 0; u < objcfg.plgr.size(); u++){
		writeDataarchs[objcfg.GetDataUrl(u)]= readDataarchs[objcfg.GetDataUrl(u)];
	}
	XrdCl::SyncResponseHandler handler2;
	CloseAllArchives(&handler2, timeout);
	handler2.WaitForResponse();
	if (handler)
	{
		handler->HandleResponse(new XrdCl::XRootDStatus(*st), nullptr);
	}
}

//---------------------------------------------------------------------------
// Get a buffer with metadata (CDFH and EOCD records)
//---------------------------------------------------------------------------
XrdZip::buffer_t RepairTool::GetMetadataBuffer()
{
  using namespace XrdZip;

  const size_t cdcnt = objcfg.plgr.size();
  std::vector<buffer_t> buffs; buffs.reserve( cdcnt ); // buffers with raw data
  std::vector<LFH> lfhs; lfhs.reserve( cdcnt );        // LFH records
  std::vector<CDFH> cdfhs; cdfhs.reserve( cdcnt );     // CDFH records

  //-------------------------------------------------------------------------
  // prepare data structures (LFH and CDFH records)
  //-------------------------------------------------------------------------
  uint64_t offset = 0;
  uint64_t cdsize = 0;
  mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  for( size_t i = 0; i < cdcnt; ++i )
  {
    std::string fn = std::to_string( i );                          // file name (URL of the data archive)
    buffer_t buff( writeDataarchs[objcfg.GetDataUrl(i)]->GetCD() );                        // raw data buffer (central directory of the data archive)
    uint32_t cksum = objcfg.digest( 0, buff.data(), buff.size() ); // digest (crc) of the buffer
    lfhs.emplace_back( fn, cksum, buff.size(), time( 0 ) );        // LFH record for the buffer
    LFH &lfh = lfhs.back();
    cdfhs.emplace_back( &lfh, mode, offset );                      // CDFH record for the buffer
    offset += LFH::lfhBaseSize + fn.size() + buff.size();          // shift the offset
    cdsize += cdfhs.back().cdfhSize;                               // update central directory size
    buffs.emplace_back( std::move( buff ) );                       // keep the buffer for later
  }

  uint64_t zipsize = offset + cdsize + EOCD::eocdBaseSize;
  buffer_t zipbuff; zipbuff.reserve( zipsize );

  //-------------------------------------------------------------------------
  // write into the final buffer LFH records + raw data
  //-------------------------------------------------------------------------
  for( size_t i = 0; i < cdcnt; ++i )
  {
    lfhs[i].Serialize( zipbuff );
    std::copy( buffs[i].begin(), buffs[i].end(), std::back_inserter( zipbuff ) );
  }
  //-------------------------------------------------------------------------
  // write into the final buffer CDFH records
  //-------------------------------------------------------------------------
  for( size_t i = 0; i < cdcnt; ++i )
    cdfhs[i].Serialize( zipbuff );
  //-------------------------------------------------------------------------
  // prepare and write into the final buffer the EOCD record
  //-------------------------------------------------------------------------
  EOCD eocd( offset, cdcnt, cdsize );
  eocd.Serialize( zipbuff );

  return zipbuff;
}

void RepairTool::CloseAllArchives(XrdCl::ResponseHandler *handler, uint16_t timeout){
	//-------------------------------------------------------------------------
	    // First, check the global status, if we are in an error state just
	    // fail the request.
	    //-------------------------------------------------------------------------
	auto log = XrdCl::DefaultEnv::GetLog();

	    const size_t size = objcfg.plgr.size();
	    //-------------------------------------------------------------------------
	    // prepare the metadata (the Central Directory of each data ZIP)
	    //-------------------------------------------------------------------------
	    auto zipbuff = objcfg.nomtfile ? nullptr :
	                   std::make_shared<XrdZip::buffer_t>( GetMetadataBuffer() );
	    //-------------------------------------------------------------------------
	    // prepare the pipelines ...
	    //-------------------------------------------------------------------------
	    std::vector<XrdCl::Pipeline> closes;
	    std::vector<XrdCl::Pipeline> save_metadata;
	    closes.reserve( size );
	    std::string closeTime = std::to_string( time(NULL) );

	    // since the filesize was already determined in previous writes we keep it
	    //XrdCl::Pipeline p2 = ReadSize(0);
	    //XrdCl::Async(std::move(p2));

	    std::vector<XrdCl::xattr_t> xav{ {"xrdec.filesize", std::to_string(filesize)},
	                                     {"xrdec.strpver", closeTime.c_str()},
										 {"xrdec.corrupted", std::to_string(0)}};

	    for( size_t i = 0; i < size; ++i )
	    {
	    // we only close written archives since any replaced archives are closed in InvalidateReplaceArchive method
	      //-----------------------------------------------------------------------
	      // close ZIP archives with data
	      //-----------------------------------------------------------------------
	      if( writeDataarchs[objcfg.GetDataUrl(i)]->IsOpen() )
	      {
	    	  XrdCl::Pipeline p = XrdCl::SetXAttr( writeDataarchs[objcfg.GetDataUrl(i)]->GetFile(), xav )
	                          | XrdCl::CloseArchive( *writeDataarchs[objcfg.GetDataUrl(i)] );
	        closes.emplace_back( std::move( p ) );
	      }
	      else if(writeDataarchs[objcfg.GetDataUrl(i)]->archsize > 0){
	    	  log->Error(XrdCl::XRootDMsg, "Archive %s not open but rather %d", objcfg.GetDataUrl(i), writeDataarchs[objcfg.GetDataUrl(i)]->openstage );
	    	  XrdCl::Pipeline p = XrdCl::SetXAttr( writeDataarchs[objcfg.GetDataUrl(i)]->GetFile(), xav )
	    	  	                          | XrdCl::CloseArchive( *writeDataarchs[objcfg.GetDataUrl(i)] );
	    	  closes.emplace_back( std::move( p ) );
	      }
	      //-----------------------------------------------------------------------
	      // replicate the metadata
	      //-----------------------------------------------------------------------
	      if( zipbuff )
	      {
	        std::string url = objcfg.GetMetadataUrl( i );
	        metadataarchs.emplace_back( std::make_shared<XrdCl::File>(
	            Config::Instance().enable_plugins ) );
	        XrdCl::Pipeline p = XrdCl::Open( *metadataarchs[i], url, XrdCl::OpenFlags::New | XrdCl::OpenFlags::Write )
	                          | XrdCl::Write( *metadataarchs[i], 0, zipbuff->size(), zipbuff->data() )
	                          | XrdCl::Close( *metadataarchs[i] )
	                          | XrdCl::Final( [zipbuff]( const XrdCl::XRootDStatus& ){ } );

	        save_metadata.emplace_back( std::move( p ) );
	      }
	    }

	    auto pipehndl = [=](const XrdCl::XRootDStatus &st) { // set the central directories in ZIP archives (if we use metadata files)
	    		auto itr = writeDataarchs.begin();
	    		for (; itr != writeDataarchs.end(); ++itr) {
	    			auto &zipptr = itr->second;
	    			auto url = itr->first;
	    			if(zipptr->archsize > 0 && zipptr->openstage != XrdCl::ZipArchive::None){
	    				log->Error(XrdCl::XRootDMsg, "Archive wasn't properly closed: %s, status %d", url, zipptr->openstage );
	    			}
	    		}
	    		if (handler)
	    			handler->HandleResponse(new XrdCl::XRootDStatus(st), nullptr);
	    	};

	    //-------------------------------------------------------------------------
	    // If we were instructed not to create the the additional metadata file
	    // do the simplified close
	    //-------------------------------------------------------------------------
	    if( save_metadata.empty() )
	    {
	      XrdCl::Pipeline p = XrdCl::Parallel( closes ).AtLeast( objcfg.nbchunks )| XrdCl::Final(pipehndl);
	      XrdCl::Async( std::move( p ), timeout );
	      return;
	    }

	    //-------------------------------------------------------------------------
	    // compose closes & save_metadata:
	    //  - closes must be successful at least for #data + #parity
	    //  - save_metadata must be successful at least for #parity + 1
	    //-------------------------------------------------------------------------
	    XrdCl::Pipeline p = XrdCl::Parallel(
	        XrdCl::Parallel( closes ).AtLeast( objcfg.nbchunks ),
	        XrdCl::Parallel( save_metadata ).AtLeast( objcfg.nbparity + 1 )
	      );
	    XrdCl::Async( std::move( p ), timeout );
}

void RepairTool::TryOpen(XrdCl::ResponseHandler *handler, XrdCl::OpenFlags::Flags flags, uint16_t timeout){
	const size_t size = objcfg.plgr.size();
	std::vector<XrdCl::Pipeline> opens;
	opens.reserve(size);
	std::vector<XrdCl::Pipeline> healthRead;
	healthRead.reserve(size);
	std::shared_ptr<std::atomic<bool>> userBlocked = std::make_shared<std::atomic<bool>>();
	userBlocked->store(false, std::memory_order_relaxed);
	for (size_t i = 0; i < size; ++i)
	{
		// generate the URL
		std::string url = objcfg.GetDataUrl(i);
		auto archive = std::make_shared<XrdCl::ZipArchive>(
				Config::Instance().enable_plugins);
		// create the file object
		readDataarchs.emplace(url, archive);
		//writeDataarchs.emplace(url, archive);
		// open the archive
		if (objcfg.nomtfile)
		{
			opens.emplace_back(
					XrdCl::OpenArchive(*readDataarchs[url], url, flags)

					>> [userBlocked](const XrdCl::XRootDStatus &st)
					{
						// if some user currently reads from that archive, TODO: Which code?
							if(!st.IsOK())
							{
								if(st.errNo == XErrorCode::kXR_FileLocked)
								{
									userBlocked->store(true, std::memory_order_relaxed);
									std::stringstream ss;
									XrdCl::DefaultEnv::GetLog()->Error(XrdCl::XRootDMsg, "File is locked");
								}
								else
								{
									std::stringstream ss;
									ss << "Zip Open failed: " << st.ToString() << "\n" << std::flush;
									XrdCl::DefaultEnv::GetLog()->Error(XrdCl::XRootDMsg, &(ss.str()[0]));
								}
							}
						}
								);
		}
		else
			opens.emplace_back(OpenOnly(*readDataarchs[url], url, true));
		healthRead.emplace_back(CheckHealthExists(i));
	}

	auto pipehndl =
			[=](const XrdCl::XRootDStatus &st)
			{ // set the central directories in ZIP archives (if we use metadata files)
						if (userBlocked->load() || !st.IsOK())
						{
							if(userBlocked->load())
								XrdCl::DefaultEnv::GetLog()->Error(XrdCl::XRootDMsg, "Blocked by user");
							else{
								std::stringstream s;
								s << "Archive opening pipeline error: " << st.GetErrorMessage();
								XrdCl::DefaultEnv::GetLog()->Error(XrdCl::XRootDMsg, &(s.str()[0]));
							}
							if(handler)handler->HandleResponse(new XrdCl::XRootDStatus(XrdCl::stError, "One or more archives blocked by read requests"), nullptr);
							return;
						}
						auto itr = readDataarchs.begin();
						for (; itr != readDataarchs.end(); ++itr)
						{
							const std::string &url = itr->first;
							auto &zipptr = itr->second;
							XrdCl::DefaultEnv::GetLog()->Debug(XrdCl::XRootDMsg, "Archive with status %d", zipptr->openstage);
							if (zipptr->openstage == XrdCl::ZipArchive::Done && zipptr->archsize == 0)
							{
								XrdCl::DefaultEnv::GetLog()->Debug(XrdCl::XRootDMsg, "Found empty archive");
								continue;
							}
							// this only happens for mtfiles
							if (zipptr->openstage == XrdCl::ZipArchive::NotParsed)
								zipptr->SetCD(metadata[url]);
							else if (zipptr->openstage != XrdCl::ZipArchive::Done)
							{
								ReplaceURL(url);
								if(!metadata.empty())
									AddMissing(metadata[url]);
							}
							if(zipptr->openstage == XrdCl::ZipArchive::Done)
							{
								auto itr = zipptr->cdmap.begin();
								for (; itr != zipptr->cdmap.end(); ++itr)
								{
									try
									{
										size_t blknb = fntoblk(itr->first);
										urlmap.emplace(itr->first, url);
										if (blknb > lstblk)
											lstblk = blknb;
									}
									catch (std::invalid_argument&)
									{
										XrdCl::DefaultEnv::GetLog()->Error(XrdCl::XRootDMsg, "Invalid file name detected.");

									}
								}
							}
						}
						metadata.clear();
						auto sem = std::make_shared<XrdSysSemaphore>(0);
						{
							std::shared_ptr<ThreadEndSemaphore> ptr = std::make_shared<ThreadEndSemaphore>(sem);
							// Check that all LFH and CDFH are correct
							CheckAllMetadata(ptr, timeout);
						}
						sem->Wait();
						// call user handler
						if (handler)
						{
							handler->HandleResponse(new XrdCl::XRootDStatus(), nullptr);
						}
					};
	// in parallel open the data files and read the metadata, only tested for nomtfile
	XrdCl::Pipeline p =
			objcfg.nomtfile ?
					XrdCl::Parallel(opens).AtLeast(objcfg.nbdata)
							| XrdCl::Parallel(healthRead).AtLeast(objcfg.nbdata)
							| ReadSize(0) | XrdCl::Final(pipehndl) :
					XrdCl::Parallel(ReadMetadata(0),
							XrdCl::Parallel(opens).AtLeast(objcfg.nbdata))
							>> pipehndl;
	XrdCl::Async(std::move(p), timeout);
}

XrdCl::Pipeline RepairTool::CheckHealthExists(size_t index){
	  std::string url = objcfg.GetDataUrl( index );
	  		  return XrdCl::ListXAttr(readDataarchs[url]->GetFile()) >>
	  				  [index, url, this] (XrdCl::XRootDStatus &st, std::vector<XrdCl::XAttr> attrs){
	  			  	  	 for(auto it = attrs.begin(); it != attrs.end(); it++){
	  			  	  		 if(it->name == "xrdec.corrupted"){
	  			  	  			 XrdCl::DefaultEnv::GetLog()->Debug(XrdCl::XRootDMsg, "Found corrupted flag, reading value");
	  			  	  			 XrdCl::Pipeline::Replace(ReadHealth(index));
	  			  	  		 }
	  			  	  	 }
	  		  };
  }

  XrdCl::Pipeline RepairTool::ReadHealth(size_t index){
		  std::string url = objcfg.GetDataUrl( index );
		  return XrdCl::GetXAttr( readDataarchs[url]->GetFile(), "xrdec.corrupted" ) >>
	          [index, url, this]( XrdCl::XRootDStatus &st, std::string &damage)
	          {
				if (st.IsOK()) {
					try {
						int damaged = std::stoi(damage);
						if (damaged > 0){
							this->readDataarchs[url]->openstage = XrdCl::ZipArchive::Error;
							XrdCl::DefaultEnv::GetLog()->Error(XrdCl::XRootDMsg, "Found corrupted archive %s", url);
						}
					} catch (std::invalid_argument&) {
						return;
					}
				}
				return;
	          };
  }

void RepairTool::CheckAllMetadata(std::shared_ptr<ThreadEndSemaphore> sem, uint16_t timeout) {
	uint64_t numBlocks = ceil(
			(filesize / (float)objcfg.chunksize) / objcfg.nbdata);
	for (size_t blkid = 0; blkid < numBlocks; blkid++) {
		std::vector<size_t> stripesMissing;
		for (size_t strpid = 0; strpid < objcfg.nbdata + objcfg.nbparity;
				strpid++) {
			std::string fn = objcfg.GetFileName(blkid, strpid);
				// if the block/stripe does not exist it means we are reading passed the end of the file
				auto itr = urlmap.find(fn);
				if (itr == urlmap.end()) {
				}
				else CompareLFHToCDFH(sem, blkid, strpid, timeout);
		}
	}
}

void RepairTool::InvalidateReplaceArchive(const std::string &url, std::shared_ptr<XrdCl::ZipArchive> zipptr, uint16_t timeout){
	XrdCl::Pipeline p;
	if(zipptr->IsOpen()){
		std::vector<XrdCl::xattr_t> xav{ {"xrdec.corrupted", std::to_string(1)} };
		p = XrdCl::SetXAttr( zipptr->archive, xav )
		                          | XrdCl::CloseArchive( *zipptr);
		XrdCl::Async(std::move(p), timeout);
	}

}

void RepairTool::CompareLFHToCDFH(std::shared_ptr<ThreadEndSemaphore> sem, uint16_t blkid, uint16_t strpid, uint16_t timeout){
	std::string fn = objcfg.GetFileName(blkid, strpid);
	// if the block/stripe does not exist it means we are reading passed the end of the file
	auto itr = urlmap.find(fn);
	if (itr == urlmap.end()) {
		return;
	}
	// get the URL of the ZIP archive with the respective data
	const std::string &url = itr->second;
	if (redirectionMap.find(url) != redirectionMap.end()) {
		// the url was already marked as non existant / not reachable, skip
		return;
	}
	if(readDataarchs.find(url) == readDataarchs.end()){
		// the url doesn't exist / not reachable, we need to redirect.
		// Should be caught by previous OpenInUpdateMode
		ReplaceURL(url);
	}
	std::shared_ptr<XrdCl::ZipArchive> &zipptr = readDataarchs[url];
	if(zipptr->openstage != XrdCl::ZipArchive::Done){
		ReplaceURL(url);
	}
	auto cditr = zipptr->cdmap.find(fn);
	if (cditr == zipptr->cdmap.end()) {
		ReplaceURL(url);
		return;
	}

	XrdZip::CDFH *cdfh = zipptr->cdvec[cditr->second].get();
	uint32_t offset = XrdZip::CDFH::GetOffset(*cdfh);
	uint64_t cdOffset =
			zipptr->zip64eocd ?
					zipptr->zip64eocd->cdOffset : zipptr->eocd->cdOffset;
	uint64_t nextRecordOffset =
			(cditr->second + 1 < zipptr->cdvec.size()) ?
					XrdZip::CDFH::GetOffset(*zipptr->cdvec[cditr->second + 1]) :
					cdOffset;
	int64_t readSize = (nextRecordOffset - offset) - cdfh->compressedSize;

	if(readSize < 0){
		ReplaceURL(url);
		return;
	}


	std::shared_ptr<buffer_t> lfhbuf;
	lfhbuf = std::make_shared<buffer_t>();
	lfhbuf->reserve(readSize);

	std::shared_ptr<ThreadEndSemaphore> localSem(sem);

	auto pipehndl = [=](const XrdCl::XRootDStatus &st) {
					if (st.IsOK() && localSem != nullptr) {
						try {
							XrdZip::LFH lfh(lfhbuf->data(), readSize);
							if (lfh.ZCRC32 != cdfh->ZCRC32|| lfh.compressedSize != cdfh->compressedSize
									|| lfh.compressionMethod != cdfh->compressionMethod
									|| lfh.extraLength != cdfh->extraLength
									|| lfh.filename != cdfh->filename
									|| lfh.filenameLength != cdfh->filenameLength
									|| lfh.generalBitFlag != cdfh->generalBitFlag
									|| lfh.minZipVersion != cdfh->minZipVersion
									|| lfh.uncompressedSize != cdfh->uncompressedSize) {
								// metadata damaged, mark archive as damaged and replace url
								ReplaceURL(url);
								return;
							}
							//---------------------------------------------------
							// All is good
							//---------------------------------------------------
							else {
								return;
							}
						} catch (const std::exception &e) {
							// Couldn't parse metadata, metadata damaged, same case.
							ReplaceURL(url);
							return;
						}
					} else {
						// Couldn't even read from file?! Shouldn't happen
						ReplaceURL(url);
						return;
					}
				};

	XrdCl::Pipeline p = XrdCl::Read(zipptr->archive, offset, readSize, lfhbuf->data()) | XrdCl::Final(pipehndl);

	Async( std::move( p ), timeout );

}

void RepairTool::ReplaceURL(const std::string &url){
	urlMutex.lock();
	if(redirectionMap.find(url) != redirectionMap.end()){
		urlMutex.unlock();
		return;
	}
	if (objcfg.plgrReplace.size() > currentReplaceIndex) {
			const std::string replacementPlgr = objcfg.plgrReplace[currentReplaceIndex];
			// save a mapping from old to new urls so user knows where actual data is
			std::stringstream ss;
			ss << "Replaced archive: " << url << ", " << replacementPlgr << "\n" << std::flush;
			XrdCl::DefaultEnv::GetLog()->Debug(XrdCl::XRootDMsg, &(ss.str()[0]));

			redirectionMap[url] = replacementPlgr;
			currentReplaceIndex++;
		}else{
			XrdCl::DefaultEnv::GetLog()->Error(XrdCl::XRootDMsg,"Critical error, can't find replacement host for %s", url );
			redirectionMap[url] = "null";
		}
	urlMutex.unlock();
}

//-----------------------------------------------------------------------
// Add all the entries from given Central Directory to missing
//-----------------------------------------------------------------------
void RepairTool::AddMissing(const buffer_t &cdbuff) {
	const char *buff = cdbuff.data();
	size_t size = cdbuff.size();
	// parse Central Directory records
	XrdZip::cdvec_t cdvec;
	XrdZip::cdmap_t cdmap;
	std::tie(cdvec, cdmap) = XrdZip::CDFH::Parse(buff, size);
	auto itr = cdvec.begin();
	for (; itr != cdvec.end(); ++itr) {
		XrdZip::CDFH &cdfh = **itr;
		missing.insert(cdfh.filename);
	}
}

//-----------------------------------------------------------------------
//! Read size from xattr
//!
//! @param index : placement's index
//-----------------------------------------------------------------------
XrdCl::Pipeline RepairTool::ReadSize(size_t index) {
	std::string url = objcfg.GetDataUrl(index);
	return XrdCl::GetXAttr(readDataarchs[url]->GetFile(), "xrdec.filesize")
			>> [index, this](XrdCl::XRootDStatus &st, std::string &size) {
		 XrdCl::DefaultEnv::GetLog()->Debug(XrdCl::XRootDMsg,"Checking Size.." );
				if (!st.IsOK()) {
					XrdCl::DefaultEnv::GetLog()->Error(XrdCl::XRootDMsg,"Cant read file size" );
					//-------------------------------------------------------------
					// Check if we can recover the error or a diffrent location
					//-------------------------------------------------------------
					if (index + 1 < objcfg.plgr.size())
						XrdCl::Pipeline::Replace(ReadSize(index + 1));
					return;
				}
				auto newFileSize = std::stoull(size);
				if(newFileSize == 0){
					XrdCl::DefaultEnv::GetLog()->Error(XrdCl::XRootDMsg,"Read file size 0" );
					if (index + 1 < objcfg.plgr.size())
						XrdCl::Pipeline::Replace(ReadSize(index + 1));
					return;
				}
				else {
					filesize = newFileSize;
					XrdCl::DefaultEnv::GetLog()->Debug(XrdCl::XRootDMsg,"Filesize set to %d", filesize );
				}
			};
}

//-----------------------------------------------------------------------
// Parse metadata from chunk info object
//-----------------------------------------------------------------------
bool RepairTool::ParseMetadata(XrdCl::ChunkInfo &ch) {
	const size_t mincnt = objcfg.nbdata + objcfg.nbparity;
	const size_t maxcnt = objcfg.plgr.size();

	char *buffer = reinterpret_cast<char*>(ch.buffer);
	size_t length = ch.length;

	for (size_t i = 0; i < maxcnt; ++i) {
		uint32_t signature = XrdZip::to<uint32_t>(buffer);
		if (signature != XrdZip::LFH::lfhSign) {
			if (i + 1 < mincnt)
				return false;
			break;
		}
		XrdZip::LFH lfh(buffer);
		// check if we are not reading passed the end of the buffer
		if (lfh.lfhSize + lfh.uncompressedSize > length)
			return false;
		buffer += lfh.lfhSize;
		length -= lfh.lfhSize;
		// verify the checksum
		uint32_t crc32val = objcfg.digest(0, buffer, lfh.uncompressedSize);
		if (crc32val != lfh.ZCRC32)
			return false;
		// keep the metadata
		std::string url = objcfg.GetDataUrl(std::stoull(lfh.filename));
		metadata.emplace(url, buffer_t(buffer, buffer + lfh.uncompressedSize));
		buffer += lfh.uncompressedSize;
		length -= lfh.uncompressedSize;
	}

	return true;
}

//-----------------------------------------------------------------------
// Read metadata for the object
//-----------------------------------------------------------------------
XrdCl::Pipeline RepairTool::ReadMetadata(size_t index) {
	const size_t size = objcfg.plgr.size();
	// create the File object
	auto file = std::make_shared<XrdCl::File>(
			Config::Instance().enable_plugins);
	// prepare the URL for Open operation
	std::string url = objcfg.GetMetadataUrl(index);
	// arguments for the Read operation
	XrdCl::Fwd<uint32_t> rdsize;
	XrdCl::Fwd<void*> rdbuff;

	return XrdCl::Open(*file, url, XrdCl::OpenFlags::Read)
			>> [=](XrdCl::XRootDStatus &st, XrdCl::StatInfo &info) mutable {
				if (!st.IsOK()) {
					if (index + 1 < size)
						XrdCl::Pipeline::Replace(ReadMetadata(index + 1));
					return;
				}
				// prepare the args for the subsequent operation
				rdsize = info.GetSize();
				rdbuff = new char[info.GetSize()];
			}
			| XrdCl::Read(*file, 0, rdsize, rdbuff)
					>> [=](XrdCl::XRootDStatus &st, XrdCl::ChunkInfo &ch) {
						if (!st.IsOK()) {
							if (index + 1 < size)
								XrdCl::Pipeline::Replace(
										ReadMetadata(index + 1));
							return;
						}
						// now parse the metadata
						if (!ParseMetadata(ch)) {
							if (index + 1 < size)
								XrdCl::Pipeline::Replace(
										ReadMetadata(index + 1));
							return;
						}
					} | XrdCl::Close(*file) >> [](XrdCl::XRootDStatus &st) {
				if (!st.IsOK())
					XrdCl::Pipeline::Ignore(); // ignore errors, we don't really care
			} | XrdCl::Final([rdbuff, file](const XrdCl::XRootDStatus&) {
				// deallocate the buffer if necessary
				if (rdbuff.Valid()) {
					char *buffer = reinterpret_cast<char*>(*rdbuff);
					delete[] buffer;
				}
			});
}

void RepairTool::Read( size_t blknb, size_t strpnb, buffer_t &buffer, callback_t cb, uint16_t timeout )
{
  // generate the file name (blknb/strpnb)
  std::string fn = objcfg.GetFileName( blknb, strpnb );
  // if the block/stripe does not exist it means we are reading passed the end of the file
  auto itr = urlmap.find( fn );
  if( itr == urlmap.end() )
  {
    auto st = !IsMissing( fn ) ? XrdCl::XRootDStatus() :
              XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errNotFound );
    ThreadPool::Instance().Execute( cb, st, 0 );
    return;
  }
  // get the URL of the ZIP archive with the respective data
  const std::string &url = itr->second;

  if(redirectionMap.find(url) != redirectionMap.end()){
	  auto st = XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errRedirect);
	  ThreadPool::Instance().Execute(cb, st, 0);
	  return;
  }

  // get the ZipArchive object
  auto &zipptr = readDataarchs[url];
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
  XrdCl::Async( XrdCl::ReadFrom( *zipptr, fn, 0, rdsize, buffer.data() ) >>
                  [zipptr, fn, cb, &buffer, this, url, timeout]( XrdCl::XRootDStatus &st, XrdCl::ChunkInfo &ch )
                  {
                    //---------------------------------------------------
                    // If read failed there's nothing to do, just pass the
                    // status to user callback
                    //---------------------------------------------------
                    if( !st.IsOK() )
                    {
                      cb( XrdCl::XRootDStatus(st.status, "Read failed"), 0 );
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
                  	  cb( XrdCl::XRootDStatus( XrdCl::stError, "Chksum unequal" ), 0 );
                      return;
                    }
								//---------------------------------------------------
								// All is good, we can call now the user callback
								//---------------------------------------------------
								cb(XrdCl::XRootDStatus(), ch.length);
								return;

                  }, timeout );
}

//-----------------------------------------------------------------------
//! Check if chunk file name is missing
//-----------------------------------------------------------------------
bool RepairTool::IsMissing( const std::string &fn )
{
  // if the chunk is in the missing set return true
  if( missing.count( fn ) ) return true;
  // if we don't have a metadata file and the chunk exceeds last chunk
  // also return true
  try{
      if( objcfg.nomtfile && fntoblk( fn ) <= lstblk ) return true;}
      catch(...){}// otherwise return false
  return false;
}


}
