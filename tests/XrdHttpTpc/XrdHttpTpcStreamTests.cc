#undef NDEBUG

#include "XrdHttpTpc/XrdHttpTpcStream.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"

#include <algorithm>
#include <cstring>
#include <gtest/gtest.h>
#include <memory>
#include <utility>
#include <vector>
#include <unistd.h>

using namespace testing;

class XrdHttpTpcStreamTests : public Test {};

// Minimal XrdSfsFile implementation used to observe which writes the Stream
// reordering layer sends to the backing filesystem.
class MemorySfsFile : public XrdSfsFile {
public:
  MemorySfsFile() : XrdSfsFile("test", 0) {}

  int open(const char *, XrdSfsFileOpenMode, mode_t,
           const XrdSecEntity * = 0, const char * = 0) override {
    return SFS_OK;
  }

  int close() override {
    return SFS_OK;
  }

  int fctl(const int, const char *, XrdOucErrInfo &) override {
    return SFS_OK;
  }

  const char *FName() override {
    return "memory";
  }

  int getMmap(void **addr, off_t &size) override {
    *addr = nullptr;
    size = 0;
    return SFS_ERROR;
  }

  XrdSfsXferSize read(XrdSfsFileOffset, XrdSfsXferSize) override {
    return 0;
  }

  XrdSfsXferSize read(XrdSfsFileOffset offset, char *buffer,
                      XrdSfsXferSize size) override {
    if (offset < 0 || static_cast<size_t>(offset) > m_data.size()) {
      return SFS_ERROR;
    }
    size_t available = m_data.size() - static_cast<size_t>(offset);
    size_t to_copy = std::min(static_cast<size_t>(size), available);
    if (!to_copy) {
      return 0;
    }
    memcpy(buffer, &m_data[static_cast<size_t>(offset)], to_copy);
    return static_cast<XrdSfsXferSize>(to_copy);
  }

  int read(XrdSfsAio *) override {
    return SFS_ERROR;
  }

  XrdSfsXferSize write(XrdSfsFileOffset offset, const char *buffer,
                       XrdSfsXferSize size) override {
    if (offset < 0) {
      return SFS_ERROR;
    }
    if (!size) {
      return 0;
    }
    size_t begin = static_cast<size_t>(offset);
    size_t end = begin + static_cast<size_t>(size);
    if (m_data.size() < end) {
      m_data.resize(end);
    }
    memcpy(&m_data[begin], buffer, static_cast<size_t>(size));
    m_writes.emplace_back(offset, size);
    return size;
  }

  int write(XrdSfsAio *) override {
    return SFS_ERROR;
  }

  int stat(struct stat *buf) override {
    memset(buf, 0, sizeof(*buf));
    buf->st_size = m_data.size();
    return SFS_OK;
  }

  int sync() override {
    return SFS_OK;
  }

  int sync(XrdSfsAio *) override {
    return SFS_OK;
  }

  int truncate(XrdSfsFileOffset size) override {
    if (size < 0) {
      return SFS_ERROR;
    }
    m_data.resize(static_cast<size_t>(size));
    return SFS_OK;
  }

  int getCXinfo(char cxtype[4], int &cxrsz) override {
    memset(cxtype, 0, 4);
    cxrsz = 0;
    return SFS_OK;
  }

  const std::vector<char> &Data() const {
    return m_data;
  }

  const std::vector<std::pair<XrdSfsFileOffset, XrdSfsXferSize>> &Writes() const {
    return m_writes;
  }

private:
  std::vector<char> m_data;
  std::vector<std::pair<XrdSfsFileOffset, XrdSfsXferSize>> m_writes;
};

TEST_F(XrdHttpTpcStreamTests, FlushesExactlyFullEmptyBuffer) {
  XrdSysLogger logger(STDERR_FILENO, 0);
  XrdSysError log(&logger, "StreamTest");
  auto file = std::make_unique<MemorySfsFile>();
  auto raw_file = file.get();
  TPC::Stream stream(std::move(file), 1, 8, log);

  // Reproduce the case where a single callback exactly fills an empty reorder
  // buffer.  The buffer is contiguous with the stream offset, so it must be
  // written immediately and returned to the available-buffer pool.
  ASSERT_EQ(8, stream.Write(0, "abcdefgh", 8, false));
  ASSERT_EQ(1u, stream.AvailableBuffers());
  ASSERT_EQ(1u, raw_file->Writes().size());
  EXPECT_EQ(0, raw_file->Writes()[0].first);
  EXPECT_EQ(8, raw_file->Writes()[0].second);
  EXPECT_EQ(std::vector<char>({'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'}),
            raw_file->Data());
}

TEST_F(XrdHttpTpcStreamTests, FlushesExactlyFullCurrentBuffer) {
  XrdSysLogger logger(STDERR_FILENO, 0);
  XrdSysError log(&logger, "StreamTest");
  auto file = std::make_unique<MemorySfsFile>();
  auto raw_file = file.get();
  TPC::Stream stream(std::move(file), 1, 8, log);

  // Reproduce the multi-stream pull stall from xrootd/xrootd#2108 in miniature:
  // a first callback partially fills the only reorder buffer, then a later
  // callback exactly completes it.  If exact fills are not flushed immediately,
  // the transfer has no active curl handles and no available buffers, so no new
  // range requests can be started.
  ASSERT_EQ(4, stream.Write(0, "abcd", 4, false));
  ASSERT_EQ(0u, stream.AvailableBuffers());
  ASSERT_TRUE(raw_file->Writes().empty());

  // Completing the buffer should trigger a backing write and make the buffer
  // available again without requiring another callback to enter Stream::Write().
  ASSERT_EQ(4, stream.Write(4, "efgh", 4, false));
  ASSERT_EQ(1u, stream.AvailableBuffers());
  ASSERT_EQ(1u, raw_file->Writes().size());
  EXPECT_EQ(0, raw_file->Writes()[0].first);
  EXPECT_EQ(8, raw_file->Writes()[0].second);
  EXPECT_EQ(std::vector<char>({'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'}),
            raw_file->Data());
}
