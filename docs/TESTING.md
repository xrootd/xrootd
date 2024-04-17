## Configuring and Running XRootD tests with CTest

XRootD tests are divided into two main categories: unit and integration
tests that can be run directly with CTest, and containerized tests that
are required to be run from within a container built with docker or podman.
This document describes how to run the former, that is, the tests that are
run just with CTest. This document assumes you are already familiar with
how to build XRootD from source. If you need instructions on how to do that,
please see the [INSTALL.md](INSTALL.md) file. There you will also find a full
list of optional features and which dependencies are required to enable them.

### Enabling tests during CMake configuration

XRootD unit and integration tests are enabled via the CMake configuration
option `-DENABLE_TESTS=ON`. Unit and integration tests may depend on CppUnit
or GoogleTest (a migration from CppUnit to GoogleTest is ongoing). Therefore,
the development packages for CppUnit and GoogleTest (i.e. `cppunit-devel` and
`gtest-devel` on RPM-based distributions) are needed in order to enable all
available tests. Here we discuss how to use the [test.cmake](../test.cmake)
CTest script to run all steps to configure and build the project, then run all
tests using CTest. The script [test.cmake](../test.cmake) can be generically
called from the top directory of the repository as shown below

```sh
xrootd $ ctest -V -S test.cmake
-- Using CMake cache file config.cmake
Run dashboard with model Experimental
   Source directory: xrootd
   Build directory: xrootd/build
   Reading ctest configuration file: xrootd/CTestConfig.cmake
   Site: example.cern.ch (Linux - x86_64)
   Build name: Linux GCC 12.3.1 RelWithDebInfo
   Use Experimental tag: 20230622-0712
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
      Start  1: XrdCl::URLTest.LocalURLs
 1/23 Test  #1: XrdCl::URLTest.LocalURLs .....................   Passed    0.01 sec
      Start  2: XrdCl::URLTest.RemoteURLs
 2/23 Test  #2: XrdCl::URLTest.RemoteURLs ....................   Passed    0.12 sec
      Start  3: XrdCl::URLTest.InvalidURLs
 3/23 Test  #3: XrdCl::URLTest.InvalidURLs ...................   Passed    0.01 sec
      Start  4: XrdHttpTests.checksumHandlerTests
 4/23 Test  #4: XrdHttpTests.checksumHandlerTests ............   Passed    0.01 sec
      Start  5: XrdHttpTests.checksumHandlerSelectionTest
 5/23 Test  #5: XrdHttpTests.checksumHandlerSelectionTest ....   Passed    0.01 sec
      Start  6: XrdCl::Poller
 6/23 Test  #6: XrdCl::Poller ................................   Passed    5.01 sec
      Start  7: XrdCl::Socket
 7/23 Test  #7: XrdCl::Socket ................................   Passed    0.02 sec
      Start  8: XrdCl::Utils
 8/23 Test  #8: XrdCl::Utils .................................   Passed    8.01 sec
      Start  9: XrdEc::AlignedWriteTest
 9/23 Test  #9: XrdEc::AlignedWriteTest ......................   Passed    0.06 sec
      Start 10: XrdEc::SmallWriteTest
10/23 Test #10: XrdEc::SmallWriteTest ........................   Passed    0.06 sec
      Start 11: XrdEc::BigWriteTest
11/23 Test #11: XrdEc::BigWriteTest ..........................   Passed    0.05 sec
      Start 12: XrdEc::VectorReadTest
12/23 Test #12: XrdEc::VectorReadTest ........................   Passed    0.06 sec
      Start 13: XrdEc::IllegalVectorReadTest
13/23 Test #13: XrdEc::IllegalVectorReadTest .................   Passed    0.06 sec
      Start 14: XrdEc::AlignedWrite1MissingTest
14/23 Test #14: XrdEc::AlignedWrite1MissingTest ..............   Passed    0.06 sec
      Start 15: XrdEc::AlignedWrite2MissingTest
15/23 Test #15: XrdEc::AlignedWrite2MissingTest ..............   Passed    0.05 sec
      Start 16: XrdEc::AlignedWriteTestIsalCrcNoMt
16/23 Test #16: XrdEc::AlignedWriteTestIsalCrcNoMt ...........   Passed    0.06 sec
      Start 17: XrdEc::SmallWriteTestIsalCrcNoMt
17/23 Test #17: XrdEc::SmallWriteTestIsalCrcNoMt .............   Passed    0.06 sec
      Start 18: XrdEc::BigWriteTestIsalCrcNoMt
18/23 Test #18: XrdEc::BigWriteTestIsalCrcNoMt ...............   Passed    0.06 sec
      Start 19: XrdEc::AlignedWrite1MissingTestIsalCrcNoMt
19/23 Test #19: XrdEc::AlignedWrite1MissingTestIsalCrcNoMt ...   Passed    0.06 sec
      Start 20: XrdEc::AlignedWrite2MissingTestIsalCrcNoMt
20/23 Test #20: XrdEc::AlignedWrite2MissingTestIsalCrcNoMt ...   Passed    0.06 sec
      Start 21: XRootD::start
21/23 Test #21: XRootD::start ................................   Passed    0.01 sec
      Start 23: XRootD::smoke-test
22/23 Test #23: XRootD::smoke-test ...........................   Passed    1.63 sec
      Start 22: XRootD::stop
23/23 Test #22: XRootD::stop .................................   Passed    1.00 sec

100% tests passed, 0 tests failed out of 23

Total Test time (real) =  16.55 sec
```

For full verbose output, use `-VV` instead of `-V`. We recommend using at least `-V`
to add some verbosity. The output is too terse to be useful otherwise.

### Customizing the Build

#### Selecting a build type, compile flags, optional features, etc

Since the script is targeted for usage with continuous integration, it tries to
load a configuration file from the `.ci` subdirectory in the source directory.
The default configuration is in the `config.cmake` file. This file is used to
pre-load the CMake cache. If found, it is passed to CMake during configuration
via the `-C` option. This file is a CMake script that should only contain CMake
`set()` commands using the `CACHE` option to populate the cache. Some effort is
made to detect and use a more specific configuration file than the generic
`config.cmake` that is used by default. For example, on Ubuntu, a file named
`ubuntu.cmake` will be used if present. The script also tries to detect the
version of the OS and use a more specific file if found for that version. For
example, on Alma Linux 8, one could use `almalinux8.cmake` which would have
higher precedence than `almalinux.cmake`. The default `config.cmake` file will
enable as many options as possible without failing if the dependencies are not
installed, so it should be sufficient in most cases.

The behavior of the [test.cmake](../test.cmake) script can also be influenced
by environment variables like `CC`, `CXX`, `CXXFLAGS`, `CMAKE_ARGS`, `CMAKE_GENERATOR`,
`CMAKE_BUILD_PARALLEL_LEVEL`, `CTEST_PARALLEL_LEVEL`, and `CTEST_CONFIGURATION_TYPE`.
These are mostly self-explanatory and can be used to override the provided defaults.
For example, to build with `clang` and use `ninja` as CMake generator, one can run:

```sh
xrootd $ env CC=clang CXX=clang++ CMAKE_GENERATOR=Ninja ctest -V -S test.cmake
```

For performance analysis and profiling with `perf`, we recommend building with

```sh
xrootd $ CXXFLAGS='-fno-omit-frame-pointer' ctest -V -C RelWithDebInfo -S test.cmake
```

For enabling link-time optimizations (LTO), we recommend using
```
CXXFLAGS='-flto -Werror=odr -Werror=lto-type-mismatch -Werror=strict-aliasing'
```

This turns some important warnings into errors to avoid potential runtime issues
with LTO. Please see GCC's manual page for descriptions of each of the warnings
above. XRootD also support using address and thread sanitizers, via the options
`-DENABLE_ASAN=ON` and `-DENABLE_TSAN=ON`, respectively. These should be enabled
using `CMAKE_ARGS`, as shown below

```sh
$ env CMAKE_ARGS="-DENABLE_TSAN=1" ctest -V -S test.cmake
```

Note that options passed by setting `CMAKE_ARGS` in the environment have higher
precedence than what is in the pre-loaded cache file, so this method can be used
to override the defaults without having to edit the pre-loaded cache file.

#### Enabling coverage, memory checking, and static analysis

The [test.cmake](../test.cmake) has are several options that allow the developer
to customize the build being tested. The main options are shown in the table
below:

| Option                   | Description                                |
|:------------------------:|:-------------------------------------------|
| **-DCOVERAGE=ON**        | Enables test coverage analysis with gcov   |
| **-DMEMCHECK=ON**        | Enables memory checking with valgrind      |
| **-DSTATIC_ANALYSIS=ON** | Enables static analysis with clang-tidy    |
| **-DINSTALL=ON**         | Enables an extra step to call make install |

When enabling coverage, a report is generated by default in the `html/`
directory inside the build directory. The results can then be viewed by
opening the file `html/coverage_details.html` in a web browser. The report
generation step can be disabled by passing the option `-DGCOVR=0` to the
script. If `gcovr` is not found, the step will be skipped automatically.
It is recommended to use a debug build to generate the coverage analysis.

The CMake build type can be specified directly on the command line with the
`-C` option of `ctest`. For example, to run a coverage build in debug mode,
showing test output when a test failure happens, one can run:

```sh
xrootd $ ctest -V --output-on-failure -C Debug -DCOVERAGE=ON -S test.cmake
```

Memory checking is performed with `valgrind` when it is enabled. In this case,
the tests are run twice, once as usual, and once with `valgrind`. The output
logs from running the tests with `valgrind` can be found in the build directory 
at `build/Testing/Temporary/MemoryChecker.<#>.log` where `<#>` corresponds to
the test number as shown when running `ctest`.

Static analysis requires `clang-tidy` and is enabled by setting `CMAKE_CXX_CLANG_TIDY`
for the build. If `clang-tidy` is not in the standard `PATH`, then it may be
necessary to set it manually instead of using the option `-DSTATIC_ANALYSIS=ON`.
For the moment XRootD does not provide yet its own configuration file for
`clang-tidy`, so the defaults will be used for the build. Warnings and errors
coming from static analysis will be shown as part of the regular build, so it is
important to enable full verbosity when enabling static analysis to be able to
see the output from `clang-tidy`.

The option `-DINSTALL=ON` will enable a step to perform a so-called staged
installation inside the build directory. It can be used to check if the
installation procedure is working as expected, by inspecting the contents of the
`install/` directory inside the build directory after installation:

```sh
xrootd $ ctest -DINSTALL=1 -S test.cmake
   Each . represents 1024 bytes of output
    ..... Size of output: 4K
   Each symbol represents 1024 bytes of output.
   '!' represents an error and '*' a warning.
    ..................................................  Size: 49K
    .. Size of output: 52K
   Each symbol represents 1024 bytes of output.
   '!' represents an error and '*' a warning.
    ............................................*!....  Size: 49K
    . Size of output: 50K
Error(s) when building project
xrootd $ tree -L 3 -d build/install
build/install
└── usr
    ├── bin
    ├── include
    │   └── xrootd
    ├── lib
    │   └── python3.11
    ├── lib64
    └── share
        ├── man
        └── xrootd

11 directories
```

Note that, as shown above, `CTest` erroneously shows build errors when
installing XRootD with this command. This is because of a deprecation
warning emitted by `pip` while installing the Python bindings and can be
safely ignored.

### Dependencies Required for Coverage, Memory Checking, and Static Analysis

#### RPM-based distributions: RedHat, Fedora, CentOS, Alma, Rocky

The [test.cmake](../test.cmake) script may also need some extra dependencies for
some of its features. For memory checking, `valgrind` is needed, and for static
analysis, `clang-tidy` is needed:

```sh
dnf install \
    clang \
    clang-tools-extra \
    valgrind
```

For coverage, you need to install `gcovr`. It is not available via `yum`/`dnf`,
but can be installed with `pip`. See https://gcovr.com/en/stable/installation.html
for more information.

Dependencies to run containerized tests with `podman` on RHEL 8/9 and derivatives
can be installed with `dnf groupinstall 'Container Management'`. On CentOS 7 and
RHEL 7, one can use `docker` by installing it with `yum install docker`. In this
case, you will need to ensure that your user is in the `docker` group so that
you can run docker without using `sudo`, and that the system daemon for Docker
is running. A quick test to check if everything is correctly setup is to try to
start a busybox image: `docker run --rm -it busybox`.

#### DEB-based distributions: Debian 11, Ubuntu 22.04

On Debian, Ubuntu, and derivatives, The extra dependencies to use with the [test.cmake](../test.cmake) script can be
installed with:

```sh
apt install clang clang-tidy valgrind gcovr
```

Dependencies to run containerized tests can be installed with
```sh
apt install podman
```

## Running XRootD Tests on other platforms with Docker and/or Podman

If you would like to run XRootD tests on other platforms, you can use
the `xrd-docker` script and associated `Dockerfile`s in the `docker/`
subdirectory. The steps needed are described below.

### Create an XRootD tarball to build in the container

The first thing that needs to be done is packaging a tarball with the version
of XRootD to be used to build in the container image. The command `xrd-docker package`
by default creates a tarball named `xrootd.tar.gz` in the current directory using the
`HEAD` of the currently checked branch. We recommend changing directory to the `docker/`
directory in the XRootD git repository in order to run these commands. Suppose
we would like to run the tests for release v5.6.4. Then, we would run
```sh
$ xrd-docker package v5.6.4
```
to create the tarball that will be used to build the container image. The
tarball created by this command is a standard tarball created with `git archive`.
Inside it, the `VERSION` file contains the expanded version which is used by the
new spec file to detect the version of XRootD being built. You can also create a
source RPM with such tarballs, but they must be built with `rpmbuild --with git`
as done in the CI builds and the `Dockerfile`s in the `docker/build/` subdirectory.

### Build the container image

The next step is to build the container image for the desired OS. It can be built
with either `docker` or `podman`. The `xrd-docker` script has the `build` command
to facilitate this. Currently, supported OSs for building are CentOS 7, AlmaLinux 8,
AlmaLinux 9, Fedora. The command to build the image is simply
```sh
$ xrd-docker build <OS>
```
where `<OS>` is one of `centos7` (default), `alma8`, `alma9`, or `fedora`. The
name simply chooses which `Dockerfile` is used from the `build/` directory, as
they are named `Dockerfile.<OS>` for each suported OS. It is possible to add new
`Dockerfile`s following this same naming scheme to support custom setups and
still use `xrd-docker build` command to build an image. The images built with
`xrd-docker build` are named simply `xrootd` (latest being a default tag added
by docker), and an extra `xrootd:<OS>` tag is added to allow having it built for
multiple OSs at the same time. The current `Dockerfile`s use the spec file and
build the image using the RPM packaging workflow, installing dependencies as
declared in the spec file, in the first stage, building the RPMs in a second
stage, then, in a third stage starting from a fresh image, the RPMs built in
stage 2 are copied over and installed with `yum` or `dnf`.

#### Switching between `docker` and `podman` if both are installed

The `xrd-docker` script takes either `docker` or `podman` if available, in this
order. If you have only one of the two installed, everything should work without
any extra setup, but if you have both installed and would like to use `podman`
instead of `docker` for building the images, it can be done by exporting an
environment variable:
```sh
$ export DOCKER=$(command -v podman)
$ xrd-docker build # uses podman from now on...
```

### Appendix

#### Setting up `podman`

Unlike `docker`, `podman` may not work out of the box after installing it. If it
doesn't, make sure that you have subuids and subgids setup for your user by
running, for example, the two commands below:

```sh
$ sudo usermod --add-subuids 1000000-1000999999 $(id -un)
$ sudo usermod --add-subgids 1000000-1000999999 $(id -un)
```

You may also have to ensure that container registries in
`/etc/containers/registries.conf`. Usually a usable configuration can be renamed
from `/etc/containers/registries.conf.example`.

Finally, you may want to try container runtimes other than the default. If you
still have problems getting started, `podman`'s documentation can be found at
`https://podman.io/docs`.
