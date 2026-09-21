#include <gtest/gtest.h>

#include <chrono>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <future>
#include <sstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>

#include "Xrd/XrdScheduler.hh"
#include "XrdOfs/XrdOfsPrepare.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSfs/XrdSfsInterface.hh"

extern "C" XrdOfsPrepare *XrdOfsgetPrepare(XrdOfsgetPrepareArguments);

// The test target links the implementation directly. Use its existing
// admission lock to arrange wakeups without adding a production test API.
namespace XrdOfsPrepGPIReal {
extern XrdSysCondVar qryCond;
extern int qryAllow;
extern int qryWait;
extern int maxFiles, maxResp;
extern bool Debug;
void ResetForTest();
extern void (*BeforeTraceForTest)();
}

namespace {

XrdScheduler &testScheduler() {
  // Scheduler threads have no stop API, so their owner must outlive all tests.
  static XrdScheduler *scheduler = []() {
    auto *result = new XrdScheduler(3, 1, 0);
    result->Start();
    return result;
  }();
  return *scheduler;
}

struct SchedulerBarrier : XrdJob {
  std::promise<void> done;
  void DoIt() override {
    done.set_value();
    delete this;
  }
};

void drainScheduler() {
  // With one worker, this runs only after PrepGRun::DoIt has drained its queue
  // and returned the runner to the free list, allowing safe fixture teardown.
  auto *barrier = new SchedulerBarrier;
  auto done = barrier->done.get_future();
  testScheduler().Schedule(barrier);
  done.wait();
}

struct TestPrepArgs {
  XrdSfsPrep prep{};
  std::string id;
  XrdOucTListFIFO paths;
  XrdOucTListFIFO oinfo;

  TestPrepArgs(const std::string &reqId, int opts,
               const std::vector<std::pair<std::string, std::string>> &files)
      : id(reqId) {
    prep.reqid = const_cast<char *>(id.c_str());
    prep.opts = opts;
    int idx = 0;
    for (const auto &f : files) {
      paths.Add(new XrdOucTList(f.first.c_str(), idx++));
      oinfo.Add(new XrdOucTList(f.second.c_str()));
    }
    prep.paths = paths.first;
    prep.oinfo = oinfo.first;
  }
};

class XrdOfsPrepGPITest : public ::testing::Test {
protected:
  std::string tempDir;
  std::string scriptPath;
  int startedFd = -1;
  int gateFd = -1;
  bool released = false;
  XrdSysLogger logger;
  XrdSysError eLog{&logger, "TestGPI"};
  XrdOucEnv env;
  std::unique_ptr<XrdOfsPrepare> prepare;
  std::vector<std::thread> queryThreads;

  XrdOfsPrepare *makePrepare(const char *parms, XrdOss *oss = nullptr) {
    prepare.reset(XrdOfsgetPrepare(&eLog, nullptr, parms, nullptr, oss, &env));
    return prepare.get();
  }

  bool waitForStarted() {
    struct pollfd pfd{startedFd, POLLIN, 0};
    int rc;
    do {
      rc = poll(&pfd, 1, 10000);
    } while (rc < 0 && errno == EINTR);
    char started;
    return rc == 1 && (pfd.revents & POLLIN) &&
           read(startedFd, &started, 1) == 1;
  }

  void releaseGate() {
    if (gateFd >= 0 && !released) {
      ASSERT_EQ(write(gateFd, "go\n", 3), 3);
      released = true;
    }
  }

  struct QueryResult {
    int rc;
    int error;
    std::string output;
  };

  std::future<QueryResult> startQuery(const std::string &id) {
    std::packaged_task<QueryResult()> task([this, id]() {
      TestPrepArgs args(id, Prep_QUERY, {{"/data/f1", ""}});
      XrdOucErrInfo info;
      int rc = prepare->query(args.prep, info);
      return QueryResult{rc, info.getErrInfo(), info.getErrText()};
    });
    auto result = task.get_future();
    queryThreads.emplace_back(std::move(task));
    return result;
  }

  bool lockQueryWaiter(XrdSysCondVarHelper &lock) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    do {
      lock.Lock(&XrdOfsPrepGPIReal::qryCond);
      if (XrdOfsPrepGPIReal::qryWait > 0) return true;
      lock.UnLock();
      std::this_thread::yield();
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
  }

  bool waitForQueryWaiter() {
    XrdSysCondVarHelper lock;
    return lockQueryWaiter(lock);
  }

  std::string executions() {
    std::ifstream log(scriptPath + ".executed");
    if (!log) return {};
    std::ostringstream contents;
    contents << log.rdbuf();
    return contents.str();
  }

  void SetUp() override {
    env.PutPtr("XrdScheduler*", &testScheduler());
    char tempPath[] = "/tmp/test_prep_gpi_XXXXXX";
    ASSERT_NE(mkdtemp(tempPath), nullptr);
    tempDir = tempPath;
    scriptPath = tempDir + "/prepare";
    ASSERT_EQ(mkfifo((scriptPath + ".started").c_str(), 0600), 0);
    ASSERT_EQ(mkfifo((scriptPath + ".gate").c_str(), 0600), 0);
    startedFd = open((scriptPath + ".started").c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    gateFd = open((scriptPath + ".gate").c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    ASSERT_GE(startedFd, 0);
    ASSERT_GE(gateFd, 0);

    const char scriptContent[] =
        "#!/bin/sh\n"
        "while [ $# -gt 0 ]; do\n"
        "  case \"$1\" in\n"
        "    --)\n"
        "      shift\n"
        "      break\n"
        "      ;;\n"
        "    *)\n"
        "      shift\n"
        "      ;;\n"
        "  esac\n"
        "done\n"
        "reqid=\"$1\"\n"
        "cmd=\"$2\"\n"
        "shift 2\n"
        "printf '%s\\n' \"$reqid\" >> \"$0.executed\"\n"
        "if [ \"$reqid\" = \"BLOCKED_QUERY\" ] || [ \"$reqid\" = \"BLOCKED_STAGE\" ]; then\n"
        "  printf s > \"$0.started\"\n"
        "  read release < \"$0.gate\"\n"
        "fi\n"
        "case \"$cmd\" in\n"
        "  query)\n"
        "    if [ \"$reqid\" = \"FAIL_QUERY\" ]; then\n"
        "      echo misleading-success-output\n"
        "      exit 1\n"
        "    elif [ \"$reqid\" = \"RAW_QUERY\" ]; then\n"
        "      printf '  leading\\n\\ntrailing  '\n"
        "      exit 0\n"
        "    elif [ \"$reqid\" = \"TRUNC_QUERY\" ]; then\n"
        "      awk 'BEGIN { for (i=0; i<150; i++) printf \"line %04d: 012345678901234567890123456789\\n\", i }'\n"
        "      exit 0\n"
        "    fi\n"
        "    echo \"QUERY_SUCCESS reqid=$reqid args=$*\"\n"
        "    exit 0\n"
        "    ;;\n"
        "  stage|cancel|evict)\n"
        "    exit 0\n"
        "    ;;\n"
        "  *)\n"
        "    exit 2\n"
        "    ;;\n"
        "esac\n";

    std::ofstream script(scriptPath);
    script << scriptContent;
    script.close();
    ASSERT_TRUE(script.good());
    ASSERT_EQ(chmod(scriptPath.c_str(), 0755), 0);
  }

  void TearDown() override {
    // Release and join even when a test assertion fails. No runner may retain
    // this fixture's logger or script when the next factory invocation resets
    // the plugin's process-global state.
    releaseGate();
    for (auto &thread : queryThreads) thread.join();
    drainScheduler();
    prepare.reset();
    XrdOfsPrepGPIReal::BeforeTraceForTest = nullptr;
    XrdOfsPrepGPIReal::ResetForTest();
    if (startedFd >= 0) close(startedFd);
    if (gateFd >= 0) close(gateFd);
    if (!scriptPath.empty()) {
      unlink(scriptPath.c_str());
      unlink((scriptPath + ".started").c_str());
      unlink((scriptPath + ".gate").c_str());
      unlink((scriptPath + ".executed").c_str());
      unlink((scriptPath + ".response").c_str());
    }
    if (!tempDir.empty()) rmdir(tempDir.c_str());
  }
};

TEST_F(XrdOfsPrepGPITest, FactoryRejectsMissingScheduler) {
  const auto parms = "-admit stage,query -run " + scriptPath;
  XrdOucEnv emptyEnv;
  std::unique_ptr<XrdOfsPrepare> prep(
      XrdOfsgetPrepare(&eLog, nullptr, parms.c_str(), nullptr, nullptr, &emptyEnv));
  EXPECT_EQ(prep, nullptr);
}

TEST_F(XrdOfsPrepGPITest, FactoryRejectsMissingEnvironment) {
  const auto parms = "-admit stage,query -run " + scriptPath;
  std::unique_ptr<XrdOfsPrepare> prep(
      XrdOfsgetPrepare(&eLog, nullptr, parms.c_str(), nullptr, nullptr, nullptr));
  EXPECT_EQ(prep, nullptr);
}

TEST_F(XrdOfsPrepGPITest, NormalQueryCapturesRawStreamOutput) {
  char parms[1024];
  snprintf(parms, sizeof(parms), "-admit stage,query,cancel,evict -run %s -maxfiles 8", scriptPath.c_str());
  auto prep = makePrepare(parms);
  ASSERT_NE(prep, nullptr);

  TestPrepArgs args("REQ1", Prep_QUERY, {{"/data/f1", ""}, {"/data/f2", ""}});
  XrdOucErrInfo eInfo;
  int rc = prep->query(args.prep, eInfo);
  EXPECT_EQ(rc, SFS_DATA);
  ASSERT_NE(eInfo.getErrText(), nullptr);
  EXPECT_NE(strstr(eInfo.getErrText(), "QUERY_SUCCESS reqid=REQ1"), nullptr);
  EXPECT_NE(strstr(eInfo.getErrText(), "/data/f1 /data/f2"), nullptr);
}

TEST_F(XrdOfsPrepGPITest, QueryWithPerFilePathCgi) {
  char parms[1024];
  snprintf(parms, sizeof(parms), "-admit stage,query,cancel,evict -cgi -run %s -maxfiles 8", scriptPath.c_str());
  auto prep = makePrepare(parms);
  ASSERT_NE(prep, nullptr);

  TestPrepArgs args("REQ_CGI", Prep_QUERY, {{"/data/f1", "authz=bearer123"}, {"/data/f2", "token=xyz"}});
  XrdOucErrInfo eInfo;
  int rc = prep->query(args.prep, eInfo);
  EXPECT_EQ(rc, SFS_DATA);
  ASSERT_NE(eInfo.getErrText(), nullptr);
  EXPECT_NE(strstr(eInfo.getErrText(), "/data/f1?authz=bearer123"), nullptr);
  EXPECT_NE(strstr(eInfo.getErrText(), "/data/f2?token=xyz"), nullptr);
}

TEST_F(XrdOfsPrepGPITest, QueryPreservesWhitespaceAndMissingFinalNewline) {
  const auto parms = "-admit query -run " + scriptPath;
  auto prep = makePrepare(parms.c_str());
  ASSERT_NE(prep, nullptr);
  TestPrepArgs args("RAW_QUERY", Prep_QUERY, {{"/data/f1", ""}});
  XrdOucErrInfo info;
  ASSERT_EQ(prep->query(args.prep, info), SFS_DATA);
  const std::string expected = "  leading\n\ntrailing  ";
  EXPECT_EQ(std::string(info.getErrText()), expected);
  EXPECT_EQ(info.getErrInfo(), static_cast<int>(expected.size() + 1));
}

TEST_F(XrdOfsPrepGPITest, QueryKeepsEmptyCgiAlignedWithPaths) {
  const auto parms = "-admit query -cgi -run " + scriptPath;
  auto prep = makePrepare(parms.c_str());
  ASSERT_NE(prep, nullptr);
  TestPrepArgs args("REQ_CGI", Prep_QUERY,
                   {{"/data/f1", ""}, {"/data/f2", "token=xyz"}, {"/data/f3", ""}});
  XrdOucErrInfo info;
  ASSERT_EQ(prep->query(args.prep, info), SFS_DATA);
  EXPECT_STREQ(info.getErrText(),
               "QUERY_SUCCESS reqid=REQ_CGI args=/data/f1 /data/f2?token=xyz /data/f3\n");
}

TEST_F(XrdOfsPrepGPITest, QueryAllowsMissingAndShortCgiLists) {
  const auto parms = "-admit query -cgi -run " + scriptPath;
  auto prep = makePrepare(parms.c_str());
  ASSERT_NE(prep, nullptr);
  TestPrepArgs args("REQ_CGI", Prep_QUERY, {{"/data/f1", ""}, {"/data/f2", ""}});
  args.prep.oinfo = nullptr;
  XrdOucErrInfo info;
  ASSERT_EQ(prep->query(args.prep, info), SFS_DATA);
  EXPECT_STREQ(info.getErrText(), "QUERY_SUCCESS reqid=REQ_CGI args=/data/f1 /data/f2\n");

  XrdOucTList cgi("token=xyz");
  args.prep.oinfo = &cgi;
  ASSERT_EQ(prep->query(args.prep, info), SFS_DATA);
  EXPECT_STREQ(info.getErrText(),
               "QUERY_SUCCESS reqid=REQ_CGI args=/data/f1?token=xyz /data/f2\n");
}

TEST_F(XrdOfsPrepGPITest, OversizedCgiReturnsE2BIG) {
  char parms[1024];
  snprintf(parms, sizeof(parms), "-admit stage,query,cancel,evict -cgi -run %s -maxfiles 8", scriptPath.c_str());
  auto prep = makePrepare(parms);
  ASSERT_NE(prep, nullptr);

  std::string hugeCgi(9000, 'x');
  TestPrepArgs args("REQ_HUGE_CGI", Prep_QUERY, {{"/data/f1", hugeCgi}});
  XrdOucErrInfo eInfo;
  int rc = prep->query(args.prep, eInfo);
  EXPECT_EQ(rc, SFS_ERROR);
  EXPECT_EQ(eInfo.getErrInfo(), E2BIG);
  EXPECT_NE(std::string(eInfo.getErrText()).find("/data/f1"), std::string::npos);
  EXPECT_NE(std::string(eInfo.getErrText()).find("8191 bytes"), std::string::npos);
  EXPECT_EQ(std::string(eInfo.getErrText()).find(hugeCgi.substr(0, 50)), std::string::npos);
  EXPECT_TRUE(executions().empty());
}

TEST_F(XrdOfsPrepGPITest, OversizedFileListReturnsE2BIG) {
  char parms[1024];
  snprintf(parms, sizeof(parms), "-admit stage,query,cancel,evict -run %s -maxfiles 2", scriptPath.c_str());
  auto prep = makePrepare(parms);
  ASSERT_NE(prep, nullptr);

  TestPrepArgs args("REQ_BIG", Prep_STAGE, {{"/f1", ""}, {"/f2", ""}, {"/f3", ""}});
  XrdOucErrInfo eInfo;
  int rc = prep->begin(args.prep, eInfo);
  EXPECT_EQ(rc, SFS_ERROR);
  EXPECT_EQ(eInfo.getErrInfo(), E2BIG);
  EXPECT_NE(std::string(eInfo.getErrText()).find("3 files (limit 2"), std::string::npos);
  EXPECT_NE(std::string(eInfo.getErrText()).find("/f3"), std::string::npos);
  EXPECT_TRUE(executions().empty());
}

TEST_F(XrdOfsPrepGPITest, QueryFailurePropagatesError) {
  char parms[1024];
  snprintf(parms, sizeof(parms), "-admit stage,query,cancel,evict -run %s -maxfiles 8", scriptPath.c_str());
  auto prep = makePrepare(parms);
  ASSERT_NE(prep, nullptr);

  TestPrepArgs args("FAIL_QUERY", Prep_QUERY, {{"/data/f1", ""}});
  XrdOucErrInfo eInfo;
  int rc = prep->query(args.prep, eInfo);
  EXPECT_EQ(rc, SFS_ERROR);
  EXPECT_EQ(eInfo.getErrInfo(), ECANCELED);
}

TEST_F(XrdOfsPrepGPITest, QueryTruncationDetected) {
  char parms[1024];
  snprintf(parms, sizeof(parms), "-admit stage,query,cancel,evict -run %s -maxfiles 8 -maxresp 2048", scriptPath.c_str());
  auto prep = makePrepare(parms);
  ASSERT_NE(prep, nullptr);

  TestPrepArgs args("TRUNC_QUERY", Prep_QUERY, {{"/data/f1", ""}});
  XrdOucErrInfo eInfo;
  int rc = prep->query(args.prep, eInfo);
  EXPECT_EQ(rc, SFS_ERROR);
  EXPECT_EQ(eInfo.getErrInfo(), ECANCELED);
}

TEST_F(XrdOfsPrepGPITest, QueryAdmissionWaitTimeout) {
  char parms[1024];
  // Set qryMaxWT to 1 second and qryAllow to 1
  snprintf(parms, sizeof(parms), "-admit query -run %s -qrywait 1 -maxquery 1 -maxfiles 8", scriptPath.c_str());
  auto prep = makePrepare(parms);
  ASSERT_NE(prep, nullptr);

  // The child confirms it owns the single slot and cannot exit until released.
  auto blocked = startQuery("BLOCKED_QUERY");
  ASSERT_TRUE(waitForStarted());

  // Thread 2 tries to run a query with 1 second timeout while the slot is busy
  TestPrepArgs timeoutArgs("TIMEDOUT_QUERY", Prep_QUERY, {{"/data/f1", ""}});
  XrdOucErrInfo timeoutInfo;
  auto start = std::chrono::steady_clock::now();
  int rc = prep->query(timeoutArgs.prep, timeoutInfo);
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

  EXPECT_EQ(rc, SFS_ERROR);
  EXPECT_EQ(timeoutInfo.getErrInfo(), ETIMEDOUT);
  // A loaded CI worker may resume late; there is no tight upper timing bound.
  EXPECT_GE(duration.count(), 900);
  EXPECT_EQ(executions(), "BLOCKED_QUERY\n");

  releaseGate();
  EXPECT_EQ(blocked.get().rc, SFS_DATA);

  // A timed-out request must not consume or leak a query slot.
  EXPECT_EQ(prep->query(timeoutArgs.prep, timeoutInfo), SFS_DATA);
  EXPECT_EQ(executions(), "BLOCKED_QUERY\nTIMEDOUT_QUERY\n");
}

TEST_F(XrdOfsPrepGPITest, WaitingQueryRunsWhenSlotIsReleased) {
  const auto parms = "-admit query -maxquery 1 -qrywait 10 -run " + scriptPath;
  ASSERT_NE(makePrepare(parms.c_str()), nullptr);

  auto blocked = startQuery("BLOCKED_QUERY");
  ASSERT_TRUE(waitForStarted());
  auto waiting = startQuery("WAITING_QUERY");
  ASSERT_TRUE(waitForQueryWaiter());
  EXPECT_EQ(executions(), "BLOCKED_QUERY\n");

  releaseGate();
  EXPECT_EQ(blocked.get().rc, SFS_DATA);
  const auto result = waiting.get();
  EXPECT_EQ(result.rc, SFS_DATA);
  EXPECT_NE(result.output.find("QUERY_SUCCESS reqid=WAITING_QUERY"), std::string::npos);
  EXPECT_EQ(executions(), "BLOCKED_QUERY\nWAITING_QUERY\n");
}

TEST_F(XrdOfsPrepGPITest, QueryRejectsSlotAvailableAfterDeadline) {
  const auto parms = "-admit query -maxquery 1 -qrywait 1 -run " + scriptPath;
  ASSERT_NE(makePrepare(parms.c_str()), nullptr);

  using namespace XrdOfsPrepGPIReal;
  {
    XrdSysCondVarHelper lock(qryCond);
    qryAllow = 0;
  }
  auto waiting = startQuery("EXPIRED_QUERY");
  {
    XrdSysCondVarHelper lock;
    ASSERT_TRUE(lockQueryWaiter(lock));
    ASSERT_EQ(qryWait, 1);
    // Model a slot release whose mutex is not returned until after the waiter's
    // deadline. Unlike a timing race between two children, this guarantees that
    // WaitMS returns with a free slot and an expired deadline simultaneously.
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    qryAllow = 1;
    qryCond.Signal();
  }
  const auto result = waiting.get();
  EXPECT_EQ(result.rc, SFS_ERROR);
  EXPECT_EQ(result.error, ETIMEDOUT);
  EXPECT_TRUE(executions().empty());
  {
    XrdSysCondVarHelper lock(qryCond);
    EXPECT_EQ(qryAllow, 1);
    EXPECT_EQ(qryWait, 0);
  }
}

TEST_F(XrdOfsPrepGPITest, SpuriousWakeupsDoNotAdmitQueryOrResetDeadline) {
  const auto parms = "-admit query -maxquery 1 -qrywait 1 -run " + scriptPath;
  ASSERT_NE(makePrepare(parms.c_str()), nullptr);

  auto blocked = startQuery("BLOCKED_QUERY");
  ASSERT_TRUE(waitForStarted());
  auto waiting = startQuery("WAITING_QUERY");
  ASSERT_TRUE(waitForQueryWaiter());
  const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (waiting.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready &&
         std::chrono::steady_clock::now() < limit) {
    XrdSysCondVarHelper lock(XrdOfsPrepGPIReal::qryCond);
    XrdOfsPrepGPIReal::qryCond.Broadcast();
  }
  // Keep signalling throughout the deadline; an implementation that starts a
  // fresh timeout after every wakeup will still be waiting here.
  EXPECT_EQ(waiting.wait_for(std::chrono::seconds(0)), std::future_status::ready);
  EXPECT_EQ(executions(), "BLOCKED_QUERY\n");
  releaseGate();
  const auto result = waiting.get();
  EXPECT_EQ(result.rc, SFS_ERROR);
  EXPECT_EQ(result.error, ETIMEDOUT);
  EXPECT_EQ(blocked.get().rc, SFS_DATA);
}

TEST_F(XrdOfsPrepGPITest, XeqAppendsQueuedRequestsWithoutCorruptingList) {
  char parms[1024];
  // Do not admit cancel to exercise queue lookup and cancellation in reqFind
  snprintf(parms, sizeof(parms), "-admit stage -run %s -maxreq 1 -maxfiles 8", scriptPath.c_str());
  auto prep = makePrepare(parms);
  ASSERT_NE(prep, nullptr);

  TestPrepArgs args1("BLOCKED_STAGE", Prep_STAGE, {{"/data/f1", ""}});
  TestPrepArgs args2("REQ2", Prep_STAGE, {{"/data/f2", ""}});
  TestPrepArgs args3("REQ3", Prep_STAGE, {{"/data/f3", ""}});
  TestPrepArgs args4("REQ4", Prep_STAGE, {{"/data/f4", ""}});
  XrdOucErrInfo eInfo;

  ASSERT_EQ(prep->begin(args1.prep, eInfo), SFS_OK);
  ASSERT_TRUE(waitForStarted());
  EXPECT_EQ(prep->begin(args2.prep, eInfo), SFS_OK);
  EXPECT_EQ(prep->begin(args3.prep, eInfo), SFS_OK);
  EXPECT_EQ(prep->begin(args4.prep, eInfo), SFS_OK);

  // Query the entire queue before altering it: all appends must be reachable.
  for (auto *args : {&args2, &args3, &args4}) {
    XrdOucErrInfo info;
    EXPECT_EQ(prep->query(args->prep, info), SFS_DATA);
    EXPECT_EQ(std::string(info.getErrText()), "Request " + args->id + " queued.");
  }

  // Remove the middle and then the head, leaving REQ4 runnable.
  XrdOucErrInfo cancelInfo3;
  EXPECT_EQ(prep->cancel(args3.prep, cancelInfo3), SFS_DATA);
  EXPECT_STREQ(cancelInfo3.getErrText(), "Request REQ3 cancelled.");
  XrdOucErrInfo cancelInfo2;
  EXPECT_EQ(prep->cancel(args2.prep, cancelInfo2), SFS_DATA);
  EXPECT_NE(strstr(cancelInfo2.getErrText(), "Request REQ2 cancelled."), nullptr);

  EXPECT_EQ(prep->cancel(args2.prep, cancelInfo2), SFS_DATA);
  EXPECT_STREQ(cancelInfo2.getErrText(), "Request REQ2 not cancellable.");

  // Cancel non-existent request
  TestPrepArgs argsMissing("MISSING", Prep_STAGE, {{"/data/missing", ""}});
  XrdOucErrInfo cancelInfoMissing;
  EXPECT_EQ(prep->cancel(argsMissing.prep, cancelInfoMissing), SFS_DATA);
  EXPECT_NE(strstr(cancelInfoMissing.getErrText(), "Request MISSING not cancellable."), nullptr);

  releaseGate();
  drainScheduler();
  EXPECT_EQ(executions(), "BLOCKED_STAGE\nREQ4\n");
}

TEST_F(XrdOfsPrepGPITest, CancelQueuedTailThenAppendAndDrainInOrder) {
  const auto parms = "-admit stage -maxreq 1 -run " + scriptPath;
  auto prep = makePrepare(parms.c_str());
  ASSERT_NE(prep, nullptr);
  TestPrepArgs running("BLOCKED_STAGE", Prep_STAGE, {{"/data/f1", ""}});
  TestPrepArgs head("HEAD", Prep_STAGE, {{"/data/f2", ""}});
  TestPrepArgs tail("TAIL", Prep_STAGE, {{"/data/f3", ""}});
  TestPrepArgs appended("APPENDED", Prep_STAGE, {{"/data/f4", ""}});
  XrdOucErrInfo info;

  ASSERT_EQ(prep->begin(running.prep, info), SFS_OK);
  ASSERT_TRUE(waitForStarted());
  EXPECT_EQ(prep->begin(head.prep, info), SFS_OK);
  EXPECT_EQ(prep->begin(tail.prep, info), SFS_OK);
  EXPECT_EQ(prep->cancel(tail.prep, info), SFS_DATA);
  EXPECT_STREQ(info.getErrText(), "Request TAIL cancelled.");
  EXPECT_EQ(prep->begin(appended.prep, info), SFS_OK);

  releaseGate();
  drainScheduler();
  EXPECT_EQ(executions(), "BLOCKED_STAGE\nHEAD\nAPPENDED\n");
}

}  // namespace

TEST_F(XrdOfsPrepGPITest, ReinitializationCannotDiscardActiveWork) {
  const auto parms = "-admit stage,query -maxfiles 2 -run " + scriptPath;
  auto *prep = makePrepare(parms.c_str());
  ASSERT_NE(prep, nullptr);
  TestPrepArgs stage("BLOCKED_STAGE", Prep_STAGE, {{"/data/f1", ""}});
  XrdOucErrInfo info;
  ASSERT_EQ(prep->begin(stage.prep, info), SFS_OK);
  ASSERT_TRUE(waitForStarted());
  const auto other = "-admit query -maxfiles 1 -run " + scriptPath;
  EXPECT_EQ(XrdOfsgetPrepare(&eLog, nullptr, other.c_str(), nullptr, nullptr, &env), nullptr);
  EXPECT_EQ(XrdOfsgetPrepare(&eLog, nullptr, "-debug -maxresp 8192 -invalid", nullptr, nullptr, &env), nullptr);
  TestPrepArgs query("REQ", Prep_QUERY, {{"/data/f1", ""}, {"/data/f2", ""}});
  EXPECT_EQ(prep->query(query.prep, info), SFS_DATA);
  releaseGate();
  drainScheduler();
  EXPECT_NE(executions().find("BLOCKED_STAGE"), std::string::npos);
}

TEST_F(XrdOfsPrepGPITest, FailedInitializationDoesNotPublishOptions) {
  EXPECT_EQ(XrdOfsgetPrepare(&eLog, nullptr, "-debug -maxresp 8192 -maxfiles 1 -invalid", nullptr, nullptr, &env), nullptr);
  const auto parms = "-admit query -run " + scriptPath;
  ASSERT_NE(makePrepare(parms.c_str()), nullptr);
  EXPECT_EQ(XrdOfsPrepGPIReal::maxFiles, 48);
  EXPECT_EQ(XrdOfsPrepGPIReal::maxResp, static_cast<int>(XrdOucEI::Max_Error_Len));
  if (!getenv("XRDDEBUG")) { EXPECT_FALSE(XrdOfsPrepGPIReal::Debug); }
}

TEST_F(XrdOfsPrepGPITest, FastDebugRequestsOwnTraceId) {
  const auto parms = "-debug -admit stage -run " + scriptPath;
  auto *prep = makePrepare(parms.c_str());
  ASSERT_NE(prep, nullptr);
  XrdOfsPrepGPIReal::BeforeTraceForTest = drainScheduler;
  for (int i = 0; i < 10; ++i) {
    TestPrepArgs args("FAST" + std::to_string(i), Prep_STAGE, {{"/data/f1", ""}});
    XrdOucErrInfo info;
    ASSERT_EQ(prep->begin(args.prep, info), SFS_OK);
  }
  drainScheduler();
}


TEST_F(XrdOfsPrepGPITest, QueryLengthIncludesExactlyOneTerminator) {
  // A file gives the child exact bytes without echo/newline transformations.
  std::ofstream(scriptPath) << "#!/bin/sh\ncat '" << scriptPath << ".response'\n";
  const auto parms = "-admit query -maxresp 2048 -run " + scriptPath;
  auto *prep = makePrepare(parms.c_str());
  ASSERT_NE(prep, nullptr);
  TestPrepArgs args("LENGTH", Prep_QUERY, {{"/data/f1", ""}});
  for (const auto &output : {std::string(), std::string("x"), std::string("x\n"),
                            std::string("a\0b", 3), std::string(2008, 'x')}) {
    SCOPED_TRACE(output.size());
    std::ofstream(scriptPath + ".response", std::ios::binary) << output;
    XrdOucErrInfo info;
    ASSERT_EQ(prep->query(args.prep, info), SFS_DATA);
    const auto expected = output.empty() ? "No information available." : output;
    ASSERT_EQ(info.getErrInfo(), static_cast<int>(expected.size() + 1));
    EXPECT_EQ(std::string(info.getErrText(), expected.size()), expected);
    EXPECT_EQ(info.getErrText()[expected.size()], '\0');
  }
  // The exact capacity succeeds, but one more byte must fail, not truncate.
  std::ofstream(scriptPath + ".response") << std::string(2009, 'x');
  XrdOucErrInfo info;
  EXPECT_EQ(prep->query(args.prep, info), SFS_ERROR);
  EXPECT_EQ(info.getErrInfo(), ECANCELED);
}

TEST_F(XrdOfsPrepGPITest, DebugEchoesCapturedAndDiscardedOutput) {
  const auto parms = "-debug -admit query -maxresp 2048 -run " + scriptPath;
  auto *prep = makePrepare(parms.c_str());
  ASSERT_NE(prep, nullptr);
  struct Capture {
    XrdSysLogger &logger;
    XrdOucTListFIFO messages;
    explicit Capture(XrdSysLogger &log) : logger(log) { logger.Capture(&messages); }
    ~Capture() { logger.Capture(nullptr); }
    std::string text() {
      std::string result;
      for (auto *m = messages.first; m; m = m->next) result += m->text;
      return result;
    }
  } capture(logger);
  TestPrepArgs raw("RAW_QUERY", Prep_QUERY, {{"/data/f1", ""}});
  XrdOucErrInfo info;
  ASSERT_EQ(prep->query(raw.prep, info), SFS_DATA);
  EXPECT_STREQ(info.getErrText(), "  leading\n\ntrailing  ");
  EXPECT_NE(capture.text().find(" +=>   leading"), std::string::npos);
  EXPECT_NE(capture.text().find(" +=> trailing  "), std::string::npos);
  TestPrepArgs truncated("TRUNC_QUERY", Prep_QUERY, {{"/data/f1", ""}});
  EXPECT_EQ(prep->query(truncated.prep, info), SFS_ERROR);
  EXPECT_NE(capture.text().find(" +=> line 0000:"), std::string::npos);
  EXPECT_NE(capture.text().find(" -=> line 0149:"), std::string::npos);
  EXPECT_NE(capture.text().find("captured 2008 bytes of output (truncated)"), std::string::npos);
}

TEST_F(XrdOfsPrepGPITest, QueryWaitOptionRejectsInvalidValues) {
  for (const auto *option : {"-qrywait", "-qrywait 0", "-qrywait 601",
                              "-qrywait -1", "-qrywait invalid", "-wait 1"}) {
    SCOPED_TRACE(option);
    const auto parms = "-admit query -run " + scriptPath + " " + option;
    EXPECT_EQ(makePrepare(parms.c_str()), nullptr);
  }
  const auto parms = "-admit query -qrywait 600 -run " + scriptPath;
  EXPECT_NE(makePrepare(parms.c_str()), nullptr);
}

namespace {
class FailedTranslation : public XrdOss {
public:
  XrdOssDF *newDir(const char *) override { return nullptr; }
  XrdOssDF *newFile(const char *) override { return nullptr; }
  int Chmod(const char *, mode_t, XrdOucEnv *) override { return -ENOTSUP; }
  int Create(const char *, const char *, mode_t, XrdOucEnv &, int) override { return -ENOTSUP; }
  int Init(XrdSysLogger *, const char *) override { return -ENOTSUP; }
  int Mkdir(const char *, mode_t, int, XrdOucEnv *) override { return -ENOTSUP; }
  int Remdir(const char *, int, XrdOucEnv *) override { return -ENOTSUP; }
  int Rename(const char *, const char *, XrdOucEnv *, XrdOucEnv *) override { return -ENOTSUP; }
  int Stat(const char *, struct stat *, int, XrdOucEnv *) override { return -ENOTSUP; }
  int Truncate(const char *, unsigned long long, XrdOucEnv *) override { return -ENOTSUP; }
  int Unlink(const char *, int, XrdOucEnv *) override { return -ENOTSUP; }
  const char *Lfn2Pfn(const char *path, char *, int, int &rc) override {
    rc = strcmp(path, "/data/bad") == 0 ? -ENOENT : 0;
    return rc ? nullptr : path;
  }
};
class TranslationDiagnostic : public XrdOfsPrepGPITest,
                              public ::testing::WithParamInterface<bool> {};
}

TEST_P(TranslationDiagnostic, NamesFailingPathWithoutDispatch) {
  FailedTranslation oss;
  const auto parms = std::string("-admit stage,query,cancel -pfn ") +
                     (GetParam() ? "-cgi " : "") + "-run " + scriptPath;
  auto *prep = makePrepare(parms.c_str(), &oss);
  ASSERT_NE(prep, nullptr);
  TestPrepArgs args("BAD_PATH", Prep_STAGE, {{"/data/good", ""}, {"/data/bad", ""}});
  XrdOucErrInfo info;
  EXPECT_EQ(prep->begin(args.prep, info), SFS_ERROR);
  EXPECT_NE(std::string(info.getErrText()).find("/data/bad"), std::string::npos);
  EXPECT_EQ(prep->query(args.prep, info), SFS_ERROR);
  EXPECT_NE(std::string(info.getErrText()).find("/data/bad"), std::string::npos);
  EXPECT_EQ(prep->cancel(args.prep, info), SFS_ERROR);
  EXPECT_NE(std::string(info.getErrText()).find("/data/bad"), std::string::npos);
  EXPECT_TRUE(executions().empty());
}
INSTANTIATE_TEST_SUITE_P(WithAndWithoutCgi, TranslationDiagnostic, ::testing::Bool());

TEST_F(XrdOfsPrepGPITest, EmptyStageIsRejectedWithoutDispatch) {
  const auto parms = "-admit stage -run " + scriptPath;
  auto *prep = makePrepare(parms.c_str());
  ASSERT_NE(prep, nullptr);
  TestPrepArgs args("EMPTY", Prep_STAGE, {});
  XrdOucErrInfo info;
  EXPECT_EQ(prep->begin(args.prep, info), SFS_ERROR);
  EXPECT_EQ(info.getErrInfo(), EINVAL);
  EXPECT_TRUE(executions().empty());
}
