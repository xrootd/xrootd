# Writing tests for XRootD

This file describes the project conventions for adding new tests in XRootD. A
uniform layout lets developers find the tests by component, run them alone, and
add new tests in the same style as the tests which are already there. The file
[docs/TESTING.md](../docs/TESTING.md) describes how to build XRootD and run the
tests using the [test.cmake](../test.cmake) CTest script.

The conventions described here add to the official documentation of the test
frameworks themselves the bits that are specific to XRootD, to avoid repetition.
Please read the [GoogleTest primer](https://google.github.io/googletest/primer.html)
and related documentation before writing unit and integration tests. For end-to-end
tests, read the [BATS documentation](https://bats-core.readthedocs.io) and the
[bats-assert](https://github.com/bats-core/bats-assert) functions.

## Contents

- [Test categories](#test-categories)
- [Content of the tests](#content-of-the-tests)
- [Unit tests](#unit-tests)
  - [Directory](#directory)
  - [File names](#file-names)
  - [Registration in CMake](#registration-in-cmake)
  - [Executables](#executables)
  - [Names in CTest](#names-in-ctest)
  - [Suite names](#suite-names)
  - [Test names](#test-names)
- [Integration tests](#integration-tests)
- [End-to-end tests](#end-to-end-tests)
  - [Files and names](#files-and-names)
  - [Helpers](#helpers)
  - [Servers](#servers)
  - [Logs and configuration](#logs-and-configuration)
  - [Example](#example)
  - [Running one file without CTest](#running-one-file-without-ctest)
- [Portability](#portability)
- [Coverage of replaced tests](#coverage-of-replaced-tests)
- [Pitfalls](#pitfalls)
- [Review checklist](#review-checklist)

## Test categories

There are many [types of software testing]. XRootD currently uses the
following types of tests, which may be expanded in the future:

- **Unit tests** test one component in the test process. They do not start
  other processes, and they do not use the network. Use GoogleTest, and GMock
  for the mock objects. Tests for `src/<Component>` are in `tests/unit/<Component>`.
- **Integration tests** test a component together with other components, or
  with real system resources. Examples are a socket on the loopback interface,
  files on disk, or a helper process. They do not start an XRootD server.
  Use GoogleTest and GMock. Put them in `tests/integration/<Name>`.
- **End-to-end tests** start real servers and run real clients against them.
  Use BATS (Bash Automated Testing System), which is in `vendor/bats`.
  BATS tests are in `tests/end-to-end/`, with subdirectories naming what is
  being tested.

The other directories of `tests/` hold code which the tests share, and older
sets of tests which have not yet been migrated to use the latest conventions:

- `tests/common`: the library `XrdTestUtils` with helpers for GoogleTest tests.
- `tests/XrdClHttpCommon` and `tests/XrdOssTests`: a transfer test library of
  XrdClHttp, and OSS plugins which inject faults. The end-to-end tests load them.
- `tests/XrdClS3`: the S3 tests of XrdClS3, with their own MinIO server (see #2977).
- `tests/XrdSsiTests`: the program `xrdshmap`, a manual test of the shared
  memory map of XrdSsi. CTest does not run it.

[types of software testing]: https://www.atlassian.com/continuous-delivery/software-testing/types-of-software-testing

## Content of the tests

These conventions apply to all categories. A test is also documentation: a reader
must understand the expected behavior from the name, the expected result from
the body, and the cause of a failure from the output alone.

- **One behavior per test.** A test verifies one behavior, and fails for one
  clear reason. As a rule of thumb, use one assertion per test. Several
  assertions are correct when they all describe the same behavior, for
  example the fields of one result. Split the assertions when they describe
  different behaviors.
- **No logic for the expected result.** Do not use loops, conditions or
  calculations to get the expected value, and do not copy the algorithm of
  the tested code. Loops are correct in the setup, in helpers, and to wait
  for a result which comes later.
- **Fixed values.** Write the inputs and the expected values in the test, for
  example `EXPECT_EQ(sum({1, 2, 3}), 6)`, not a variable with
  `expected = 1 + 2 + 3`. Do not use random inputs when the test checks a
  result which depends on them. Random values are correct for resources,
  such as ports and temporary paths. Random contents are correct when the
  test compares them with a copy, for example a file before and after a
  transfer.
- **Isolated tests.** A test must pass when it runs alone and in any
  order, and also when all other tests are deleted. A test does not
  interfere with other tests, and does not depend on the result of other
  tests. Each test makes its own data, sets up its own environment, and
  removes the resources which it makes. A shared setup of CTest, such as a
  fixture generated at configure time to setup credentials used in tests,
  is not a test, and is correct.
- **Clear failures.** Use the assertion which shows the expected and the
  actual values: `EXPECT_EQ(result, 42)`, not `EXPECT_TRUE(result == 42)`.
  In BATS, use `run` with the expected exit status, for example `run -0`,
  and `assert_output`. Use test data which shows the reason of the test, for
  example `"[abc"` for a missing bracket. Add a message with `<<` when the
  assertion alone does not explain the failure.
- **Clear names.** Give the expected behavior in the name of the test (see
  [Test names](#test-names)). The name tells the input, the tested behavior
  and the expected result, for example `RejectsUrlWithoutHost`. Do not use
  names such as `Test1`, `Example` or `CheckUser`.
- **Time limits.** Give a time limit to a test that can block, for example a
  test of concurrent code. Use the `TIMEOUT` property of CTest, or a limit in
  the test. For BATS, see [Portability](#portability).
- **Process-global state.** Some code keeps its configuration in static
  variables, and its initialization runs only once. Put the tests of each
  configuration in a separate executable (see [Executables](#executables)).

## Unit tests

### Directory

Put the tests for the component in `src/<Component>` into `tests/unit/<Component>`.
For a component in `src/XrdApps/<Name>`, use `tests/unit/<Name>`.
Add each new directory to `tests/unit/CMakeLists.txt` with `add_subdirectory()`,
in alphabetical order.

### File names

Name test source files after the source files they test, with the prefix of the
component. For example, `tests/unit/XrdOuc/XrdOucUtils.cc` tests
`src/XrdOuc/XrdOucUtils.cc`. Do not add a `Test`, `Tests` or `_test` suffix.

- When several test files test one source file, add the tested aspect, for
  example `XrdClHttpHeaderBuilderParsing.cc`.
- A file that tests a function of several source files together should be named
  after the component or the function, for example `XrdCksCombine.cc`.
- A helper file is not a test file, so it can have another name,
  for example `HeaderBuilderFixture.hh`.

### Registration in CMake

Unit tests must be registered to be run with `ctest` or `make test`. Do not call
`add_executable()` or `gtest_discover_tests()` directly for this. Use the
function `add_unit_tests()` of `tests/unit/CMakeLists.txt` instead, to ensure
that the naming conventions for targets and CTest test names are followed.

```cmake
add_unit_tests([<qualifier>] SOURCES <file>... [LIBRARIES <library>...]
               [INCLUDE_DIRECTORIES <dir>...] [NO_MAIN]
               [PROPERTIES <name> <value>...])
```

The function gets `<Component>` from the name of the directory, and it makes
the executable `<component>[-<qualifier>]-unit-tests`, with the component in
lower case. It links GoogleTest and GMock, and also `gtest_main` unless
`NO_MAIN` is set. It registers each test in CTest with `gtest_discover_tests()`,
with the prefix `unit::<Component>::` and sets a time limit for the discovery.
`PROPERTIES` sets CTest properties on each test, for example `TIMEOUT`. A call
of `target_link_libraries()` for the executable after the function must use the
keyword `PRIVATE`. For example, from `tests/unit/XrdOuc/CMakeLists.txt`:

```cmake
add_unit_tests(
  SOURCES
    XrdOucCRCCombine.cc
    XrdOucHashVal.cc
    XrdOucUtils.cc
  LIBRARIES
    XrdUtils
)
```

If the tests need `XrdServer` or another optional library or target, use an early
`return()` statement in the `CMakeLists.txt` of the directory when the dependency
is not available. Do not let tests fail due to missing dependencies.

### Executables

Use one executable for each component, without a qualifier, for example
`xrdouc-unit-tests`. A single executable is faster to compile and link than
several executables. `gtest_discover_tests()` registers each test separately
in any case, and CTest runs each test in a new process. Thus, additional
executables do not make the tests more isolated.

An additional executable may be required in certain cases, however. In these
cases, add a qualifier as first argument to `add_unit_tests()`, which is used
to indicate the area being tested or the reason why a separate executable is
needed. Examples are:

- A dependency that is only enabled in some builds.
- A test build of the tested sources: `xrdclhttp-worker-unit-tests` and
  `xrdclhttp-filesystem-unit-tests` (they use `#define`s to change the sources).
- Process-global state: `xrdxrootd-monfile-unit-tests` and
  `xrdxrootd-monxfr-unit-tests`. The monitor keeps its configuration in
  static variables, and its initialization runs only once at process startup.
  Thus, each configuration needs its own executable.

The executables of a component have the same CTest prefix, so different test
suite names must be used in each executable to avoid name clashes between tests.

### Names in CTest

The name of a test in CTest is `unit::<Component>::<Suite>.<Test>`, for
example `unit::XrdPfc::InfoTest.RefusesAnUnknownVersion`. Thus,
`ctest -R '^unit::'` runs all unit tests, and `ctest -R '^unit::XrdCl::'`
runs the unit tests of one component.

For a parameterized test, GoogleTest adds the name of the parameter, as in
`<Suite>.<Test>/<Parameter>`. Use an empty prefix to `INSTANTIATE_TEST_SUITE_P()`,
because a prefix comes before the suite name in the name of the test:

```cpp
INSTANTIATE_TEST_SUITE_P(, WorkerOptionsSetupTest, testing::Values(...));
```

A parameterized test is correct when it verifies one behavior with several
explicit examples. Write each example and its expected value in the list of
values.

### Suite names

Use `<Unit>[<Aspect>]Test`. `<Unit>` is the tested class, function or source
file, without the prefix of the component, which the CTest prefix already
carries. Use `InfoTest` for the class `XrdPfc::Info`, not `XrdPfcInfoTest`,
`InfoTests` or `InfoFixture`.

- The suffix `Test` is the convention of GoogleTest. In `TEST_F()` and
  `TEST_P()`, the suite name is the name of the fixture class, so the suffix
  is necessary to prevent clashes with the name of the tested class.
- Add `<Aspect>` when one unit has several suites, for example
  `HeaderBuilderParsingTest` and `HeaderBuilderAppendMissingTest`.
- When the tested class has the name of the component, remove only `Xrd`,
  for example `OssMirageTest` for the class `XrdOssMirage`.
- An installed header may name a fixture as a friend, for example
  `friend class ::PlugInManagerFixture`. Do not change the header to rename
  the fixture. Keep the name of the class, and give the suite name with an
  alias: `using PlugInManagerTest = PlugInManagerFixture;`. For a new friend,
  use the suite name as the name of the class.

### Test names

Use a short sentence in CamelCase that gives the expected behavior, for example
`URLTest.RejectsMalformedUrls`. Do not use underscores, and do not add a
`Test` suffix. The GoogleTest FAQ advises against underscores in these names.

- The subject can be the unit of the suite. Thus, a name can start with the
  verb, for example `HeaderParserTest.RejectsMalformedCrc32Digest`.
- When a suite tests several functions, start the name with the function, for
  example `UtilsTest.JoinUrlAddsOneSlashBetweenParts`.

## Integration tests

- **Directory:** put the tests in `tests/integration/<Name>`. `<Name>` is the
  component under test, or the function under test when the test covers more
  than one component. Add each new directory to
  `tests/integration/CMakeLists.txt` with `add_subdirectory()`.
- **File, suite and test names:** use the same conventions as for unit tests.
- **Executable names:** use `<name>-integration-tests`, with `<name>` in
  lower case.
- **Names in CTest:** use `TEST_PREFIX integration::<Name>::`. The name of a
  test in CTest is then `integration::<Name>::<Suite>.<Test>`.
- **Resources:** bind sockets to the loopback interface, and let the system
  choose the port (port 0). Put files in a directory that the test makes and
  removes. Do not use fixed paths in `/tmp`.

## End-to-end tests

The end-to-end tests start their servers with the helpers in
`tests/end-to-end/setup`. A test file loads a helper of `setup/`, directly or
through a helper of its own directory. That helper loads `helper/common.bash`,
which loads bats-support and bats-assert. Thus, the tests use the functions of
bats-assert without a `load` of their own.

### Files and names

- **Directory:** put a test file in `tests/end-to-end/<functionality>/<name>.bats`.
  `<functionality>` is the tested program or feature, for example `xrdcp/`,
  `cache/` or `monitoring/`. Put the tests of the command-line tools in
  `xrdcp/` or `xrdfs/`. Put only the tests of API programs in `xrdcl/`.
- **Registration in CMake:** register the file with `add_bats_tests(<name>)`
  in the `CMakeLists.txt` of its directory.
- **Names in CTest:** the name of the test is
  `end-to-end::<functionality>::<name>`, with `::` for each `/` in
  `<functionality>`.
- **Test names:** use a sentence in lower case that gives the expected
  behavior, for example `@test "an unknown option prints the usage"`.
- **Checks:** run `shellcheck` on new and changed test files. It finds
  common errors in shell code, such as variables without quotes. The CI
  will enforce this check later. `tests/end-to-end/.shellcheckrc` sets
  `shell=bats`, thus `shellcheck` reads the `.bats` files directly.

### Helpers

- `start_server [name] [role] [snippet.cfg...]` in `setup/server.bash` starts
  an xrootd with the configuration `base.cfg`, the role, and the snippets. The
  name and the role are `server` by default. The roles are:
  - `server`: a data server with unix authentication.
  - `server-nosec`: a data server without the security library.
  - `cache`: a caching proxy in front of the server at `$ORIGIN`.

  A snippet ends in `.cfg`. It is a file of the directory of the test, or of
  `setup/`, for example `http.cfg`, `tls.cfg`, `tpc.cfg`, `ztn.cfg`,
  `scitokens.cfg`, `krb5.cfg`, `checksum.cfg`, `checkpoint.cfg`, `faults.cfg`
  or `trace-http.cfg`. The helper exports the variables `<NAME>_URL`,
  `<NAME>_URL4`, `<NAME>_URL6`, `<NAME>_HTTP`, `<NAME>_HTTPS`, `<NAME>_PORT`,
  `<NAME>_DATA` and `<NAME>_LOG`.
- `restart_server <name>` stops a server and starts it again on the same
  port, with the same configuration and data. `server_log <name>` prints the
  lines which the server logged during the current test.
- `start_meta_manager`, `start_manager` and `start_data_server` in
  `setup/cluster.bash` start the nodes of a cluster, each an xrootd with a
  cmsd. `wait_for_cluster` waits until the cluster forms.
- `start_issuer` and `make_token <scope> [lifetime]` in `setup/server.bash`
  start a token issuer and make its tokens. `start_collector <name>` receives
  monitoring data.
- `start_kdc` in `setup/kerberos.bash` starts a Kerberos KDC (key
  distribution center).
- `stop_servers` stops all servers of the current test or file. It fails if a
  process does not stop.
- `require_commands <command>...` and `require_ipv6` in `helper/common.bash`
  skip a test when a program or IPv6 is not available. `wait_for <seconds>
  <command...>` runs a command until it succeeds, for a result which comes
  later.

All tests share one set of credentials: a CA (certificate authority), a host
certificate for `localhost` and both loopback addresses, a client certificate,
the keys of a token issuer and a macaroon secret. The CTest fixture `credentials`
makes them one time for each run, and the tests only read them.

### Servers

- Start the servers of each test in `setup()`, and stop them in `teardown()`.
  This is the default.
- Start the servers in `setup_file()` only for a setup which takes long, such
  as a cache, a cluster, or several servers. Give each test its own paths on
  the shared servers. `xrdfs/query.bats` shows this for one server.
- Each server runs on a random free port, below the temporary directory of
  BATS. Thus, files can run in parallel, and BATS removes all their files
  after the run.

A test or a helper which starts a daemon directly must obey these conventions:

- Start the daemon with the options of the systemd units:
  `-k fifo -a -w -n <name>`.
- Use a random port from 21000 to 31999. Try again, up to 5 times, when the
  daemon reports "Unable to bind socket to port". Do not match the text of
  the system error.
- Use the exit status of `xrootd -b` to know that the configuration is done.
  Do not poll the log, except for a daemon that runs in the foreground.
- Set `all.pidpath`. Without it, `xrootd` writes its PID file to
  `/tmp/<instance>/xrootd.pid`, outside the directories of BATS.

### Logs and configuration

- Each test starts with empty logs of the servers of its file. Thus, a test
  sees only its own lines.
- A test that checks the startup lines of a server must start its own server.
- Keep `base.cfg` minimal. Add the snippets `checksum.cfg`, `checkpoint.cfg`,
  `faults.cfg` and `trace-*.cfg` only to the tests that need them. Traces do
  not make the tests slower, but they make large logs. In one run of the
  suite, `http.trace` alone made two-thirds of all logs.
- Do not put a snippet with the same name in the directory of the test and
  in `setup/`. `start_server` gives an error in this case.
- `setup/server.bash` sets `XrdSecPROTOCOL=unix`. Thus, the client never
  sends the grid proxy of the user to a server, and the proxy does not change
  the results. A test of another security protocol sets `XrdSecPROTOCOL` itself.

When a test fails, BATS prints the client log of the test and the log of
each server.

### Example

A minimal test file is:

```sh
#!/usr/bin/env bash

load ../setup/server.bash

setup() {
    start_server http.cfg
}

teardown() {
    stop_servers
}

@test "a download returns the file of the server" {
    echo 'content' > "$SERVER_DATA/file"
    xrdcp "$SERVER_URL//file" "$BATS_TEST_TMPDIR/copy"
    run -0 cmp "$SERVER_DATA/file" "$BATS_TEST_TMPDIR/copy"
}
```

The file `tests/end-to-end/example/example.bats` is a longer example with
comments.

### Running one file without CTest

Give BATS the libraries and the suite setup. The suite then makes its own
credentials:

```sh
env PATH=$PWD/build/bin:$PATH BINARY_DIR=$PWD/build \
    BATS_LIB_PATH=$PWD/vendor/bats/lib/bats vendor/bats/bin/bats \
      --setup-suite-file tests/end-to-end/setup_suite.bash \
        tests/end-to-end/<functionality>/<file>.bats
```

Add `--no-tempdir-cleanup` to keep the run directories after the run. They
contain the logs and the configuration of each server. When CTest stops BATS
at a timeout, the teardown does not run. Then a reaper, which
`setup_suite.bash` starts, stops the daemons and removes their directories.
The reaper also stops a daemon which a test did not stop.

## Portability

The CI (continuous integration) runs the tests on Linux with glibc and musl,
on macOS, and in containers.

- Check XRootD error codes, such as `[3008]`, not the text of `strerror()`.
  The text may be different between musl, glibc, and macOS.
- Keep the shell code compatible with POSIX sh or bash 3.2 (version on macOS).
  Do not use associative arrays or `${x^^}`.
- For a time limit in BATS, set `BATS_TEST_TIMEOUT` at file level. Do not use
  GNU `timeout`, which macOS does not have.
- For the name of the host, use `hostname -f 2>/dev/null || uname -n`. Some
  container images, such as Fedora, have no `hostname` program, and
  `uname -n` gives no domain. Skip the test when the name does not resolve.
  The name does not resolve on all CI runners.
- Call `require_ipv6` before a test uses `[::1]`. The client uses IPv6 only
  when an interface has an IPv6 address which is not loopback and not
  link-local. Without such an address, as on the CI runners, the client
  cannot reach `[::1]`, and `require_ipv6` skips the test.
- Call `require_xattr` in server tests that make use of extended attributes.
  Linux distribution builds often use tmpfs with xattr disabled or unsupported.

## Coverage of replaced tests

When new tests replace old tests, make sure that the coverage does not drop.
Compare the covered lines, not the percentages:

1. Configure a coverage build at a short path, for example `build_cov`, with
   `CMAKE_C_FLAGS` and `CMAKE_CXX_FLAGS` set to the flags which
   `-DCOVERAGE=ON` of `test.cmake` uses (`COVERAGE_FLAGS`).
2. Delete all `*.gcda` files, and run the old tests.
3. Write a JSON report:
   `gcovr -r . --object-directory build_cov --filter src/ --json old.json`.
4. Do steps 2 and 3 again for the new tests, with `new.json`.
5. Compare the sets of (source file, line) with a count above zero, for
   example with a short Python script. Each line that only the old tests
   cover needs a new test, or a reason why it is not necessary.

Another way to check the coverage is looking at the reports in CDash or within
the build directory, when generated by `gcovr`.

## Pitfalls

- CTest ignores a fixture in `FIXTURES_REQUIRED` when no test sets it up, and
  it gives no error. Thus, when you remove a fixture, find all of its users
  first. Without the fixture, its users run without their setup and fail.
- When a test checks something that arrives asynchronously and possibly in
  several pieces, such as monitoring records or the effects of background work
  in XCache (purge, prefetch, sync), a fixed sleep before the check is flaky
  under load. In such cases, it's better to use `wait_for` on the state the
  assertion expects, for example the expected number of g-stream records.

## Review checklist

Before you submit a test, make sure that the answer to each question is yes:

1. Is the test in the correct category and directory, with the correct names?
2. Does the test verify one behavior, and do all its assertions describe that
   behavior?
3. Does the name give the expected behavior, without the body?
4. Are the inputs and the expected values written in the test, without logic
   to calculate them?
5. Does the test pass when it runs alone, in any order, and when all other
   tests are deleted?
6. Does the output of a failure show the expected and the actual values, and
   explain the defect without a debugger?
7. Does the test have a time limit if it can block, and does it obey the
   conventions for [Portability](#portability)?
8. When the test replaces old tests, does the coverage stay the same or
   increase?
