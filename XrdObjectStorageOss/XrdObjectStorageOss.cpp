#include "XrdObjectStorageOss.h"

extern XrdSysError OssEroute;

////////////////////////////////////////////////////////
int NotSupported(String error_message) {
	PrintInfo("***** Error", error_message);
	return -ENOTSUP;
}

int InputOutputError(String error_message) {
	PrintInfo("***** Error", error_message);
	return -EIO;
}

int NoEntity(String error_message) {
	PrintInfo("***** Error", error_message);
	return -ENOENT;
}


int Ok() {
	PrintInfo("***** OK");
	return XrdOssOK;
}

////////////////////////////////////////////////////////
extern "C" XrdOss* XrdOssGetStorageSystem(XrdOss* native_oss, XrdSysLogger* logger, const char* config_fn, const char* parms)
{
	OssEroute.SetPrefix("XrdObjectStorageOss_");
	OssEroute.logger(logger);

	XrdObjectStorageOss* ret = new XrdObjectStorageOss();

	ret->eDest = &OssEroute;
	ret->eDest->logger(logger);

	if (ret->Init(logger, config_fn) != XrdOssOK)
	{
		assert(false);
		return nullptr;
	}

	return ret;
}


//-----------------------------------------------------------------------------
//! Initialize the storage system V2.
//!
//! @param  lp     - Pointer to the message logging object.
//! @param  cfn    - Pointer to the configuration file.
//! @param  envP   - Pointer to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------
int XrdObjectStorageOss::Init(XrdSysLogger* logger, const char* configFn)
{
	PrintInfo("***** Init", "Oct-2021, NSDF-fabric");

	String config_filename = configFn;
	String connection_string = "";
	int num_connections;

	//get the connection string
	XrdOucStream Config;
	int fd = open(config_filename.c_str(), O_RDONLY, 0);

	Config.Attach(fd);
	for (auto it = Config.GetMyFirstWord(); it; it = Config.GetMyFirstWord())
	{
		std::string key = it;
		std::string value = Config.GetWord();;

		if (key == "xrdobjectstorageoss.connection_string")
		{
			PrintInfo("***** xrdobjectstorageoss.connection_string ", value);
			connection_string = value;
			continue;
		}

		if (key == "xrdobjectstorageoss.num_connections")
		{
			PrintInfo("***** xrdobjectstorageoss.num_connections ", value);
			num_connections = cint(value);
			continue;
		}

		if (key == "xrootd.export")
		{
			PrintInfo("***** xrootd.export", value);
			this->export_dir = value;
			continue;
		}
	}

	VisusReleaseAssert(!this->export_dir.empty());

	Config.Close();
	close(fd);

	this->net = std::make_shared<NetService>(num_connections);
	this->cloud = CloudStorage::createInstance(connection_string);

	// could be bucket + something, I need to preverve it
	// example /tmp/filename.bin == /bucket/some/prefix/filename.bin
	this->prefix = StringUtils::rtrim(Url(connection_string).getPath(), "/");

	return Ok();
}


//-----------------------------------------------------------------------------
//! Change file mode settings.
//!
//! @param  path   - Pointer to the path of the file in question.
//! @param  mode   - The new file mode setting.
//! @param  envP   - Pointer to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------
int XrdObjectStorageOss::Chmod(const char* path, mode_t mode, XrdOucEnv* envP)
{
	PrintInfo("***** Chmod", path, mode);
	return Ok(); //pretend it's ok
}

//-----------------------------------------------------------------------------
//! Create file.
//!
//! @param  tid    - Pointer to the trace identifier.
//! @param  path   - Pointer to the path of the file to create.
//! @param  mode   - The new file mode setting.
//! @param  env    - Reference to environmental information.
//! @param  opts   - Create options:
//!                  XRDOSS_mkpath - create dir path if it does not exist.
//!                  XRDOSS_new    - the file must not already exist.
//!                  oflags<<8     - open flags shifted 8 bits to the left/
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------
////////////////////////////////////////////////////////
int XrdObjectStorageOss::Create(const char* tident, const char* path, mode_t access_mode, XrdOucEnv& env, int Opts)
{
	//it's like a 'touch' for a file I'm going to open for writing later
	PrintInfo("***** Create", path, access_mode); 
	return Ok(); //pretend it's ok
}


//-----------------------------------------------------------------------------
//! Create a directory.
//!
//! @param  path   - Pointer to the path of the directory to be created.
//! @param  mode   - The directory mode setting.
//! @param  mkpath - When true the path is created if it does not exist.
//! @param  envP   - Pointer to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------
int XrdObjectStorageOss::Mkdir(const char* path, mode_t mode, int mkpath, XrdOucEnv* envP)
{
	PrintInfo("***** Mkdir", path);
	return Ok(); //pretend it's ok (on object storage there are no really directories)
}

//-----------------------------------------------------------------------------
//! Remove a directory.
//!
//! @param  path   - Pointer to the path of the directory to be removed.
//! @param  opts   - The processing options:
//!                  XRDOSS_Online   - only remove online copy
//!                  XRDOSS_isPFN    - path is already translated.
//! @param  envP   - Pointer to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------
int XrdObjectStorageOss::Remdir(const char* path, int Opts, XrdOucEnv* eP)
{
	PrintInfo("***** Remdir", path);
	return Ok(); //pretend it's ok (on object storage there are no really directories)
}



//-----------------------------------------------------------------------------
//! Rename a file or directory.
//!
//! @param  oPath   - Pointer to the path to be renamed.
//! @param  nPath   - Pointer to the path oPath is to have.
//! @param  oEnvP   - Environmental information for oPath.
//! @param  nEnvP   - Environmental information for nPath.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------
int XrdObjectStorageOss::Rename(const char* from, const char* to, XrdOucEnv* eP1, XrdOucEnv* eP2)
{
	PrintInfo("***** Rename",from,to);
	return NotSupported("rename not supported");
}

//-----------------------------------------------------------------------------
//! Return state information on a file or directory.
//!
//! @param  path   - Pointer to the path in question.
//! @param  buff   - Pointer to the structure where info it to be returned.
//! @param  opts   - Options:
//!                  XRDOSS_preop    - this is a stat prior to open.
//!                  XRDOSS_resonly  - only look for resident files.
//!                  XRDOSS_updtatm  - update file access time.
//! @param  envP   - Pointer to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------
int XrdObjectStorageOss::Stat(const char* path, struct stat* buf, int opts, XrdOucEnv* env)
{
	auto fullname = mapName(path);
	PrintInfo("***** Stat", path, fullname);

	auto item = statItem(fullname);
	if (!item)
		return NoEntity("the item is not a blob or director");

	//ID of device containing file
	buf->st_dev = 0;

	//Inode number
	buf->st_ino = 0;

	//file type and mode
	buf->st_mode = (S_IRUSR | S_IRGRP | S_IROTH) | (item->is_directory ? S_IFDIR : S_IFREG); //read only

	//Number of hard links 
	buf->st_nlink = item->is_directory ? 0 : 1;

	//User ID of owner
	buf->st_uid = getuid();

	//Group ID of owner 
	buf->st_gid = getgid();

	//Total size, in bytes
	buf->st_size = item->is_directory ? 0 : item->getContentLength();

	//Time of last access
	buf->st_atime = 0;
	
	//Time of last modification
	buf->st_mtime = 0;

	//Time of last status change
	buf->st_ctime = 0;

	return Ok();
}

//-----------------------------------------------------------------------------
//! Truncate a file.
//!
//! @param  path   - Pointer to the path of the file to be truncated.
//! @param  fsize  - The size that the file is to have.
//! @param  envP   - Pointer to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------
int XrdObjectStorageOss::Truncate(const char* path, unsigned long long size, XrdOucEnv* envP)
{
	PrintInfo("***** Truncate", path);
	return NotSupported("truncate not supported");
}

//-----------------------------------------------------------------------------
//! Remove a file.
//!
//! @param  path   - Pointer to the path of the file to be removed.
//! @param  opts   - Options:
//!                  XRDOSS_isMIG  - this is a migratable path.
//!                  XRDOSS_isPFN  - do not apply name2name to path.
//!                  XRDOSS_Online - remove only the online copy.
//! @param  envP   - Pointer to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------
int XrdObjectStorageOss::Unlink(const char* path, int Opts, XrdOucEnv* eP)
{
	auto fullname = mapName(path);
	PrintInfo("***** Unlink", path, fullname);
	return deleteBlob(fullname)? Ok() : InputOutputError("deleteBlob failed");
}


//-----------------------------------------------------------------------------
//! Open a directory.
//!
//! @param  path   - Pointer to the path of the directory to be opened.
//! @param  env    - Reference to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------
int XrdObjectStorageOssDir::Opendir(const char* path, XrdOucEnv& env)
{
	auto fullname = owner->mapName(path);
	PrintInfo("***** Opendir", path, fullname);

	if (auto dir = owner->getDir(fullname))
	{
		this->dir = dir;
		this->cursor = 0;
		return Ok();
	}

	return NoEntity("the directory does not exist");
}

////////////////////////////////////////////////////////
int XrdObjectStorageOssDir::Close(long long* retsz)
{
	PrintInfo("***** Close");
	this->dir.reset();
	this->cursor = 0;
	return Ok();
}


//-----------------------------------------------------------------------------
//! Get the next directory entry.
//!
//! @param  buff   - Pointer to buffer where a null terminated string of the
//!                  entry name is to be returned. If no more entries exist,
//!                  a null string is returned.
//! @param  blen   - Length of the buffer.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------
int XrdObjectStorageOssDir::Readdir(char* buff, int blen)
{
	PrintInfo("***** Readdir", blen, this->cursor);

	//finished
	if (this->cursor >= this->dir->childs.size())
	{
		buff[0] = 0;
		return Ok();
	}
	
	auto FULLNAME = this->dir->fullname;
	auto fullname = this->dir->childs[this->cursor]->fullname;
	VisusReleaseAssert(StringUtils::startsWith(fullname,FULLNAME));
	auto name = fullname.substr(FULLNAME.size());
	if (name[0] == '/') name = name.substr(1); //example "/aaaa" "aaaa/bbbb" -> "/bbbb" but I want bbbb

	strlcpy(buff, &name[0], blen);

	this->cursor++;
	return Ok();
}



//-----------------------------------------------------------------------------
//! Open a file.
//!
//! @param  path   - Pointer to the path of the file to be opened.
//! @param  Oflag  - Standard open flags.
//! @param  Mode   - File open mode (ignored unless creating a file).
//! @param  env    - Reference to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------
int XrdObjectStorageOssFile::Open(const char* path, int flags, mode_t mode, XrdOucEnv& env)
{
	auto fullname = owner->mapName(path);

	std::ostringstream out;

#define DebugFlag(__name__) if (flags & __name__) out<<#__name__<<" ";
	DebugFlag(O_RDONLY);
	DebugFlag(O_WRONLY);
	DebugFlag(O_RDWR);
	DebugFlag(O_APPEND);
	DebugFlag(O_ASYNC);
	DebugFlag(O_CLOEXEC);
	DebugFlag(O_CREAT);
	DebugFlag(O_DIRECT);
	DebugFlag(O_DIRECTORY);
	DebugFlag(O_DSYNC);
	DebugFlag(O_EXCL);
	DebugFlag(O_LARGEFILE);
	DebugFlag(O_NOATIME);
	DebugFlag(O_NOCTTY);
	DebugFlag(O_NOFOLLOW);
	DebugFlag(O_NONBLOCK);
	DebugFlag(O_PATH);
	DebugFlag(O_SYNC);
	DebugFlag(O_TMPFILE);
	DebugFlag(O_TRUNC);

	PrintInfo("***** Open", path, fullname, mode, out.str());

	//I am collecting all the writes (reading not allowed) and at the end I will upload the file
	if ((flags & O_WRONLY) || (flags & O_RDWR))
	{
		VisusReleaseAssert(flags && O_TRUNC);
		this->writing = CloudStorageItem::createBlob(fullname, std::make_shared<HeapMemory>());
		return Ok();
	}
	else
	{
		//assume it's reading (it seems O_RDONLY is not set)
		if (auto reading = owner->getBlob(fullname))
		{
			this->reading = reading;
			return Ok();
		}

		return NoEntity("the blob does not exist");
	}

}

////////////////////////////////////////////////////////
int XrdObjectStorageOssFile::Close(long long* retsz)
{
	PrintInfo("***** Close");

	if (this->reading)
	{ 
		this->reading.reset();
		return Ok();
	}
	else if (this->writing)
	{
		bool bOk = owner->addBlob(writing);
		this->writing.reset();
		return bOk? Ok() : InputOutputError("addBlob failed");
	}

	return Ok();
}


//-----------------------------------------------------------------------------
//! Preread file blocks into the file system cache.
//!
//! @param  offset  - The offset where the read is to start.
//! @param  size    - The number of bytes to pre-read.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------
ssize_t XrdObjectStorageOssFile::Read(off_t offset, size_t blen)
{
	PrintInfo("***** (PRE)Read", (int)offset, (int)blen);
	return NotSupported("preread not supported");
}

//-----------------------------------------------------------------------------
//! Read file bytes into a buffer.
//!
//! @param  buffer  - pointer to buffer where the bytes are to be placed.
//! @param  offset  - The offset where the read is to start.
//! @param  size    - The number of bytes to read.
//!
//! @return >= 0      The number of bytes that placed in buffer.
//! @return  < 0      -errno or -osserr upon failure (see XrdOssError.hh).
//-----------------------------------------------------------------------------
ssize_t XrdObjectStorageOssFile::Read(void* buff, off_t offset, size_t blen)
{
	PrintInfo("***** Read", (int)offset, (int)blen);

	if (this->reading)
	{
		blen = std::min((Int64)blen, this->reading->getContentLength() - offset);
		memcpy(buff, this->reading->body->c_ptr() + offset, blen);
		PrintInfo("***** returning blen",blen);
		return blen;
	}
	else if (this->writing)
	{
		this->writing.reset();
		return NotSupported("cannot read while writing");
	}
	else
	{
		return NotSupported("read failed (neither reading or writing)");
	}
}


//-----------------------------------------------------------------------------
//! Write file bytes from a buffer.
//!
//! @param  buffer  - pointer to buffer where the bytes reside.
//! @param  offset  - The offset where the write is to start.
//! @param  size    - The number of bytes to write.
//!
//! @return >= 0      The number of bytes that were written.
//! @return <  0      -errno or -osserr upon failure (see XrdOssError.hh).
//-----------------------------------------------------------------------------
ssize_t XrdObjectStorageOssFile::Write(const void* buff, off_t offset, size_t blen)
{
	PrintInfo("***** Write", (int)offset, (int)blen);

	if (this->reading)
	{
		this->reading.reset();
		return NotSupported("cannto write while reading");
	}
	else if (this->writing)
	{
		if (offset != writing->body->c_size())
			return InputOutputError("offset is not at the end of the body");

		if (!writing->body->resize(offset + blen, __FILE__, __LINE__))
			return InputOutputError("body resize failed");

		memcpy(writing->body->c_ptr() + offset, buff, blen);
		PrintInfo("***** returning blen", blen);
		return blen;
	}

	return NotSupported("tried a write wile not writing");
}

//-----------------------------------------------------------------------------
//! Return state information for this file.
//!
//! @param  buf    - Pointer to the structure where info it to be returned.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------
int XrdObjectStorageOssFile::Fstat(struct stat* buf)
{
	PrintInfo("***** Fstat");

	if (!this->reading && !this->writing)
		return NotSupported("cannot stat");

	//ID of device containing file
	buf->st_dev = 0;

	//Inode number
	buf->st_ino = 0;

	//file type and mode
	buf->st_mode = (S_IRUSR | S_IRGRP | S_IROTH) | S_IFREG; //read only

	//Number of hard links 
	buf->st_nlink = 1;

	//User ID of owner
	buf->st_uid = getuid();

	//Group ID of owner 
	buf->st_gid = getgid();

	//Total size, in bytes
	buf->st_size = reading? reading->getContentLength() : writing->body->c_size();

	//Time of last access
	buf->st_atime = 0;

	//Time of last modification
	buf->st_mtime = 0;

	//Time of last status change
	buf->st_ctime = 0;

	return Ok();
}


XrdVERSIONINFO(XrdOssGetStorageSystem, XrdObjectStorageOss);



