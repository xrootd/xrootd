/*-----------------------------------------------------------------------------
Copyright(c) 2010 - 2018 ViSUS L.L.C.,
Scientific Computing and Imaging Institute of the University of Utah

ViSUS L.L.C., 50 W.Broadway, Ste. 300, 84101 - 2044 Salt Lake City, UT
University of Utah, 72 S Central Campus Dr, Room 3750, 84112 Salt Lake City, UT

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met :

* Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

For additional information about this project contact : pascucci@acm.org
For support : support@visus.net
-----------------------------------------------------------------------------*/

#include <Visus/File.h>
#include <Visus/Thread.h>
#include <Visus/Utils.h>
#include <Visus/Time.h>
#include "osdep.hxx"


namespace Visus {



/////////////////////////////////////////////////////////////////////////////////////////
class PosixFile : public File::Pimpl
{
public:


  String filename;
  bool   can_read = false;
  bool   can_write = false;

  int    handle = -1;
  Int64  cursor = -1;

  //constructor
  PosixFile() {
  }

  //destructor
  virtual ~PosixFile() {
    close();
  }

  //isOpen
  virtual bool isOpen() const override {
    return this->handle != -1;
  }

  //canRead
  virtual bool canRead() const override {
    return can_read;
  }

  //canWrite
  virtual bool canWrite() const override {
    return can_write;
  }

  //getFilename
  virtual String getFilename() const override {
    return this->filename;
  }

  //open
  virtual bool open(String filename, String file_mode, File::Options options) override;

  //close
  virtual void close() override;

  //size
  virtual Int64 size() override;

  //write 
  virtual bool write(Int64 pos, Int64 tot, const unsigned char* buffer) override;

  //read
  virtual bool read(Int64 pos, Int64 tot, unsigned char* buffer) override;

  //seek
  bool seek(Int64 value);

  //GetOpenErrorExplanation
  static String GetOpenErrorExplanation();

};



/////////////////////////////////////////////////////////////////////
bool PosixFile::open(String filename, String file_mode, File::Options options) 
{
  bool bRead = StringUtils::contains(file_mode, "r");
  bool bWrite = StringUtils::contains(file_mode, "w");
  bool bMustCreate = options & File::MustCreateFile;

  int imode = O_BINARY;
  if (bRead && bWrite) imode |= O_RDWR;
  else if (bRead)           imode |= O_RDONLY;
  else if (bWrite)          imode |= O_WRONLY;
  else  VisusAssert(false);

  int create_flags = 0;

  if (bMustCreate)
  {
    imode |= O_CREAT | O_EXCL;

#if WIN32 
    create_flags |= (S_IREAD | S_IWRITE);
#else
    create_flags |= (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
#endif
  }

  for (int nattempt = 0; nattempt < (bMustCreate ? 2 : 1); nattempt++)
  {
    if (nattempt)
      FileUtils::createDirectory(Path(filename).getParent());

    this->handle = ::open(filename.c_str(), imode, create_flags);

    if (isOpen())
      break;
  }

  if (!isOpen())
  {
    if (!(bMustCreate && errno == EEXIST) && !(!bMustCreate && errno == ENOENT))
    {
      std::ostringstream out; out << Thread::getThreadId();
      PrintWarning("Thread[", out.str(), "] ERROR opening file ", filename, GetOpenErrorExplanation());
    }
    return false;
  }

  onOpenEvent();
  this->can_read = bRead;
  this->can_write = bWrite;
  this->filename = filename;
  this->cursor = 0;
  return true;
}

/////////////////////////////////////////////////////////////////////
void PosixFile::close() 
{
  if (!isOpen())
    return;

  ::close(this->handle);

  this->handle = -1;
  this->cursor = -1;
  this->can_read = false;
  this->can_write = false;
  this->filename = "";
}

/////////////////////////////////////////////////////////////////////
Int64 PosixFile::size() 
{
  if (!isOpen())
    return false;

  Int64 ret = LSeeki64(this->handle, 0, SEEK_END);

  if (ret < 0)
  {
    this->cursor = -1;
    return ret;
  }

  this->cursor = ret;
  return ret;
}

/////////////////////////////////////////////////////////////////////
bool PosixFile::write(Int64 pos, Int64 tot, const unsigned char* buffer) 
{
  if (!isOpen() || tot < 0 || !can_write)
    return false;

  if (tot == 0)
    return true;

  if (!seek(pos))
    return false;

  for (Int64 remaining = tot; remaining;)
  {
    int chunk = (remaining >= INT_MAX) ? INT_MAX : (int)remaining;
    int n = ::write(this->handle, buffer, chunk);

    if (n <= 0)
    {
      this->cursor = -1;
      return false;
    }

    onWriteEvent(n);
    remaining -= n;
    buffer += n;
  }

  if (this->cursor >= 0)
    this->cursor += tot;

  return true;
}

/////////////////////////////////////////////////////////////////////
bool PosixFile::read(Int64 pos, Int64 tot, unsigned char* buffer) 
{
  if (!isOpen() || tot < 0 || !can_read)
    return false;

  if (tot == 0)
    return true;

  if (!seek(pos))
    return false;

  for (Int64 remaining = tot; remaining;)
  {
    int chunk = (remaining >= INT_MAX) ? INT_MAX : (int)remaining;
    int n = ::read(this->handle, buffer, chunk);

    if (n <= 0)
    {
      this->cursor = -1;
      return false;
    }

    onReadEvent(n);
    remaining -= n;
    buffer += n;
  }

  if (this->cursor >= 0)
    this->cursor += tot;

  return true;
}


/////////////////////////////////////////////////////////////////////
bool PosixFile::seek(Int64 value)
{
  if (!isOpen())
    return false;

  // useless call
  if (this->cursor >= 0 && this->cursor == value)
    return true;

  bool bOk = LSeeki64(this->handle, value, SEEK_SET) >= 0;

  if (!bOk) {
    this->cursor = -1;
    return false;
  }
  else
  {
    this->cursor = value;
    return true;
  }
}

/////////////////////////////////////////////////////////////////////
String PosixFile::GetOpenErrorExplanation()
{
  switch (errno)
  {
  case EACCES:
    return "EACCES Tried to open a read-only file for writing, file's sharing mode does not allow the specified operations, or the given path is a directory.";
  case EEXIST:
    return"EEXIST _O_CREAT and _O_EXCL flags specified, but filename already exists.";
  case EINVAL:
    return"EINVAL Invalid oflag or pmode argument.";
  case EMFILE:
    return"EMFILE No more file descriptors are available (too many files are open).";
  case ENOENT:
    return"ENOENT File or path not found.";
  default:
    return cstring("Unknown errno", errno);
  }
}


/////////////////////////////////////////////////////////////////////////////////////////
class MemoryMappedFile : public File::Pimpl
{
public:

#if WIN32
  void* file = nullptr;
  void* mapping = nullptr;
#else
  int fd = -1;
#endif

  bool        can_read = false;
  bool        can_write = false;
  String      filename;
  Int64       nbytes = 0;
  char* mem = nullptr;

  //constructor
  MemoryMappedFile() {
  }

  //destryctor
  virtual ~MemoryMappedFile() {
    close();
  }

  //isOpen
  virtual bool isOpen() const override {
    return mem != nullptr;
  }

  //canRead
  virtual bool canRead() const override {
    return can_read;
  }

  //canWrite
  virtual bool canWrite() const override {
    return can_write;
  }

  //getFilename
  virtual String getFilename() const override {
    return this->filename;
  }

  //open
  virtual bool open(String filename, String file_mode, File::Options options) override;

  //close
  virtual void close() override;

  //size
  virtual Int64 size() override {
    return nbytes;
  }

  //write  
  virtual bool write(Int64 pos, Int64 tot, const unsigned char* buffer) override;

  //read
  virtual bool read(Int64 pos, Int64 tot, unsigned char* buffer) override;

};




/////////////////////////////////////////////////////////////////////
bool MemoryMappedFile::open(String filename, String file_mode, File::Options options) 
{
  close();

  bool bMustCreate = options & File::MustCreateFile;

  //not supported
  if (file_mode.find("w") != String::npos || bMustCreate) {
    VisusAssert(false);
    return false;
  }

#if WIN32 
  {
    this->file = CreateFileA(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (file == INVALID_HANDLE_VALUE) {
      close();
      return false;
    }

    this->nbytes = GetFileSize(file, nullptr);
    this->mapping = CreateFileMapping(file, nullptr, PAGE_READONLY, 0, 0, nullptr);

    if (mapping == nullptr) {
      close();
      return false;
    }

    this->mem = (char*)MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
  }
#else
  {
    this->fd = ::open(filename.c_str(), O_RDONLY);
    if (this->fd == -1) {
      close();
      return false;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
      close();
      return false;
    }

    this->nbytes = sb.st_size;
    this->mem = (char*)mmap(nullptr, nbytes, PROT_READ, MAP_PRIVATE, fd, 0);
  }
#endif

  if (!mem) {
    close();
    return false;
  }


  onOpenEvent();
  this->filename = filename;
  this->can_read = file_mode.find("r") != String::npos;
  this->can_write = file_mode.find("w") != String::npos;
  return true;
}

/////////////////////////////////////////////////////////////////////
void MemoryMappedFile::close() 
{
  if (!isOpen())
    return;

#if WIN32 
  {
    if (mem)
      UnmapViewOfFile(mem);

    if (mapping)
      CloseHandle(mapping);

    if (file != INVALID_HANDLE_VALUE)
      CloseHandle(file);

    mapping = nullptr;
    file = INVALID_HANDLE_VALUE;
  }
#else
  {
    if (mem)
      munmap(mem, nbytes);

    if (fd != -1)
    {
      ::close(fd);
      fd = -1;
    }
  }
#endif

  this->can_read = false;
  this->can_write = false;
  this->nbytes = 0;
  this->mem = nullptr;
  this->filename = "";
}



/////////////////////////////////////////////////////////////////////
bool MemoryMappedFile::write(Int64 pos, Int64 tot, const unsigned char* buffer) 
{
  if (!isOpen() || (pos + tot) > this->nbytes)
    return false;

  memcpy(mem + pos, buffer, (size_t)tot);
  onWriteEvent(tot);
  return true;
}

/////////////////////////////////////////////////////////////////////
bool MemoryMappedFile::read(Int64 pos, Int64 tot, unsigned char* buffer) 
{
  if (!isOpen() || (pos + tot) > this->nbytes)
    return false;

  memcpy(buffer, mem + pos, (size_t)tot);
  onReadEvent(tot);
  return true;
}


/////////////////////////////////////////////////////////////////////////////////////////
#if WIN32
class Win32File : public File::Pimpl
{
public:

  VISUS_NON_COPYABLE_CLASS(Win32File)
  
  String filename;
  bool   can_read = false;
  bool   can_write = false;
  HANDLE handle = INVALID_HANDLE_VALUE;
  Int64  cursor = -1;  

  //constructor
  Win32File() {
  }

  //destructor
  virtual ~Win32File() {
    close();
  }

  //isOpen
  virtual bool isOpen() const override {
    return handle != INVALID_HANDLE_VALUE;
  }

  //open
  virtual bool open(String filename, String file_mode, File::Options options) override;
  
  //close
  virtual void close() override;

  //canRead
  virtual bool canRead() const override {
    return can_read;
  }

  //canWrite
  virtual bool canWrite() const override {
    return can_write;
  }

  //getFilename
  virtual String getFilename() const override {
    return filename;
  }

  //size
  virtual Int64 size() override;

  //write  
  virtual bool write(Int64 pos, Int64 tot, const unsigned char* buffer) override;

  //read (should be portable to 32 and 64 bit OS)
  virtual bool read(Int64 pos, Int64 tot, unsigned char* buffer) override;

  //seek
  virtual bool seek(Int64 value);

};



/////////////////////////////////////////////////////////////////////
bool Win32File::open(String filename, String file_mode, File::Options options) {

  bool bRead = StringUtils::contains(file_mode, "r");
  bool bWrite = StringUtils::contains(file_mode, "w");
  bool bMustCreate = options & File::MustCreateFile;

  this->handle = CreateFile(
    filename.c_str(),
    (bRead ? GENERIC_READ : 0) | (bWrite ? GENERIC_WRITE : 0),
    bWrite ? 0 : FILE_SHARE_READ,
    NULL,
    bMustCreate ? CREATE_NEW : OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL, // | FILE_FLAG_NO_BUFFERING DOES NOT WORK..because I must align to blocksize
    NULL);

  if (!isOpen())
  {
    close();
    return false;
  }

  onOpenEvent();
  this->can_read = bRead;
  this->can_write = bWrite;
  this->filename = filename;
  this->cursor = 0;
  return true;
}

/////////////////////////////////////////////////////////////////////
void Win32File::close() 
{
  if (!isOpen())
    return;

  CloseHandle(handle);
  this->handle = INVALID_HANDLE_VALUE;
  this->can_read = false;
  this->can_write = false;
  this->filename = "";
  this->cursor = -1;
}


/////////////////////////////////////////////////////////////////////
Int64 Win32File::size() {

  if (!isOpen())
    return false;

  LARGE_INTEGER ret;
  ZeroMemory(&ret, sizeof(ret));

  LARGE_INTEGER zero;
  ZeroMemory(&zero, sizeof(zero));

  bool bOk = SetFilePointerEx(this->handle, zero, &ret, FILE_END);
  if (!bOk)
    return (this->cursor = -1);
  else
    return (this->cursor = ret.QuadPart);
}


/////////////////////////////////////////////////////////////////////
bool Win32File::write(Int64 pos, Int64 tot, const unsigned char* buffer) {

  if (!isOpen() || tot < 0 || !can_write)
    return false;

  if (tot == 0)
    return true;

  if (!seek(pos))
    return false;

  for (Int64 remaining = tot; remaining;)
  {
    int chunk = (remaining >= INT_MAX) ? INT_MAX : (int)remaining;

    DWORD _num_write_ = 0;
    int n = WriteFile(handle, buffer, chunk, &_num_write_, NULL) ? _num_write_ : 0;

    if (n <= 0)
    {
      this->cursor = -1;
      return false;
    }

    onWriteEvent(n);
    remaining -= n;
    buffer += n;
  }

  if (this->cursor >= 0)
    this->cursor += tot;

  return true;

}

/////////////////////////////////////////////////////////////////////
bool Win32File::read(Int64 pos, Int64 tot, unsigned char* buffer)  {

  if (!isOpen() || tot < 0 || !can_read)
    return false;

  if (tot == 0)
    return true;

  if (!seek(pos))
    return false;

  for (Int64 remaining = tot; remaining;)
  {
    int chunk = (remaining >= INT_MAX) ? INT_MAX : (int)remaining;

    DWORD __num_read__ = 0;
    int n = ReadFile(handle, buffer, chunk, &__num_read__, NULL) ? __num_read__ : 0;

    if (n <= 0)
    {
      this->cursor = -1;
      return false;
    }

    onReadEvent(n);
    remaining -= n;
    buffer += n;
  }

  if (this->cursor >= 0)
    this->cursor += tot;

  return true;

}


/////////////////////////////////////////////////////////////////////
bool Win32File::seek(Int64 value)
{
  if (!isOpen())
    return false;

  // useless call
  if (this->cursor >= 0 && this->cursor == value)
    return true;

  LARGE_INTEGER offset;
  ZeroMemory(&offset, sizeof(offset));
  offset.QuadPart = value;

  LARGE_INTEGER new_file_pointer;
  ZeroMemory(&new_file_pointer, sizeof(new_file_pointer));
  bool bOk = SetFilePointerEx(handle, offset, &new_file_pointer, FILE_BEGIN);

  if (!bOk) {
    this->cursor = -1;
    return false;
  }
  else
  {
    this->cursor = value;
    return true;
  }
}


#endif




/////////////////////////////////////////////////////////////////////////
bool File::open(String filename, String file_mode, Options options)
{
  close();

  //NOTE fopen/fclose is even slower than _open/_close
  //pimpl.reset(new Win32File()); //don't see any advantage using Win32File
  //pimpl.reset(new MemoryMappedFile()); THIS IS THE SLOWEST
  pimpl.reset(new PosixFile());

  if (!pimpl->open(filename, file_mode, options)) {
    pimpl.reset();
    return false;
  }

  return true;
}

/////////////////////////////////////////////////////////////////////////
bool FileUtils::existsDirectory(Path path)
{
  if (path.empty()) 
    return false;

  //special case [/] | [c:]
  if (path.isRootDirectory())
    return true;

  String fullpath=path.toString();

  struct Stat64 status;
  if (Stat64(fullpath.c_str(), &status) != 0)
    return false;


  if (!S_ISDIR(status.st_mode))
    return false;

  return true;
}

/////////////////////////////////////////////////////////////////////////
bool FileUtils::existsFile(Path path)
{
  if (path.empty()) 
    return false;

  String fullpath=path.toString();

  struct Stat64 status;

  if (Stat64(fullpath.c_str(), &status) != 0)
    return false;

  //TODO: probably here i need to specific test if it's a regular file or symbolic link
  if (!S_ISREG(status.st_mode))
    return false;

  return  true;
}

/////////////////////////////////////////////////////////////////////////
Int64 FileUtils::getFileSize(Path path)
{
  if (path.empty()) 
    return false;

  String fullpath=path.toString();

  struct Stat64 status;
  if (Stat64(fullpath.c_str(), &status) != 0)
    return 0;

  return status.st_size;

}

/////////////////////////////////////////////////////////////////////////
Int64 FileUtils::getTimeLastModified(Path path)
{
  if (path.empty()) 
    return false;

  String fullpath=path.toString();

  struct Stat64 status;
  if (Stat64(fullpath.c_str(), &status) != 0)
    return 0;

  return static_cast<Int64>(status.st_mtime);
}

/////////////////////////////////////////////////////////////////////////
Int64 FileUtils::getTimeLastAccessed(Path path)
{
  if (path.empty()) 
    return false;

  String fullpath=path.toString();

  struct Stat64 status;
  if (Stat64(fullpath.c_str(), &status) != 0)
    return 0;

  return static_cast<Int64>(status.st_atime);
}

/////////////////////////////////////////////////////////////////////////
bool FileUtils::removeFile(Path path)
{
  if (path.empty()) 
    return false;

  String fullpath=path.toString();

  return ::remove(fullpath.c_str())==0?true:false;
}

/////////////////////////////////////////////////////////////////////////
bool FileUtils::createDirectory(Path path,bool bCreateParents)
{
  //path invalid or already exists
  if (path.empty() || existsDirectory(path))
    return false;

  //need to create my parent
  if (bCreateParents)
  {
    Path parent=path.getParent(); VisusAssert(!parent.empty());
    if (!existsDirectory(parent) && !createDirectory(parent,true))
      return false;
  }

  return osdep::createDirectory(path.toString());
}

/////////////////////////////////////////////////////////////////////////
bool FileUtils::removeDirectory(Path path)
{
  if (path.empty()) 
    return false;

  String fullpath=path.toString();
  return osdep::removeDirectory(fullpath);
}

/////////////////////////////////////////////////////////////////////////
bool FileUtils::touch(Path path)
{
  File file;
  return file.createAndOpen(path.toString(), "rw");
}

bool VERBOSE_FILE_LOCK = false;

/////////////////////////////////////////////////////////////////////////
void FileUtils::lock(Path path)
{
  VisusAssert(!path.empty());
  String fullpath=path.toString();

  int pid = Utils::getPid();

  String lock_filename=fullpath+ ".lock";

  //let's try a little more
  Time T1=Time::now();
  Time last_info_time=T1;
  for (int nattempt=0; ;nattempt++)
  {
    File file;
    if (file.createAndOpen(lock_filename, "rw"))
    {
      file.close();

      if (VERBOSE_FILE_LOCK)
        PrintInfo("PID",pid,"got file lock",lock_filename);

      return;
    }

    //let the user know that I'm still waiting
    if (last_info_time.elapsedMsec()>1000)
    {
      PrintInfo("PID",pid,"waiting for lock on",lock_filename);
      last_info_time=Time::now();
      VERBOSE_FILE_LOCK =true;
    }

    Thread::yield();
  }
}

/////////////////////////////////////////////////////////////////////////
void FileUtils::unlock(Path path)
{
  VisusAssert(!path.empty());

  int pid = Utils::getPid();

  String fullpath = path.toString();

  String lock_filename = fullpath + ".lock";
  bool bRemoved = ::remove(lock_filename.c_str()) == 0 ? true : false;
  if (!bRemoved)
    ThrowException("cannot remove lock file",lock_filename);

  if (VERBOSE_FILE_LOCK)
    PrintInfo("PID", pid, "released file lock", lock_filename);

}

/////////////////////////////////////////////////////////////////////////
bool FileUtils::copyFile(String src_filename, String dst_filename, bool bFailIfExist)
{
  int buffer_size = 1024 * 1024; //1 Mb
  char* buffer=new char[buffer_size];
  
  int src = ::open(src_filename.c_str(), O_RDONLY | O_BINARY, 0);
  if (src == -1) {
    delete [] buffer;
    return false;
  }

  int dst = ::open(dst_filename.c_str(), O_WRONLY | O_CREAT | (bFailIfExist ? 0 : O_TRUNC), 0644);
  if (dst == -1) {
    delete [] buffer;
    return false;
  }

  int size;
  while ((size = ::read(src, buffer, buffer_size)) > 0)
  {
    if (::write(dst, buffer, size)!=size)
      return false;
  }

  close(src);
  close(dst);

  delete [] buffer;
  return true;
}

/////////////////////////////////////////////////////////////////////////
bool FileUtils::moveFile(String src_filename, String dst_filename)
{
  return std::rename(src_filename.c_str(), dst_filename.c_str()) == 0;
}

/////////////////////////////////////////////////////////////////////////
bool FileUtils::createLink(String existing_file, String new_file)
{
  return osdep::createLink(existing_file, new_file);
}

/////////////////////////////////////////////////////////////////////////
std::vector<String> FileUtils::findFilesInDirectory(String dir)
{
  return osdep::findFilesInDirectory(dir);
}

} //namespace Visus
