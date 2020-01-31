#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClMessageUtils.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "xrootd/XrdCl/XrdClFile.hh"
#include "xrootd/XrdCl/XrdClFileSystem.hh"
#include "xrootd/private/XrdCl/XrdClFileOperations.hh"
#include "xrootd/private/XrdCl/XrdClFileSystemOperations.hh"
#include "xrootd/private/XrdCl/XrdClParallelOperation.hh"

#define __user__ "RPOENARU"

#include <iostream>
#include <chrono>
#include <string>
#include <ctime>
#include <future>
#include <thread>

void newLine()
{
  std::cout << "\n";
}

void showTime()
{
  auto now = std::chrono::system_clock::now();
  std::time_t showtime = std::chrono::system_clock::to_time_t(now);
  std::cout << " " << std::ctime(&showtime) << " ";
}

void asyncFunction()
{
  std::cout << std::this_thread::get_id() << "\n";
  std::cout << "Started waiting...";
  newLine();
  std::this_thread::sleep_for(std::chrono::seconds((int)1.5));
  std::cout << "Finished waiting...";
  newLine();
}

void fileReader()
{
  XrdCl::File file;
  std::cout << "Starting file opening..."
            << "\n";
  std::string fileURL = "root://localhost//tmp/myfile.txt";
  std::cout << "Do open ... " << std::endl;
  XrdCl::XRootDStatus st = file.Open(fileURL, XrdCl::OpenFlags::Update);
  if (!st.IsOK())
  {
    std::cout << "Open failed" << std::endl;
    std::cout << st.ToString() << std::endl;
    return;
  }
  std::cout << "Open done!" << std::endl;
}

void fileReaderPointer(XrdCl::File &file)
{
  std::string fileURL = "root://localhost//tmp/myfile.txt";
  std::cout << "Do open ... " << std::endl;
  // XrdCl::XRootDStatus st = file.Open(fileURL, XrdCl::OpenFlags::Update);
  auto st = file.Open(fileURL, XrdCl::OpenFlags::Update);
  if (!st.IsOK())
  {
    std::cout << "Open failed" << std::endl;
    std::cout << st.ToString() << std::endl;
    return;
  }
  std::cout << "Open done!" << std::endl;
}

//python bindings

int main(int argc, char *argv[])
{
  std::string filepath = "root://localhost//tmp/myfile.txt";
  std::string server = "root://localhost/";
  uint32_t size = 11;
  uint64_t offset = 0;
  char buff[11];

  XrdCl::File f;
  XrdCl::FileSystem fs(server);
  XrdCl::ChunkList chunks;

  //promise
  std::future<XrdCl::ChunkInfo> resp;
  std::future<XrdCl::ChunkInfo> respVec;

  // XrdCl::File f;
  std::cout << "opening..."
            << "\n";
  auto p1 = XrdCl::Open(f, filepath, XrdCl::OpenFlags::Read);
  std::cout << "reading..."
            << "\n";
  auto p2 = XrdCl::Read(f, offset, size, buff) >> resp;
  std::cout << "closing..."
            << "\n";
  auto p3 = XrdCl::Close(f);

  XrdCl::Pipeline p = p1 | p2 | p3;
  auto statusFileOpen = XrdCl::WaitFor(std::move(p));
  std::cout << std::string(buff, size);
  newLine();

  /*   std::cout << "Do open ... " << std::endl;
  XrdCl::XRootDStatus st = f.Open("root://localhost//tmp/myfile.txt", XrdCl::OpenFlags::Update);
  if (!st.IsOK())
  {
    std::cout << "Open failed" << std::endl;
    std::cout << st.ToString() << std::endl;
    return -1;
  }
  std::cout << "Open done!" << std::endl;

  std::cout << "Waiting for 1s ..." << std::endl;
  sleep(5);
  std::cout << "... done waiting!" << std::endl;

  uint64_t offset = 20;
  uint32_t size = 20;
  uint32_t bytesRead = 0;
  char buffer[size];

  std::cout << "Do the 1st read ... " << std::endl;
  st = f.Read(offset, size, buffer, bytesRead);
  if (!st.IsOK())
  {
    std::cout << "Read failed" << std::endl;
    std::cout << st.ToString() << std::endl;
    return -1;
  }
  std::cout << "Read done!" << std::endl;

  std::cout << "bytes read : " << bytesRead << std::endl;
  std::cout << std::string(buffer, bytesRead) << std::endl;
 */
  /* 
  XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
  env->PutInt("SubStreamsPerChannel", 2);
  std::cout << "API TEST at";
  showTime();

  //passing by reference avoids SEGMENTATION FAULT
  XrdCl::File fileP;
  fileReaderPointer(fileP);

  //async threads
  std::cout << "The main thread is:" << std::this_thread::get_id() << "\n";
  auto f1 = std::async(std::launch::async, asyncFunction);
  std::cout << "before promise GET"
            << "\n";
  f1.get();
  std::cout << "after promise GET"
            << "\n"; */
}

/* 
int main( int argc, char *argv[] )
{

  XrdCl::File f;

  std::cout << "Do open ... " << std::endl;
  XrdCl::XRootDStatus st = f.Open( "roots://localhost//some/path/to/file", XrdCl::OpenFlags::Update );
  if( !st.IsOK() )
  {
    std::cout << "Open failed" << std::endl;
    std::cout << st.ToString() << std::endl;
    return -1;
  }
  std::cout << "Open done!" << std::endl;

  std::cout << "Waiting for 5s ..." << std::endl;
  sleep( 5 );
  std::cout << "... done waiting!" << std::endl;

  uint64_t offset = 20;
  uint32_t size   = 20;
  uint32_t bytesRead = 0;
  char buffer[size];

  std::cout << "Do the 1st read ... " << std::endl;
  st = f.Read( offset, size, buffer, bytesRead );
  if( !st.IsOK() )
  {
    std::cout << "Read failed" << std::endl;
    std::cout << st.ToString() << std::endl;
    return -1;
  }
  std::cout << "Read done!" << std::endl;

  std::cout << "bytes read : " << bytesRead << std::endl;
  std::cout << std::string( buffer, bytesRead ) << std::endl;

  std::cout << "Do the 2nd read ... " << std::endl;
  st = f.Read( offset, size, buffer, bytesRead );
  if( !st.IsOK() )
  {
    std::cout << "Read failed" << std::endl;
    std::cout << st.ToString() << std::endl;
    return -1;
  }
  std::cout << "Read done!" << std::endl;

  std::cout << "bytes read : " << bytesRead << std::endl;
  std::cout << std::string( buffer, bytesRead ) << std::endl;

  std::cout << "Do close ... " << std::endl;
  st = f.Close();
  if( !st.IsOK() )
  {
    std::cout << "Close failed" << std::endl;
    std::cout << st.ToString() << std::endl;
    return -1;
  }
  std::cout << "Close done!" << std::endl;

  return 0;
}
 */