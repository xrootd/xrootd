#include <gtest/gtest.h>

#include <climits>
#include <cstring>
#include <string>

extern unsigned long XrdOucHashVal(const char *KeyVal);
extern unsigned long XrdOucHashVal2(const char *KeyVal, int KeyLen);

namespace {

// The 29-byte key uses both the remainder and the full-word paths of the hash.
const std::string key = "MaxBuffSize/with/a/longer/key";

} // namespace

TEST(XrdOucHashVal, MatchesKnownValue)
{
  // Values of the original implementation on little-endian hosts, which
  // must not change. Big-endian hosts must give the same values.
#if ULONG_MAX > 0xffffffffUL
  const unsigned long expected = 0x3240474176261431UL;
#else
  const unsigned long expected = 0x62424d64UL;
#endif
  EXPECT_EQ(XrdOucHashVal(key.c_str()), expected);
}

TEST(XrdOucHashVal, MisalignedKeyHashesLikeAlignedKey)
{
  // XrdOucEnv hashes names that sit at arbitrary offsets inside a larger
  // buffer, which UBSan flagged as a misaligned unsigned long load. Most
  // offsets leave the word loop misaligned, whatever the key length.
  for (size_t offset = 1; offset < sizeof(unsigned long); offset++) {
    alignas(unsigned long) char buffer[64] = {};
    std::memcpy(buffer + offset, key.data(), key.size());
    EXPECT_EQ(XrdOucHashVal2(buffer + offset, key.size()),
              XrdOucHashVal(key.c_str())) << "offset " << offset;
  }
}

TEST(XrdOucHashVal, ShortKeyIsItsOwnLittleEndianBytes)
{
  EXPECT_EQ(XrdOucHashVal("abc"), 0x636261UL);
}
