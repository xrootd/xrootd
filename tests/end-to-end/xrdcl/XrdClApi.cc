//------------------------------------------------------------------------------
// xrdcl-api: run single XrdCl calls against a server, for the end-to-end
// tests of the XrdCl API calls which no command-line tool makes.
//
// Each command does one call, or a short sequence of calls, prints the result
// to stdout and exits with 0 on success, 1 on error, 2 on a usage error, and 3
// when the server does not support the operation.
//------------------------------------------------------------------------------

#include "XProtocol/XProtocol.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClCheckpointOperation.hh"
#include "XrdCl/XrdClFileOperations.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClPollerBuiltIn.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClURL.hh"

#include <arpa/inet.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace XrdCl;

namespace
{

constexpr int ExitError       = 1;
constexpr int ExitUsage       = 2;
constexpr int ExitUnsupported = 3;

struct Failure
{
  int         code;
  std::string message;
};

// Throw a failure for a status which is not OK
void Check( const XRootDStatus &st, const std::string &what )
{
  if( st.IsOK() ) return;

  if( st.code == errErrorResponse && st.errNo == kXR_Unsupported )
    throw Failure{ ExitUnsupported, what + ": " + st.ToStr() };

  throw Failure{ ExitError, what + ": " + st.ToStr() };
}

void Usage( const std::string &message )
{
  throw Failure{ ExitUsage, message };
}

uint64_t ToNumber( const std::string &text )
{
  size_t end = 0;
  uint64_t value = 0;

  try { value = std::stoull( text, &end ); }
  catch( ... ) { end = 0; }

  if( end == 0 || end != text.size() )
    Usage( "not a number: '" + text + "'" );

  return value;
}

// Split "a:b:c" into its fields
std::vector<std::string> Fields( const std::string &text )
{
  std::vector<std::string> fields;
  std::stringstream ss( text );
  std::string field;

  while( std::getline( ss, field, ':' ) )
    fields.push_back( field );

  return fields;
}

// Copy the stream buffer, not through std::istreambuf_iterator, for which
// GCC 13 warns about a null pointer dereference in libstdc++
std::string ReadStream( std::istream &in )
{
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

std::string ReadLocal( const std::string &path )
{
  std::ifstream in( path, std::ios::binary );

  if( !in ) throw Failure{ ExitError, "cannot read local file " + path };

  return ReadStream( in );
}

void Print( const void *data, size_t size )
{
  std::cout.write( static_cast<const char*>( data ), size );
}

// A file which is open for the lifetime of the object
class OpenFile
{
  public:
    OpenFile( const std::string &url, OpenFlags::Flags flags )
    {
      Check( file.Open( url, flags, Access::UR | Access::UW ), "open" );
    }

    ~OpenFile()
    {
      if( file.IsOpen() ) XRootDStatus st = file.Close();
    }

    void Close() { Check( file.Close(), "close" ); }

    uint64_t Size()
    {
      StatInfo *info = nullptr;
      Check( file.Stat( false, info ), "stat" );
      std::unique_ptr<StatInfo> owner( info );
      return info->GetSize();
    }

    File file;
};

//------------------------------------------------------------------------------
// Reads
//------------------------------------------------------------------------------

// read <url> <offset> <size>: File::Read, prints the data
int Read( const std::vector<std::string> &args )
{
  if( args.size() != 3 ) Usage( "read <url> <offset> <size>" );

  OpenFile f( args[0], OpenFlags::Read );
  std::vector<char> buffer( ToNumber( args[2] ) );
  uint32_t bytesRead = 0;

  Check( f.file.Read( ToNumber( args[1] ), buffer.size(), buffer.data(),
                      bytesRead ), "read" );
  Print( buffer.data(), bytesRead );
  f.Close();
  return 0;
}

// pgread <url> <offset> <size>: File::PgRead, prints the data
int PgRead( const std::vector<std::string> &args )
{
  if( args.size() != 3 ) Usage( "pgread <url> <offset> <size>" );

  OpenFile f( args[0], OpenFlags::Read );
  std::vector<char> buffer( ToNumber( args[2] ) );
  std::vector<uint32_t> cksums;
  uint32_t bytesRead = 0;

  Check( f.file.PgRead( ToNumber( args[1] ), buffer.size(), buffer.data(),
                        cksums, bytesRead ), "pgread" );
  Print( buffer.data(), bytesRead );
  f.Close();
  return 0;
}

// readv <url> <offset>:<size>...: File::VectorRead, prints the data of all
// chunks in order
int ReadV( const std::vector<std::string> &args )
{
  if( args.size() < 2 ) Usage( "readv <url> <offset>:<size>..." );

  OpenFile f( args[0], OpenFlags::Read );
  ChunkList chunks;
  std::vector<std::vector<char>> buffers;

  for( size_t i = 1; i < args.size(); ++i )
  {
    auto fields = Fields( args[i] );
    if( fields.size() != 2 ) Usage( "not <offset>:<size>: " + args[i] );
    buffers.emplace_back( ToNumber( fields[1] ) );
  }

  for( size_t i = 1; i < args.size(); ++i )
  {
    auto &buffer = buffers[i - 1];
    chunks.emplace_back( ToNumber( Fields( args[i] )[0] ), buffer.size(),
                         buffer.data() );
  }

  VectorReadInfo *info = nullptr;
  Check( f.file.VectorRead( chunks, nullptr, info ), "readv" );
  std::unique_ptr<VectorReadInfo> owner( info );

  for( auto &chunk : info->GetChunks() )
    Print( chunk.buffer, chunk.length );

  f.Close();
  return 0;
}

// stat <url> [force]: File::Stat, prints the size
int Stat( const std::vector<std::string> &args )
{
  if( args.empty() || args.size() > 2 || ( args.size() == 2 && args[1] != "force" ) )
    Usage( "stat <url> [force]" );

  OpenFile f( args[0], OpenFlags::Read );
  StatInfo *info = nullptr;

  Check( f.file.Stat( args.size() == 2, info ), "stat" );
  std::unique_ptr<StatInfo> owner( info );
  std::cout << info->GetSize() << '\n';
  f.Close();
  return 0;
}

//------------------------------------------------------------------------------
// Writes, on files which exist
//------------------------------------------------------------------------------

// write <url> <offset>: File::Write of stdin, then File::Sync
int Write( const std::vector<std::string> &args )
{
  if( args.size() != 2 ) Usage( "write <url> <offset>" );

  std::string data = ReadStream( std::cin );
  OpenFile f( args[0], OpenFlags::Update );

  Check( f.file.Write( ToNumber( args[1] ), data.size(), data.data() ), "write" );
  Check( f.file.Sync(), "sync" );
  f.Close();
  return 0;
}

// pgwrite <url> <offset>: File::PgWrite of stdin
int PgWrite( const std::vector<std::string> &args )
{
  if( args.size() != 2 ) Usage( "pgwrite <url> <offset>" );

  std::string data = ReadStream( std::cin );
  OpenFile f( args[0], OpenFlags::Update );
  std::vector<uint32_t> cksums;

  Check( f.file.PgWrite( ToNumber( args[1] ), data.size(), data.data(),
                         cksums ), "pgwrite" );
  f.Close();
  return 0;
}

// writev <url> <offset> <file>...: File::WriteV, one buffer for each file
int WriteV( const std::vector<std::string> &args )
{
  if( args.size() < 3 ) Usage( "writev <url> <offset> <file>..." );

  std::vector<std::string> data;
  std::vector<iovec> iov;

  for( size_t i = 2; i < args.size(); ++i )
    data.push_back( ReadLocal( args[i] ) );

  for( auto &d : data )
    iov.push_back( { d.data(), d.size() } );

  OpenFile f( args[0], OpenFlags::Update );
  Check( f.file.WriteV( ToNumber( args[1] ), iov.data(), iov.size() ), "writev" );
  f.Close();
  return 0;
}

// vwrite <url> <offset>:<file>...: File::VectorWrite, one chunk for each file
int VectorWrite( const std::vector<std::string> &args )
{
  if( args.size() < 2 ) Usage( "vwrite <url> <offset>:<file>..." );

  std::vector<std::string> data;
  ChunkList chunks;

  for( size_t i = 1; i < args.size(); ++i )
  {
    auto fields = Fields( args[i] );
    if( fields.size() != 2 ) Usage( "not <offset>:<file>: " + args[i] );
    data.push_back( ReadLocal( fields[1] ) );
  }

  for( size_t i = 1; i < args.size(); ++i )
  {
    auto &d = data[i - 1];
    chunks.emplace_back( ToNumber( Fields( args[i] )[0] ), d.size(), d.data() );
  }

  OpenFile f( args[0], OpenFlags::Update );
  Check( f.file.VectorWrite( chunks ), "vwrite" );
  f.Close();
  return 0;
}

// truncate <url> <size>: File::Truncate on the open file
int Truncate( const std::vector<std::string> &args )
{
  if( args.size() != 2 ) Usage( "truncate <url> <size>" );

  OpenFile f( args[0], OpenFlags::Update );
  Check( f.file.Truncate( ToNumber( args[1] ) ), "truncate" );
  f.Close();
  return 0;
}

// checkpoint <url> <offset> commit|rollback: begin a checkpoint, write stdin
// at the offset with File::ChkptWrt, then commit or roll back
int Checkpoint( const std::vector<std::string> &args )
{
  if( args.size() != 3 || ( args[2] != "commit" && args[2] != "rollback" ) )
    Usage( "checkpoint <url> <offset> commit|rollback" );

  std::string data = ReadStream( std::cin );
  uint64_t offset = ToNumber( args[1] );
  auto end = ( args[2] == "commit" ? ChkPtCode::COMMIT : ChkPtCode::ROLLBACK );
  OpenFile f( args[0], OpenFlags::Update );

  Check( WaitFor( XrdCl::Checkpoint( f.file, ChkPtCode::BEGIN ) ), "checkpoint begin" );
  Check( WaitFor( ChkptWrt( f.file, offset, data.size(), data.data() ) ), "checkpointed write" );
  Check( WaitFor( XrdCl::Checkpoint( f.file, end ) ), "checkpoint " + args[2] );
  f.Close();
  return 0;
}

//------------------------------------------------------------------------------
// Extended attributes of an open file
//------------------------------------------------------------------------------

// xattr <url> set <name> <value> | get <name> | del <name> | list
int FileXAttr( const std::vector<std::string> &args )
{
  const std::string usage = "xattr <url> set <name> <value> | get <name> | del <name> | list";

  if( args.size() < 2 ) Usage( usage );

  const std::string &op = args[1];
  bool update = ( op == "set" || op == "del" );
  OpenFile f( args[0], update ? OpenFlags::Update : OpenFlags::Read );
  std::vector<XAttrStatus> result;

  if( op == "set" && args.size() == 4 )
    Check( f.file.SetXAttr( { { args[2], args[3] } }, result ), "set xattr" );
  else if( op == "del" && args.size() == 3 )
    Check( f.file.DelXAttr( { args[2] }, result ), "del xattr" );
  else if( op == "get" && args.size() == 3 )
  {
    std::vector<XAttr> attrs;
    Check( f.file.GetXAttr( { args[2] }, attrs ), "get xattr" );
    for( auto &a : attrs )
    {
      Check( a.status, "get xattr " + a.name );
      std::cout << a.value << '\n';
    }
  }
  else if( op == "list" && args.size() == 2 )
  {
    std::vector<XAttr> attrs;
    Check( f.file.ListXAttr( attrs ), "list xattr" );
    for( auto &a : attrs )
      std::cout << a.name << '=' << a.value << '\n';
  }
  else
    Usage( usage );

  for( auto &r : result )
    Check( r.status, op + " xattr " + r.name );

  f.Close();
  return 0;
}

//------------------------------------------------------------------------------
// Clones
//------------------------------------------------------------------------------

// clone <src-url> <dst-url> <dstoff>:<srcoff>:<size>...: create the target on
// the file system of the source, then clone the ranges of the source into it
int Clone( const std::vector<std::string> &args )
{
  if( args.size() < 3 ) Usage( "clone <src-url> <dst-url> <dstoff>:<srcoff>:<size>..." );

  OpenFile src( args[0], OpenFlags::Read );
  File dst;
  CloneLocations locs;

  for( size_t i = 2; i < args.size(); ++i )
  {
    auto fields = Fields( args[i] );
    if( fields.size() != 3 ) Usage( "not <dstoff>:<srcoff>:<size>: " + args[i] );
    locs.Add( src.file, ToNumber( fields[0] ), ToNumber( fields[1] ),
              ToNumber( fields[2] ) );
  }

  Check( dst.OpenUsingTemplate( src.file, args[1],
                                OpenFlags::New | OpenFlags::Update | OpenFlags::Samefs,
                                Access::UR | Access::UW ), "open target" );
  Check( dst.Clone( locs ), "clone" );
  Check( dst.Close(), "close target" );
  src.Close();
  return 0;
}

// dup <src-url> <dst-url>: create the target as a duplicate of the source
int Dup( const std::vector<std::string> &args )
{
  if( args.size() != 2 ) Usage( "dup <src-url> <dst-url>" );

  OpenFile src( args[0], OpenFlags::Read );
  File dst;

  Check( dst.OpenUsingTemplate( src.file, args[1],
                                OpenFlags::New | OpenFlags::Update | OpenFlags::Dup,
                                Access::UR | Access::UW ), "open duplicate" );
  Check( dst.Close(), "close duplicate" );
  src.Close();
  return 0;
}

//------------------------------------------------------------------------------
// File system requests
//------------------------------------------------------------------------------

// ping <url>: FileSystem::Ping
int Ping( const std::vector<std::string> &args )
{
  if( args.size() != 1 ) Usage( "ping <url>" );

  FileSystem fs{ URL( args[0] ) };
  Check( fs.Ping(), "ping" );
  return 0;
}

// protocol <url>: FileSystem::Protocol, prints the version and the host
// information flags in hexadecimal
int Protocol( const std::vector<std::string> &args )
{
  if( args.size() != 1 ) Usage( "protocol <url>" );

  FileSystem fs{ URL( args[0] ) };
  ProtocolInfo *info = nullptr;

  Check( fs.Protocol( info ), "protocol" );
  std::unique_ptr<ProtocolInfo> owner( info );
  std::cout << std::hex << "version=" << info->GetVersion()
            << " hostinfo=" << info->GetHostInfo() << '\n';
  return 0;
}

// sendinfo <url> <text>: FileSystem::SendInfo, prints the size of the id
int SendInfo( const std::vector<std::string> &args )
{
  if( args.size() != 2 ) Usage( "sendinfo <url> <text>" );

  FileSystem fs{ URL( args[0] ) };
  Buffer *id = nullptr;

  Check( fs.SendInfo( args[1], id ), "sendinfo" );
  std::unique_ptr<Buffer> owner( id );
  std::cout << id->GetSize() << '\n';
  return 0;
}

// dirlist <url> <path> [chunked]: FileSystem::DirList with stat information,
// prints the name of each entry. A chunked listing needs the asynchronous
// call, which gets one response for each chunk; the number of responses goes
// to stderr.
int DirList( const std::vector<std::string> &args )
{
  if( args.size() < 2 || args.size() > 3 || ( args.size() == 3 && args[2] != "chunked" ) )
    Usage( "dirlist <url> <path> [chunked]" );

  FileSystem fs{ URL( args[0] ) };

  if( args.size() == 2 )
  {
    DirectoryList *list = nullptr;
    Check( fs.DirList( args[1], DirListFlags::Stat, list ), "dirlist" );
    std::unique_ptr<DirectoryList> owner( list );

    for( auto it = list->Begin(); it != list->End(); ++it )
      std::cout << ( *it )->GetName() << '\n';

    return 0;
  }

  std::mutex mutex;
  std::condition_variable doneCV;
  bool done = false;
  size_t responses = 0;
  XRootDStatus last;
  std::vector<std::string> names;

  auto handler = ResponseHandler::Wrap( [&]( XRootDStatus &st, AnyObject &rsp )
  {
    std::lock_guard<std::mutex> lock( mutex );
    DirectoryList *list = nullptr;
    rsp.Get( list );
    if( list )
      for( auto it = list->Begin(); it != list->End(); ++it )
        names.push_back( ( *it )->GetName() );
    ++responses;
    if( !st.IsOK() || st.code != suContinue )
    {
      last = st;
      done = true;
      doneCV.notify_all();
    }
  } );

  Check( fs.DirList( args[1], DirListFlags::Stat | DirListFlags::Chunked, handler ),
         "dirlist" );

  std::unique_lock<std::mutex> lock( mutex );
  doneCV.wait( lock, [&] { return done; } );
  Check( last, "dirlist" );

  for( auto &name : names )
    std::cout << name << '\n';
  std::cerr << "responses: " << responses << '\n';
  return 0;
}

//------------------------------------------------------------------------------
// Concurrency
//------------------------------------------------------------------------------

// threads <url> <count>: read the file with one File object, each thread a
// part of it, all at the same time, then print the parts in order
int Threads( const std::vector<std::string> &args )
{
  if( args.size() != 2 ) Usage( "threads <url> <count>" );

  OpenFile f( args[0], OpenFlags::Read );
  const uint64_t count = ToNumber( args[1] );
  const uint64_t size  = f.Size();
  const uint64_t part  = ( size + count - 1 ) / count;
  std::vector<char> data( size );
  std::vector<std::thread> threads;
  std::atomic<bool> ok{ true };

  for( uint64_t i = 0; i < count; ++i )
    threads.emplace_back( [&, i]
    {
      uint64_t offset = std::min( i * part, size );
      uint32_t length = std::min( part, size - offset ), bytesRead = 0;
      if( !f.file.Read( offset, length, data.data() + offset, bytesRead ).IsOK() ||
          bytesRead != length )
        ok = false;
    } );

  for( auto &t : threads ) t.join();

  if( !ok ) throw Failure{ ExitError, "a concurrent read failed" };

  Print( data.data(), data.size() );
  f.Close();
  return 0;
}

// fork <url> <children> [seconds]: read the file, then fork the children one
// after the other; each child reads the file again with the inherited File
// object and must get the same data. Then close the file and wait for the
// given time, thus the idle connection can expire in the parent.
int Fork( const std::vector<std::string> &args )
{
  if( args.size() < 2 || args.size() > 3 ) Usage( "fork <url> <children> [seconds]" );

  const uint64_t pause = ( args.size() == 3 ? ToNumber( args[2] ) : 0 );

  OpenFile f( args[0], OpenFlags::Read );
  const uint64_t size = f.Size();
  std::vector<char> data( size ), again( size );
  uint32_t bytesRead = 0;

  Check( f.file.Read( 0, size, data.data(), bytesRead ), "read" );

  for( uint64_t i = 0; i < ToNumber( args[1] ); ++i )
  {
    pid_t pid = fork();

    if( pid < 0 ) throw Failure{ ExitError, "fork failed" };

    if( pid == 0 )
    {
      bool same = f.file.Read( 0, size, again.data(), bytesRead ).IsOK() &&
                  bytesRead == size && again == data;
      _exit( same && f.file.Close().IsOK() ? 0 : 1 );
    }

    int status = 0;
    if( waitpid( pid, &status, 0 ) != pid || !WIFEXITED( status ) ||
        WEXITSTATUS( status ) != 0 )
      throw Failure{ ExitError, "child " + std::to_string( i ) + " failed" };
  }

  f.Close();
  std::this_thread::sleep_for( std::chrono::seconds( pause ) );
  return 0;
}

// open-users <host:port> <path> <users> [seconds]: open the path as each user
// u0, u1, ... in turn, thus with one channel for each user, and print the
// error number of each failed open; then wait for the given time
int OpenUsers( const std::vector<std::string> &args )
{
  if( args.size() < 3 || args.size() > 4 ) Usage( "open-users <host:port> <path> <users> [seconds]" );

  for( uint64_t i = 0; i < ToNumber( args[2] ); ++i )
  {
    File file;
    std::string url = "root://u" + std::to_string( i ) + "@" + args[0] + "/" + args[1];
    XRootDStatus st = file.Open( url, OpenFlags::Read );

    if( st.IsOK() ) st = file.Close();
    else std::cout << st.errNo << '\n';
  }

  if( args.size() == 4 )
    std::this_thread::sleep_for( std::chrono::seconds( ToNumber( args[3] ) ) );

  return 0;
}

//------------------------------------------------------------------------------
// Transport: the XRootD handshake over a raw Socket
//------------------------------------------------------------------------------

constexpr uint32_t HandShakeResponseSize = 16;

ClientInitHandShake HandShake()
{
  ClientInitHandShake hs{};
  hs.fourth = htonl( 4 );
  hs.fifth  = htonl( 2012 );
  return hs;
}

// Print the protocol version and the server type of a handshake response
void PrintHandShake( const char *response )
{
  ServerInitHandShake body;
  memcpy( &body, response + 4, sizeof( body ) );
  std::cout << "msglen=" << ntohl( body.msglen )
            << " protover=" << std::hex << ntohl( body.protover ) << std::dec
            << " type=" << ntohl( body.msgval ) << '\n';
}

void Connect( Socket &socket, const std::string &host, const std::string &port,
              int family = AF_INET6 )
{
  Check( socket.Initialize( family ), "socket" );
  Check( socket.Connect( host, ToNumber( port ) ), "connect" );

  // the second call returns the name which the first one stored
  if( socket.GetSockName().empty() || socket.GetSockName().empty() )
    throw Failure{ ExitError, "the connected socket has no name" };
}

// socket <host> <port> [ipv4]: send the handshake with Socket::WriteRaw, read
// the response with Socket::ReadRaw, and print it. The socket is of the IPv6
// family, which also connects to IPv4 addresses, or of the IPv4 family.
int SocketHandShake( const std::vector<std::string> &args )
{
  if( args.size() < 2 || args.size() > 3 || ( args.size() == 3 && args[2] != "ipv4" ) )
    Usage( "socket <host> <port> [ipv4]" );

  Socket socket;
  Connect( socket, args[0], args[1], args.size() == 3 ? AF_INET : AF_INET6 );

  ClientInitHandShake hs = HandShake();
  char response[HandShakeResponseSize];
  uint32_t bytes = 0;

  Check( socket.WriteRaw( &hs, sizeof( hs ), 10, bytes ), "write handshake" );
  Check( socket.ReadRaw( response, sizeof( response ), 10, bytes ), "read response" );

  PrintHandShake( response );
  socket.Close();
  return 0;
}

// Read the handshake responses of the sockets on read events of the poller
class HandShakeReader : public SocketHandler
{
  public:
    void Initialize( Poller *poller ) override { pPoller = poller; }

    void Event( uint8_t type, Socket *socket ) override
    {
      std::lock_guard<std::mutex> lock( pMutex );
      std::string &data = pData[socket];

      if( type & ReadyToRead )
      {
        char buffer[HandShakeResponseSize];
        ssize_t n;
        while( data.size() < HandShakeResponseSize &&
               ( n = ::read( socket->GetFD(), buffer,
                             HandShakeResponseSize - data.size() ) ) > 0 )
          data.append( buffer, n );
      }

      if( data.size() == HandShakeResponseSize || ( type & ReadTimeOut ) )
      {
        pPoller->EnableReadNotification( socket, false );
        ++pDone;
        pDoneCV.notify_all();
      }
    }

    // Wait until each socket has its response or a timeout
    void Wait( size_t count )
    {
      std::unique_lock<std::mutex> lock( pMutex );
      pDoneCV.wait( lock, [&] { return pDone >= count; } );
    }

    std::map<Socket*, std::string> pData;

  private:
    Poller                  *pPoller = nullptr;
    std::mutex               pMutex;
    std::condition_variable  pDoneCV;
    size_t                   pDone = 0;
};

// poller <host> <port> <sockets>: send the handshake on each socket, read the
// responses on read events of the built-in poller, and print each response
int PollerHandShake( const std::vector<std::string> &args )
{
  if( args.size() != 3 ) Usage( "poller <host> <port> <sockets>" );

  PollerBuiltIn poller;
  HandShakeReader reader;
  std::vector<Socket> sockets( ToNumber( args[2] ) );

  // Stop the thread of the poller before the reader and the sockets go, also
  // on an error. Stop and Finalize do nothing the second time.
  struct PollerStop
  {
    PollerBuiltIn &poller;
    ~PollerStop() { poller.Stop(); poller.Finalize(); }
  } pollerStop{ poller };

  if( !poller.Initialize() || !poller.Start() )
    throw Failure{ ExitError, "cannot start the poller" };

  for( auto &socket : sockets )
  {
    Connect( socket, args[0], args[1] );

    if( !poller.AddSocket( &socket, &reader ) ||
        !poller.EnableReadNotification( &socket, true, 10 ) ||
        !poller.IsRegistered( &socket ) )
      throw Failure{ ExitError, "cannot register a socket" };

    ClientInitHandShake hs = HandShake();
    uint32_t bytes = 0;
    Check( socket.WriteRaw( &hs, sizeof( hs ), 10, bytes ), "write handshake" );
  }

  reader.Wait( sockets.size() );

  for( auto &socket : sockets )
    if( !poller.RemoveSocket( &socket ) || poller.IsRegistered( &socket ) )
      throw Failure{ ExitError, "cannot remove a socket" };

  poller.Stop();
  poller.Finalize();

  for( auto &socket : sockets )
  {
    const std::string &response = reader.pData[&socket];
    if( response.size() != HandShakeResponseSize )
      throw Failure{ ExitError, "no complete response on " + socket.GetSockName() };
    PrintHandShake( response.data() );
    socket.Close();
  }

  return 0;
}

const std::map<std::string, std::function<int( const std::vector<std::string>& )>> Commands =
{
  { "read",       Read            },
  { "pgread",     PgRead          },
  { "readv",      ReadV           },
  { "stat",       Stat            },
  { "write",      Write           },
  { "pgwrite",    PgWrite         },
  { "writev",     WriteV          },
  { "vwrite",     VectorWrite     },
  { "truncate",   Truncate        },
  { "checkpoint", Checkpoint      },
  { "xattr",      FileXAttr       },
  { "clone",      Clone           },
  { "dup",        Dup             },
  { "ping",       Ping            },
  { "protocol",   Protocol        },
  { "sendinfo",   SendInfo        },
  { "dirlist",    DirList         },
  { "threads",    Threads         },
  { "fork",       Fork            },
  { "open-users", OpenUsers       },
  { "socket",     SocketHandShake },
  { "poller",     PollerHandShake },
};

} // namespace

int main( int argc, char **argv )
{
  if( argc < 2 || !Commands.count( argv[1] ) )
  {
    std::cerr << "usage: xrdcl-api <command> <args...>, with the commands:";
    for( auto &c : Commands ) std::cerr << ' ' << c.first;
    std::cerr << '\n';
    return ExitUsage;
  }

  try
  {
    return Commands.at( argv[1] )( std::vector<std::string>( argv + 2, argv + argc ) );
  }
  catch( const Failure &f )
  {
    std::cout.flush();
    std::cerr << "xrdcl-api " << argv[1] << ": " << f.message << '\n';
    return f.code;
  }
}
