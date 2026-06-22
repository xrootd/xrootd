#undef NDEBUG

#include "XrdMacaroons/XrdMacaroonsUtils.hh"

#include <string>

#include <gtest/gtest.h>

using namespace Macaroons;

// Path normalization only collapses runs of consecutive slashes.  It does not
// resolve "." or ".." components, nor does it strip trailing slashes, so paths
// that differ only by those remain distinct.
TEST(XrdMacaroons, NormalizeSlashes)
{
    // Already-normal paths are returned unchanged.
    EXPECT_EQ(NormalizeSlashes("/foo/bar"), "/foo/bar");
    EXPECT_EQ(NormalizeSlashes("/"), "/");
    EXPECT_EQ(NormalizeSlashes(""), "");

    // Runs of slashes are collapsed to a single slash.
    EXPECT_EQ(NormalizeSlashes("//foo////bar"), "/foo/bar");
    EXPECT_EQ(NormalizeSlashes("/foo//bar"), "/foo/bar");
    EXPECT_EQ(NormalizeSlashes("///"), "/");

    // Trailing slashes are significant and must be preserved.
    EXPECT_EQ(NormalizeSlashes("/foo/bar/"), "/foo/bar/");

    // "." and ".." are not treated as a hierarchy and survive normalization.
    EXPECT_EQ(NormalizeSlashes("/foo/baz//../bar"), "/foo/baz/../bar");
    EXPECT_EQ(NormalizeSlashes("/foo/./bar"), "/foo/./bar");
}

// ISO 8601 duration parsing for the macaroon 'validity' field.
TEST(XrdMacaroons, DetermineValidityValid)
{
    EXPECT_EQ(determine_validity("PT1S"), 1);
    EXPECT_EQ(determine_validity("PT30S"), 30);
    EXPECT_EQ(determine_validity("PT1M"), 60);
    EXPECT_EQ(determine_validity("PT1H"), 3600);
    EXPECT_EQ(determine_validity("PT24H"), 24 * 3600);

    // Multiple components accumulate.
    EXPECT_EQ(determine_validity("PT1H30M"), 3600 + 30 * 60);
    EXPECT_EQ(determine_validity("PT1H1M1S"), 3600 + 60 + 1);

    // "PT" with no components is a (degenerate) zero-second duration.
    EXPECT_EQ(determine_validity("PT"), 0);
}

TEST(XrdMacaroons, DetermineValidityInvalid)
{
    // Missing the mandatory "PT" prefix.
    EXPECT_EQ(determine_validity(""), -1);
    EXPECT_EQ(determine_validity("1H"), -1);
    EXPECT_EQ(determine_validity("P1H"), -1);

    // A number without a unit suffix.
    EXPECT_EQ(determine_validity("PT1"), -1);

    // An unsupported unit (only S, M, H are allowed; not days/weeks).
    EXPECT_EQ(determine_validity("PT1D"), -1);
    EXPECT_EQ(determine_validity("PT1W"), -1);

    // A unit with no preceding number.
    EXPECT_EQ(determine_validity("PTS"), -1);
    EXPECT_EQ(determine_validity("PTfoo"), -1);
}
