//------------------------------------------------------------------------------
// Tests of XrdCl::JobManager
//------------------------------------------------------------------------------

#include "XrdCl/XrdClJobManager.hh"

#include <gtest/gtest.h>

#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace
{
  // A job which runs a function, and deletes itself after it
  class FunctionJob : public XrdCl::Job
  {
    public:
      explicit FunctionJob( std::function<void()> f ) : function( std::move( f ) ) {}

      void Run( void* ) override
      {
        function();
        delete this;
      }

    private:
      std::function<void()> function;
  };
}

// The fork handler of XrdCl stops and starts the job manager. When a callback
// forks, the worker which runs the callback stops the job manager, and the
// new start makes a new worker in its place. The old worker must then end.
//
// The scenario runs in a child process. After a timeout, a worker can still
// run a job which uses the state of the scenario, or wait in Stop() for the
// mutex of the manager. Then no cleanup is safe, and the child ends with
// _exit(), which runs no destructors. The parent limits the time of the child.
namespace
{
  [[noreturn]] void Fail( const char *defect )
  {
    std::fprintf( stderr, "%s\n", defect );
    _exit( 1 );
  }

  [[noreturn]] void RestartTheManagerInAJob()
  {
    XrdCl::JobManager manager( 1 );
    if( !manager.Initialize() || !manager.Start() )
      Fail( "the job manager did not start" );

    pthread_t restarter;
    std::promise<void> restarted;

    manager.QueueJob( new FunctionJob( [&]
    {
      restarter = pthread_self();
      manager.Stop();
      manager.Start();
      restarted.set_value();
    } ) );

    if( restarted.get_future().wait_for( 10s ) != std::future_status::ready )
      Fail( "the job which restarts the job manager did not end within 10 s" );

    // Each job takes some time, thus two workers would both run some of them
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<pthread_t> runners;

    for( int i = 0; i < 20; ++i )
      manager.QueueJob( new FunctionJob( [&]
      {
        std::this_thread::sleep_for( 5ms );
        std::lock_guard<std::mutex> lock( mutex );
        runners.push_back( pthread_self() );
        cv.notify_one();
      } ) );

    std::unique_lock<std::mutex> lock( mutex );
    if( !cv.wait_for( lock, 10s, [&] { return runners.size() == 20; } ) )
      Fail( "the 20 jobs did not end within 10 s" );

    for( pthread_t runner : runners )
      if( pthread_equal( runner, restarter ) )
        Fail( "the worker which restarted the job manager ran more jobs" );

    lock.unlock();
    manager.Stop();
    manager.Finalize();
    _exit( 0 );
  }
}

TEST(JobManagerTest, WorkerWhichRestartsTheManagerRunsNoMoreJobs)
{
  pid_t pid = fork();
  ASSERT_NE( pid, -1 ) << "fork failed";
  if( pid == 0 ) RestartTheManagerInAJob();

  int status = 0;
  pid_t rc;
  auto deadline = std::chrono::steady_clock::now() + 30s;
  while( ( rc = waitpid( pid, &status, WNOHANG ) ) == 0 &&
         std::chrono::steady_clock::now() < deadline )
    std::this_thread::sleep_for( 10ms );

  if( rc == 0 )
  {
    kill( pid, SIGKILL );
    waitpid( pid, &status, 0 );
    FAIL() << "the child process did not end within 30 s";
  }

  ASSERT_EQ( rc, pid ) << "waitpid failed";
  ASSERT_TRUE( WIFEXITED( status ) ) << "the child process ended by a signal";
  EXPECT_EQ( WEXITSTATUS( status ), 0 ) << "the defect is in the output of the child process";
}
