#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClMessageUtils.hh"

#include <iostream>

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

  uint64_t offset = 20;
  uint32_t size   = 20;
  uint32_t bytesRead = 0;
  char buffer[size];

  std::cout << "Do read ... " << std::endl;
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
