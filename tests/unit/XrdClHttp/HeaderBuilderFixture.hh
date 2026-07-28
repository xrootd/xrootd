#ifndef XRDCLHTTP_HEADERBUILDERFIXTURE_HH
#define XRDCLHTTP_HEADERBUILDERFIXTURE_HH

#include "XrdClHttp/XrdClHttpHeaderBuilder.hh"

#include <gtest/gtest.h>

// Each test states the headers it asks for the way xrdcp does for its --header
// option: a newline separated specification, handed to the builder verbatim.
class HeaderBuilderFixture : public testing::Test {
protected:
    XrdClHttp::HeaderBuilder::HeaderList headers;
};

#endif
