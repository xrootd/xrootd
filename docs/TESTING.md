## Configuring and Running XRootD tests with CTest

This document describes how to build XRootD and run the test suite with the
CTest script [test.cmake](../test.cmake). The file
[tests/README.md](../tests/README.md) gives the layout of the tests, and the
rules to follow when writing new tests for XRootD. The file
[docker/README.md](../docker/README.md) tells how to build and test XRootD in
containers of other distributions.

This document assumes that you know how to build XRootD from source. The file
[INSTALL.md](INSTALL.md) gives the instructions, if needed, and lists the
optional features, and the dependencies of each feature, as well as how to
install the dependencies.

### Contents

- [Enabling tests during CMake configuration](#enabling-tests-during-cmake-configuration)
- [Running all steps with `test.cmake`](#running-all-steps-with-testcmake)
  - [Running a subset of the tests](#running-a-subset-of-the-tests)
  - [Conditions for the tests which start a server](#conditions-for-the-tests-which-start-a-server)
- [Customizing the Build](#customizing-the-build)
  - [Selecting a build type, compile flags, optional features, etc](#selecting-a-build-type-compile-flags-optional-features-etc)
  - [Building with a sanitizer](#building-with-a-sanitizer)
  - [Enabling coverage, memory checking, and static analysis](#enabling-coverage-memory-checking-and-static-analysis)
- [Dependencies Required for Coverage, Memory Checking, and Static Analysis](#dependencies-required-for-coverage-memory-checking-and-static-analysis)
  - [RPM-based distributions](#rpm-based-distributions)
  - [DEB-based distributions](#deb-based-distributions)

### Enabling tests during CMake configuration

The CMake option `-DENABLE_TESTS=ON` enables the tests. The unit tests and the
integration tests use [GoogleTest](https://github.com/google/googletest) and
[GoogleMock](https://github.com/google/googletest/tree/main/googlemock) from
[vendor/googletest](../vendor/googletest). The end-to-end tests use
[BATS](https://bats-core.readthedocs.io/) (Bash Automated Testing System) from
[vendor/bats](../vendor/bats). Thus, the tests need no test framework from the
system, and we do not recommend distributions to unbundle BATS or GoogleTest,
as nothing that is installed depends on them, they are pure test dependencies.
We bundle them to guarantee minimum versions for features used in XRootD tests.

The option `ENABLE_SERVER_TESTS` (on by default) enables the tests which require
a server. CMake turns it off in a build of the client only (`XRDCL_ONLY` or
`XRDCL_LIB_ONLY`), and when the user is `root`, since `xrootd` and `cmsd`
cannot be run as the `root` user.

When the Python bindings are enabled as well, the test suite includes the
tests for the bindings, which use [pytest](https://pytest.org). The Python
interpreter of the build must import pytest, or the configuration fails.
Install pytest, or disable these tests with `-DENABLE_PYTHON=OFF` or
`-DENABLE_SERVER_TESTS=OFF`.

### Running all steps with `test.cmake`

The CTest script [test.cmake](../test.cmake) configures and builds the project,
then runs all tests with CTest. Run it from the top directory of the repository:

```sh
ctest -V -S test.cmake
```

Example:
```sh
xrootd $ ctest -V -S test.cmake
Using CMake cache file config.cmake
Run dashboard with model Experimental
   Source directory: xrootd
   Build directory: xrootd/build
   Reading ctest configuration file: xrootd/CTestConfig.cmake
   Site: example.cern.ch (Linux - x86_64)
   Build name: Fedora Linux 42 GCC 15.1.1 Debug
   Use Experimental tag: 20261103-0712
   Updating the repository: xrootd
   Use GIT repository type
   Old revision of repository is: 6fce466a5f9b369f45ef2592c2ae246de1f13103
   New revision of repository is: 6fce466a5f9b369f45ef2592c2ae246de1f13103
   Gathering version information (one . per revision):

Configure project
   Each . represents 1024 bytes of output
    ..... Size of output: 4K
Build project
   Each symbol represents 1024 bytes of output.
   '!' represents an error and '*' a warning.
    ..................................................  Size: 49K
    .. Size of output: 52K
   0 Compiler errors
   0 Compiler warnings
Test project xrootd/build
...
```

Use at least `-V`, because the output without it is too short to be useful.
Use `-VV` for the full output. Optionally, you can customize defaults using
environment variables, as suggested in the example below, which you can copy
into your `.bashrc`:

```bash
export CLICOLOR_FORCE=true
export CMAKE_COLOR_DIAGNOSTICS=true
export CMAKE_VERBOSE_MAKEFILE=true
export CTEST_OUTPUT_ON_FAILURE=true
export CTEST_PROGRESS_OUTPUT=true

NCORES=$(getconf NPROCESSORS_ONLN)
export CMAKE_BUILD_PARALLEL_LEVEL=${NCORES}
export CTEST_PARALLEL_LEVEL=${NCORES}
```

The `test.cmake` script empties the build directory each time it runs, so for
incremental work, build with `cmake --build build`, and run the tests with
`ctest --test-dir build`. When you configure a build by hand, you can load the
same cache file as `test.cmake` with `cmake -S . -B build -C .ci/config.cmake`.

#### Running a subset of the tests

The option `-R` of `ctest` selects the tests whose names match a regular
expression, and `-N` lists the tests without running them. The name of a test
starts with its category, thus `ctest -R '^unit::'` runs only the unit tests.
The name of a unit test also contains its component, as in the example below:

```sh
ctest --test-dir build -R '^unit::XrdCl::URLTest\.'
Test project xrootd/build
    Start 48: unit::XrdCl::URLTest.ParsesLocalUrlsAsFilesOnLocalhost
1/3 Test #48: unit::XrdCl::URLTest.ParsesLocalUrlsAsFilesOnLocalhost ...   Passed    0.02 sec
    Start 49: unit::XrdCl::URLTest.ParsesAllPartsOfRemoteUrls
2/3 Test #49: unit::XrdCl::URLTest.ParsesAllPartsOfRemoteUrls ..........   Passed    0.26 sec
    Start 50: unit::XrdCl::URLTest.RejectsMalformedUrls
3/3 Test #50: unit::XrdCl::URLTest.RejectsMalformedUrls ................   Passed    0.01 sec

100% tests passed, 0 tests failed out of 3

Total Test time (real) =   0.31 sec
```

The name of an end-to-end test contains the directory of its file. For example,
`ctest --test-dir build -R '^end-to-end::cache::'` runs the tests in
`tests/end-to-end/cache`. Add `--output-on-failure` to see the logs of a test
which fails, or set `CTEST_OUTPUT_ON_FAILURE=true` in your environment to make
this the default. The file [tests/README.md](../tests/README.md) tells how to
run one file of end-to-end tests without CTest.

#### Conditions for the tests which start a server

- Install `bash`, `curl`, `openssl` and `python3` for the end-to-end tests.
  Some tests use short Python scripts to check a server response, for
  example a JSON document. Some tests also need `jq`, `getfattr`, `setfattr`, `md5sum`, `zip`,
  `unzip`, or the programs of a Kerberos KDC (key distribution center), and
  skip when they are not available. CMake registers the test of the progress
  bar only when `script` is available. The FUSE tests need a FUSE device, and
  run only with `-DENABLE_FUSE_TESTS=ON`.
- Give the tests enough space in `TMPDIR`. The tests write their files below
  `TMPDIR`, or below `/tmp` when `TMPDIR` is not set. The tests of
  `xrdcl/cluster.bats` write a file of 2 GiB and about 400 MiB of other files.
  Set `TMPDIR` to a directory with enough space when `/tmp` is a small tmpfs.
- The admin path of `xrootd` is a Unix socket, and the path of a Unix socket
  has a limit of about 100 characters. Thus, the tests put the admin path in
  `TMPDIR` only when the path of `TMPDIR` is short, and in `/tmp` otherwise.

### Customizing the Build

#### Selecting a build type, compile flags, optional features, etc

The `test.cmake` script loads a configuration file from the `.ci` directory of
the source, and gives it to CMake with the option `-C` to fill the cache. The
file is a CMake script with only `set()` commands with the option `CACHE`. The
script uses the most specific file which it finds:

1. `<os><version>.cmake`, for example `almalinux8.cmake` on AlmaLinux 8.
2. `<os>.cmake`, for example `ubuntu.cmake` on Ubuntu.
3. `config.cmake`, the default.

The default `config.cmake` enables as many options as possible, and does not
fail when a dependency is not installed. Thus, it is sufficient in most cases.

These environment variables also change the behavior of `test.cmake`: `CC`,
`CXX`, `CXXFLAGS`, `CMAKE_ARGS`, `CMAKE_GENERATOR`, `CMAKE_BUILD_PARALLEL_LEVEL`,
`CTEST_PARALLEL_LEVEL`, and `CTEST_CONFIGURATION_TYPE`. For example, to build with
`clang` and use `ninja` as the CMake generator, run:

```sh
CC=clang CXX=clang++ CMAKE_GENERATOR=Ninja ctest -V -S test.cmake
```

For performance analysis and profiling with `perf`, we recommend this build:

```sh
CXXFLAGS='-fno-omit-frame-pointer' ctest -V -C RelWithDebInfo -S test.cmake
```

For link-time optimizations (LTO), we recommend these flags:

```
CXXFLAGS='-flto -Werror=odr -Werror=lto-type-mismatch -Werror=strict-aliasing'
```

The flags change some important warnings into errors, to prevent problems at
run time with LTO. The manual page of GCC describes each of these warnings.
Give extra CMake options in `CMAKE_ARGS`, as shown below:

```sh
CMAKE_ARGS="-DENABLE_XRDEC=1" ctest -V -S test.cmake
```

The options in `CMAKE_ARGS` have higher precedence than the options of the
cache file. Thus, you can use `CMAKE_ARGS` to override the defaults without
editing the cache file.

#### Building with a sanitizer

Use `-DSANITIZE=<type>` to build with a sanitizer, where `<type>` is what the
compiler expects after `-fsanitize=`.

```sh
ctest -V -DSANITIZE=address -S test.cmake
CC=clang CXX=clang++ ctest -V -DSANITIZE=thread -S test.cmake
```

`test.cmake` prepares the build and the environment for both GCC and Clang:

- It selects the matching `CTEST_MEMORYCHECK_TYPE`, so that the tests run under
  `ctest_memcheck`, and the findings can be sent to CDash with `-DCDASH=1`.
- It sets the options of the runtime in `ASAN_OPTIONS` and the other
  `<RUNTIME>_OPTIONS` variables. The undefined behavior sanitizer needs
  `halt_on_error=1`. Without it, the sanitizer reports a finding and
  continues, and the test passes.
- It turns the leak detection of the address sanitizer off (`detect_leaks=0`).
  Use `-DSANITIZE=leak` to find leaks.
- It leaves out the `vptr` check of the undefined behavior sanitizer. That
  check refers to the type information of every polymorphic class it sees.
  `XrdUtils` only declares `XrdOss`, `XrdOssDF`, and `XrdSecProtector`, so the
  link fails with `undefined reference to typeinfo for ...`.
- With Clang, it links the libraries against the shared runtime with
  `-shared-libsan`, and adds the directory of the runtime to the RPATH. Clang
  links a static runtime into executables only, and leaves shared libraries
  with undefined sanitizer symbols. A program which is not instrumented, like
  the interpreter which runs the tests for the Python bindings, cannot resolve
  them.
- It sets `verify_asan_link_order=0`. Such a program receives the runtime only
  when it opens one of the libraries, and the address sanitizer rejects this
  by default.
- It raises the reserve of static thread-local storage with `GLIBC_TUNABLES`.
  Without it, the same program cannot open a library built with the thread or
  leak sanitizer, and fails with `cannot allocate memory in static TLS block`.
- With Clang, it leaves the sources under `vendor/` uninstrumented, through the
  special case list in [.ci/ignorelist.txt](../.ci/ignorelist.txt). GCC has no
  equivalent option.

To silence a known finding, add an entry to `.ci/<runtime>.supp`, where
`<runtime>` is `asan`, `lsan`, `msan`, `tsan`, or `ubsan`. `test.cmake` gives
the file to the runtime with the `suppressions` option when the file exists.
Prefer a suppression to turning a whole check off. Prefer both to an entry in
the special case list, which removes the instrumentation altogether.

The address sanitizer runs with `detect_odr_violation=1`, which reports a
global defined twice only when the two definitions have different sizes. A few
components are compiled into both a library and a program which loads it, and
the plug-in loader expects every library to define the version of its own entry
points. Those duplicates are normal, and level 2 reports all of them.

#### Enabling coverage, memory checking, and static analysis

`test.cmake` has several options which change the build that it tests. The
table below shows the main options:

| Option                   | Description                                |
|:------------------------:|:-------------------------------------------|
| **-DCDASH=ON**           | Enables build result submission to CDash   |
| **-DCOVERAGE=ON**        | Enables test coverage analysis with gcov   |
| **-DMEMCHECK=ON**        | Enables memory checking with valgrind      |
| **-DSTATIC_ANALYSIS=ON** | Enables static analysis with clang-tidy    |
| **-DSANITIZE=<type>**    | Enables `<type>` sanitizer (e.g. thread)   |
| **-DINSTALL=ON**         | Enables an extra step to call make install |

With coverage enabled, `test.cmake` writes a report with `gcovr` to the
`coverage/` directory inside the build directory. Open the file
`coverage/index.html` in a web browser. Pass `-DGCOVR=0` to the script to
skip the report. When `gcovr` is not found, the script also skips the report.
Use a debug build for coverage. Debug is the default build type of `test.cmake`.

Give the CMake build type with the option `-C` of `ctest`. For example, to run
a coverage build in debug mode, and show the output of the failing tests, run:

```sh
ctest -V --output-on-failure -C Debug -DCOVERAGE=ON -S test.cmake
```

With memory checking enabled, the tests run under `valgrind`, instead of the
normal run. The logs of `valgrind` are in the build directory at
`build/Testing/Temporary/MemoryChecker.<#>.log`, where `<#>` is the number of
the test in the output of `ctest`.

Static analysis needs `clang-tidy`. The option `-DSTATIC_ANALYSIS=ON` sets
`CMAKE_CXX_CLANG_TIDY` for the build. If `clang-tidy` is not in `PATH`, set
`CMAKE_CXX_CLANG_TIDY` by hand instead of using the option. XRootD has a simple
configuration file for `clang-tidy`, with only `clang-analyzer-core.*` enabled.
The warnings and errors of `clang-tidy` are part of the output of the build.
Thus, use full verbosity with static analysis.

The option `-DINSTALL=ON` adds a step which installs the build into the
directory `install/` inside the build directory. Inspect the contents of this
directory to make sure that the installation works as expected:

```sh
xrootd $ ctest -DINSTALL=1 -S test.cmake
xrootd $ tree -L 3 -d build/install
build/install
└── usr
    ├── bin
    ├── include
    │   └── xrootd
    ├── lib
    │   └── python3.11
    ├── lib64
    └── share
        ├── man
        └── xrootd

11 directories
```

### Dependencies Required for Coverage, Memory Checking, and Static Analysis

#### RPM-based distributions

For memory checking, `test.cmake` needs `valgrind`, and for static analysis,
it needs `clang-tidy`:

```sh
dnf install clang clang-tools-extra valgrind
```

For coverage, install `gcov` and `gcovr` (optional). For `gcovr` specifically,
if not available in your distro (it is available on Alma 10 and Fedora at least),
you can install it with `pip`. See https://gcovr.com/en/stable/installation.html
for more information.

#### DEB-based distributions

On Debian, Ubuntu, and derivatives, install the extra dependencies with:

```sh
apt install clang clang-tidy valgrind gcov gcovr
```
