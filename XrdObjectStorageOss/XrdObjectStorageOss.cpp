#include <XrdSys/XrdSysError.hh>
#include <XrdOuc/XrdOucString.hh>
#include <XrdOuc/XrdOucStream.hh>
#include <XrdOss/XrdOssError.hh>
#include <XrdOuc/XrdOucEnv.hh>
#include <XrdSys/XrdSysPlatform.hh>
#include <XrdVersion.hh>

#include "XrdObjectStorageOss.h"

#include <fcntl.h>
#include <assert.h>

extern XrdSysError OssEroute;

#define PrintInfo(dst, ...) dst->printLine(__FILE__,__LINE__, cstring(__VA_ARGS__))

std::string     cstring() { return ""; }
std::string     cstring(bool      v) { return v ? "True" : "False"; }
std::string     cstring(int32_t   v) { return std::to_string(v); }
std::string     cstring(uint32_t  v) { return std::to_string(v); }
std::string     cstring(float     v) { return std::to_string(v); }
std::string     cstring(double    v) { return std::to_string(v); }
std::string     cstring(int64_t   v) { return std::to_string(v); }
std::string     cstring(uint64_t  v) { return std::to_string(v); }
std::string     cstring(const std::string& value) { return value; }
std::string     cstring(const char* value) { return std::string(value); }
std::string     cstring(char* value) { return std::string(value); }

template <typename Left, typename... Args>
inline std::string cstring(Left left, Args&&... args) {
	auto A = cstring(left);
	auto B = cstring(std::forward<Args>(args)...);
	return A + (A.empty() || B.empty() ? "" : " ") + B;
}

//in general unsupported when we want to create/write somethig
#define Unsupported -ENOTSUP

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

////////////////////////////////////////////////////////
class XrdObjectStorageOss::Pimpl
{
public:
	Pimpl() {}
};


////////////////////////////////////////////////////////
XrdObjectStorageOss::XrdObjectStorageOss()
{
	this->pimpl = new Pimpl();

}

////////////////////////////////////////////////////////
XrdObjectStorageOss::~XrdObjectStorageOss()
{
	delete pimpl;
}

////////////////////////////////////////////////////////
void XrdObjectStorageOss::printLine(std::string file, int line, std::string msg)
{
	msg = cstring(file, line, msg);
	eDest->Say(msg.c_str());
}


//-----------------------------------------------------------------------------
//! Obtain a new director object to be used for future directory requests.
//!
//! @param  tident - The trace identifier.
//!
//! @return pointer- Pointer to an XrdOssDF object.
//! @return nil    - Insufficient memory to allocate an object.
//-----------------------------------------------------------------------------
XrdOssDF* XrdObjectStorageOss::newDir(const char* tident)
{
	//Obtain a new director object to be used for future directory requests.
	return new XrdObjectStorageOssDir(this);
}

//-----------------------------------------------------------------------------
//! Obtain a new file object to be used for a future file requests.
//!
//! @param  tident - The trace identifier.
//!
//! @return pointer- Pointer to an XrdOssDF object.
//! @return nil    - Insufficient memory to allocate an object.
//-----------------------------------------------------------------------------
XrdOssDF* XrdObjectStorageOss::newFile(const char* tident)
{
	//Obtain a new file object to be used for a future file requests. 
	return new XrdObjectStorageOssFile(this);
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
	return Unsupported;
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
	return Unsupported;
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
	PrintInfo(this, "XrdObjectStorageOss::Init","Oct-2021, NSDF-fabric");

	this->config_filename = configFn;

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
			PrintInfo(this, "xrdobjectstorageoss.connection_string ", value);
			this->connection_string = value;
			break;
		}
	}

	Config.Close();
	close(fd);

	return XrdOssOK;
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
	PrintInfo(this, "XrdObjectStorageOss::Mkdir", path);
	return Unsupported;
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
	PrintInfo(this, "XrdObjectStorageOss::Remdir", path);
	return Unsupported;
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
	PrintInfo(this, "XrdObjectStorageOss::Rename",from,to);
	return Unsupported;
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
	PrintInfo(this, "XrdObjectStorageOss::Stat", path);

	//TODO
	assert(false);

	const bool is_directory = false;
	const int permissions = S_IRWXU | S_IRWXG | S_IRWXO;
	const int bytesize = is_directory ? 4096 : 1024 * 1024;

	//ID of device containing file
	buf->st_dev = 0;

	//Inode number
	buf->st_ino = 0;

	//file type and mode
	buf->st_mode = permissions | (is_directory ? S_IFDIR : S_IFREG);

	//Number of hard links 
	buf->st_nlink = is_directory ? 0 : 1;

	//User ID of owner
	buf->st_uid = getuid();

	//Group ID of owner 
	buf->st_gid = getgid();

	//Total size, in bytes
	buf->st_size = bytesize;

	//Time of last access
	buf->st_atime = 0;
	
	//Time of last modification
	buf->st_mtime = 0;

	//Time of last status change
	buf->st_ctime = 0;

	return XrdOssOK;
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
	PrintInfo(this, "XrdObjectStorageOss::Truncate", path);
	return Unsupported;
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
	PrintInfo(this, "XrdObjectStorageOss::Unlink", path);
	return Unsupported;
}


////////////////////////////////////////////////////////
class XrdObjectStorageOssDir::Pimpl
{
public:
	Pimpl() {}
};

////////////////////////////////////////////////////////
XrdObjectStorageOssDir::XrdObjectStorageOssDir(XrdObjectStorageOss* oss_)
	: oss(oss_)
{
	this->pimpl = new Pimpl();
}

////////////////////////////////////////////////////////
XrdObjectStorageOssDir::~XrdObjectStorageOssDir()
{
	Close();
	delete pimpl;
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
	PrintInfo(oss, "XrdObjectStorageOssDir::Opendir", path);
	this->path = path;
	this->cursor = 0;
	return XrdOssOK;
}

////////////////////////////////////////////////////////
int XrdObjectStorageOssDir::Close(long long* retsz)
{
	PrintInfo(oss, "XrdObjectStorageOssDir::Close", path);
	this->cursor = 0;
	this->path = "";
	return XrdOssOK;
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
	PrintInfo(oss, "XrdObjectStorageOssDir::Readdir", path, cursor);

	if (cursor<1)
		strlcpy(buff, "test.txt", blen);
	else
		*buff = '\0';

	cursor++;
	return XrdOssOK;
}



////////////////////////////////////////////////////////
class XrdObjectStorageOssFile::Pimpl
{
public:
	Pimpl() {}
};

////////////////////////////////////////////////////////
XrdObjectStorageOssFile::XrdObjectStorageOssFile(XrdObjectStorageOss* oss_)
	: oss(oss_)
{
	this->pimpl = new Pimpl();
	this->fd = 0;
}

////////////////////////////////////////////////////////
XrdObjectStorageOssFile::~XrdObjectStorageOssFile()
{
	Close();
	delete pimpl;
}

////////////////////////////////////////////////////////
int XrdObjectStorageOssFile::Close(long long* retsz)
{
	PrintInfo(oss, "XrdObjectStorageOssFile::Close", filename);
	this->filename = "";
	return XrdOssOK;
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
	PrintInfo(oss, "XrdObjectStorageOssFile::Open", path);
	this->filename = path;
	return XrdOssOK;
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
	PrintInfo(oss, "XrdObjectStorageOssFile::(PRE)Read", filename, offset,blen);
	return Unsupported;
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
	//TODO
	PrintInfo(oss, "XrdObjectStorageOssFile::Read", filename, offset, blen);
	return blen;
}


////////////////////////////////////////////////////////
ssize_t XrdObjectStorageOssFile::Write(const void* buff, off_t offset, size_t blen)
{
	PrintInfo(oss, "XrdObjectStorageOssFile::Write", filename, offset, blen);
	return Unsupported;
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
	PrintInfo(oss, "XrdObjectStorageOssFile::Fstat", filename);

	//TODO
	assert(false);

	const int permissions = S_IRWXU | S_IRWXG | S_IRWXO;
	const int bytesize = 1024 * 1024;

	//ID of device containing file
	buf->st_dev = 0;

	//Inode number
	buf->st_ino = 0;

	//file type and mode
	buf->st_mode = permissions | S_IFREG;

	//Number of hard links 
	buf->st_nlink = 1;

	//User ID of owner
	buf->st_uid = getuid();

	//Group ID of owner 
	buf->st_gid = getgid();

	//Total size, in bytes
	buf->st_size = bytesize;

	//Time of last access
	buf->st_atime = 0;

	//Time of last modification
	buf->st_mtime = 0;

	//Time of last status change
	buf->st_ctime = 0;

	return XrdOssOK;
}


XrdVERSIONINFO(XrdOssGetStorageSystem, XrdObjectStorageOss);



