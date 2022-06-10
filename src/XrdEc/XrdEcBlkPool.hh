#ifndef SRC_XRDEC_XRDECBLKPOOL_HH_
#define SRC_XRDEC_XRDECBLKPOOL_HH_

#include "XrdEc/XrdEcUtilities.hh"
#include "XrdEc/XrdEcObjCfg.hh"
#include "XrdEc/XrdEcConfig.hh"
#include "XrdEc/XrdEcThreadPool.hh"
#include "XrdEc/XrdEcReader.hh"

#include <vector>
#include <condition_variable>
#include <mutex>
#include <future>

namespace XrdEc
{
class BlockPool
{
public:

	static BlockPool& Instance()
	{
		static BlockPool instance;
		return instance;
	}

	std::shared_ptr<block_t> Create(ObjCfg &objcfg, Reader &reader, size_t blkid)
	{
		std::unique_lock<std::mutex> lck(mtx);
		//---------------------------------------------------------------------
		// If pool is not empty, recycle existing buffer
		//---------------------------------------------------------------------
		if (!pool.empty())
		{
			std::shared_ptr<block_t> block(std::move(pool.front()));
			pool.pop();

			// almost what the block constructor does except we dont make new stripes.
			//block->reader = reader;
			block->blkid = blkid;
			block->state = std::vector<block_t::state_t>(objcfg.nbchunks, block_t::Empty);
			block->pending = std::vector<block_t::pending_t>(objcfg.nbchunks);
			block->recovering = 0;
			block->redirectionIndex = 0;

			return std::move(block);
		}
		//---------------------------------------------------------------------
		// Check if we can create a new buffer object without exceeding the
		// the maximum size of the pool
		//---------------------------------------------------------------------
		if (currentsize < totalsize)
		{
			std::shared_ptr<block_t> block = std::make_shared<block_t>(blkid, reader, objcfg);
			++currentsize;
			return std::move(block);
		}
		//---------------------------------------------------------------------
		// If not, we have to wait until there is a buffer we can recycle
		//---------------------------------------------------------------------
		while (pool.empty())
			cv.wait(lck);
		std::shared_ptr<block_t> block(std::move(pool.front()));
		pool.pop();

		// almost what the block constructor does except we dont make new stripes.
		//block->reader = reader;
		block->blkid = blkid;
		block->state = std::vector<block_t::state_t>(objcfg.nbchunks,
				block_t::Empty);
		block->pending = std::vector<block_t::pending_t>(objcfg.nbchunks);
		block->recovering = 0;
		block->redirectionIndex = 0;

		return std::move(block);
	}

	//-----------------------------------------------------------------------
	//! Give back a buffer to the poool
	//-----------------------------------------------------------------------
	void Recycle(std::shared_ptr<block_t> block)
	{
		//if (block invalid)
		//	return;
		std::unique_lock<std::mutex> lck(mtx);

		pool.emplace(std::move(block));
		cv.notify_all();
	}

private:

	//-----------------------------------------------------------------------
	// Default constructor
	//-----------------------------------------------------------------------
	BlockPool() :
			totalsize(1024), currentsize(0)
	{
	}

	BlockPool(const BlockPool&) = delete;            //< Copy constructor
	BlockPool(BlockPool&&) = delete;                 //< Move constructor
	BlockPool& operator=(const BlockPool&) = delete; //< Copy assigment operator
	BlockPool& operator=(BlockPool&&) = delete;      //< Move assigment operator

	const size_t totalsize;   //< maximum size of the pool
	size_t currentsize; //< current size of the pool
	std::condition_variable cv;
	std::mutex mtx;
	std::queue<std::shared_ptr<block_t>> pool;        //< the pool itself
};
}

#endif /* SRC_XRDEC_XRDECBLKPOOL_HH_ */
