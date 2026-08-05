target="xroot://sdfiana028.sdf.slac.stanford.edu//xdir/TestFile"

# The first argument designates the type of test being run and should
# be either "demand" or "r/t". We set a default here.
test="${1:-unknown}"

# When the test is for demand checksums, the server should be started
# with the XrdCksTestOD.cf config file.

# When the test is for r/t checksums, the server should be started
# with the XrdCksTestRT.cf config file.
#
./TestCheckSum $test adler32 64fce8a2 XrdCksTestFile $target
./TestCheckSum $test crc32   55bd9dea XrdCksTestFile $target
./TestCheckSum $test crc32c  cf3aa257 XrdCksTestFile $target
./TestCheckSum $test md5 b7018d4b11d10edbdba4a240e71d4976 XrdCksTestFile $target
