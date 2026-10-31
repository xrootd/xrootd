#include "XrdPfc/XrdPfcPathParseTools.hh"

#include <gtest/gtest.h>

class PathParseToolTest : public ::testing::Test {
protected:
    std::vector<std::string> dirs { "vultures", "nest", "quite", "high", "in", "a",
                                    "directory", "tree" };
    std::string file { "an_egg.gzip" };
    std::string path { "" };

    const int n_dirs = dirs.size();

    void clear_path() { path = ""; }
    std::string get_lfn() { return path + "/" + file; }
};

using namespace XrdPfc;

TEST_F(PathParseToolTest, SplitParser)
{
    for (int i = 0; i < n_dirs; ++i)
    {
        path += "/" + dirs[i];
        SplitParser sp(get_lfn(), "/");
        int n_tokens = i + 2;
        EXPECT_EQ(sp.pre_count_n_tokens(), n_tokens);
        for (int it = 0; it < i + 1; ++it)
        {
            EXPECT_EQ(sp.get_token_as_string(), dirs[it]);
        }
        EXPECT_EQ(std::string(sp.get_reminder()), file);
    }
}

TEST_F(PathParseToolTest, PathTokenizer)
{
    int  max_depth;
    bool parse_as_lfn;

    parse_as_lfn = true;
    {
        // Separate test for files in root directory.
        max_depth = 1024;
        PathTokenizer pt(get_lfn(), max_depth, parse_as_lfn);
        ASSERT_EQ(pt.m_n_dirs, 0);
        ASSERT_EQ(std::string(pt.m_reminder), file);
    }
    for (int i = 0; i < n_dirs; ++i)
    {
        path += "/" + dirs[i];
        {
            max_depth = 1024;
            PathTokenizer pt(get_lfn(), max_depth, parse_as_lfn);
            ASSERT_EQ(pt.m_n_dirs, i + 1);
            ASSERT_EQ(std::string(pt.m_reminder), file);
        }
        {
            max_depth = 0;
            PathTokenizer pt(get_lfn(), max_depth, parse_as_lfn);
            ASSERT_EQ(pt.m_n_dirs, 0);
            ASSERT_EQ(std::string(pt.m_reminder), get_lfn());
        }
    }
    clear_path();

    parse_as_lfn = false;
    for (int i = 0; i < n_dirs; ++i)
    {
        path += "/" + dirs[i];
        {
            max_depth = 1024;
            PathTokenizer pt(get_lfn(), max_depth, parse_as_lfn);
            ASSERT_EQ(pt.m_n_dirs, i + 2);
            ASSERT_EQ(std::string(pt.m_reminder), std::string(""));
        }
        {
            max_depth = 0;
            PathTokenizer pt(get_lfn(), max_depth, parse_as_lfn);
            ASSERT_EQ(pt.m_n_dirs, 0);
            ASSERT_EQ(std::string(pt.m_reminder), get_lfn());
        }
    }
    clear_path();
}
