// These tests use real nonblocking sockets: positive reads are stOK, and only
// an empty socket returns suRetry. No impossible socket-count/status mocks.
#include "XrdCl/XrdClAsyncMsgReader.hh"
#include "XrdCl/XrdClAnyObject.hh"
#include "XrdCl/XrdClInQueue.hh"
#include "XrdCl/XrdClPoller.hh"
#include "XrdCl/XrdClStream.hh"
#include "XrdCl/XrdClXRootDMsgHandler.hh"
#include "XrdCl/XrdClXRootDTransport.hh"

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace
{
  // The reader is driven explicitly, so this unit fixture needs no worker or
  // poller threads. The Python reproduction exercises the real event loops.
  class NoopPoller : public XrdCl::Poller
  {
    public:
      bool Initialize() override { return true; }
      bool Finalize() override { return true; }
      bool Start() override { return true; }
      bool Stop() override { return true; }
      bool AddSocket( XrdCl::Socket*, XrdCl::SocketHandler* ) override
      { return addSockets.load(); }
      bool RemoveSocket( XrdCl::Socket* ) override { return true; }
      void ShutdownEvents( XrdCl::Socket* ) override {}
      bool EnableReadNotification( XrdCl::Socket*, bool, time_t ) override
      { return true; }
      bool EnableWriteNotification( XrdCl::Socket*, bool, time_t ) override
      { return writeNotifications; }
      bool IsRegistered( XrdCl::Socket* ) override { return false; }
      bool IsRunning() const override { return false; }
      std::atomic<bool> addSockets{ true };
      bool writeNotifications = true;
  };

  class TestJobManager
  {
    public:
      TestJobManager() : manager( 1 )
      {
        if( !manager.Initialize() || !manager.Start() )
          throw std::runtime_error( "job manager" );
      }
      ~TestJobManager()
      {
        manager.Stop();
        manager.Finalize();
      }
      void Drain()
      {
        class Barrier : public XrdCl::Job
        {
          public:
            explicit Barrier( XrdSysSemaphore &done ) : done( done ) {}
            void Run( void* ) override
            { done.Post(); }
            XrdSysSemaphore &done;
        };
        barriers.emplace_back( new Barrier( done ) );
        manager.QueueJob( barriers.back().get() );
        done.Wait();
      }
      XrdCl::JobManager manager;
    private:
      XrdSysSemaphore done{ 0 };
      std::vector<std::unique_ptr<XrdCl::Job>> barriers;
  };

  class TestTransport : public XrdCl::XRootDTransport
  {
    public:
      uint16_t SubStreamNumber( XrdCl::AnyObject& ) override
      { return streams; }
      XrdCl::PathID MultiplexSubStream( XrdCl::Message*, XrdCl::AnyObject&,
                                       XrdCl::PathID *hint = nullptr ) override
      { return hint ? *hint : XrdCl::PathID( path, path ); }
      uint16_t streams = 1;
      uint16_t path = 0;
  };

  class RecordingResponse : public XrdCl::ResponseHandler
  {
    public:
      void HandleResponse( XrdCl::XRootDStatus *status,
                           XrdCl::AnyObject *response ) override
      {
        ++calls;
        code = status->code;
        delete status;
        delete response;
      }
      unsigned calls = 0;
      uint16_t code = 0;
  };

  class FrameHandler : public XrdCl::MsgHandler
  {
    public:
      explicit FrameHandler( time_t deadline, uint16_t sid = 0x1234 ) :
        deadline( deadline ), sid( sid ), sendResults( 0 ), events( 0 ),
        lastEvent( Ready ) {}
      uint16_t GetSid() const override { return sid; }
      time_t GetExpiration() override { return deadline; }
      uint16_t InspectStatusRsp() override { return None; }
      void OnStatusReady( const XrdCl::Message*,
                          XrdCl::XRootDStatus status ) override
      {
        ++sendResults;
        lastSendStatus = status;
      }
      uint8_t OnStreamEvent( StreamEvent event,
                             XrdCl::XRootDStatus status ) override
      {
        if( event == Timeout && eventEntered )
        {
          eventEntered->Post();
          eventRelease->Wait();
        }
        ++events;
        lastEvent = event;
        lastStatus = status;
        return RemoveHandler;
      }
      uint16_t Examine( std::shared_ptr<XrdCl::Message> &message ) override
      {
        response = message;
        auto *header = reinterpret_cast<ServerResponseHeader*>(
          message->GetBuffer() );
        return NoProcess | (header->status == kXR_ok ? RemoveHandler : None);
      }
      time_t deadline;
      uint16_t sid;
      unsigned sendResults;
      XrdCl::XRootDStatus lastSendStatus;
      unsigned events;
      StreamEvent lastEvent;
      XrdCl::XRootDStatus lastStatus;
      std::shared_ptr<XrdCl::Message> response;
      XrdSysSemaphore *eventEntered = nullptr;
      XrdSysSemaphore *eventRelease = nullptr;
  };

  class CompletesAtDeadlineHandler : public XrdCl::MsgHandler
  {
    public:
      explicit CompletesAtDeadlineHandler( time_t deadline ) :
        deadline( deadline ), completed( false ) {}
      uint16_t GetSid() const override { return 0x1234; }
      time_t GetExpiration() override { return deadline; }
      uint16_t InspectStatusRsp() override { return None; }
      void OnStatusReady( const XrdCl::Message*, XrdCl::XRootDStatus ) override {}
      uint16_t Examine( std::shared_ptr<XrdCl::Message>& ) override
      { return Raw | NoProcess; }
      XrdCl::XRootDStatus ReadMessageBody( XrdCl::Message*,
                                           XrdCl::Socket *socket,
                                           uint32_t &bytesRead ) override
      {
        int count = 0;
        XrdCl::XRootDStatus status = socket->Read( &byte, 1, count );
        bytesRead = count;
        if( !status.IsOK() || status.code == XrdCl::suRetry ) return status;
        const auto bound = std::chrono::steady_clock::now() +
                           std::chrono::seconds( 3 );
        while( time( 0 ) < deadline )
        {
          if( std::chrono::steady_clock::now() >= bound )
            return XrdCl::XRootDStatus( XrdCl::stError,
                                        XrdCl::errOperationExpired );
          std::this_thread::yield();
        }
        completed = true;
        return XrdCl::XRootDStatus();
      }
      time_t deadline;
      bool completed;
      char byte;
  };

  struct ReaderSession
  {
    explicit ReaderSession( time_t deadline ) :
      url( "root://127.0.0.1:1094//deadline-test" ), handler( deadline ),
      stream( &url ), name( "deadline-test" )
    {
      int fds[2];
      if( socketpair( AF_UNIX, SOCK_STREAM, 0, fds ) ||
          fcntl( fds[0], F_SETFL, O_NONBLOCK ) == -1 )
        throw std::runtime_error( "nonblocking socketpair" );
      peer = fds[1];
      socket.reset( new XrdCl::Socket( fds[0], XrdCl::Socket::Connected ) );
      transport.InitializeChannel( url, channelData );
      stream.SetTransport( &transport );
      stream.SetPoller( &poller );
      stream.SetIncomingQueue( &incoming );
      stream.SetChannelData( &channelData );
      if( !stream.Initialize().IsOK() ) throw std::runtime_error( "stream" );
      bool removed = false;
      incoming.AddMessageHandler( &handler, removed );
      reader.reset( new XrdCl::AsyncMsgReader(
        transport, *socket, name, stream, 0 ) );
    }
    ~ReaderSession()
    {
      incoming.RemoveMessageHandler( &handler );
      close( peer );
    }
    void Send( const char *bytes, size_t length )
    {
      if( write( peer, bytes, length ) != static_cast<ssize_t>( length ) )
        throw std::runtime_error( "small socketpair write" );
    }
    int Pending() const
    {
      int pending = -1;
      if( ioctl( socket->GetFD(), FIONREAD, &pending ) )
        throw std::runtime_error( "FIONREAD" );
      return pending;
    }
    XrdCl::URL url;
    TestTransport transport;
    NoopPoller poller;
    XrdCl::InQueue incoming;
    XrdCl::AnyObject channelData;
    FrameHandler handler;
    XrdCl::Stream stream;
    std::string name;
    int peer;
    std::unique_ptr<XrdCl::Socket> socket;
    std::unique_ptr<XrdCl::AsyncMsgReader> reader;
  };

  std::string Frame( uint16_t status, const std::string &body )
  {
    ServerResponseHeader header = {};
    header.streamid[0] = 0x34;
    header.streamid[1] = 0x12;
    header.status = htons( status );
    header.dlen = htonl( body.size() );
    return std::string( reinterpret_cast<const char*>( &header ),
                        sizeof( header ) ) + body;
  }

}

// User: A server finishes a reply header after its request deadline but leaves
// the declared body incomplete. The client must stop instead of waiting again.
// Fails on current master 3bb3c0c32: the reader returns suRetry and keeps waiting.
TEST( ClientTimeouts, ExpiredHeadersStopBeforeBodyForPartialAndFinalReplies )
{
  struct Case
  {
    std::unique_ptr<ReaderSession> session;
    std::string wire;
    size_t split;
  };
  std::vector<Case> cases;
  const time_t setupDeadline = time( 0 ) + 60;
  for( auto status : { kXR_oksofar, kXR_ok } )
    for( size_t split = 1; split < sizeof( ServerResponseHeader ); ++split )
    {
      SCOPED_TRACE( ::testing::Message() << status << "/" << split );
      Case item = { std::unique_ptr<ReaderSession>(
                      new ReaderSession( setupDeadline ) ),
                    Frame( status, "entry\n" ), split };
      item.session->Send( item.wire.data(), split );
      EXPECT_EQ( item.session->reader->Read().code, XrdCl::suRetry );
      cases.emplace_back( std::move( item ) );
    }

  const time_t deadline = time( 0 ) + 2;
  for( auto &item : cases ) item.session->handler.deadline = deadline;
  const auto bound = std::chrono::steady_clock::now() + std::chrono::seconds( 3 );
  while( time( 0 ) < deadline && std::chrono::steady_clock::now() < bound )
    std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
  ASSERT_GE( time( 0 ), deadline );
  for( auto &item : cases )
  {
    item.session->Send( item.wire.data() + item.split,
                        sizeof( ServerResponseHeader ) - item.split );
    EXPECT_EQ( item.session->reader->Read().code,
               XrdCl::errOperationExpired );
    EXPECT_EQ( item.session->Pending(), 0 );
  }
}

// User: Normal network fragmentation delivers a directory reply one byte at a
// time before its deadline. Both partial and final frames must stay valid.
// Valid-path control: passes on current master 3bb3c0c32; this guards the new checks
// against rejecting valid short reads or mistaking EAGAIN for a disconnection.
TEST( ClientTimeouts, EveryByteFragmentationCompletesBeforeDeadline )
{
  for( auto status : { kXR_oksofar, kXR_ok } )
  {
    ReaderSession session( time( 0 ) + 60 );
    const std::string body( "alpha\nbeta\ngamma\n" );
    const auto wire = Frame( status, body );
    for( size_t index = 0; index < wire.size(); ++index )
    {
      SCOPED_TRACE( index );
      session.Send( wire.data() + index, 1 );
      auto result = session.reader->Read();
      ASSERT_TRUE( result.IsOK() );
      if( index + 1 < wire.size() ) EXPECT_EQ( result.code, XrdCl::suRetry );
      else EXPECT_NE( result.code, XrdCl::suRetry );
    }
    ASSERT_TRUE( session.handler.response );
    EXPECT_EQ( std::string( session.handler.response->GetBuffer( 8 ),
                            body.size() ), body );
    EXPECT_EQ( session.Pending(), 0 );
  }
}

// User: A peer starts a partial reply on time, then dribbles the remaining
// bytes after the deadline. Activity must not extend the request indefinitely.
// Fails on current master 3bb3c0c32: late bytes
// are accepted because only the inactivity timer and fenced tick are checked.
TEST( ClientTimeouts, IncompleteBodyExpiresDespiteNewReadableBytes )
{
  ReaderSession session( time( 0 ) + 2 );
  const auto wire = Frame( kXR_oksofar, "unfinished entry\n" );
  session.Send( wire.data(), sizeof( ServerResponseHeader ) + 1 );
  ASSERT_EQ( session.reader->Read().code, XrdCl::suRetry );
  const auto bound = std::chrono::steady_clock::now() + std::chrono::seconds( 3 );
  while( time( 0 ) < session.handler.deadline &&
         std::chrono::steady_clock::now() < bound )
    std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
  ASSERT_GE( time( 0 ), session.handler.deadline );
  const size_t remaining = wire.size() - sizeof( ServerResponseHeader ) - 1;
  session.Send( wire.data() + sizeof( ServerResponseHeader ) + 1, remaining );
  EXPECT_EQ( session.reader->Read().code, XrdCl::errOperationExpired );
  EXPECT_EQ( session.Pending(), static_cast<int>( remaining ) );
}

// User: A server stops sending an already-declared response body, so no new
// readable event can run the reader's deadline check. The poller's read timeout
// must tear down both partial and final stalled frames at the request deadline.
// Fails on current master 3bb3c0c32: OnReadTimeout ignores the active
// incoming helper; e8848d15b tears it down but reports Broken, not Timeout.
TEST( ClientTimeouts, ReadTimeoutTearsDownExpiredPartialAndFinalBodies )
{
  for( auto status : { kXR_oksofar, kXR_ok } )
  {
    SCOPED_TRACE( status );
    TestJobManager jobs;
    ReaderSession session( time( 0 ) - 1 );
    session.stream.SetJobManager( &jobs.manager );
    auto response = std::make_shared<XrdCl::Message>(
      sizeof( ServerResponseHeader ) );
    memset( response->GetBuffer(), 0, response->GetSize() );
    auto *header = reinterpret_cast<ServerResponseHeader*>(
      response->GetBuffer() );
    header->streamid[0] = 0x34;
    header->streamid[1] = 0x12;
    header->status = status;
    header->dlen = 16;
    session.stream.InstallIncHandler( response, 0 );

    const bool keepReading = session.stream.OnReadTimeout( 0 );
    jobs.Drain();
    EXPECT_FALSE( keepReading );
    EXPECT_EQ( session.handler.events, 1u );
    EXPECT_EQ( session.handler.lastEvent, XrdCl::MsgHandler::Timeout );
    EXPECT_EQ( session.handler.lastStatus.code,
               XrdCl::errOperationExpired );
  }
}

// User: A response body is temporarily idle while its request still has a
// future deadline. A poller timeout check must keep the socket and request.
// Valid-path control: current master 3bb3c0c32 keeps reading; Branch guard: this
// proves the new incoming-body check compares the deadline before closing.
TEST( ClientTimeouts, ReadTimeoutKeepsAnUnexpiredIncomingBody )
{
  TestJobManager jobs;
  ReaderSession session( time( 0 ) + 60 );
  session.stream.SetJobManager( &jobs.manager );
  auto response = std::make_shared<XrdCl::Message>(
    sizeof( ServerResponseHeader ) );
  memset( response->GetBuffer(), 0, response->GetSize() );
  auto *header = reinterpret_cast<ServerResponseHeader*>(
    response->GetBuffer() );
  header->streamid[0] = 0x34;
  header->streamid[1] = 0x12;
  header->status = kXR_oksofar;
  header->dlen = 16;
  session.stream.InstallIncHandler( response, 0 );

  EXPECT_TRUE( session.stream.OnReadTimeout( 0 ) );
  jobs.Drain();
  EXPECT_EQ( session.handler.events, 0u );
}

// User: A parallel data socket stalls in a response body past the request
// deadline while the control socket remains usable. The timed-out operation
// must be reported immediately rather than waiting for a later global tick,
// whether or not another send is queued on that data path.
// Fails on current master 3bb3c0c32: OnReadTimeout returns true and
// reports nothing; Branch guard: the first fix closes the socket but likewise
// reinserts the fenced handler without reporting it.
TEST( ClientTimeouts, PeripheralReadTimeoutReportsTheExpiredRequest )
{
  for( bool queuedSend : { false, true } )
  {
    SCOPED_TRACE( queuedSend );
    TestJobManager jobs;
    XrdCl::Message pending( sizeof( ClientRequest ) );
    memset( pending.GetBuffer(), 0, pending.GetSize() );
    FrameHandler pendingHandler( time( 0 ) + 60, 0x5678 );
    ReaderSession session( time( 0 ) - 1 );
    session.stream.SetJobManager( &jobs.manager );
    session.transport.streams = 2;
    session.stream.OnConnect( 0 );
    jobs.Drain();

    if( queuedSend )
    {
      session.stream.OnConnect( 1 );
      session.transport.path = 1;
      ASSERT_TRUE( session.stream.Send( &pending, &pendingHandler, false,
                                        time( 0 ) + 60 ).IsOK() );
    }

    auto response = std::make_shared<XrdCl::Message>(
      sizeof( ServerResponseHeader ) );
    memset( response->GetBuffer(), 0, response->GetSize() );
    auto *header = reinterpret_cast<ServerResponseHeader*>(
      response->GetBuffer() );
    header->streamid[0] = 0x34;
    header->streamid[1] = 0x12;
    header->status = kXR_oksofar;
    header->dlen = 16;
    session.stream.InstallIncHandler( response, 1 );

    EXPECT_FALSE( session.stream.OnReadTimeout( 1 ) );
    jobs.Drain();
    EXPECT_EQ( session.handler.events, 1u );
    EXPECT_EQ( session.handler.lastEvent, XrdCl::MsgHandler::Timeout );
    EXPECT_EQ( session.handler.lastStatus.code, XrdCl::errOperationExpired );
    EXPECT_EQ( pendingHandler.sendResults, 0u );
  }
}

// User: A parallel response expires with a queued send, then moving that send
// to the control path fails. Fatal recovery must report Timeout only to the due
// request and the connection failure to unrelated future requests.
// Fails on current master 3bb3c0c32: the peripheral timeout is
// ignored; Branch guard: the first fix fans errOperationExpired out through
// FatalError, making every request on the session appear to have timed out.
TEST( ClientTimeouts, PeripheralFatalRecoveryDoesNotExpireUnrelatedRequests )
{
  TestJobManager jobs;
  XrdCl::Message pending( sizeof( ClientRequest ) );
  memset( pending.GetBuffer(), 0, pending.GetSize() );
  FrameHandler pendingHandler( time( 0 ) + 60, 0x5678 );
  FrameHandler future( time( 0 ) + 60, 0x9abc );
  ReaderSession session( time( 0 ) - 1 );
  session.stream.SetJobManager( &jobs.manager );
  session.transport.streams = 2;
  session.stream.OnConnect( 0 );
  session.stream.OnConnect( 1 );
  jobs.Drain();

  session.transport.path = 1;
  ASSERT_TRUE( session.stream.Send( &pending, &pendingHandler, false,
                                    time( 0 ) + 60 ).IsOK() );
  session.poller.writeNotifications = false;
  bool removed = false;
  session.incoming.AddMessageHandler( &future, removed );
  session.incoming.AssignTimeout( &future );
  auto response = std::make_shared<XrdCl::Message>(
    sizeof( ServerResponseHeader ) );
  memset( response->GetBuffer(), 0, response->GetSize() );
  auto *header = reinterpret_cast<ServerResponseHeader*>(
    response->GetBuffer() );
  header->streamid[0] = 0x34;
  header->streamid[1] = 0x12;
  header->status = kXR_oksofar;
  header->dlen = 16;
  session.stream.InstallIncHandler( response, 1 );

  EXPECT_FALSE( session.stream.OnReadTimeout( 1 ) );
  jobs.Drain();
  EXPECT_EQ( session.handler.events, 1u );
  EXPECT_EQ( session.handler.lastEvent, XrdCl::MsgHandler::Timeout );
  EXPECT_EQ( session.handler.lastStatus.code, XrdCl::errOperationExpired );
  EXPECT_EQ( future.events, 1u );
  EXPECT_EQ( future.lastEvent, XrdCl::MsgHandler::FatalError );
  EXPECT_EQ( future.lastStatus.code, XrdCl::errPollerError );
  EXPECT_EQ( pendingHandler.sendResults, 1u );
  EXPECT_TRUE( pendingHandler.lastSendStatus.IsFatal() );
  EXPECT_EQ( pendingHandler.lastSendStatus.code, XrdCl::errPollerError );
}

// User: A data-path response expires after its control socket has failed, so
// the queued send cannot be moved onto a usable path. Only the due request is
// a timeout; other requests see the connection failure.
// Branch guard: fails on current master 3bb3c0c32 in this forced seam. Stock does
// not route a stalled body here; once this branch does, removing the rewrite reports
// errOperationExpired as a fatal error to every unrelated request.
TEST( ClientTimeouts, PeripheralTimeoutWithUnavailableControlIsRequestLocal )
{
  TestJobManager jobs;
  XrdCl::Message pending( sizeof( ClientRequest ) );
  memset( pending.GetBuffer(), 0, pending.GetSize() );
  FrameHandler pendingHandler( time( 0 ) + 60, 0x5678 );
  FrameHandler future( time( 0 ) + 60, 0x9abc );
  ReaderSession session( time( 0 ) - 1 );
  session.stream.SetJobManager( &jobs.manager );
  session.transport.streams = 2;
  session.stream.OnConnect( 0 );
  session.stream.OnConnect( 1 );
  jobs.Drain();

  session.transport.path = 1;
  ASSERT_TRUE( session.stream.Send( &pending, &pendingHandler, false,
                                    time( 0 ) + 60 ).IsOK() );
  const auto active = session.stream.OnReadyToWrite( 1 );
  ASSERT_EQ( active.first, &pending );
  ASSERT_EQ( active.second, &pendingHandler );
  session.incoming.RemoveMessageHandler( &pendingHandler );
  session.incoming.RemoveMessageHandler( &session.handler );
  session.stream.OnError(
    0, XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errSocketError ) );
  jobs.Drain();

  bool removed = false;
  session.incoming.AddMessageHandler( &pendingHandler, removed );
  session.incoming.AddMessageHandler( &session.handler, removed );
  session.incoming.AddMessageHandler( &future, removed );
  session.incoming.AssignTimeout( &future );
  auto response = std::make_shared<XrdCl::Message>(
    sizeof( ServerResponseHeader ) );
  memset( response->GetBuffer(), 0, response->GetSize() );
  auto *header = reinterpret_cast<ServerResponseHeader*>(
    response->GetBuffer() );
  header->streamid[0] = 0x34;
  header->streamid[1] = 0x12;
  header->status = kXR_oksofar;
  header->dlen = 16;
  session.stream.InstallIncHandler( response, 1 );

  session.stream.OnError(
    1, XrdCl::XRootDStatus( XrdCl::stError,
                            XrdCl::errOperationExpired ) );
  jobs.Drain();
  EXPECT_EQ( session.handler.events, 1u );
  EXPECT_EQ( session.handler.lastEvent, XrdCl::MsgHandler::Timeout );
  EXPECT_EQ( session.handler.lastStatus.code, XrdCl::errOperationExpired );
  EXPECT_EQ( future.events, 1u );
  EXPECT_EQ( future.lastEvent, XrdCl::MsgHandler::FatalError );
  EXPECT_EQ( future.lastStatus.code, XrdCl::errSocketError );
  EXPECT_EQ( pendingHandler.sendResults, 1u );
  EXPECT_TRUE( pendingHandler.lastSendStatus.IsFatal() );
  EXPECT_EQ( pendingHandler.lastSendStatus.code, XrdCl::errSocketError );
}

// User: A control-socket response expires while another request is queued,
// and reconnect setup then fails. The due request must receive Timeout before
// the connection failure is reported to its peer.
// Branch guard: fails on current master 3bb3c0c32 in this forced seam, which
// stock cannot reach from an incomplete body. The deadline fix makes
// that route live but fatally reports the expired request without the scan.
TEST( ClientTimeouts, MainFatalRecoveryReportsTheExpiredRequestFirst )
{
  TestJobManager jobs;
  XrdCl::Message pending( sizeof( ClientRequest ) );
  memset( pending.GetBuffer(), 0, pending.GetSize() );
  FrameHandler pendingHandler( time( 0 ) + 60, 0x5678 );
  FrameHandler future( time( 0 ) + 60, 0x9abc );
  ReaderSession session( time( 0 ) - 1 );
  session.stream.SetJobManager( &jobs.manager );
  session.stream.OnConnect( 0 );

  ASSERT_TRUE( session.stream.Send( &pending, &pendingHandler, false,
                                    time( 0 ) + 60 ).IsOK() );
  bool removed = false;
  session.incoming.AddMessageHandler( &future, removed );
  session.incoming.AssignTimeout( &future );
  auto response = std::make_shared<XrdCl::Message>(
    sizeof( ServerResponseHeader ) );
  memset( response->GetBuffer(), 0, response->GetSize() );
  auto *header = reinterpret_cast<ServerResponseHeader*>(
    response->GetBuffer() );
  header->streamid[0] = 0x34;
  header->streamid[1] = 0x12;
  header->status = kXR_oksofar;
  header->dlen = 16;
  session.stream.InstallIncHandler( response, 0 );

  session.poller.addSockets = false;
  session.stream.OnError(
    0, XrdCl::XRootDStatus( XrdCl::stError,
                            XrdCl::errOperationExpired ) );
  jobs.Drain();
  EXPECT_EQ( session.handler.events, 1u );
  EXPECT_EQ( session.handler.lastEvent, XrdCl::MsgHandler::Timeout );
  EXPECT_EQ( session.handler.lastStatus.code, XrdCl::errOperationExpired );
  EXPECT_EQ( future.events, 1u );
  EXPECT_EQ( future.lastEvent, XrdCl::MsgHandler::FatalError );
  EXPECT_FALSE( future.lastStatus.IsOK() );
  EXPECT_NE( future.lastStatus.code, XrdCl::errOperationExpired );
  EXPECT_EQ( pendingHandler.sendResults, 1u );
  EXPECT_TRUE( pendingHandler.lastSendStatus.IsFatal() );
  EXPECT_NE( pendingHandler.lastSendStatus.code,
             XrdCl::errOperationExpired );
}

// User: A reply expires while fatal recovery begins, and another thread sends
// a request while the timeout callback is running. The branch before this test
// admitted that request and then failed it as part of the older socket error;
// this guard proves fatal state and its original queue are captured atomically.
TEST( ClientTimeouts, FatalRecoveryCannotSweepAConcurrentSend )
{
  TestJobManager jobs;
  XrdCl::Message pending( sizeof( ClientRequest ) );
  XrdCl::Message concurrent( sizeof( ClientRequest ) );
  memset( pending.GetBuffer(), 0, pending.GetSize() );
  memset( concurrent.GetBuffer(), 0, concurrent.GetSize() );
  FrameHandler pendingHandler( time( 0 ) + 60, 0x5678 );
  FrameHandler concurrentHandler( time( 0 ) + 60, 0x9abc );
  ReaderSession session( time( 0 ) - 1 );
  session.stream.SetJobManager( &jobs.manager );
  session.stream.OnConnect( 0 );
  ASSERT_TRUE( session.stream.Send( &pending, &pendingHandler, false,
                                    time( 0 ) + 60 ).IsOK() );

  auto response = std::make_shared<XrdCl::Message>(
    sizeof( ServerResponseHeader ) );
  memset( response->GetBuffer(), 0, response->GetSize() );
  auto *header = reinterpret_cast<ServerResponseHeader*>(
    response->GetBuffer() );
  header->streamid[0] = 0x34;
  header->streamid[1] = 0x12;
  header->status = kXR_oksofar;
  header->dlen = 16;
  session.stream.InstallIncHandler( response, 0 );

  XrdSysSemaphore entered( 0 ), release( 0 );
  session.handler.eventEntered = &entered;
  session.handler.eventRelease = &release;
  session.poller.addSockets = false;
  std::thread failing( [&] {
    session.stream.OnError(
      0, XrdCl::XRootDStatus( XrdCl::stError,
                              XrdCl::errOperationExpired ) );
  } );
  entered.Wait();
  session.poller.addSockets = true;
  session.stream.OnConnect( 0 );
  XrdCl::XRootDStatus status = session.stream.Send(
    &concurrent, &concurrentHandler, false, time( 0 ) + 60 );
  release.Post();
  failing.join();
  jobs.Drain();

  EXPECT_TRUE( status.IsOK() );
  EXPECT_EQ( concurrentHandler.sendResults, 0u );
  EXPECT_EQ( pendingHandler.sendResults, 1u );
}

// User: One slow reply reaches its deadline while unrelated requests share the
// multiplexed connection. Closing that desynchronized socket must expire the
// slow request without labelling every future-deadline request as expired.
// Branch guard: fails on current master 3bb3c0c32 in this forced seam. Stock
// never routes a stalled-body deadline here; e8848d15b does and expires
// every handler.
TEST( ClientTimeouts, ExpiredBodyDoesNotExpireUnrelatedRequests )
{
  TestJobManager jobs;
  ReaderSession session( time( 0 ) - 1 );
  session.stream.SetJobManager( &jobs.manager );
  FrameHandler future( time( 0 ) + 60, 0x5678 );
  bool removed = false;
  session.incoming.AddMessageHandler( &future, removed );
  session.incoming.AssignTimeout( &future );

  auto response = std::make_shared<XrdCl::Message>(
    sizeof( ServerResponseHeader ) );
  memset( response->GetBuffer(), 0, response->GetSize() );
  auto *header = reinterpret_cast<ServerResponseHeader*>(
    response->GetBuffer() );
  header->streamid[0] = 0x34;
  header->streamid[1] = 0x12;
  header->status = kXR_oksofar;
  header->dlen = 16;
  session.stream.InstallIncHandler( response, 0 );

  session.stream.OnError(
    0, XrdCl::XRootDStatus( XrdCl::stError,
                            XrdCl::errOperationExpired ) );
  jobs.Drain();
  EXPECT_EQ( session.handler.events, 1u );
  EXPECT_EQ( session.handler.lastEvent, XrdCl::MsgHandler::Timeout );
  EXPECT_EQ( session.handler.lastStatus.code, XrdCl::errOperationExpired );
  EXPECT_EQ( future.events, 1u );
  EXPECT_EQ( future.lastEvent, XrdCl::MsgHandler::Broken );
  EXPECT_EQ( future.lastStatus.code, XrdCl::errSocketError );
}

// User: A socket fails just after the request deadline while a response body
// is active. The reported socket failure must retain its original meaning.
// Valid-path control: current master 3bb3c0c32 reports Broken/errSocketError;
// Branch guard: dropping the status-code discriminator changes it to Timeout.
TEST( ClientTimeouts, SocketErrorDoesNotBecomeAnIncomingTimeout )
{
  TestJobManager jobs;
  ReaderSession session( time( 0 ) - 1 );
  session.stream.SetJobManager( &jobs.manager );
  auto response = std::make_shared<XrdCl::Message>(
    sizeof( ServerResponseHeader ) );
  memset( response->GetBuffer(), 0, response->GetSize() );
  auto *header = reinterpret_cast<ServerResponseHeader*>(
    response->GetBuffer() );
  header->streamid[0] = 0x34;
  header->streamid[1] = 0x12;
  header->status = kXR_oksofar;
  header->dlen = 16;
  session.stream.InstallIncHandler( response, 0 );

  session.stream.OnError(
    0, XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errSocketError ) );
  jobs.Drain();
  EXPECT_EQ( session.handler.events, 1u );
  EXPECT_EQ( session.handler.lastEvent, XrdCl::MsgHandler::Broken );
  EXPECT_EQ( session.handler.lastStatus.code, XrdCl::errSocketError );
}

// User: A complete response finishes reconstruction exactly as its deadline is
// reached. Once every declared byte is consumed, the client must dispatch that
// frame instead of treating its already-finished body as an unfinished timeout.
// Branch guard: current master 3bb3c0c32 passes; published revision e8848d15b
// consumes the whole frame and then returns errOperationExpired from ReadDone.
TEST( ClientTimeouts, CompletedBodyIsDispatchedAtTheDeadline )
{
  const time_t deadline = time( 0 ) + 2;
  CompletesAtDeadlineHandler handler( deadline );
  ReaderSession session( deadline );
  session.incoming.RemoveMessageHandler( &session.handler );
  bool removed = false;
  session.incoming.AddMessageHandler( &handler, removed );
  const auto wire = Frame( kXR_oksofar, "x" );
  session.Send( wire.data(), wire.size() );

  const auto result = session.reader->Read();
  EXPECT_TRUE( result.IsOK() ) << result.ToString();
  EXPECT_TRUE( handler.completed );
  EXPECT_EQ( session.Pending(), 0 );
  session.incoming.RemoveMessageHandler( &handler );

  for( const std::string &body : { std::string(), std::string( "x" ) } )
  {
    ReaderSession late( time( 0 ) - 1 );
    const auto complete = Frame( kXR_ok, body );
    late.Send( complete.data(), complete.size() );
    EXPECT_TRUE( late.reader->Read().IsOK() );
    EXPECT_EQ( late.Pending(), 0 );
  }
}

// User: A plugin deliberately registers an incoming handler without a deadline.
// Valid-path control: passes on current master 3bb3c0c32; zero remains the no-deadline
// sentinel rather than expiring each response as soon as its header arrives.
TEST( ClientTimeouts, ZeroDeadlineStillAllowsCompleteReply )
{
  ReaderSession session( 0 );
  const auto wire = Frame( kXR_ok, "entry\n" );
  session.Send( wire.data(), wire.size() );
  auto result = session.reader->Read();
  EXPECT_TRUE( result.IsOK() );
  EXPECT_NE( result.code, XrdCl::suRetry );
}

// User: A directory stream completes one chunk and then its final response on
// the same TCP connection. A subsequent request must not inherit the previous
// request's expired deadline. Valid-path control: passes on current master 3bb3c0c32;
// this specifically guards cleanup of the helper consulted by the new check.
TEST( ClientTimeouts, CompletedFramesDoNotLeaveAStaleDeadline )
{
  ReaderSession session( time( 0 ) + 2 );
  for( auto status : { kXR_oksofar, kXR_ok } )
  {
    const auto wire = Frame( status, "entry\n" );
    session.Send( wire.data(), wire.size() );
    auto result = session.reader->Read();
    ASSERT_TRUE( result.IsOK() );
    ASSERT_NE( result.code, XrdCl::suRetry );
    session.reader->Reset();
  }
  const auto bound = std::chrono::steady_clock::now() + std::chrono::seconds( 3 );
  while( time( 0 ) < session.handler.deadline &&
         std::chrono::steady_clock::now() < bound )
    std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
  ASSERT_GE( time( 0 ), session.handler.deadline );
  ASSERT_EQ( session.reader->Read().code, XrdCl::suRetry );
  session.handler.deadline = time( 0 ) + 60;
  bool removed = false;
  session.incoming.AddMessageHandler( &session.handler, removed );
  const auto wire = Frame( kXR_ok, "next request\n" );
  session.Send( wire.data(), wire.size() );
  auto result = session.reader->Read();
  EXPECT_TRUE( result.IsOK() );
  EXPECT_NE( result.code, XrdCl::suRetry );
}

// User: A directory reply arrives in chunks and its request expires while the
// poller still holds the handler for an unfinished chunk. Tick must retain the
// handler until the socket reader relinquishes it, just as it does for reads.
// Fails on current master 3bb3c0c32: the timeout callback returns
// RemoveHandler for dirlist while the reader still has its raw pointer. This
// proves the missing fence, not a claimed deterministic end-user UAF execution.
TEST( ClientTimeouts, DirectoryPartialHandlerSurvivesUntilReaderReleasesIt )
{
  XrdCl::URL url( "root://127.0.0.1:1094//directory" );
  auto *request = new XrdCl::Message( sizeof( ClientRequest ) );
  memset( request->GetBuffer(), 0, request->GetSize() );
  auto *req = reinterpret_cast<ClientRequest*>( request->GetBuffer() );
  req->header.requestid = htons( kXR_dirlist );
  XrdCl::XRootDMsgHandler handler( request, nullptr, &url, {}, nullptr );
  // A response can precede the writer's completion notification. Keeping that
  // notification pending prevents final user callbacks in this focused test;
  // it does not change the independent timeout-fence/RemoveHandler decision.
  handler.OnReadyToSend( request );
  auto response = std::make_shared<XrdCl::Message>( sizeof( ServerResponseHeader ) );
  memset( response->GetBuffer(), 0, response->GetSize() );
  auto *header = reinterpret_cast<ServerResponseHeader*>( response->GetBuffer() );
  header->status = kXR_oksofar;
  header->dlen = 20;
  ASSERT_EQ( handler.Examine( response ), XrdCl::MsgHandler::NoProcess );
  XrdCl::XRootDStatus timeout( XrdCl::stError, XrdCl::errOperationExpired );
  ASSERT_EQ( handler.OnStreamEvent( XrdCl::MsgHandler::Timeout, timeout ), 0 );
  handler.PartialReceived();
  EXPECT_EQ( handler.OnStreamEvent( XrdCl::MsgHandler::Timeout, timeout ),
             XrdCl::MsgHandler::RemoveHandler );
}

// User: A busy server asks the client to wait for an asynchronous reply, but
// stalls while sending the declared kXR_waitresp body. Fails on current master
// 3bb3c0c32: Timeout removes the handler while InMessageHelper retains
// its non-owning pointer. The final call models OnIncoming releasing that borrow.
TEST( ClientTimeouts, WaitResponseBodyKeepsHandlerAliveUntilReaderReleasesIt )
{
  XrdCl::URL url( "root://127.0.0.1:1094//wait-response" );
  auto *request = new XrdCl::Message( sizeof( ClientRequest ) );
  memset( request->GetBuffer(), 0, request->GetSize() );
  auto *req = reinterpret_cast<ClientRequest*>( request->GetBuffer() );
  req->header.requestid = htons( kXR_stat );
  XrdCl::XRootDMsgHandler handler( request, nullptr, &url, {}, nullptr );
  handler.OnReadyToSend( request );
  auto response = std::make_shared<XrdCl::Message>(
    sizeof( ServerResponseHeader ) );
  memset( response->GetBuffer(), 0, response->GetSize() );
  auto *header = reinterpret_cast<ServerResponseHeader*>(
    response->GetBuffer() );
  header->status = kXR_waitresp;
  header->dlen = sizeof( uint32_t );
  ASSERT_EQ( handler.Examine( response ), XrdCl::MsgHandler::Ignore );
  XrdCl::XRootDStatus timeout( XrdCl::stError, XrdCl::errOperationExpired );
  ASSERT_EQ( handler.OnStreamEvent( XrdCl::MsgHandler::Timeout, timeout ), 0 );
  handler.PartialReceived();
  EXPECT_EQ( handler.OnStreamEvent( XrdCl::MsgHandler::Timeout, timeout ),
             XrdCl::MsgHandler::RemoveHandler );
}

// User: A valid kXR_waitresp frame completes, then the original request reaches
// its deadline while awaiting the asynchronous reply. Branch guard: current master
// 3bb3c0c32 passes without this fence; the branch must prove OnIncoming clears the newly
// required one.
TEST( ClientTimeouts, CompletedWaitResponseDropsIncomingHelperFence )
{
  ReaderSession session( time( 0 ) + 60 );
  session.incoming.RemoveMessageHandler( &session.handler );
  auto *request = new XrdCl::Message( sizeof( ClientRequest ) );
  memset( request->GetBuffer(), 0, request->GetSize() );
  auto *req = reinterpret_cast<ClientRequest*>( request->GetBuffer() );
  req->header.streamid[0] = 0x34;
  req->header.streamid[1] = 0x12;
  req->header.requestid = htons( kXR_stat );
  XrdCl::XRootDMsgHandler handler( request, nullptr, &session.url, {}, nullptr );
  handler.OnReadyToSend( request );
  bool removed = false;
  session.incoming.AddMessageHandler( &handler, removed );
  uint32_t seconds = htonl( 1 );
  std::string body( sizeof( seconds ), '\0' );
  memcpy( &body[0], &seconds, sizeof( seconds ) );
  const auto wire = Frame( kXR_waitresp, body );
  session.Send( wire.data(), wire.size() );
  EXPECT_TRUE( session.reader->Read().IsOK() );
  XrdCl::XRootDStatus timeout( XrdCl::stError, XrdCl::errOperationExpired );
  EXPECT_EQ( handler.OnStreamEvent( XrdCl::MsgHandler::Timeout, timeout ),
             XrdCl::MsgHandler::RemoveHandler );
  session.incoming.RemoveMessageHandler( &handler );
}

// User: A chunked-response API delivers kXR_oksofar through a worker while its
// deadline fires on the tick thread. The worker's borrowed handler must remain
// valid until processing finishes, then become eligible for the next timeout.
// Fails on current master 3bb3c0c32: no fence protects the queued
// Process job; Branch guard: Process must also take that new fence back down.
TEST( ClientTimeouts, ChunkedResponseFenceSpansQueuedProcessingOnly )
{
  class Blocker : public XrdCl::Job
  {
    public:
      Blocker( XrdSysSemaphore &entered, XrdSysSemaphore &release ) :
        entered( entered ), release( release ) {}
      void Run( void* ) override
      {
        entered.Post();
        release.Wait();
      }
      XrdSysSemaphore &entered;
      XrdSysSemaphore &release;
  };
  TestJobManager jobs;
  ReaderSession session( time( 0 ) + 60 );
  session.stream.SetJobManager( &jobs.manager );
  session.incoming.RemoveMessageHandler( &session.handler );

  auto *request = new XrdCl::Message( sizeof( ClientRequest ) );
  memset( request->GetBuffer(), 0, request->GetSize() );
  auto *req = reinterpret_cast<ClientRequest*>( request->GetBuffer() );
  req->header.streamid[0] = 0x34;
  req->header.streamid[1] = 0x12;
  req->header.requestid = htons( kXR_ping );
  RecordingResponse responseHandler;
  XrdCl::XRootDMsgHandler handler(
    request, &responseHandler, &session.url, {}, nullptr );
  handler.SetOksofarAsAnswer( true );
  auto *hosts = new XrdCl::HostList();
  hosts->emplace_back( session.url );
  handler.SetHostList( hosts );
  handler.OnReadyToSend( request );
  bool removed = false;
  session.incoming.AddMessageHandler( &handler, removed );

  XrdSysSemaphore entered( 0 ), release( 0 );
  Blocker blocker( entered, release );
  jobs.manager.QueueJob( &blocker );
  entered.Wait();
  const auto wire = Frame( kXR_oksofar, "" );
  session.Send( wire.data(), wire.size() );
  EXPECT_TRUE( session.reader->Read().IsOK() );
  XrdCl::XRootDStatus timeout( XrdCl::stError, XrdCl::errOperationExpired );
  EXPECT_EQ( handler.OnStreamEvent( XrdCl::MsgHandler::Timeout, timeout ), 0 );

  release.Post();
  jobs.Drain();
  EXPECT_EQ( responseHandler.calls, 1u );
  EXPECT_EQ( responseHandler.code, XrdCl::suContinue );
  EXPECT_EQ( handler.OnStreamEvent( XrdCl::MsgHandler::Timeout, timeout ),
             XrdCl::MsgHandler::RemoveHandler );
  session.incoming.RemoveMessageHandler( &handler );
}
