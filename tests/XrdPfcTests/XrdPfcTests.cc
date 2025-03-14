
#include "XrdPfc/XrdPfcPathParseTools.hh"

#include <gtest/gtest.h>

using namespace XrdPfc;

TEST(XrdPfcTests, PathTokenizer)
{
    PathTokenizer("/pelican/hello_world.txt", 0, true);
    PathTokenizer("/pelican/hello_world.txt", -1, true);
    PathTokenizer("/pelican/hello_world.txt", 1, true);
    PathTokenizer("/pelican/hello_world.txt", 2, true);

    PathTokenizer("/pelican/hello_world.txt", 0, false);
    PathTokenizer("/pelican/hello_world.txt", -1, false);
    PathTokenizer("/pelican/hello_world.txt", 1, false);
    PathTokenizer("/pelican/hello_world.txt", 2, false);
}
