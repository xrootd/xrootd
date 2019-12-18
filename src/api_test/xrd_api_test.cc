#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClMessageUtils.hh"
#include "XrdCl/XrdClDefaultEnv.hh"

#define __user__ "RPOENARU"

#include <iostream>
#include <chrono>
#include <string>
#include <ctime>

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

int main()
{
  std::cout << "API TEST at";
  showTime();
  std::cout << std::string(__user__);
  newLine();
}

/* 
int main( int argc, char *argv[] )
{
//  XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
//  env->PutInt( "SubStreamsPerChannel", 2 );

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