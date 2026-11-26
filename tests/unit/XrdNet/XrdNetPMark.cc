//------------------------------------------------------------------------------
// Unit tests for XrdNetPMark::getEA and Handle::Valid.
//
// getEA splits scitag.flow into a 9-bit experiment and a 6-bit activity.
// The name is the CGI key, and the last one wins. Activity 0 with an
// experiment in range is a partial mark. A present value that is not one
// integer in 65..65535 is reported as 0,0. A missing key is not a tag.
//------------------------------------------------------------------------------

#include "XrdNet/XrdNetPMark.hh"

#include <gtest/gtest.h>

namespace {

void ExpectFlow(const char *cgi, int experiment, int activity)
{
  int experimentId = -1, activityId = -1;

  if (!XrdNetPMark::getEA(cgi, experimentId, activityId)) {
    ADD_FAILURE() << cgi << ": getEA returned false";
    return;
  }
  EXPECT_EQ(experimentId, experiment) << cgi;
  EXPECT_EQ(activityId, activity) << cgi;
  EXPECT_TRUE(XrdNetPMark::Handle(nullptr, experimentId, activityId).Valid()) << cgi;
}

void ExpectZero(const char *cgi)
{
  int experimentId = -1, activityId = -1;

  if (!XrdNetPMark::getEA(cgi, experimentId, activityId)) {
    ADD_FAILURE() << cgi << ": getEA returned false";
    return;
  }
  EXPECT_EQ(experimentId, 0) << cgi;
  EXPECT_EQ(activityId, 0) << cgi;
  EXPECT_TRUE(XrdNetPMark::Handle(nullptr, experimentId, activityId).Valid()) << cgi;
}

} // namespace

// 144 is 2 << 6 | 16. The query form is what an open CGI carries.
TEST(PMarkTest, Flow144IsExperiment2Activity16)
{
  ExpectFlow("scitag.flow=144", 2, 16);
  ExpectFlow("scitag.flow=144&pmark.appname=http-get", 2, 16);
}

// 1024 is 16 << 6 | 0. Activity 0 must stay valid so the partial mark is emitted.
TEST(PMarkTest, Flow1024IsPartialAndValid)
{
  ExpectFlow("scitag.flow=1024", 16, 0);
}

// 65535 is 1023 << 6 | 63, the top of the 16-bit range.
TEST(PMarkTest, Flow65535IsExperiment1023Activity63)
{
  ExpectFlow("scitag.flow=65535", 1023, 63);
}

// A present value outside 65..65535 is the 0,0 mark, which Valid accepts.
TEST(PMarkTest, IllegalFlowIsTheZeroMark)
{
  ExpectZero("scitag.flow=64");
  ExpectZero("scitag.flow=0");
  ExpectZero("scitag.flow=65536");
}

TEST(PMarkTest, MissingFlowIsNotATag)
{
  int experimentId = -1, activityId = -1;

  EXPECT_FALSE(XrdNetPMark::getEA(nullptr, experimentId, activityId));
  EXPECT_FALSE(XrdNetPMark::getEA("", experimentId, activityId));
  EXPECT_FALSE(XrdNetPMark::getEA("xrd.close=1", experimentId, activityId));
}

// note=scitag.flow=1024 is the key "note". The last real scitag.flow wins,
// and an optional leading + is still that integer.
TEST(PMarkTest, TheKeyIsTheNameAndTheLastOneWins)
{
  ExpectFlow("note=scitag.flow=1024&scitag.flow=144", 2, 16);
  ExpectFlow("scitag.flow=144&note=scitag.flow=1024", 2, 16);
  ExpectFlow("scitag.flow=1024&scitag.flow=144", 2, 16);
  ExpectFlow("scitag.flow=+144", 2, 16);
}

// A present value has to be the whole token. 144abc is not 144.
TEST(PMarkTest, APartialValueIsTheZeroMark)
{
  ExpectZero("scitag.flow=144abc");
  ExpectZero("scitag.flow=abc");
  ExpectZero("scitag.flow=144&scitag.flow=abc");
}

TEST(PMarkTest, AKeyThatOnlyContainsTheNameIsNotATag)
{
  int experimentId = -1, activityId = -1;

  EXPECT_FALSE(XrdNetPMark::getEA("xscitag.flow=144", experimentId, activityId));
  EXPECT_EQ(experimentId, 0);
  EXPECT_EQ(activityId, 0);
}

// Activity 0 is valid across the experiment field, and only 0,0 has no
// experiment. Experiment 1024 is past that 9-bit field.
TEST(PMarkTest, PartialMarkStaysInsideTheExperimentField)
{
  EXPECT_TRUE(XrdNetPMark::Handle(nullptr, 1, 0).Valid());
  EXPECT_TRUE(XrdNetPMark::Handle(nullptr, 1023, 0).Valid());
  EXPECT_FALSE(XrdNetPMark::Handle(nullptr, 0, 1).Valid());
  EXPECT_FALSE(XrdNetPMark::Handle(nullptr, 1024, 0).Valid());
}
