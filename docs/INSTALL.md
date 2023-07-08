## Building and Installing XRootD from Source Code

### XRootD Required and Optional Build Dependencies

The required build-time dependencies are: bash, cmake, make, a C++ compiler with
support for C++14, kernel headers from the OS, and development packages for
libuuid, OpenSSL, and zlib. The optional features are shown in the table below
together with the dependencies required to enable them.

| Feature               | Dependencies                             |
|:----------------------|:-----------------------------------------|
| FUSE support          | fuse-devel                               |
| HTTP support (client) | davix-devel                              |
| HTTP support (server) | libcurl-devel                            |
| Erasure coding        | isa-l / autoconf automake libtool yasm   |
| Kerberos5             | krb5-devel                               |
| Macaroons             | json-c-devel libmacaroons-devel          |
| Python bindings       | python3-devel python3-setuptools         |
| readline              | readline-devel                           |
| SciTokens             | scitokens-cpp-devel                      |
| systemd support       | systemd-devel                            |
| VOMS                  | voms-devel                               |
| Testing               | cppunit-devel gtest-devel                |

Other optional dependencies: tinyxml-devel, libxml2-devel. These have bundled
copies which are used if not found in the system. In the following sections, we
show how to install all available dependencies in most of the supported operating
systems.

#### RPM-based distributions: RedHat, Fedora, CentOS, Alma, Rocky

On RedHat Enterprise Linux and derivatives, all dependencies are available,
except for Intel's [isa-l](https://github.com/intel/isa-l) library. You may
build and install isa-l from source, but alternatively, you can simple install
isa-l dependencies (i.e. `autoconf`, `automake`, `libtool`, and `yasm`) and let
the bundled isa-l be built along XRootD. On Fedora, it's not necessary to
install the `epel-release` package, but on most others it is required, as some
dependencies are only available in [EPEL](https://docs.fedoraproject.org/en-US/epel/).
It may also be necessary to enable other repositories manually if they are not
already enabled by default, like the `PowerTools`, `crb`, or `scl` repositories.
On CentOS 7, this can be done by installing `centos-release-scl` in addition to
`epel-release`. The command to install all dependencies is shown below. You may,
however, need to replace `dnf` with `yum` or prepend `sudo` to this command,
depending on the distribution:

```sh
dnf install \
    cmake \
    cppunit-devel \
    curl-devel \
    davix-devel \
    diffutils \
    file \
    fuse-devel \
    gcc-c++ \
    git \
    gtest-devel \
    json-c-devel \
    krb5-devel \
    libmacaroons-devel \
    libtool \
    libuuid-devel \
    libxml2-devel \
    make \
    openssl-devel \
    python3-devel \
    python3-setuptools \
    readline-devel \
    scitokens-cpp-devel \
    systemd-devel \
    tinyxml-devel \
    voms-devel \
    yasm \
    zlib-devel
```

On CentOS 7, the default compiler is too old, so `devtoolset-11` should be used
instead. It can be enabled afterwards with `source /opt/rh/devtoolset-11/enable`.
Moreover, `cmake` installs CMake 2.x on CentOS 7, so `cmake3` needs to be installed
instead and `cmake3`/`ctest3` used in the command line.

#### DEB-based distrubutions: Debian 11, Ubuntu 22.04

On Debian 11 and Ubuntu 22.04, all necessary dependencies are available in the
system. In earlier versions, some of XRootD's optional features may have to be
disabled if their dependencies are not available to be installed via apt.

```sh
apt install \
    cmake \
    davix-dev \
    g++ \
    libcppunit-dev \
    libcurl4-openssl-dev \
    libfuse-dev \
    libgtest-dev \
    libisal-dev \
    libjson-c-dev \
    libkrb5-dev \
    libmacaroons-dev \
    libreadline-dev \
    libscitokens-dev \
    libssl-dev \
    libsystemd-dev \
    libtinyxml-dev \
    libxml2-dev \
    make \
    pkg-config \
    python3-dev \
    python3-setuptools \
    uuid-dev \
    voms-dev \
    zlib1g-dev
```

### Homebrew on macOS

On macOS, XRootD is available to install via Homebrew. We recommend using it to
install dependencies as well when building XRootD from source:

```sh
brew install \
    cmake \
    cppunit \
    curl \
    davix \
    gcc \
    googletest \
    isa-l \
    krb5 \
    libxml2 \
    libxcrypt \
    make \
    openssl@1.1 \
    pkg-config \
    python@3.11 \
    readline \
    zlib \
```

Homebrew is also available on Linux, where `utils-linux` is required as
an extra dependency since uuid symbols are not provided by the kernel like
on macOS. On Linux, `libfuse@2` may be installed to enable FUSE support.

## Building from Source Code with CMake

XRootD uses [CMake](https://cmake.org) as its build generator. CMake
is used during configuration to generate the actual build system that
is used to build the project with a build tool like `make` or `ninja`.
If you are new to CMake, we recommend reading the official
[tutorial](https://cmake.org/cmake/help/latest/guide/tutorial/index.html)
which provides a step-by-step guide on how to get started with CMake.
For the anxious user, assuming the repository is cloned into the `xrootd`
directory under the current working directory, the basic workflow is

```sh
cmake -S xrootd -B xrootd_build
cmake --build xrootd_build --parallel
cmake --install xrootd_build
```

If you'd like to install somewhere other than the default of `/usr/local`,
then you need to pass the option `-DCMAKE_INSTALL_PREFIX=<installdir>` to
the first command, with the location where you'd like to install as argument.

The table below documents the main build options that can be used to customize
the build:

|    CMake Option    | Default | Description
|:-------------------|:--------|:--------------------------------------------------------------
| `ENABLE_FUSE`      |  TRUE   | Enable FUSE filesystem driver
| `ENABLE_HTTP`      |  TRUE   | Enable HTTP component (XrdHttp)
| `ENABLE_KRB5`      |  TRUE   | Enable Kerberos 5 authentication
| `ENABLE_MACAROONS` |  TRUE   | Enable Macaroons plugin (server only)
| `ENABLE_PYTHON`    |  TRUE   | Enable building of the Python bindings
| `ENABLE_READLINE`  |  TRUE   | Enable readline support in the commandline utilities
| `ENABLE_SCITOKENS` |  TRUE   | Enable SciTokens plugin (server only)
| `ENABLE_VOMS`      |  TRUE   | Enable VOMS plug-in
| `ENABLE_XRDCLHTTP` |  TRUE   | Enable xrdcl-http plugin
| `ENABLE_XRDCL`     |  TRUE   | Enable XRootD client
| `ENABLE_XRDEC`     |  FALSE  | Enable support for erasure coding
| `ENABLE_ASAN`      |  FALSE  | Build with adress sanitizer enabled
| `ENABLE_TSAN`      |  FALSE  | Build with thread sanitizer enabled
| `ENABLE_TESTS`     |  FALSE  | Enable unit tests
| `FORCE_ENABLED`    |  FALSE  | Fail CMake configuration if enabled components cannot be built
| `XRDCL_LIB_ONLY`   |  FALSE  | Build only the client libraries and necessary dependencies
| `XRDCL_ONLY`       |  FALSE  | Build only the client and necessary dependencies
| `USE_SYSTEM_ISAL`  |  FALSE  | Use isa-l library installed in the system

### Running XRootD Tests

After you've built the project, you should run the unit and integration tests
with CTest to ensure that they all pass. This can be done simply by running
`ctest` from the build directory. However, we also provide a CMake script to
automate more advanced testing, including enabling a coverage report, memory checking with
`valgrind`, and static analysis with `clang-tidy`. The script is named [test.cmake](../test.cmake)
and can be found in the top directory of the repository. Its usage is documented in the file
[TESTING.md](TESTING.md).
