#undef NDEBUG

#include "XrdOuc/XrdOucUtils.hh"
#include "XrdOuc/XrdOucTUtils.hh"
#include "XrdOuc/XrdOucPrivateUtils.hh"

#include <map>
#include <string>

#include <gtest/gtest.h>

class XrdOucUtilsTests : public ::testing::Test {};

/* String we replace token values with */
static const std::string redacted = "REDACTED";

/*
 * These checks are meant to ensure tokens as described in sections below are
 * not leaked into the output of either the server or the client in log lines.
 *
 * Access Token Types:   https://datatracker.ietf.org/doc/html/rfc6749#section-7.1
 * Authorization Header: https://datatracker.ietf.org/doc/html/rfc6750#section-2.1
 *
 */

static const std::string token_prefixes[] = {
  "",
  "Bearer",
  "Bearer ",
  "Bearer%20",
  "Bearertoken",
  "Bearertoken ",
  "Bearer token",
  "Bearer token ",
  "Bearer%20token",
  "Bearer%20token%20",
};

static const std::string tokens[] = {
  /* short tokens */
  "my_secret_token",
  "my.secret.token/with~special+chars==",

  /* macaroon */
  "MDAxY2xvY2F0aW9uIE9wdGlvbmFsLmVtcHR5CjAwMThpZGVudGlmaWVyIGh"
  "sQ0kremlRCjAwMTVjaWQgaWlkOnBGTTA1MnJTCjAwMjFjaWQgaWQ6MjAwMj"
  "sxMDAxLDIwMDIsMDtwzXVsCjAwMjhjaWQgYmVmb3JlOjIwMTktMDQtMTdUM"
  "Dk6NTE6MjIuODQwWgowMDE5Y2lkIGhvbWU6L1VzZXJzL3BhdWwKMDAyZnNp"
  "Z25hdHVyZSCT6Lea6oBIEpiF2KOsZ1FQvLeoXve_a3q38TZTBWhM1Qo",

  /* eos token */
  "zteos64:MDAwMDAyMGN4nON6z8jFXFReIbBj16edbBqMRmL6qfnF-snJiWlplfoZJSUF8SWpxSX6F"
  "oxeKqlmhiapqWZGuonGpkm6hoapaboW5iaWummJhmbGqaamKZaJqUHzGaPdijIV_PLLFAxNgcjK0M"
  "TK0EzByMDIJFahNDPFyiC6KD-_JFYhHYldkpmSmldiBeLoGRuYmBobW1iZGJs65OQnJ-Zk5BeXKOQ"
  "l5qaC5RVS8qwUCoryS6xK8zIrFBILCqwUQCqsEGpT8nMTM_MgAhC2QnpqvpVCcWlKvpWhkIKUmL3V"
  "4wPOwTNXqXcybNGTYjp9eOrjQr",

  /* encoded eos token */
  "zteos64%3AMDAwMDAyMGN4nON6z8jFXFReIbBj16edbBqMRmL6qfnF-snJiWlplfoZJSUF8SWpxSX6F"
  "oxeKqlmhiapqWZGuonGpkm6hoapaboW5iaWummJhmbGqaamKZaJqUHzGaPdijIV_PLLFAxNgcjK0M"
  "TK0EzByMDIJFahNDPFyiC6KD-_JFYhHYldkpmSmldiBeLoGRuYmBobW1iZGJs65OQnJ-Zk5BeXKOQ"
  "l5qaC5RVS8qwUCoryS6xK8zIrFBILCqwUQCqsEGpT8nMTM_MgAhC2QnpqvpVCcWlKvpWhkIKUmL3V"
  "4wPOwTNXqXcybNGTYjp9eOrjQr",

  /* demo scitoken */
  "eyJhbGciOiJSUzI1NiIsImtpZCI6ImtleS1yczI1NiIsInR5cCI6IkpXVCJ9.eyJ2ZXIiOiJzY2l0"
  "b2tlbjoyLjAiLCJhdWQiOiJodHRwczovL2RlbW8uc2NpdG9rZW5zLm9yZyIsImlzcyI6Imh0dHBzO"
  "i8vZGVtby5zY2l0b2tlbnMub3JnIiwiZXhwIjoxNzMwODk3NTU1LCJpYXQiOjE3MzA4OTY5NTUsIm"
  "5iZiI6MTczMDg5Njk1NSwianRpIjoiYzUxOTM0OWEtMzRlMi00MTg2LTljMTMtNDU3Njk1ZjQwNTk"
  "3In0.q0aLuqK8BpI7FqPw7VJYym2B3SLyYiU_7xH_y_dD-jmdOUuH8pySgvsCzlrKcqgVY6-E8ggq"
  "fM09HqAMJCe5MRiOpZj34D8zSU3kgTC8bh9fjy6sYgTwnmzkCGXO5xdf_H7Xw1VO2eVOPJUHtsmc7"
  "pa6_geLmHiJvSthKgd9XceRyQ5R8q9T5E03LsAmks4rhTC1dJaCGB2EUguKxXhos2dBk09MhPQOB7"
  "jvQKFPXu9tdJb7eWNMPETxnWTJF7kn5zKIs1by2bcHtpdEpOIQ3qfGZhThzUeZ9NZC0FXsyKhoKoJ"
  "EAkevGtNbs72NqZr3scxVUj_zHK6QIWe2UI7dKg",

  /* malicious "tokens" */
  std::string(65536, 'X'),
  std::string(65536, '.'),
};

static const std::string plain_urls[] = {
  /* empty URL */
  "",

  /* some realistic URLs */
  "root://eos.cern.ch//eos/",
  "https://my.cdash.org/index.php?project=XRootD",
  "https://gcc.gnu.org/bugzilla/show_bug.cgi?id=86164",
  "https://zoom.us/j/6215903872?pwd=MzErQXJNXXXXX.RsS2VLSVgzVmtrdz09&omn=62242940660#success",
  "https://p0@F4HP7QL65F.local:61631//first/namespace/token_gen/test1171569942?&timeout=9.5s",
  "root://localhost:10940//data/cb4aacf1-6f28-42f2-b68a-90a73460f424.dat?xrdcl.requuid=26dab270-6d8a-43b1-8ebc-befb372c0d60",

  /* malicious "URLs" */
  std::string(65536, 'A'),
  std::string(65536, '.'),
  std::string(65536, '&'),
  std::string(65536, ' '),
};

static const std::string authz_strings[] = {
  "authz=REDACTED",
  " authz=REDACTED ",
  " 'authz=REDACTED' ",
  " \"authz=REDACTED\" ",
  "authz=REDACTED&scitag.flow=144&test=abcd",
  "scitag.flow=144&authz=REDACTED&test=abcd",
  "scitag.flow=144&test=abcd&authz=REDACTED",
  "authz=REDACTED&test=test2&authz=REDACTED",
  "authz=REDACTED&test=test2&authz=REDACTED&authz=REDACTED&test=test2&authz=REDACTED",
  "/path/test.txt?scitag.flow=44&authz=REDACTED done close.",
  "/path/test.txt?authz=REDACTED&scitag.flow=44 done close.",
  "(message: kXR_stat (path: /tmp/xrootd/public/foo?authz=REDACTED&pelican.timeout=3s, flags: none) ).",
  "(message: kXR_stat (path: /tmp/xrootd/public/foo?pelican.timeout=3s&authz=REDACTED, flags: none) ).",
  "Appended header field to opaque info: 'authz=REDACTED'",
  "Processing source entry: /etc/passwd, target file: root://localhost:1094//tmp/passwd?authz=REDACTED",
  "240919 08:11:07 20995 unknown.3:33@[::1] Pss_Stat: url=pelican://p0@F4HP7QL65F.local:" /* no comma! */
  "61631//first/namespace/token_gen/test1171569942?&authz=REDACTED&pelican.timeout=9.5s"
};

static const std::string authz_headers[] = {
  "authorization:REDACTED",
  "authorization :REDACTED",
  "authorization : REDACTED",
  "Authorization:REDACTED",
  "Authorization: REDACTED",
  "Authorization :REDACTED",
  "Authorization : REDACTED",
  "transferHeaderauthorization: REDACTED",
  "transferHeaderauthorization :REDACTED",
  "transferHeaderauthorization : REDACTED",
  "TransferHeaderAuthorization: REDACTED",
  "TransferHeaderAuthorization :REDACTED",
  "TransferHeaderAuthorization : REDACTED",
  "WWW-Authenticate: REDACTED",
  "Proxy-Authenticate: REDACTED",
  "WWW-Authenticate : REDACTED",
  "Proxy-Authenticate : REDACTED",
};

/* Check that plain URLs not containing a token remain intact */

TEST(XrdOucUtilsTests, RedactToken_PlainURLs)
{
  for (std::string str : plain_urls)
    ASSERT_EQ(str, obfuscateAuth(str));
}

/* Check URLs with an empty token as a special case. This is needed
 * because in the case an empty token is provided with prefix "Bearer ",
 * that is, containing a space at the end, a word from the actual output
 * will be redacted as if it were the value of the token.
 *
 * Example:
 *   "/test.txt?scitag.flow=44&authz=Bearer done close."
 * Becomes:
 *   "/test.txt?scitag.flow=44&authz=REDACTED close."
 *
 * In this special case, whenever the prefix ends with a space, we check
 * only that the word "REDACTED" appears in the output.
 */

TEST(XrdOucUtilsTests, RedactToken_AuthzCGI_EmptyToken)
{
  for (std::string authz : authz_strings) {
    for (std::string prefix : token_prefixes) {
      std::string str = authz;

      size_t pos = 0;
      while ((pos = str.find(redacted, pos)) != std::string::npos)
        str = str.replace(pos, redacted.size(), prefix);

      std::string obfuscated_str = obfuscateAuth(str);

      /* Assert that we do find the word "REDACTED" in the output */
      ASSERT_TRUE(obfuscated_str.find(redacted) != std::string::npos);

      /* Skip input/output equality check since when prefix ends with a
       * space, or input contains "REDACTED " with a space, an extra word
       * will be consumed as if it were the token value. */
    }
  }
}

TEST(XrdOucUtilsTests, RedactToken_AuthzCGI_ValidToken)
{
  size_t pos = 0;
  for (std::string authz : authz_strings) {
    for (std::string prefix : token_prefixes) {
      for (std::string token : tokens) {
        std::string str = authz;

        pos = 0;
        /* Replace all "REDACTED" strings with a token value in the test string */
        while ((pos = str.find(redacted, pos)) != std::string::npos)
          str = str.replace(pos, redacted.size(), prefix + token);

        /* Call obfuscateAuth(str) to redact token values */
        std::string obfuscated_str = obfuscateAuth(str);

        pos = 0;
        /* Replace all token values back with "REDACTED" in the test string */
        while ((pos = str.find(token, pos)) != std::string::npos)
          str = str.replace(pos, token.size(), redacted);

        /* Assert that we do not find the token value in the output */
        ASSERT_TRUE(obfuscated_str.find(token) == std::string::npos)
          << "\ntoken = '" << token << "'\n str = '" << obfuscated_str << "'" << std::endl;

        /* Assert that we do find the word "REDACTED" in the output */
        ASSERT_TRUE(obfuscated_str.find(redacted) != std::string::npos);

        /* Assert that we get back the original string after redaction */
        ASSERT_EQ(str, obfuscated_str);
      }
    }
  }
}

TEST(XrdOucUtilsTests, RedactToken_AuthHeader)
{
  size_t pos = 0;
  for (std::string header : authz_headers) {
    for (std::string prefix : token_prefixes) {
      for (std::string token : tokens) {
        std::string str = header;

        pos = 0;
        /* replace all "REDACTED" strings with a token value in the test string */
        while ((pos = str.find(redacted, pos)) != std::string::npos)
          str = str.replace(pos, redacted.size(), prefix + token);

        /* Call obfuscateAuth(str) to redact token values */
        std::string obfuscated_str = obfuscateAuth(str);

        pos = 0;
        /* Replace all token values back with "REDACTED" in the test string */
        while ((pos = str.find(token, pos)) != std::string::npos)
          str = str.replace(pos, token.size(), redacted);

        /* Assert that we do not find the token value in the output */
        ASSERT_TRUE(obfuscated_str.find(token) == std::string::npos)
          << "\ntoken = '" << token << "'\n str = '" << obfuscated_str << "'" << std::endl;

        /* Assert that we do find the word "REDACTED" in the output */
        ASSERT_TRUE(obfuscated_str.find(redacted) != std::string::npos);

        /* Assert that we get back the original string after redaction */
        ASSERT_EQ(str, obfuscated_str);
      }
    }
  }
}

TEST(XrdOucUtilsTests, caseInsensitiveFind) {
  {
    std::map<std::string, std::string> map;
    ASSERT_EQ(map.end(), XrdOucTUtils::caseInsensitiveFind(map, "test"));
  }
  {
    std::map<std::string, std::string> map { {"test","lowercase"}, {"TEST2","uppercase"},{"AnotherTest", "UpperCamelCase"}};

    ASSERT_EQ("lowercase", XrdOucTUtils::caseInsensitiveFind(map, "test")->second);
    ASSERT_EQ("uppercase", XrdOucTUtils::caseInsensitiveFind(map, "test2")->second);
    ASSERT_EQ("UpperCamelCase", XrdOucTUtils::caseInsensitiveFind(map, "anothertest")->second);
    map[""] = "empty";
    ASSERT_EQ("empty", XrdOucTUtils::caseInsensitiveFind(map, "")->second);
  }
}
