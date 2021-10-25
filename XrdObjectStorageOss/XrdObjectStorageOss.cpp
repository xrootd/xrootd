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

#include "Kernel.h"
#include "CloudStorage.h"

using namespace Visus;

extern XrdSysError OssEroute;

//in general unsupported when we want to create/write somethig
#define Unsupported -ENOTSUP

////////////////////////////////////////////////////////
class XrdObjectStorageOss::Pimpl
{
public:

	XrdObjectStorageOss* owner;
	SharedPtr<NetService> service;
	SharedPtr<CloudStorage> cloud;

	//constructor
	Pimpl(XrdObjectStorageOss* owner_) : owner(owner_) {

		KernelModule::attach();
	}

	~Pimpl()
	{
		KernelModule::detach();
	}

	//init
	void init()
	{
		this->service = std::make_shared<NetService>(owner->num_connections);
		this->cloud = CloudStorage::createInstance(owner->connection_string);
	}

	//stat
	SharedPtr<CloudStorageItem> stat(String fullname, Aborted aborted = Aborted())
	{
		PrintInfo("stat", fullname);
		fullname = correctFullName(fullname);

		if (auto ret = getFromCache(fullname))
			return ret;

		auto ret = cloud->getBlob(service, fullname, /*head*/true, aborted).get(); 
		if (ret)
			return addToCache(ret);

		ret = cloud->getDir(service, fullname, aborted).get();
		if (ret) 
			return addToCache(ret);

		return SharedPtr<CloudStorageItem>(); //add failed
	}


	//getBlob
	SharedPtr<CloudStorageItem> getBlob(String fullname, Aborted aborted=Aborted())
	{
		PrintInfo("getBlob", fullname);
		fullname = correctFullName(fullname);

		if (auto ret = getFromCache(fullname))
		{
			if (ret->is_directory)
				return SharedPtr<CloudStorageItem>();
			
			if (ret->body)
				return ret;

			// I need to retried the body again, since it was probably evicted from cache
		}

		auto ret =cloud->getBlob(service, fullname, /*head*/false, aborted).get(); //I want the body so I need to requery
		return ret ? addToCache(ret) : SharedPtr<CloudStorageItem>();
	}

	//getDir
	SharedPtr<CloudStorageItem>  getDir(String fullname, Aborted aborted = Aborted())
	{
		PrintInfo("getDir", fullname);

		fullname = correctFullName(fullname);

		if (auto ret = getFromCache(fullname))
			return ret && ret->is_directory? ret : SharedPtr<CloudStorageItem>();

		auto ret = cloud->getDir(service, fullname, aborted).get();
		if (!ret)
			return SharedPtr<CloudStorageItem>();

		//this are useful for stat (speed up things)
		addToCache(ret);
		for (auto child : ret->childs)
			addToCache(child);

		return ret;
	}

private:

	std::map<String, SharedPtr<CloudStorageItem> > cache;

	//addToCache
	SharedPtr<CloudStorageItem> addToCache(SharedPtr<CloudStorageItem> item)
	{
		VisusReleaseAssert(item);
		auto cached = std::make_shared<CloudStorageItem>(*item);
		cached->body.reset(); //caching (but non the body)
		cache[cached->fullname] = cached;
		return item;
	}

	//getFromCache
	SharedPtr<CloudStorageItem> getFromCache(String fullname)
	{
		auto it = cache.find(fullname);
		return it != cache.end()? it->second : SharedPtr<CloudStorageItem>();
	}

	//correctFullName (example: /mnt/tmp/visus.idx -> /visus.idx)
	String correctFullName(String fullname)
	{
		VisusReleaseAssert(StringUtils::startsWith(fullname, owner->export_dir));
		fullname = fullname.substr(owner->export_dir.size());
		if (fullname.empty() || fullname[0] != '/') fullname = "/" + fullname;
		return fullname;
	}
};


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
	PrintInfo("XrdObjectStorageOss::Init", "Oct-2021, NSDF-fabric");

	this->config_filename = configFn;
	this->export_dir = "";

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
			PrintInfo("xrdobjectstorageoss.connection_string ", value);
			this->connection_string = value;
			continue;
		}


		if (key == "xrdobjectstorageoss.num_connections")
		{
			PrintInfo("xrdobjectstorageoss.num_connections ", value);
			this->num_connections = cint(value);
			continue;
		}

		if (key == "xrootd.export")
		{
			PrintInfo("xrootd.export", value);
			this->export_dir = value;
			continue;
		}
	}

	VisusReleaseAssert(!this->export_dir.empty());

	Config.Close();
	close(fd);

	pimpl->init();

	return XrdOssOK;
}



////////////////////////////////////////////////////////
XrdObjectStorageOss::XrdObjectStorageOss()
{
	this->pimpl = new Pimpl(this);

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
	PrintInfo("XrdObjectStorageOss::Mkdir", path);
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
	PrintInfo("XrdObjectStorageOss::Remdir", path);
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
	PrintInfo("XrdObjectStorageOss::Rename",from,to);
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
	PrintInfo("XrdObjectStorageOss::Stat", path);

	auto item = pimpl->stat(path);
	if (!item)
		return -ENOENT;

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
	PrintInfo("XrdObjectStorageOss::Truncate", path);
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
	PrintInfo("XrdObjectStorageOss::Unlink", path);
	return Unsupported;
}

/////////////////////////////////////////////////////
class XrdObjectStorageOssDir::Pimpl
{
public:

	SharedPtr<CloudStorageItem> dir;
	int cursor = 0;

	Pimpl() {}

};

////////////////////////////////////////////////////////
XrdObjectStorageOssDir::XrdObjectStorageOssDir(XrdObjectStorageOss* owner_) : owner(owner_)
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
	PrintInfo("XrdObjectStorageOssDir::Opendir", path);

	auto dir = owner->pimpl->getDir(path);
	if (!dir || !dir->is_directory)
		return -ENOENT; //error

	pimpl->dir = dir;
	pimpl->cursor = 0;
	return XrdOssOK;
}

////////////////////////////////////////////////////////
int XrdObjectStorageOssDir::Close(long long* retsz)
{
	PrintInfo("XrdObjectStorageOssDir::Close");
	pimpl->dir.reset();
	pimpl->cursor = 0;
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
	PrintInfo("XrdObjectStorageOssDir::Readdir", blen, pimpl->cursor);

	//finished
	if (pimpl->cursor >= pimpl->dir->childs.size())
	{
		buff[0] = 0;
		return 0;
	}
	
	auto FULLNAME = pimpl->dir->fullname;
	auto fullname = pimpl->dir->childs[pimpl->cursor]->fullname;
	VisusReleaseAssert(StringUtils::startsWith(fullname,FULLNAME));
	auto name = fullname.substr(FULLNAME.size());
	if (name[0] == '/') name = name.substr(1); //example "/aaaa" "aaaa/bbbb" -> "/bbbb" but I want bbbb

	strlcpy(buff, &name[0], blen);

	pimpl->cursor++;
	return XrdOssOK;
}


class XrdObjectStorageOssFile::Pimpl
{
public:
	SharedPtr<CloudStorageItem> blob;
};

////////////////////////////////////////////////////////
XrdObjectStorageOssFile::XrdObjectStorageOssFile(XrdObjectStorageOss* owner_) : owner(owner_)
{
	this->fd = 0;
	this->pimpl = new Pimpl();
}

////////////////////////////////////////////////////////
XrdObjectStorageOssFile::~XrdObjectStorageOssFile()
{
	Close();
	delete pimpl;
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
	PrintInfo("XrdObjectStorageOssFile::Open", path);

	auto blob = owner->pimpl->getBlob(path);
	if (!blob || blob->is_directory || !blob->body || !blob->getContentLength())
		return -ENOENT;

	pimpl->blob = blob;
	return XrdOssOK;
}

////////////////////////////////////////////////////////
int XrdObjectStorageOssFile::Close(long long* retsz)
{
	PrintInfo("XrdObjectStorageOssFile::Close");
	pimpl->blob.reset();
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
	PrintInfo("XrdObjectStorageOssFile::(PRE)Read", (int)offset, (int)blen);
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
	PrintInfo("XrdObjectStorageOssFile::Read", offset, blen);

	if (!pimpl->blob)
		return -ENOENT;

	blen = std::min((Int64)blen, pimpl->blob->getContentLength()-offset);

	memcpy(buff, pimpl->blob->body->c_ptr() + offset, blen);
	return blen;
}


////////////////////////////////////////////////////////
ssize_t XrdObjectStorageOssFile::Write(const void* buff, off_t offset, size_t blen)
{
	PrintInfo("XrdObjectStorageOssFile::Write", offset, blen);
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
	PrintInfo("XrdObjectStorageOssFile::Fstat");

	if (!pimpl->blob)
		return -ENOENT;

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
	buf->st_size = pimpl->blob->getContentLength();

	//Time of last access
	buf->st_atime = 0;

	//Time of last modification
	buf->st_mtime = 0;

	//Time of last status change
	buf->st_ctime = 0;

	return XrdOssOK;
}


XrdVERSIONINFO(XrdOssGetStorageSystem, XrdObjectStorageOss);



