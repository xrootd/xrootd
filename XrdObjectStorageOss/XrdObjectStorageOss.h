#ifndef __XRD_OBJECT_STORAGE_OSS_HH__
#define __XRD_OBJECT_STORAGE_OSS_HH__

#include <XrdOss/XrdOss.hh>
#include <stdio.h>

#include <string>



//////////////////////////////////////////////////////////////////////////////////
class XrdObjectStorageOss : public XrdOss
{
public:

	XrdSysError* eDest = nullptr;
	std::string config_filename;
	std::string connection_string;

	class Pimpl;

	//constructor
	XrdObjectStorageOss();

	//destructor
	virtual ~XrdObjectStorageOss();

	//printLine
	//printLine
	void printLine(std::string file, int line, std::string msg);

	//virtuals
	virtual XrdOssDF* newDir(const char* tident);
	virtual XrdOssDF* newFile(const char* tident);

	virtual int     Chmod(const char*, mode_t mode, XrdOucEnv* eP = 0);
	virtual int     Create(const char*, const char*, mode_t, XrdOucEnv&, int opts = 0);
	virtual int     Init(XrdSysLogger*, const char*);
	virtual int     Mkdir(const char*, mode_t mode, int mkpath = 0, XrdOucEnv* eP = 0);
	virtual int     Remdir(const char*, int Opts = 0, XrdOucEnv* eP = 0);
	virtual int     Rename(const char*, const char*, XrdOucEnv* eP1 = 0, XrdOucEnv* eP2 = 0);
	virtual int     Stat(const char*, struct stat*, int opts = 0, XrdOucEnv* eP = 0);
	virtual int     Truncate(const char*, unsigned long long, XrdOucEnv* eP = 0);
	virtual int     Unlink(const char* path, int Opts = 0, XrdOucEnv* eP = 0);

private:

	Pimpl* pimpl=nullptr;


};

//////////////////////////////////////////////////////////////////////////////////
class XrdObjectStorageOssDir : public XrdOssDF
{
public:

	class Pimpl;

	//constructor
	XrdObjectStorageOssDir(XrdObjectStorageOss* oss);

	//destructor
	virtual ~XrdObjectStorageOssDir();

	//virtuals
	virtual int Opendir(const char*, XrdOucEnv&);
	virtual int Readdir(char* buff, int blen);
	virtual int Close(long long* retsz = 0);

private:

	XrdObjectStorageOss* oss;
	Pimpl* pimpl=nullptr;
	std::string path;
	int cursor = 0;
};


//////////////////////////////////////////////////////////////////////////////////
class XrdObjectStorageOssFile : public XrdOssDF
{
public:

	class Pimpl;

	//constructor
	XrdObjectStorageOssFile(XrdObjectStorageOss* oss);

	//destructor
	virtual ~XrdObjectStorageOssFile();

	//virtuals
	virtual int     Open(const char* path, int flags, mode_t mode, XrdOucEnv& env);
	virtual int     Close(long long* retsz = 0);
	virtual ssize_t Read(off_t offset, size_t blen);
	virtual ssize_t Read(void* buff, off_t offset, size_t blen);
	virtual int     Fstat(struct stat* buff);
	virtual ssize_t Write(const void* buff, off_t offset, size_t blen);
	virtual int     getFD() { return fd; }

private:
	XrdObjectStorageOss* oss;
	Pimpl* pimpl=nullptr;
	std::string filename;
};



#endif //__XRD_OBJECT_STORAGE_OSS_HH__


