#include "XrdSys/XrdSysIOEvents.hh"

#include <gtest/gtest.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <thread>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
using namespace XrdSys::IOEvents;

void Check(bool condition) { if (!condition) _exit(1); }

class SocketPair
{
public:
  SocketPair() { Check(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0); }
  ~SocketPair() { for (int value : fd) if (value >= 0) close(value); }
  void Write() { Check(write(fd[1], "x", 1) == 1); }
  void Hangup() { close(fd[1]); fd[1] = -1; }
  int fd[2];
};

Poller *MakePoller()
{
  int error = 0;
  Poller *poller = Poller::Create(error);
  Check(poller && !error);
  return poller;
}

struct RecordingCallback : CallBack
{
  XrdSysSemaphore done{0};
  std::atomic<int> events{0}, errors{0};
  bool Event(Channel *, void *, int flags) override
  {
    events = flags;
    done.Post();
    return false;
  }
  void Fatal(Channel *, void *, int error, const char *) override
  {
    errors = error;
    done.Post();
  }
};

#ifdef XRD_SYS_IOEVENTS_FORCE_PORT_REARM
class EventPortPoller : public Poller
{
public:
  EventPortPoller() : Poller(-1, -1)
  {
    pollTid = XrdSysThread::ID();
    cmdFD = 0;
  }

  void Dispatch(Channel *channel, int events, int error = 0)
  {
    pollTid = XrdSysThread::ID();
    const char *text = error ? "polling" : nullptr;
    bool locked = false;
    if (!CbkXeq(channel, events, error, text))
      Exclude(channel, locked, false);
  }

  void ResetCounts() { modifies = 0; excludes = 0; }

  std::atomic<int> modifies{0}, excludes{0};
  std::atomic<bool> blockDeleteExclude{false};
  XrdSysSemaphore deleteExcludeEntered{0}, deleteExcludeRelease{0};

protected:
  void Begin(XrdSysSemaphore *, int &, const char **) override {}
  void Exclude(Channel *, bool &, bool verify) override
  {
    ++excludes;
    if (verify && blockDeleteExclude)
    {
      deleteExcludeEntered.Post();
      deleteExcludeRelease.Wait();
    }
  }
  bool Include(Channel *, int &, const char **, bool &) override { return true; }
  bool Modify(Channel *channel, int &, const char **, bool &) override
  {
    (void)channel->GetFD();
    ++modifies;
    return true;
  }
  void Shutdown() override {}
};
#endif
}

// User: A plugin asks for fatal notifications, handles a disconnect, and then
// releases that channel. Stock leaves callback mode set, so Delete hangs after
// the callback has already finished.
TEST(IOEventsCallback, FatalCompletionAllowsLaterDeletion)
{
  ASSERT_EXIT(([] {
    alarm(5);
    Poller *poller = MakePoller();
    SocketPair failed, healthy;
    RecordingCallback fatal, good;
    Channel *channel = new Channel(poller, failed.fd[0], &fatal);
    Channel *other = new Channel(poller, healthy.fd[0], &good);
    Check(channel->Enable(Channel::readEvents | Channel::errorEvents));
    Check(other->Enable(Channel::readEvents));
    failed.Hangup();
    fatal.done.Wait();
    healthy.Write();
    good.done.Wait();
    channel->Delete();
    other->Delete();
    delete poller;
    _exit(0);
  }()), ::testing::ExitedWithCode(0), "");
}

// User: One thread releases a failed plugin channel while Fatal is still
// running. Stock never acknowledges that callback completion, so Delete waits
// forever.
TEST(IOEventsCallback, FatalCompletionAcknowledgesConcurrentDeletion)
{
  ASSERT_EXIT(([] {
    alarm(5);
    struct BlockingFatal : CallBack
    {
      XrdSysSemaphore entered{0}, release{0};
      bool Event(Channel *, void *, int) override { _exit(2); }
      void Fatal(Channel *, void *, int, const char *) override
      {
        entered.Post();
        release.Wait();
      }
    } callback;
    Poller *poller = MakePoller();
    SocketPair failed;
    Channel *channel = new Channel(poller, failed.fd[0], &callback);
    Check(channel->Enable(Channel::readEvents | Channel::errorEvents));
    failed.Hangup();
    callback.entered.Wait();
    XrdSysSemaphore deleting(0);
    std::thread deleter([&] { deleting.Post(); channel->Delete(); });
    deleting.Wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    callback.release.Post();
    deleter.join();
    delete poller;
    _exit(0);
  }()), ::testing::ExitedWithCode(0), "");
}

// User: A plugin replaces a failed socket from inside Fatal and continues on
// the replacement. Stock fatal cleanup removes the new registration, so it
// never receives input.
TEST(IOEventsCallback, FatalCallbackCanReplaceTheDescriptor)
{
  ASSERT_EXIT(([] {
    alarm(5);
    struct Replacing : RecordingCallback
    {
      int replacement = -1;
      void Fatal(Channel *channel, void *, int error, const char *) override
      {
        errors = error;
        channel->SetFD(replacement);
        Check(channel->Enable(Channel::readEvents));
        done.Post();
      }
    } callback;
    Poller *poller = MakePoller();
    SocketPair failed, replacement;
    callback.replacement = replacement.fd[0];
    Channel *channel = new Channel(poller, failed.fd[0], &callback);
    Check(channel->Enable(Channel::readEvents | Channel::errorEvents));
    failed.Hangup();
    callback.done.Wait();
    replacement.Write();
    callback.done.Wait();
    Check(callback.errors != 0 &&
          (callback.events.load() & CallBack::ReadyToRead));
    channel->Delete();
    delete poller;
    _exit(0);
  }()), ::testing::ExitedWithCode(0), "");
}

// User: An ordinary callback withdraws its descriptor while another thread
// releases the channel. This passes stock; it guards the new replacement state
// so Delete still waits until the callback returns.
TEST(IOEventsCallback, SetFDPreservesCallbackOwnershipUntilReturn)
{
  ASSERT_EXIT(([] {
    alarm(5);
    struct Withdrawing : CallBack
    {
      XrdSysSemaphore entered{0}, release{0};
      bool Event(Channel *channel, void *, int) override
      {
        channel->SetFD(-1);
        entered.Post();
        release.Wait();
        return true;
      }
      void Fatal(Channel *, void *, int, const char *) override { _exit(2); }
    } callback;
    Poller *poller = MakePoller();
    SocketPair socket;
    Channel *channel = new Channel(poller, socket.fd[0], &callback);
    Check(channel->Enable(Channel::readEvents));
    socket.Write();
    callback.entered.Wait();
    std::atomic<bool> deleted{false};
    XrdSysSemaphore deleting(0);
    std::thread deleter([&] {
      deleting.Post();
      channel->Delete();
      deleted = true;
    });
    deleting.Wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Check(!deleted);
    callback.release.Post();
    deleter.join();
    Check(deleted);
    delete poller;
    _exit(0);
  }()), ::testing::ExitedWithCode(0), "");
}

// User: A callback withdraws its descriptor and its owner deletes the channel
// after the callback finishes. This passes stock; it proves the new replacement
// state returns to idle instead of making that later Delete hang.
TEST(IOEventsCallback, SetFDTransitionClearsAfterCallbackReturns)
{
  ASSERT_EXIT(([] {
    alarm(5);
    struct Withdrawing : CallBack
    {
      XrdSysSemaphore done{0};
      bool Event(Channel *channel, void *, int) override
      {
        channel->SetFD(-1);
        done.Post();
        return true;
      }
      void Fatal(Channel *, void *, int, const char *) override { _exit(2); }
    } callback;
    Poller *poller = MakePoller();
    SocketPair socket, barrierSocket;
    RecordingCallback barrier;
    Channel *channel = new Channel(poller, socket.fd[0], &callback);
    Channel *fence = new Channel(poller, barrierSocket.fd[0], &barrier);
    Check(channel->Enable(Channel::readEvents));
    Check(fence->Enable(Channel::readEvents));
    socket.Write();
    callback.done.Wait();
    barrierSocket.Write();
    barrier.done.Wait();
    channel->Delete();
    fence->Delete();
    delete poller;
    _exit(0);
  }()), ::testing::ExitedWithCode(0), "");
}

#ifdef XRD_SYS_IOEVENTS_FORCE_PORT_REARM
// User: A healthy Solaris channel remains enabled after ordinary input. This
// control proves moving rearm into common completion still rearms it once.
TEST(IOEventsCallback, EventPortRearmsLiveCallbackExactlyOnce)
{
  struct KeepEnabled : CallBack
  {
    bool Event(Channel *, void *, int) override { return true; }
  } callback;
  EventPortPoller poller;
  Channel *channel = new Channel(&poller, -1, &callback);
  ASSERT_TRUE(channel->Enable(Channel::readEvents));
  poller.ResetCounts();
  poller.Dispatch(channel, CallBack::ReadyToRead);
  EXPECT_EQ(poller.modifies.load(), 1);
  channel->Delete();
}

// User: A Solaris plugin receives Fatal for a failed descriptor. The earlier
// #2957 revision incorrectly rearmed that failed descriptor after completion.
TEST(IOEventsCallback, EventPortFatalCompletionDoesNotRearm)
{
  struct FatalOnly : CallBack
  {
    bool Event(Channel *, void *, int) override { return false; }
    void Fatal(Channel *, void *, int error, const char *) override
    { Check(error == EPIPE); }
  } callback;
  EventPortPoller poller;
  Channel *channel = new Channel(&poller, -1, &callback);
  ASSERT_TRUE(channel->Enable(Channel::readEvents | Channel::errorEvents));
  poller.ResetCounts();
  poller.Dispatch(channel, 0, EPIPE);
  EXPECT_EQ(poller.modifies.load(), 0);
  EXPECT_EQ(poller.excludes.load(), 1);
  channel->Delete();
}

// User: A Solaris Fatal callback deletes its own channel. The earlier #2957
// revision returned to its backend and dereferenced that freed channel.
TEST(IOEventsCallback, EventPortFatalCallbackCanDeleteItsChannel)
{
  struct SelfDeleting : CallBack
  {
    bool Event(Channel *, void *, int) override { return false; }
    void Fatal(Channel *channel, void *, int, const char *) override
    { channel->Delete(); }
  } callback;
  EventPortPoller poller;
  Channel *channel = new Channel(&poller, -1, &callback);
  ASSERT_TRUE(channel->Enable(Channel::readEvents | Channel::errorEvents));
  poller.ResetCounts();
  poller.Dispatch(channel, 0, EPIPE);
  EXPECT_EQ(poller.modifies.load(), 0);
}

// User: One Solaris thread deletes a failed channel while Fatal is running.
// The earlier #2957 revision acknowledged deletion and then rearmed through
// storage that the deleting thread could free.
TEST(IOEventsCallback, EventPortConcurrentDeleteCannotRaceRearm)
{
  struct BlockingFatal : CallBack
  {
    XrdSysSemaphore entered{0}, release{0};
    bool Event(Channel *, void *, int) override { return false; }
    void Fatal(Channel *, void *, int, const char *) override
    {
      entered.Post();
      release.Wait();
    }
  } callback;
  EventPortPoller poller;
  Channel *channel = new Channel(&poller, -1, &callback);
  ASSERT_TRUE(channel->Enable(Channel::readEvents | Channel::errorEvents));
  poller.ResetCounts();
  poller.blockDeleteExclude = true;
  std::thread dispatcher([&] { poller.Dispatch(channel, 0, EPIPE); });
  callback.entered.Wait();
  std::thread deleter([&] { channel->Delete(); });
  poller.deleteExcludeEntered.Wait();
  callback.release.Post();
  poller.deleteExcludeRelease.Post();
  dispatcher.join();
  deleter.join();
  EXPECT_EQ(poller.modifies.load(), 0);
}
#endif
