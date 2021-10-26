#ifndef __XRD_OBJECT_STORAGE_OSS_HH__
#define __XRD_OBJECT_STORAGE_OSS_HH__

#include <XrdOss/XrdOss.hh>

#include <XrdSys/XrdSysError.hh>
#include <XrdOuc/XrdOucString.hh>
#include <XrdOuc/XrdOucStream.hh>
#include <XrdOss/XrdOssError.hh>
#include <XrdOuc/XrdOucEnv.hh>
#include <XrdSys/XrdSysPlatform.hh>
#include <XrdVersion.hh>


#include <stdio.h>
#include <string>
#include <fcntl.h>
#include <assert.h>

#include <Visus/Kernel.h>
#include <Visus/CloudStorage.h>

using namespace Visus;

//predeclaration
class XrdObjectStorageOss;

//////////////////////////////////////////////////////////////////////////////////
class XrdObjectStorageOssDir : public XrdOssDF
{
public:

	XrdObjectStorageOss* owner;

	SharedPtr<CloudStorageItem> dir;
	int cursor = 0;

	//constructor
	XrdObjectStorageOssDir(XrdObjectStorageOss* owner_) : owner(owner_) {
	}

	//destructor
	virtual ~XrdObjectStorageOssDir() {
		Close();
	}

	//virtuals
	virtual int Opendir(const char*, XrdOucEnv&) override;
	virtual int Readdir(char* buff, int blen) override;
	virtual int Close(long long* retsz = 0) override;


};


//////////////////////////////////////////////////////////////////////////////////
class XrdObjectStorageOssFile : public XrdOssDF
{
public:

	XrdObjectStorageOss*        owner;
	SharedPtr<CloudStorageItem> writing;
	SharedPtr<CloudStorageItem> reading;

	//constructor
	XrdObjectStorageOssFile(XrdObjectStorageOss* owner_) : owner(owner_) {
		this->fd = 0;
	}

	//destructor
	virtual ~XrdObjectStorageOssFile() {
		Close();
	}

	//virtuals
	virtual int     Open(const char* path, int flags, mode_t mode, XrdOucEnv& env) override;
	virtual int     Close(long long* retsz = 0) override;
	virtual ssize_t Read(off_t offset, size_t blen) override;
	virtual ssize_t Read(void* buff, off_t offset, size_t blen) override;
	virtual int     Fstat(struct stat* buff) override;
	virtual ssize_t Write(const void* buff, off_t offset, size_t blen) override;
	virtual int     getFD() override { return fd; }

};


//////////////////////////////////////////////////////////////////////////////////
class XrdObjectStorageOss : public XrdOss
{
public:

	XrdSysError* eDest = nullptr;
	std::string             export_dir;
	SharedPtr<NetService>   net;
	SharedPtr<CloudStorage> cloud;
	String                  prefix; //bucket/whatever in the connection string

	//constructor
	XrdObjectStorageOss() {
		KernelModule::attach();

	}

	//destructor
	virtual ~XrdObjectStorageOss() {
		KernelModule::detach();
	}

	//printLine
	void printLine(std::string file, int line, std::string msg)
	{
		msg = cstring(file, line, msg);
		eDest->Say(msg.c_str());
	}

	//virtuals
	virtual XrdOssDF* newDir(const char* tident) override {
		return new XrdObjectStorageOssDir(this);
	}

	//newFile
	virtual XrdOssDF* newFile(const char* tident) override {
		return new XrdObjectStorageOssFile(this);
	}

	virtual int     Chmod(const char*, mode_t mode, XrdOucEnv* eP = 0) override;
	virtual int     Create(const char*, const char*, mode_t, XrdOucEnv&, int opts = 0) override;
	virtual int     Init(XrdSysLogger*, const char*) override;
	virtual int     Mkdir(const char*, mode_t mode, int mkpath = 0, XrdOucEnv* eP = 0) override;
	virtual int     Remdir(const char*, int Opts = 0, XrdOucEnv* eP = 0) override;
	virtual int     Rename(const char*, const char*, XrdOucEnv* eP1 = 0, XrdOucEnv* eP2 = 0) override;
	virtual int     Stat(const char*, struct stat*, int opts = 0, XrdOucEnv* eP = 0) override;
	virtual int     Truncate(const char*, unsigned long long, XrdOucEnv* eP = 0) override;
	virtual int     Unlink(const char* path, int Opts = 0, XrdOucEnv* eP = 0) override;

public:

	//mapName (example: /mnt/tmp/visus.idx -> /bucket/whatever/visus.idx)
	String mapName(String fullname)
	{
		VisusReleaseAssert(StringUtils::startsWith(fullname, this->export_dir));
		return  this->prefix + fullname.substr(this->export_dir.size());
	}

	//statItem
	SharedPtr<CloudStorageItem> statItem(String fullname)
	{
		if (auto ret = getFromCache(fullname))
			return ret;

			//first try if it's a blob
		if (auto ret = cloud->getBlob(net, fullname, /*head*/true).get())
		{
			addToCache(ret);
			return ret;
		}

		//then try if it's a directory
		if (auto ret = cloud->getDir(net, fullname).get())
		{
			addToCache(ret);
			return ret;
		}
		
		return SharedPtr<CloudStorageItem>();
	}


	//addBlob
	bool addBlob(SharedPtr<CloudStorageItem> blob)
	{
		bool bOk = cloud->addBlob(net, blob).get() ? true : false;
		if (bOk)
			addToCache(blob);
		return bOk;
	}

	//delBlob
	bool deleteBlob(String fullname)
	{
		if (!cloud->deleteBlob(net, fullname).get())
			return false;

		removeFromCache(fullname);
		return true;
	}

	//getBlob
	SharedPtr<CloudStorageItem> getBlob(String fullname)
	{
		if (auto ret = getFromCache(fullname))
		{
			if (ret->is_directory)
				return SharedPtr<CloudStorageItem>();

			if (ret->body)
				return ret;

			// I need to retried the body again, since it was probably evicted from cache
		}

		//I want the body so I need to requery
		if (auto ret = cloud->getBlob(net, fullname, /*head*/false).get())
		{
			addToCache(ret);
			return ret;
		}

		return SharedPtr<CloudStorageItem>();
	}

	//getDir
	SharedPtr<CloudStorageItem>  getDir(String fullname)
	{
		if (auto ret = getFromCache(fullname))
			return ret && ret->is_directory ? ret : SharedPtr<CloudStorageItem>();

		if (auto ret = cloud->getDir(net, fullname).get())
		{
			addToCache(ret);
			for (auto child : ret->childs)
				addToCache(child);
			return ret;
		}

		//this are useful for stat (speed up things)
		return SharedPtr<CloudStorageItem>();
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
		return it != cache.end() ? it->second : SharedPtr<CloudStorageItem>();
	}

	//removeFromCache
	void removeFromCache(String fullname) {
		auto it = cache.find(fullname);
		if (it!=cache.end())
			cache.erase(it);
	}


};



#endif //__XRD_OBJECT_STORAGE_OSS_HH__


