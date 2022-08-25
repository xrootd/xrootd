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
#ifndef SRC_XRDEC_XRDECREPAIRTOOL_HH_
#define SRC_XRDEC_XRDECREPAIRTOOL_HH_

#include "XrdEc/XrdEcWrtBuff.hh"
#include "XrdEc/XrdEcThreadPool.hh"

#include "XrdEc/XrdEcReader.hh"

#include "XrdCl/XrdClFileOperations.hh"
#include "XrdCl/XrdClParallelOperation.hh"
#include "XrdCl/XrdClZipOperations.hh"

#include "XrdEc/XrdEcObjCfg.hh"

#include "XrdCl/XrdClZipArchive.hh"
#include "XrdCl/XrdClOperations.hh"

#include <string>
#include <unordered_map>
#include <unordered_set>

#include <sys/stat.h>

namespace XrdEc {

struct ThreadEndSemaphore{
	ThreadEndSemaphore(std::shared_ptr<XrdSysSemaphore> s) : sem(s) {}
	~ThreadEndSemaphore(){sem->Post();}
	std::shared_ptr<XrdSysSemaphore> sem;
};

class RepairTool {

public:
	RepairTool(ObjCfg &objcfg, uint16_t _bufferLimit = 512) :
			objcfg(objcfg), reader(objcfg), lstblk(0), filesize(0),
			checkAfterRepair(false){
		currentBlockChecked.store(0);
		currentReplaceIndex = 0;
		chunksRepaired.store(0);
		repairFailed = false;
		finishedRepair = false;
		chunkRepairsWritten.store(0);
		bufferLimit = _bufferLimit;
		currentBuffers = 0;
		st = nullptr;
	}
	virtual ~RepairTool() {
	}
	/*
	 * Repairs the file specified in the objcfg by overwriting or writing to a new host
	 * Specify replacementPlgr in the objcfg!
	 */
	void RepairFile(bool checkAgainAfterRepair, XrdCl::ResponseHandler *handler, uint16_t timeout = 0);
	/*
	 * Checks the file specified in the objcfg
	 * Specify replacementPlgr in the objcfg even though they won't be written to.
	 */
	void CheckFile(XrdCl::ResponseHandler *handler, uint16_t timeout = 0);
	/*
	 * The number of blocks that have read all of their stripes but possibly not written to disk yet
	 */
	std::atomic<uint32_t> currentBlockChecked;
	/*
	 * amount of chunks that have to be overwritten or rewritten
	 */
	std::atomic<uint64_t> chunksRepaired;
	/*
	 * number of chunk writes that have terminated successfully.
	 */
	std::atomic<uint64_t> chunkRepairsWritten;
	/*
	 * Did the repair fail at some point (e.g. non restorable stripe)
	 */
	bool repairFailed;
private:

	/**
	 * Initializes read/write Dataarchs, opens them and checks whether any need to be replaced.
	 * @param handler
	 * @param timeout
	 */
	void OpenInUpdateMode(XrdCl::ResponseHandler *handler,
				uint16_t timeout = 0);
	/**
	 * Opens read Dataarchs and fills the redirection map, but doesn't create writeDataarchs yet.
	 * @param handler
	 * @param timeout
	 */
	void TryOpen(XrdCl::ResponseHandler *handler, XrdCl::OpenFlags::Flags flags, uint16_t timeout = 0);
    //-----------------------------------------------------------------------
    //! Read size from xattr
    //!
    //! @param index : placement's index
    //-----------------------------------------------------------------------
    XrdCl::Pipeline ReadSize( size_t index );
    /**
     * Checks whether the "corrupted" flag exists, if yes, ReadHealth is called
     * @param index
     * @return
     */
    XrdCl::Pipeline CheckHealthExists(size_t index);
    /**
     * Reads the XAttr "xrdec.corrupted". If it is > 0, the archive's openstage is set to error.
     * @param index
     * @return
     */
    XrdCl::Pipeline ReadHealth(size_t index);
	/**
	 * Checks all LFHs against their CDFHs
	 * @param sem Is passed to each check to increase reference count
	 */
	void CheckAllMetadata(std::shared_ptr<ThreadEndSemaphore> sem, uint16_t timeout);
	/**
	 * Reads and compares LFHs to the already read CDFHs and replaces and closes archives that have faulty metadata.
	 * @param sem
	 * @param blkid
	 * @param strpid
	 */
	void CompareLFHToCDFH(std::shared_ptr<ThreadEndSemaphore> sem, uint16_t blkid, uint16_t strpid, uint16_t timeout);
	/**
	 * Replaces archive with one on a different host, then closes old archive and marks it as corrupted.
	 * @param url
	 * @param zipptr
	 */
	void InvalidateReplaceArchive(const std::string &url, std::shared_ptr<XrdCl::ZipArchive> zipptr, uint16_t timeout);
	/**
	 * Creates a new archive on a different host, adds an entry to redirectionMap but writeDataarchs keeps it in the same url entry for easier referencing.
	 * @param url
	 */
	void ReplaceURL(const std::string &url);

    //-----------------------------------------------------------------------
    //! Checks the block at currentBlockIndex and executes error correction
    //-----------------------------------------------------------------------
	void CheckBlock(uint16_t timeout);
	/**
	 * Checks all stripes of the current block. Returns false if the block is finished (in positive or negative way).
	 * @param self
	 * @param writer
	 * @return
	 */
	static bool error_correction( std::shared_ptr<block_t> &self, RepairTool *writer, uint16_t timeout );
    /**
     * Initiates the actual read from disk, calls update_callback afterwards
     * @param blknb
     * @param strpnb
     * @param buffer
     * @param cb
     * @param timeout
     * @param exactControl
     */
	void Read( size_t blknb, size_t strpnb, std::shared_ptr<buffer_t> buffer, callback_t cb, uint16_t timeout = 0);
	/**
	 * Initiates the actual read from disk, calls update_callback afterwards
	 * @param blknb
	 * @param strpnb
	 * @param buffer
	 * @param cb
	 * @param timeout
	 * @param exactControl
	 */
	void Read( size_t blknb, size_t strpnb, buffer_t &buffer, callback_t cb, uint16_t timeout = 0);
	/**
	 * Sets the state of the stripe we read to okay or missing and calls error correction again.
	 * @param self
	 * @param tool
	 * @param strpid
	 * @return
	 */
	static callback_t update_callback(std::shared_ptr<block_t> &self, RepairTool *tool, size_t strpid, uint16_t timeout);
	/**
	 * Used for CheckFile: If the read was unsuccessful due to corrupted data (checksum violation) message the user.
	 * @param self
	 * @param tool
	 * @param strpid
	 * @return
	 */
	static callback_t read_callback(std::shared_ptr<ThreadEndSemaphore> sem, size_t blkid, size_t strpid, RepairTool *tool);
	/**
	 * Writes the content of the stripe to its corresponding writeDataarch by writing into or appending.
	 * @param blk
	 * @param strpid
	 * @return
	 */
	XrdCl::XRootDStatus WriteChunk(std::shared_ptr<block_t> blk, size_t strpid, uint16_t timeout);

	/**
	 * Closes all archives in writeDataarchs and sets their corrupted flag to 0.
	 * @param handler
	 * @param timeout
	 */
	void CloseAllArchives(XrdCl::ResponseHandler *handler, uint16_t timeout = 0);



    void AddMissing(const buffer_t &cdbuff);
    bool IsMissing(const std::string &fn);

    // not really used since we don't have a metadata file

	XrdZip::buffer_t GetMetadataBuffer();
	XrdCl::Pipeline ReadMetadata( size_t index );
    //-----------------------------------------------------------------------
    //! Parse metadata from chunk info object
    //!
    //! @param ch : chunk info object returned by a read operation
    //-----------------------------------------------------------------------
    bool ParseMetadata( XrdCl::ChunkInfo &ch );


	ObjCfg &objcfg;
	// unused reader only for initialization of block_t
	Reader reader;
	std::vector<std::shared_ptr<XrdCl::File>>        metadataarchs;      //< ZIP archives with metadata
	Reader::dataarchs_t readDataarchs; //> map URL to ZipArchive object
	Reader::dataarchs_t writeDataarchs; //> map URL to ZipArchives for writing (may be different!)
	Reader::metadata_t metadata;  //> map URL to CD metadata
	Reader::urlmap_t urlmap;    //> map blknb/strpnb (data chunk) to URL
	Reader::urlmap_t redirectionMap; //> map corrupted url to new url
	size_t currentReplaceIndex;
	Reader::missing_t missing;   //> set of missing stripes
	std::shared_ptr<block_t> block;  //> cache for the block we are reading from
	std::mutex blkmtx;    //> mutex guarding the block from parallel access
	size_t lstblk;    //> last block number
	uint64_t filesize;  //> file size (obtained from xattr)

	// for replacing URLs
	std::mutex urlMutex;

	XrdCl::XRootDStatus* st;

	/*
	 * The mutex for access to the finishedRepair variable and the condition variable
	 */
	std::mutex finishedRepairMutex;
	/*
	 * Locked by finishedRepirMutex and waits until finishedRepair and written == demanded and blocksChecked == totalBlocks
	 */
	std::condition_variable repairVar;
	/*
	 * Indicates that all block repair have been initialized
	 */
	bool finishedRepair;

	/*
	 * If true, check the whole file again after repair to confirm everything was written successfully and correctly
	 */
	bool checkAfterRepair;

	/*
	 * Limits the number of buffers for the check file function
	 */
	uint16_t bufferLimit;
	/*
	 * current amount of buffers in checkFile
	 */
	uint16_t currentBuffers;
	std::mutex bufferCountMutex;
	std::condition_variable waitBuffers;

};
}

#endif
